/////////////////////////////////////////////////////////
// File: AchievementHandler.cpp
// Date: 2026-05-26
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Implements AchievementHandler which tracks
//              achievement files for active game sessions
/////////////////////////////////////////////////////////

#include "AchievementHandler.h"
#include "../tools/parsers/CodexParser.h"
#include "../tools/Logger.h"

#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <climits>

/////////////////////////////////////////////////////////////////////

static constexpr size_t INOTIFY_BUF = (sizeof(struct inotify_event) + NAME_MAX + 1) * 32;

/////////////////////////////////////////////////////////////////////

AchievementHandler::AchievementHandler() :
    m_thread(),
    m_mutex(),
    m_stateMutex(),
    m_stateCv(),
    m_inotifyFd(-1)
{
    m_running.store(false);
    m_paused.store(false);
}

AchievementHandler::~AchievementHandler()
{
    Stop();

    // Free all parsers before clearing the map
    for (auto& [id, parser] : m_parsers)
    {
        delete parser;
    }
    m_parsers.clear();
}

/////////////////////////////////////////////////////////////////////
////////////////////////////// PUBLIC ///////////////////////////////
/////////////////////////////////////////////////////////////////////

void AchievementHandler::Init()
{
    // Create non-blocking inotify instance with close-on-exec flag
    m_inotifyFd = inotify_init1(IN_CLOEXEC | IN_NONBLOCK);
    if (m_inotifyFd == -1)
    {
        Logger::Error("[AchievementHandler] inotify_init1 failed: " + std::string(strerror(errno)));
        return;
    }

    Logger::Log("[AchievementHandler] Initialized.");
}

/////////////////////////////////////////////////////////////////////

void AchievementHandler::Start()
{
    if (m_running.load())
    {
        return;
    }

    if (m_inotifyFd == -1)
    {
        Logger::Error("[AchievementHandler] Cannot start: not initialized.");
        return;
    }

    // Launch the inotify watch loop on a background thread
    m_running.store(true);
    m_paused.store(false);
    m_thread = std::thread(&AchievementHandler::WatchLoop, this);
    Logger::Log("[AchievementHandler] Started.");
}

/////////////////////////////////////////////////////////////////////

void AchievementHandler::Pause()
{
    m_paused.store(true);
    m_stateCv.notify_one();
    Logger::Log("[AchievementHandler] Paused.");
}

/////////////////////////////////////////////////////////////////////

void AchievementHandler::Resume()
{
    m_paused.store(false);
    m_stateCv.notify_one();
    Logger::Log("[AchievementHandler] Resumed.");
}

/////////////////////////////////////////////////////////////////////

void AchievementHandler::Stop()
{
    if (!m_running.load())
    {
        return;
    }

    // Signal the watch loop to exit and close inotify fd to unblock any pending read
    m_running.store(false);
    m_stateCv.notify_one();

    if (m_inotifyFd != -1)
    {
        close(m_inotifyFd);
        m_inotifyFd = -1;
    }

    if (m_thread.joinable())
    {
        m_thread.join();
    }

    Logger::Log("[AchievementHandler] Stopped.");
}

/////////////////////////////////////////////////////////////////////

void AchievementHandler::AddTarget(int targetId, const std::string& appIdDirPath, const std::string& emulatorType)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    // Skip if this target is already being tracked
    if (m_sessions.count(targetId))
    {
        Logger::Log("[AchievementHandler] Target already tracked: targetId=" + std::to_string(targetId));
        return;
    }

    // Instantiate the appropriate parser for the emulator type
    AchievementParser* parser = CreateParser(emulatorType);
    if (!parser)
    {
        Logger::Log("[AchievementHandler] No parser for emulator type: " + emulatorType);
        return;
    }

    WatchSession session;
    session.targetId = targetId;
    session.appIdDirPath = appIdDirPath;
    session.emulatorType = emulatorType;

    // Watch directory for file creation/move
    session.dirWd = inotify_add_watch(m_inotifyFd, appIdDirPath.c_str(), IN_CREATE | IN_MOVED_TO);
    if (session.dirWd == -1)
    {
        Logger::Error("[AchievementHandler] inotify_add_watch (dir) failed for " + appIdDirPath + ": " + strerror(errno));
        delete parser;
        return;
    }

    m_wdToTarget[session.dirWd] = targetId;
    m_parsers[targetId] = parser;
    m_sessions[targetId] = std::move(session);

    // If achievement file already exists: initial read + add file watch
    WatchSession& stored = m_sessions[targetId];
    const std::string filePath = stored.appIdDirPath + "/" + m_parsers[targetId]->GetFileName();
    if (access(filePath.c_str(), F_OK) == 0)
    {
        AddFileWatch(stored);
        ReadInitial(stored);
    }

    Logger::Log("[AchievementHandler] Target added: targetId=" + std::to_string(targetId) + " emu=" + emulatorType);
}

/////////////////////////////////////////////////////////////////////

void AchievementHandler::RemoveTarget(int targetId)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_sessions.find(targetId);
    if (it == m_sessions.end())
    {
        // targetId not found in sessions
        return;
    }

    WatchSession& session = it->second;

    // Remove inotify watches for both the directory and the file
    if (session.dirWd != -1)
    {
        inotify_rm_watch(m_inotifyFd, session.dirWd);
        m_wdToTarget.erase(session.dirWd);
    }

    if (session.fileWd != -1)
    {
        inotify_rm_watch(m_inotifyFd, session.fileWd);
        m_wdToTarget.erase(session.fileWd);
    }

    delete m_parsers[targetId];
    m_parsers.erase(targetId);
    m_sessions.erase(it);

    Logger::Log("[AchievementHandler] Target removed: targetId=" + std::to_string(targetId));
}

/////////////////////////////////////////////////////////////////////

std::vector<AchievementData> AchievementHandler::PollUnhandled(int targetId)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    std::vector<AchievementData> result;

    auto it = m_sessions.find(targetId);
    if (it == m_sessions.end())
    {
        return result;
    }

    // Collect changed entries and mark them as handled
    for (auto& [key, ach] : it->second.achievements)
    {
        if (!ach.handled)
        {
            result.push_back(ach);
            ach.handled = true;
        }
    }

    return result;
}

/////////////////////////////////////////////////////////////////////
///////////////////////////// PRIVATE ///////////////////////////////
/////////////////////////////////////////////////////////////////////

void AchievementHandler::WatchLoop()
{
    alignas(struct inotify_event) char buf[INOTIFY_BUF];

    while (m_running.load())
    {
        {
            std::unique_lock<std::mutex> lock(m_stateMutex);
            m_stateCv.wait(lock, [this]() {
                return !m_running.load() || !m_paused.load();
            });
        }

        if (!m_running.load())
        {
            break;
        }

        // IN_NONBLOCK: read() returns immediately with EAGAIN if no events are queued
        ssize_t n = read(m_inotifyFd, buf, INOTIFY_BUF);

        if (n < 0)
        {
            // No events available yet, back off and retry
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                std::unique_lock<std::mutex> lock(m_stateMutex);
                m_stateCv.wait_for(lock, std::chrono::milliseconds(200), [this]() {
                    return !m_running.load() || m_paused.load();
                });
                continue;
            }

            // read() may return -1 with a garbage errno after Stop() closes the fd,
            // check m_running before treating it as a real error
            if (!m_running.load())
            {
                break;
            }

            Logger::Error("[AchievementHandler] inotify read error: " + std::string(strerror(errno)));
            break;
        }

        // Walk the event buffer and dispatch each event
        for (char* p = buf; p < buf + n; )
        {
            auto* ev = reinterpret_cast<struct inotify_event*>(p);
            p += sizeof(struct inotify_event) + ev->len;

            // IN_IGNORED is sent by the kernel when a watch descriptor is removed
            // (either by us or because the watched file was deleted). Skip it.
            if (ev->mask & IN_IGNORED)
            {
                continue;
            }

            std::lock_guard<std::mutex> lock(m_mutex);
            HandleInotifyEvent(ev);
        }
    }

    Logger::Log("[AchievementHandler] WatchLoop exited.");
}

/////////////////////////////////////////////////////////////////////

void AchievementHandler::HandleInotifyEvent(const struct inotify_event* ev)
{
    // Reverse-lookup which session owns this watch descriptor
    auto wdIt = m_wdToTarget.find(ev->wd);
    if (wdIt == m_wdToTarget.end())
    {
        return;
    }

    const int targetId = wdIt->second;

    auto sessionIt = m_sessions.find(targetId);
    if (sessionIt == m_sessions.end())
    {
        return;
    }

    WatchSession& session = sessionIt->second;

    // ev->name is only populated for directory watches, it holds the filename
    // of the entry that triggered the event, not the directory itself
    const std::string fileName = (ev->len > 0) ? std::string(ev->name) : "";

    // Directory event: achievement file appeared
    if (ev->wd == session.dirWd && (ev->mask & (IN_CREATE | IN_MOVED_TO)))
    {
        // Ignore unrelated files created in the same directory
        const std::string expected = m_parsers[targetId]->GetFileName();
        if (fileName != expected)
        {
            return;
        }

        Logger::Log("[AchievementHandler] Achievement file appeared: targetId=" + std::to_string(targetId) + " file=" + fileName);

        // Start watching the file itself if not already doing so
        if (session.fileWd == -1)
        {
            AddFileWatch(session);
        }

        // First time seeing the file, snapshot current state silently
        if (!session.initialReadDone)
        {
            ReadInitial(session);
        }
        return;
    }

    // File event: achievement file was written
    if (ev->wd == session.fileWd && (ev->mask & (IN_CLOSE_WRITE | IN_MODIFY)))
    {
        // Prefer IN_CLOSE_WRITE over IN_MODIFY - MODIFY fires per write() call so a single save may trigger it multiple times before the file is complete
        // IN_CLOSE_WRITE fires once when the file handle is closed after writing
        Logger::Log("[AchievementHandler] Achievement file changed: targetId=" + std::to_string(targetId));
        ReadAndDiff(session);
        return;
    }

    // File was deleted or moved away
    if (ev->wd == session.fileWd && (ev->mask & (IN_DELETE_SELF | IN_MOVE_SELF)))
    {
        Logger::Log("[AchievementHandler] Achievement file removed: targetId=" + std::to_string(targetId));

        // Clean up the file watch
        m_wdToTarget.erase(session.fileWd);
        inotify_rm_watch(m_inotifyFd, session.fileWd);
        session.fileWd = -1;
        // Reset initial read flag
        session.initialReadDone = false;
    }
}

/////////////////////////////////////////////////////////////////////

void AchievementHandler::ReadInitial(WatchSession& session)
{
    const std::string filePath = session.appIdDirPath + "/" + m_parsers[session.targetId]->GetFileName();
    std::vector<AchievementData> parsed = m_parsers[session.targetId]->Parse(filePath);

    for (const auto& ach : parsed)
    {
        // Store current state and let daemon silently sync DB without notification.
        AchievementData entry = ach;
        entry.handled = false;
        entry.newlyUnlocked = false; // Override with false, so we prevent notification spam on game/software startup
        session.achievements[ach.key] = entry;
    }

    session.initialReadDone = true;
    Logger::Log("[AchievementHandler] Initial read done: targetId=" + std::to_string(session.targetId) + " count=" + std::to_string(parsed.size()));
}

/////////////////////////////////////////////////////////////////////

void AchievementHandler::ReadAndDiff(WatchSession& session)
{
    const std::string filePath = session.appIdDirPath + "/" + m_parsers[session.targetId]->GetFileName();
    std::vector<AchievementData> parsed = m_parsers[session.targetId]->Parse(filePath);

    int newUnlocks = 0;

    for (const auto& ach : parsed)
    {
        auto it = session.achievements.find(ach.key);

        const bool wasAchieved = (it != session.achievements.end()) && it->second.achieved;
        const bool progressChanged = (it != session.achievements.end()) && (it->second.curProgress != ach.curProgress || it->second.maxProgress != ach.maxProgress);

        if (ach.achieved && !wasAchieved)
        {
            // Newly unlocked during this session, flag for notification
            AchievementData entry = ach;
            entry.handled = false;
            entry.newlyUnlocked = true;
            session.achievements[ach.key] = entry;
            newUnlocks++;

            Logger::Log("[AchievementHandler] Achievement unlocked: targetId=" + std::to_string(session.targetId) + " key=" + ach.key);
        }
        else if (it == session.achievements.end())
        {
            // Achievement not seen before
            AchievementData entry = ach;
            entry.handled = false;
            session.achievements[ach.key] = entry;
        }
        else if (progressChanged)
        {
            AchievementData entry = ach;
            entry.handled = false;
            session.achievements[ach.key] = entry;
        }
    }

    if (newUnlocks > 0)
    {
        Logger::Log("[AchievementHandler] Diff done: targetId=" + std::to_string(session.targetId) + " newUnlocks=" + std::to_string(newUnlocks));
    }
}

/////////////////////////////////////////////////////////////////////

void AchievementHandler::AddFileWatch(WatchSession& session)
{
    const std::string filePath = session.appIdDirPath + "/" + m_parsers[session.targetId]->GetFileName();

    // Register inotify watch for writes, deletions and renames on the achievement file
    session.fileWd = inotify_add_watch(m_inotifyFd, filePath.c_str(), IN_CLOSE_WRITE | IN_MODIFY | IN_DELETE_SELF | IN_MOVE_SELF);
    if (session.fileWd == -1)
    {
        Logger::Error("[AchievementHandler] inotify_add_watch (file) failed: " + std::string(strerror(errno)));
        return;
    }

    m_wdToTarget[session.fileWd] = session.targetId;
}

/////////////////////////////////////////////////////////////////////

AchievementParser* AchievementHandler::CreateParser(const std::string& emulatorType)
{
    // Instantiate the correct parser subclass based on emulator type
    if (emulatorType == "CODEX")
    {
        return new CodexParser();
    }

    return nullptr;
}
