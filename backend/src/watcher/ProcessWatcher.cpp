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
#include "Defines.h"
#include "../tools/Logger.h"
#include "../tools/Utils.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#if defined(_WIN32)
    #include <windows.h>
    #include <tlhelp32.h>
#else
    #include <unistd.h>
#endif

namespace fs = std::filesystem;

#define COMPONENT "ProcessWatcher"

/////////////////////////////////////////////////////////////////////

ProcessWatcher::ProcessWatcher() :
    m_thread(),
    m_mutex()
{
    m_targets = {};
    m_meta = {};
    m_active = {};
    m_activeIds = {};
    m_running.store(false);
    m_pollIntervalSec = 5;
    onProcessStarted = nullptr;
    onProcessStopped = nullptr;
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
    // Serialize target replacement against /proc scan loop
    std::lock_guard<std::mutex> lock(m_mutex);

    // Build set of incoming target IDs for fast lookup
    std::unordered_set<int> newIds;
    for (const auto& t : targets)
    {
        newIds.insert(t.targetId);
    }

    // Stop active sessions that no longer exist in incoming target list
    for (auto it = m_active.begin(); it != m_active.end(); )
    {
        if (!newIds.count(it->targetId))
        {
            const long secs = static_cast<long>(time(nullptr) - it->sessionStart);
            LOG_BE(Urgency::Warning, "Target removed while active - targetId=%d playtime=%lds", it->targetId, secs);
            
            if (onProcessStopped)
            {
                onProcessStopped(it->targetId, secs);
            }
            m_activeIds.erase(it->targetId);
            it = m_active.erase(it);
        }
        else
        {
            ++it;
        }
    }

    m_targets = targets;

    // Rebuild executable metadata cache used by process matching
    m_meta.clear();
    m_meta.reserve(m_targets.size());
    for (const auto& t : m_targets)
    {
        m_meta.push_back(BuildMeta(t.executablePath));
    }

    LOG_BE(Urgency::Debug, "Targets set: %zu", m_targets.size());
}

/////////////////////////////////////////////////////////////////////

void ProcessWatcher::SetPollIntervalSec(uint8_t seconds)
{
    m_pollIntervalSec.store(seconds);
}

/////////////////////////////////////////////////////////////////////

void ProcessWatcher::Start()
{
    if (m_running.load())
    {
        return;
    }

    // Launch polling thread
    m_running.store(true);
    m_thread = std::thread(&ProcessWatcher::PollLoop, this);
    
    LOG_BE(Urgency::Debug, "Started.");
}

/////////////////////////////////////////////////////////////////////

void ProcessWatcher::Stop()
{
    if (!m_running.load())
    {
        return;
    }

    m_running.store(false);
    if (m_thread.joinable())
    {
        m_thread.join();
    }
    
    LOG_BE(Urgency::Debug, "Stopped.");
}

/////////////////////////////////////////////////////////////////////
///////////////////////////// PRIVATE ///////////////////////////////
/////////////////////////////////////////////////////////////////////

void ProcessWatcher::PollLoop()
{
    // Keep scanning /proc until daemon shutdown
    while (m_running.load())
    {
        ScanProc();
        std::this_thread::sleep_for(std::chrono::seconds(m_pollIntervalSec.load()));
    }
}

/////////////////////////////////////////////////////////////////////

void ProcessWatcher::ScanProc()
{
    std::lock_guard<std::mutex> lock(m_mutex);

    const size_t totalTargets = m_targets.size();

    // Map executablePath -> pid (from live /proc scan)
#if defined(_WIN32)
    std::unordered_map<std::string, uint32_t> running;

    // Take a snapshot of all currently running processes in the system
    const HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot != INVALID_HANDLE_VALUE)
    {
        // Iterate through the process snapshot sequentially
        PROCESSENTRY32 entry{sizeof(entry)};
        for (BOOL ok = Process32First(snapshot, &entry); ok; ok = Process32Next(snapshot, &entry))
        {
            // Open the process handle with limited query permissions to read its metadata
            const HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, entry.th32ProcessID);
            if (!process)
            {
                continue;
            }
            char path[MAX_PATH]{};
            DWORD size = MAX_PATH;

            // Retrieve the full executable file path for the given process handle
            const bool hasPath = QueryFullProcessImageNameA(process, 0, path, &size) != FALSE;
            CloseHandle(process);
            if (!hasPath)
            {
                continue;
            }

            // Convert the running process path to lowercase
            const std::string runningPath = Utils::ToLower(std::string(path, size));
            for (size_t i = 0; i < totalTargets; ++i)
            {
                // Normalize the target path delimiters and convert to lowercase
                std::string targetPath = m_meta[i].exePath;
                std::replace(targetPath.begin(), targetPath.end(), '/', '\\');
                targetPath = Utils::ToLower(std::move(targetPath));

                // If this target hasn't been mapped yet and paths match, register the running process ID
                if (!running.contains(m_meta[i].exePath) && runningPath == targetPath)
                {
                    running.emplace(m_meta[i].exePath, entry.th32ProcessID);
                }    
            }

            // Early exit if all target processes have already been found
            if (running.size() >= totalTargets)
            {
                break;
            }
        }
        CloseHandle(snapshot);
    }
#else
    std::unordered_map<std::string, pid_t> running;
    running.reserve(32);

    // Scan numeric /proc entries and collect running target executables
    for (const auto& entry : fs::directory_iterator("/proc"))
    {
        const std::string name = entry.path().filename().string();
        if (name.find_first_not_of("0123456789") != std::string::npos)
        {
            continue;
        }     

        const pid_t pid = static_cast<pid_t>(std::stoi(name));
        const std::string cmdline = ReadCmdline(name);
        if (cmdline.empty())
        {
            continue;
        }

        for (size_t i = 0; i < totalTargets; ++i)
        {
            const auto& m = m_meta[i];
            if (running.count(m.exePath))
            {
                continue;
            }

            if (MatchCmdline(cmdline, m, name))
            {
                // Store first matching pid for this executable path
                running.emplace(m.exePath, pid);
                break;
            }
        }

        // All not-yet-active targets have been found
        if (running.size() >= totalTargets)
        {
            break;
        }
    }
#endif

    // Check if any target just started
    for (size_t i = 0; i < totalTargets; ++i)
    {
        const auto& target = m_targets[i];
        if (m_activeIds.count(target.targetId))
        {
            continue;
        }

        const auto it = running.find(target.executablePath);
        if (it != running.end())
        {
            // Create active session state for playtime tracking
            ActiveProcess ap = {};
            ap.targetId = target.targetId;
            ap.executablePath = target.executablePath;
            ap.pid = it->second;
            ap.sessionStart   = time(nullptr);

            m_active.push_back(ap);
            m_activeIds.insert(ap.targetId);
            
            LOG_BE(Urgency::Info, "Process started - targetId=%d exeFile=%s pid=%d", ap.targetId, m_meta[i].exeFilename.c_str(), ap.pid);
            
            if (onProcessStarted)
            {
                onProcessStarted(ap.targetId, ap.executablePath, static_cast<uint32_t>(ap.pid));
            }
        }
    }

    // Check if any active process has stopped
    for (auto it = m_active.begin(); it != m_active.end(); )
    {
        const bool stillRunning = running.count(it->executablePath) > 0;
        if (!stillRunning)
        {
            // Emit stop callback with elapsed session time
            const long secs = static_cast<long>(time(nullptr) - it->sessionStart);
            
            LOG_BE(Urgency::Info, "Process stopped - targetId=%d playtime=%lds", it->targetId, secs);
            
            if (onProcessStopped)
            {
                onProcessStopped(it->targetId, secs);
            }
            m_activeIds.erase(it->targetId);
            it = m_active.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

/////////////////////////////////////////////////////////////////////
////////////////////////// PRIVATE STATIC ///////////////////////////
/////////////////////////////////////////////////////////////////////

ProcessWatcher::TargetMeta ProcessWatcher::BuildMeta(const std::string& exePath)
{
    TargetMeta m = {};

    // Cache original executable path and filename for exact matches
    m.exePath = exePath;
    m.exeFilename = ExtractFilename(exePath);

#if !defined(_WIN32)
    // Build Wine Z: path variant from Linux absolute path
    m.zPath  = "Z:";
    for (char c : exePath)
    {
        m.zPath += (c == '/') ? '\\' : c;
    }

    m.zPathLower    = m.zPath;
    m.zPathLower[0] = 'z';

    // Cache parent directory for weaker Proton/relative launch matches
    const auto dirEnd = exePath.rfind('/');
    if (dirEnd != std::string::npos)
    {
        m.dir = exePath.substr(0, dirEnd);
    }
#else
    const auto dirEnd = exePath.find_last_of("/\\");
    if (dirEnd != std::string::npos)
    {
        m.dir = exePath.substr(0, dirEnd);
    }
#endif

    return m;
}

/////////////////////////////////////////////////////////////////////

std::string ProcessWatcher::ExtractFilename(const std::string& path)
{
    std::string filename = "";

    // Extract filename from a path (works for both Linux "/" and Wine "\" paths)
    auto pos = path.rfind('/');
    if (pos == std::string::npos)
    {
        pos = path.rfind('\\');
    }

    if (pos == std::string::npos)
    {
        filename = path;
        return filename;
    }

    filename = path.substr(pos + 1);
    return filename;
}

/////////////////////////////////////////////////////////////////////

#if !defined(_WIN32)
std::string ProcessWatcher::ReadCmdline(const std::string& pid)
{
    std::string cmdline = "";

    // Read /proc/<pid>/cmdline; null-byte separators replaced with spaces
    std::ifstream f("/proc/" + pid + "/cmdline", std::ios::binary);
    if (!f)
    {
        return cmdline;
    }

    std::string raw((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    for (auto& c : raw)
    {
        if (c == '\0')
        {
            c = ' ';
        }
    }

    cmdline = raw;
    return cmdline;
}

/////////////////////////////////////////////////////////////////////

pid_t ProcessWatcher::MatchCmdline(const std::string& cmdline, const TargetMeta& m, const std::string& pid)
{
    pid_t matched = 0;

    // Reject meta-processes
    if (cmdline.find("grep ") != std::string::npos || cmdline.find("find ") != std::string::npos)
    {
        return matched;
    }

    // 1 - Full Linux path verbatim
    if (cmdline.find(m.exePath) != std::string::npos)
    {
        matched = 1;
        return matched;
    }

    // 2 - Z:-converted path (native Wine maps Linux root as Z:\)
    if (cmdline.find(m.zPath) != std::string::npos || cmdline.find(m.zPathLower) != std::string::npos)
    {
        matched = 1;
        return matched;
    }

    // 3 - .exe filename + parent dir both appear  (Proton/UMU S:\ rewrite)
    if (!m.dir.empty() && cmdline.find(m.exeFilename) != std::string::npos && cmdline.find(m.dir) != std::string::npos)
    {
        matched = 1;
        return matched;
    }  

    // 4 - .exe filename only; verify via /proc/<pid>/cwd  (Wine relative path launch)
    if (!m.dir.empty() && cmdline.find(m.exeFilename) != std::string::npos)
    {
        char cwdBuf[4096] = {};
        const std::string link = "/proc/" + pid + "/cwd";
        const ssize_t len = readlink(link.c_str(), cwdBuf, sizeof(cwdBuf) - 1);
        if (len > 0 && m.dir == std::string(cwdBuf, len))
        {
            matched = 1;
            return matched;
        }
    }

    return matched;
}
#endif
