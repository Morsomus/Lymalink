/////////////////////////////////////////////////////////
// File: AchievementHandler.cpp
// Date: 2026-05-26
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Implements AchievementHandler which tracks
//              achievement files for active game sessions
/////////////////////////////////////////////////////////

#include "AchievementHandler.h"
#include "Defines.h"
#include "../tools/parsers/RUNECodexParser.h"
#include "../tools/parsers/GoldbergParser.h"
#include "../tools/parsers/GoGNParser.h"
#include "../tools/parsers/RLDParser.h"
#include "../tools/Logger.h"

#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <climits>

#define COMPONENT "AchievementHandler"

/////////////////////////////////////////////////////////////////////

static constexpr size_t INOTIFY_BUF = (sizeof(struct inotify_event) + NAME_MAX + 1) * 32;
static constexpr auto MODIFY_DEBOUNCE = std::chrono::milliseconds(200);

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
        LOG_BE(Urgency::Critical, "inotify_init1 failed: %s", strerror(errno));
        return;
    }

    LOG_BE(Urgency::Debug, "Initialized.");
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
        LOG_BE(Urgency::Critical, "Cannot start: not initialized.");
        return;
    }

    // Launch the inotify watch loop on a background thread
    m_running.store(true);
    m_paused.store(false);
    m_thread = std::thread(&AchievementHandler::WatchLoop, this);
    
    LOG_BE(Urgency::Debug, "Started.");
}

/////////////////////////////////////////////////////////////////////

void AchievementHandler::Pause()
{
    m_paused.store(true);
    m_stateCv.notify_one();
    
    LOG_BE(Urgency::Debug, "Paused.");
}

/////////////////////////////////////////////////////////////////////

void AchievementHandler::Resume()
{
    m_paused.store(false);
    m_stateCv.notify_one();
    
    LOG_BE(Urgency::Debug, "Resumed.");
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

    LOG_BE(Urgency::Debug, "Stopped.");
}

/////////////////////////////////////////////////////////////////////

void AchievementHandler::AddTarget(int targetId, const std::string& appIdDirPath, const std::string& emulatorType)
{
    bool appIdDirUnavailable = false;
    bool shouldReturn = false;

    {
        std::lock_guard<std::mutex> lock(m_mutex);

        // Skip if this target is already being tracked
        if (m_sessions.count(targetId))
        {
            LOG_BE(Urgency::Debug, "Target already tracked: targetId=%d", targetId);
            return;
        }

        // Instantiate the appropriate parser for the emulator type
        AchievementParser* parser = CreateParser(emulatorType);
        if (!parser)
        {
            LOG_BE(Urgency::Critical, "No parser for emulator type: %s", emulatorType.c_str());
            return;
        }

        WatchSession session;
        session.targetId = targetId;
        session.appIdDirPath = appIdDirPath;
        session.emulatorType = emulatorType;

        // Watch directory for file creation/move and directory removal.
        session.dirWd = inotify_add_watch(m_inotifyFd, appIdDirPath.c_str(), IN_CREATE | IN_MOVED_TO | IN_DELETE_SELF | IN_MOVE_SELF);
        if (session.dirWd == -1)
        {
            const int savedErrno = errno;
            if (savedErrno == ENOENT || savedErrno == ENOTDIR)
            {
                LOG_BE(Urgency::Warning, "AppID dir unavailable for targetId=%d path=%s: %s", targetId, appIdDirPath.c_str(), strerror(savedErrno));
                appIdDirUnavailable = true;
            }
            else
            {
                LOG_BE(Urgency::Critical, "inotify_add_watch (dir) failed for %s: %s", appIdDirPath.c_str(), strerror(savedErrno));
            }
            delete parser;
            shouldReturn = true;
        }

        if (!shouldReturn)
        {
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

            LOG_BE(Urgency::Debug, "Target added: targetId=%d emu=%s", targetId, emulatorType.c_str());
        }
    }

    if (appIdDirUnavailable && onAppIdDirUnavailable)
    {
        onAppIdDirUnavailable(targetId, appIdDirPath);
    }

    if (shouldReturn)
    {
        return;
    }
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

    RemoveSessionLocked(targetId);

    LOG_BE(Urgency::Debug, "Target removed: targetId=%d", targetId);
}

/////////////////////////////////////////////////////////////////////

void AchievementHandler::RemoveSessionLocked(int targetId)
{
    auto it = m_sessions.find(targetId);
    if (it == m_sessions.end())
    {
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

std::vector<AchievementData> AchievementHandler::ReadAchievementFileOnce(int targetId, const std::string& appIdDirPath, const std::string& emulatorType)
{
    std::vector<AchievementData> parsed;

    // Manual rescan uses the same parser family without adding a persistent inotify session
    AchievementParser* parser = CreateParser(emulatorType);
    if (!parser)
    {
        return parsed;
    }

    const std::string filePath = appIdDirPath + "/" + parser->GetFileName();
    if (access(filePath.c_str(), F_OK) != 0)
    {
        LOG_BE(Urgency::Debug, "Achievement file not present for one-shot read: targetId=%d path=%s", targetId, filePath.c_str());
        delete parser;
        return parsed;
    }

    try
    {
        parsed = parser->Parse(filePath);
    }
    catch (const std::exception& e)
    {
        LOG_BE(Urgency::Warning, "Failed to parse achievement file during one-shot read: targetId=%d path=%s error=%s", targetId, filePath.c_str(), e.what());
    }

    delete parser;
    LOG_BE(Urgency::Debug, "One-shot achievement read done: targetId=%d count=%zu", targetId, parsed.size());
    return parsed;
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

        {
            std::lock_guard<std::mutex> lock(m_mutex);
            const auto now = std::chrono::steady_clock::now();
            for (auto& [targetId, session] : m_sessions)
            {
                if (session.modifyPending && session.modifyDeadline <= now)
                {
                    session.modifyPending = false;
                    LOG_BE(Urgency::Debug, "Achievement file modify debounce elapsed: targetId=%d", targetId);
                    ReadAndDiff(session);
                }
            }
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

            LOG_BE(Urgency::Critical, "inotify read error: %s", strerror(errno));
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

            std::pair<int, std::string> unavailableDir;
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                unavailableDir = HandleInotifyEvent(ev);
            }

            if (unavailableDir.first > 0 && onAppIdDirUnavailable)
            {
                onAppIdDirUnavailable(unavailableDir.first, unavailableDir.second);
            }
        }
    }

    LOG_BE(Urgency::Debug, "WatchLoop exited.");
}

/////////////////////////////////////////////////////////////////////

std::pair<int, std::string> AchievementHandler::HandleInotifyEvent(const struct inotify_event* ev)
{
    // Reverse-lookup which session owns this watch descriptor
    auto wdIt = m_wdToTarget.find(ev->wd);
    if (wdIt == m_wdToTarget.end())
    {
        return {};
    }

    const int targetId = wdIt->second;

    auto sessionIt = m_sessions.find(targetId);
    if (sessionIt == m_sessions.end())
    {
        return {};
    }

    WatchSession& session = sessionIt->second;
    const std::string appIdDirPath = session.appIdDirPath;

    if (ev->wd == session.dirWd && (ev->mask & (IN_DELETE_SELF | IN_MOVE_SELF)))
    {
        LOG_BE(Urgency::Warning, "AppID dir removed while tracked: targetId=%d path=%s", targetId, appIdDirPath.c_str());
        RemoveSessionLocked(targetId);
        return {targetId, appIdDirPath};
    }

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
            return {};
        }

        LOG_BE(Urgency::Info, "Achievement file appeared: targetId=%d file=%s", targetId, fileName.c_str());

        // Replacements can arrive before IN_MOVE_SELF for the old inode. Drop the stale watch so later changes are observed on the new file
        if (session.fileWd != -1)
        {
            LOG_BE(Urgency::Debug, "Replacing stale achievement file watch: targetId=%d oldWd=%d", targetId, session.fileWd);
            m_wdToTarget.erase(session.fileWd);
            inotify_rm_watch(m_inotifyFd, session.fileWd);
            session.fileWd = -1;
        }
        AddFileWatch(session);
        session.modifyPending = false;

        // Existing files are snapshotted silently in AddTarget(). A file that appears while tracking is active may be Emulators's first write for an unlock, so diff it against the session state
        if (!session.initialReadDone)
        {
            LOG_BE(Urgency::Debug, "Achievement file first appeared during active tracking, diffing current state: targetId=%d", targetId);
            session.initialReadDone = true;
        }
        else
        {
            LOG_BE(Urgency::Debug, "Achievement file replaced during active tracking, diffing against existing baseline: targetId=%d", targetId);
        }
        ReadAndDiff(session);
        return {};
    }

    // File event: achievement file was written
    if (ev->wd == session.fileWd && (ev->mask & IN_CLOSE_WRITE))
    {
        // IN_CLOSE_WRITE fires once when the file handle is closed after writing.
        session.modifyPending = false;
        LOG_BE(Urgency::Debug, "Achievement file changed: targetId=%d", targetId);
        ReadAndDiff(session);
        return {};
    }

    // Some writers might keep the file open. Debounce IN_MODIFY so parsing happens after writes settle instead of once per write() call
    if (ev->wd == session.fileWd && (ev->mask & IN_MODIFY))
    {
        session.modifyPending = true;
        session.modifyDeadline = std::chrono::steady_clock::now() + MODIFY_DEBOUNCE;
        return {};
    }

    // File was deleted or moved away
    if (ev->wd == session.fileWd && (ev->mask & (IN_DELETE_SELF | IN_MOVE_SELF)))
    {
        LOG_BE(Urgency::Info, "Achievement file removed: targetId=%d", targetId);

        // Clean up the file watch
        m_wdToTarget.erase(session.fileWd);
        inotify_rm_watch(m_inotifyFd, session.fileWd);
        session.fileWd = -1;
        session.modifyPending = false;
    }

    return {};
}

/////////////////////////////////////////////////////////////////////

void AchievementHandler::ReadInitial(WatchSession& session)
{
    const std::string filePath = session.appIdDirPath + "/" + m_parsers[session.targetId]->GetFileName();
    std::vector<AchievementData> parsed;
    try
    {
        parsed = m_parsers[session.targetId]->Parse(filePath);
    }
    catch (const std::exception& e)
    {
        LOG_BE(Urgency::Warning, "Failed to parse achievement file: targetId=%d path=%s error=%s", session.targetId, filePath.c_str(), e.what());
        return;
    }

    for (const auto& ach : parsed)
    {
        // Store current state and let daemon silently sync DB without notification.
        AchievementData entry = ach;
        entry.handled = false;
        entry.newlyUnlocked = false; // Override with false, so we prevent notification spam on game/software startup
        session.achievements[ach.key] = entry;
    }

    session.initialReadDone = true;
    LOG_BE(Urgency::Info, "Initial read done: targetId=%d count=%zu", session.targetId, parsed.size());
}

/////////////////////////////////////////////////////////////////////

void AchievementHandler::ReadAndDiff(WatchSession& session)
{
    const std::string filePath = session.appIdDirPath + "/" + m_parsers[session.targetId]->GetFileName();
    std::vector<AchievementData> parsed;
    try
    {
        parsed = m_parsers[session.targetId]->Parse(filePath);
    }
    catch (const std::exception& e)
    {
        LOG_BE(Urgency::Warning, "Failed to parse achievement file: targetId=%d path=%s error=%s", session.targetId, filePath.c_str(), e.what());
        return;
    }

    int newUnlocks = 0;

    for (const auto& ach : parsed)
    {
        auto it = session.achievements.find(ach.key);

        const bool wasAchieved = (it != session.achievements.end()) && it->second.achieved;
        const bool progressChanged = (it != session.achievements.end()) &&
            ((ach.hasCurProgress && (!it->second.hasCurProgress || it->second.curProgress != ach.curProgress)) ||
            (ach.hasMaxProgress && (!it->second.hasMaxProgress || it->second.maxProgress != ach.maxProgress)));

        if (ach.achieved && !wasAchieved)
        {
            // Newly unlocked during this session, flag for notification
            AchievementData entry = ach;
            entry.handled = false;
            entry.newlyUnlocked = true;
            session.achievements[ach.key] = entry;
            newUnlocks++;

            LOG_BE(Urgency::Debug, "Achievement unlocked: targetId=%d key=%s", session.targetId, ach.key.c_str());
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
        LOG_BE(Urgency::Debug, "Diff done: targetId=%d newUnlocks=%d", session.targetId, newUnlocks);
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
        LOG_BE(Urgency::Critical, "inotify_add_watch (file) failed for %s: %s", filePath.c_str(), strerror(errno));
        return;
    }

    m_wdToTarget[session.fileWd] = session.targetId;
    LOG_BE(Urgency::Debug, "Added file watch for targetId=%d path=%s", session.targetId, filePath.c_str());
}

/////////////////////////////////////////////////////////////////////

AchievementParser* AchievementHandler::CreateParser(const std::string& emulatorType)
{
    // Instantiate the correct achievement file parser subclass based on emulator type
    if (emulatorType == "CODEX" || emulatorType == "RUNE")
    {
        LOG_BE(Urgency::Debug, "Creating CODEX/RUNE parser.");
        return new RUNECodexParser();
    }
    else if (emulatorType == "GOLDBERG")
    {
        LOG_BE(Urgency::Debug, "Creating Goldberg parser.");
        return new GoldbergParser();
    }
    else if (emulatorType == "GOG-N")
    {
        LOG_BE(Urgency::Debug, "Creating GOG Nemirtingas parser.");
        return new GoGNParser();
    }
    else if (emulatorType == "RLD")
    {
        LOG_BE(Urgency::Debug, "Creating Reloaded parser.");
        return new RLDParser();
    }

    LOG_BE(Urgency::Warning, "Unknown emulator type: %s", emulatorType.c_str());
    return nullptr;
}
