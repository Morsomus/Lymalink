/////////////////////////////////////////////////////////
// File: AchievementHandlerWin.cpp
// Date: 2026-06-20
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Implements polling achievement watcher for Windows
/////////////////////////////////////////////////////////

#include "AchievementHandler.h"
#include "Defines.h"
#include "../tools/Logger.h"
#include "../tools/parsers/GoldbergParser.h"
#include "../tools/parsers/GoGNParser.h"
#include "../tools/parsers/RUNECodexParser.h"

#include <filesystem>
#include <exception>
#include <thread>

#define COMPONENT "AchievementHandler"

namespace fs = std::filesystem;

/////////////////////////////////////////////////////////////////////

AchievementHandler::AchievementHandler() :
    m_inotifyFd(-1)
{
    m_running.store(false);
    m_paused.store(false);
}

AchievementHandler::~AchievementHandler()
{
    Stop();

    for (auto& [targetId, parser] : m_parsers)
    {
        (void)targetId;
        delete parser;
    }
    m_parsers.clear();
}

/////////////////////////////////////////////////////////////////////
////////////////////////////// PUBLIC ///////////////////////////////
/////////////////////////////////////////////////////////////////////

void AchievementHandler::Init()
{
    // -
}

/////////////////////////////////////////////////////////////////////

void AchievementHandler::Start()
{
    if (m_running.exchange(true))
    {
        return;
    }

    m_thread = std::thread(&AchievementHandler::WatchLoop, this);
}

/////////////////////////////////////////////////////////////////////

void AchievementHandler::Pause()
{
    m_paused.store(true);
    m_stateCv.notify_all();
}

/////////////////////////////////////////////////////////////////////

void AchievementHandler::Resume()
{
    m_paused.store(false);
    m_stateCv.notify_all();
}

/////////////////////////////////////////////////////////////////////

void AchievementHandler::Stop()
{
    if (!m_running.exchange(false))
    {
        return;
    }

    m_stateCv.notify_all();
    if (m_thread.joinable())
    {
        m_thread.join();
    }
}

/////////////////////////////////////////////////////////////////////

void AchievementHandler::AddTarget(int targetId, const std::string& appIdDirPath, const std::string& emulatorType)
{
    // Ensure the directory exists before continuing
    if (!fs::is_directory(appIdDirPath))
    {
        if (onAppIdDirUnavailable)
        {
            onAppIdDirUnavailable(targetId, appIdDirPath);
        }
        return;
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_sessions.contains(targetId))
    {
        return;
    }

    // Resolve the proper parser strategy
    AchievementParser* parser = CreateParser(emulatorType);
    if (!parser)
    {
        LOG_BE(Urgency::Warning, "No parser for emulator type: %s", emulatorType.c_str());
        return;
    }

    // Initialize and map new watch session
    WatchSession session{};
    session.targetId = targetId;
    session.appIdDirPath = appIdDirPath;
    session.emulatorType = emulatorType;
    m_parsers[targetId] = parser;
    m_sessions[targetId] = std::move(session);

    // Perform initial read to establish baseline state
    ReadInitial(m_sessions[targetId]);
}

/////////////////////////////////////////////////////////////////////

void AchievementHandler::RemoveTarget(int targetId)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    RemoveSessionLocked(targetId);
}

/////////////////////////////////////////////////////////////////////

std::vector<AchievementData> AchievementHandler::PollUnhandled(int targetId)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<AchievementData> values;

    // Look up requested session data
    const auto session = m_sessions.find(targetId);
    if (session == m_sessions.end())
    {
        return values;
    }

    // Gather unhandled changes and mark them clean
    for (auto& [key, value] : session->second.achievements)
    {
        (void)key;
        if (!value.handled)
        {
            values.push_back(value);
            value.handled = true;
        }
    }

    return values;
}

/////////////////////////////////////////////////////////////////////
///////////////////////////// PRIVATE ///////////////////////////////
/////////////////////////////////////////////////////////////////////

void AchievementHandler::WatchLoop()
{
    while (m_running.load())
    {
        // Callbacks update DB scan state, so collect unavailable paths while holding m_mutex and invoke them afterward
        std::vector<std::pair<int, std::string>> unavailableDirs;
        if (!m_paused.load())
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            for (auto it = m_sessions.begin(); it != m_sessions.end(); )
            {
                WatchSession& session = it->second;
                if (!fs::is_directory(session.appIdDirPath))
                {
                    // Polling equivalent of Linux IN_DELETE_SELF/IN_MOVE_SELF: drop session and request a new path scan
                    unavailableDirs.emplace_back(session.targetId, session.appIdDirPath);
                    const int targetId = session.targetId;
                    ++it;
                    RemoveSessionLocked(targetId);
                    continue;
                }

                const fs::path filePath = fs::path(session.appIdDirPath) / m_parsers[session.targetId]->GetFileName();
                if (!fs::is_regular_file(filePath))
                {
                    // File can be recreated in same AppId directory, so retain session like Linux retains directory watch
                    if (session.achievementFilePresent)
                    {
                        LOG_BE(Urgency::Info, "Achievement file removed: targetId=%d", session.targetId);
                        session.achievementFilePresent = false;
                    }
                    ++it;
                    continue;
                }

                if (!session.achievementFilePresent)
                {
                    session.achievementFilePresent = true;
                    if (!session.initialReadDone)
                    {
                        LOG_BE(Urgency::Info, "Achievement file appeared: targetId=%d", session.targetId);
                        // File first appeared after tracking began: diff it, never create a silent startup baseline
                        session.initialReadDone = true;
                    }
                }

                // Polling equivalent of Linux write/create events: compare current file state against cached data
                ReadAndDiff(session);
                ++it;
            }
        }

        for (const auto& [targetId, appIdDirPath] : unavailableDirs)
        {
            LOG_BE(Urgency::Warning, "AppID dir removed while tracked: targetId=%d path=%s", targetId, appIdDirPath.c_str());
            if (onAppIdDirUnavailable)
            {
                onAppIdDirUnavailable(targetId, appIdDirPath);
            }
        }

        // Poll every second, but pause, resume and stop wake this thread immediately
        std::unique_lock<std::mutex> lock(m_stateMutex);
        if (m_paused.load())
        {
            m_stateCv.wait(lock, [this]() {
                return !m_running.load() || !m_paused.load();
            });
        }
        else
        {
            m_stateCv.wait_for(lock, std::chrono::seconds(1), [this]() {
                return !m_running.load() || m_paused.load();
            });
        }
    }
}

/////////////////////////////////////////////////////////////////////

void AchievementHandler::RemoveSessionLocked(int targetId)
{
    // Delete allocated parser mapping
    const auto parser = m_parsers.find(targetId);
    if (parser != m_parsers.end())
    {
        delete parser->second;
        m_parsers.erase(parser);
    }
    m_sessions.erase(targetId);
}

/////////////////////////////////////////////////////////////////////

void AchievementHandler::AddFileWatch(WatchSession& session)
{
    // No-op placeholder for Windows polling
    (void)session;
}

/////////////////////////////////////////////////////////////////////

void AchievementHandler::ReadInitial(WatchSession& session)
{
    // Construct local file target path
    const fs::path filePath = fs::path(session.appIdDirPath) / m_parsers[session.targetId]->GetFileName();
    if (!fs::is_regular_file(filePath))
    {
        return;
    }
    session.achievementFilePresent = true;

    try
    {
        // Parse baseline profile file structure
        for (AchievementData value : m_parsers[session.targetId]->Parse(filePath.string()))
        {
            value.handled = false;
            value.newlyUnlocked = false;
            session.achievements[value.key] = std::move(value);
        }
        session.initialReadDone = true;
    }
    catch (const std::exception& error)
    {
        LOG_BE(Urgency::Warning, "Initial achievement parse failed: targetId=%d error=%s", session.targetId, error.what());
    }
}

/////////////////////////////////////////////////////////////////////

void AchievementHandler::ReadAndDiff(WatchSession& session)
{
    // Build target update path location
    const fs::path filePath = fs::path(session.appIdDirPath) / m_parsers[session.targetId]->GetFileName();
    if (!fs::is_regular_file(filePath))
    {
        return;
    }

    try
    {
        // Compare active live state against cached session records
        for (const AchievementData& parsed : m_parsers[session.targetId]->Parse(filePath.string()))
        {
            const auto previous = session.achievements.find(parsed.key);

            // Check if user hit unlocking condition rules since last tick
            const bool newlyUnlocked = parsed.achieved && (previous == session.achievements.end() || !previous->second.achieved);
            const bool progressChanged = previous != session.achievements.end() &&
                ((parsed.hasCurProgress && (!previous->second.hasCurProgress || parsed.curProgress != previous->second.curProgress)) ||
                (parsed.hasMaxProgress && (!previous->second.hasMaxProgress || parsed.maxProgress != previous->second.maxProgress)));
            const bool changed = previous == session.achievements.end() || newlyUnlocked || progressChanged;
            if (!changed)
            {
                continue;
            }

            // Copy and mark records for external consumption
            AchievementData value = parsed;
            value.handled = false;
            value.newlyUnlocked = newlyUnlocked;
            session.achievements[value.key] = std::move(value);
        }
        session.initialReadDone = true;
    }
    catch (const std::exception& error)
    {
        LOG_BE(Urgency::Warning, "Achievement parse failed: targetId=%d error=%s", session.targetId, error.what());
    }
}

/////////////////////////////////////////////////////////////////////

AchievementParser* AchievementHandler::CreateParser(const std::string& emulatorType)
{
    if (emulatorType == "CODEX" || emulatorType == "RUNE")
    {
        return new RUNECodexParser();
    }
    if (emulatorType == "GOLDBERG")
    {
        return new GoldbergParser();
    }
    if (emulatorType == "GOG-N")
    {
        return new GoGNParser();
    }

    return nullptr;
}
