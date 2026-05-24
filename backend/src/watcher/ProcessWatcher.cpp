/////////////////////////////////////////////////////////
// File: ProcessWatcher.cpp
// Date: 2026-05-22
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Implements ProcessWatcher class which
//              monitors selected targets activity
//              for playtime and scan triggers
/////////////////////////////////////////////////////////

#include "ProcessWatcher.h"
#include "../tools/Logger.h"

#include <chrono>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <unistd.h>

namespace fs = std::filesystem;

/////////////////////////////////////////////////////////////////////

ProcessWatcher::ProcessWatcher()
{
    m_targets = {};
    m_active = {};
    m_running.store(false);
    m_pollIntervalSec = 5;
}

ProcessWatcher::~ProcessWatcher()
{
    if (m_thread.joinable())
    {
        Stop();
    }
}

/////////////////////////////////////////////////////////////////////
////////////////////////////// PUBLIC ///////////////////////////////
/////////////////////////////////////////////////////////////////////

void ProcessWatcher::SetTargets(const std::vector<WatchTarget>& targets)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_targets = targets;
    Logger::Log("[ProcessWatcher] Targets set: " + std::to_string(m_targets.size()));
}

/////////////////////////////////////////////////////////////////////

void ProcessWatcher::Start()
{
    if (m_running.load())
    {
        return;
    }

    m_running.store(true);
    m_thread  = std::thread(&ProcessWatcher::PollLoop, this);
    Logger::Log("[ProcessWatcher] Started.");
}

/////////////////////////////////////////////////////////////////////

void ProcessWatcher::Stop()
{
    m_running.store(false);
    if (m_thread.joinable())
    {
        m_thread.join();
    }
    Logger::Log("[ProcessWatcher] Stopped.");
}

/////////////////////////////////////////////////////////////////////
///////////////////////////// PRIVATE ///////////////////////////////
/////////////////////////////////////////////////////////////////////

void ProcessWatcher::PollLoop()
{
    while (m_running.load())
    {
        ScanProc();
        std::this_thread::sleep_for(std::chrono::seconds(m_pollIntervalSec));
    }
}

/////////////////////////////////////////////////////////////////////

void ProcessWatcher::ScanProc()
{
    std::lock_guard<std::mutex> lock(m_mutex);

    // Build a set of exe paths of targets not yet active + already active (to detect stops)
    std::unordered_set<std::string> watchedPaths;
    for (const auto& target : m_targets)
    {
        watchedPaths.insert(target.executablePath);
    }
    for (const auto& a : m_active)
    {
        watchedPaths.insert(a.executablePath);
    }

    // Collect matching running exe paths from /proc
    std::unordered_map<std::string, pid_t> running;
    for (const auto& entry : fs::directory_iterator("/proc"))
    {
        const std::string name = entry.path().filename().string();
        if (name.find_first_not_of("0123456789") != std::string::npos)
        {
            continue;
        }
        const std::string exeLink = "/proc/" + name + "/exe";
        char buf[4096] = {};
        const ssize_t len = readlink(exeLink.c_str(), buf, sizeof(buf) - 1);
        if (len <= 0)
        {
            continue;
        }
        std::string path(buf, len);
        if (watchedPaths.count(path))
        {
            running.emplace(std::move(path), static_cast<pid_t>(std::stoi(name)));
        }
    }

    // Check if any target just started
    for (const auto& target : m_targets)
    {
        const bool alreadyActive = [&]() {
            for (const auto& a : m_active)
            {
                if (a.targetId == target.targetId)
                {
                    return true;
                }
            }
            return false;
        }();
        if (alreadyActive)
        {
            continue;
        }
        const auto it = running.find(target.executablePath);
        if (it != running.end())
        {
            ActiveProcess ap;
            ap.targetId = target.targetId;
            ap.executablePath = target.executablePath;
            ap.pid = it->second;
            ap.sessionStart = time(nullptr);
            m_active.push_back(ap);
            Logger::Log("[ProcessWatcher] Process started - targetId=" + std::to_string(target.targetId) + " pid=" + std::to_string(ap.pid));
            if (onProcessStarted)
            {
                onProcessStarted(ap.targetId, ap.executablePath);
            }
        }
    }

    // Check if any active process has stopped
    for (auto it = m_active.begin(); it != m_active.end(); )
    {
        const bool stillRunning = running.count(it->executablePath) > 0;
        if (!stillRunning)
        {
            const long secs = static_cast<long>(time(nullptr) - it->sessionStart);
            Logger::Log("[ProcessWatcher] Process stopped - targetId=" + std::to_string(it->targetId) + " playtime=" + std::to_string(secs) + "s");
            if (onProcessStopped)
            {
                onProcessStopped(it->targetId, secs);
            }
            it = m_active.erase(it);
        }
        else
        {
            ++it;
        }
    }
}
