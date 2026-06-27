/////////////////////////////////////////////////////////
// File: ProcessWatcher.h
// Date: 2026-05-22
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Declares ProcessWatcher class which
//              monitors selected targets activity
//              for playtime and scan triggers
/////////////////////////////////////////////////////////

#pragma once

#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include <functional>
#include <cstdint>
#include <ctime>
#include <unordered_set>
#if !defined(_WIN32)
    #include <sys/types.h>
#endif

struct WatchTarget {
    int targetId;
    std::string executablePath;   // full path, e.g. /home/user/Program/.../program.exe
};

class ProcessWatcher
{
public:
    ProcessWatcher();
    ~ProcessWatcher();

    void SetTargets(const std::vector<WatchTarget>& targets);
    void Start();
    void Stop();

    // Callbacks
    std::function<void(int targetId, const std::string& executablePath, uint32_t pid)> onProcessStarted;
    std::function<void(int targetId, long secondsPlayed)> onProcessStopped;

private:
    struct TargetMeta {
        std::string exePath;
        std::string exeFilename;
        std::string zPath;       // Z:-converted path for native Wine
        std::string zPathLower;  // lowercase z: variant
        std::string dir;         // parent directory for medium/weak match
    };

    struct ActiveProcess {
        int targetId;
        std::string executablePath;
#if defined(_WIN32)
        uint32_t pid;
#else
        pid_t pid;
#endif
        time_t sessionStart;
    };

    std::thread m_thread;
    std::mutex m_mutex;
    std::vector<WatchTarget> m_targets;
    std::vector<TargetMeta> m_meta; // cache
    std::vector<ActiveProcess> m_active;
    std::unordered_set<int> m_activeIds;
    std::atomic<bool> m_running;
    uint8_t m_pollIntervalSec;

    void PollLoop();
    void ScanProc();

    TargetMeta BuildMeta(const std::string& exePath);
    std::string ExtractFilename(const std::string& path);
#if !defined(_WIN32)
    std::string ReadCmdline(const std::string& pid);
    pid_t MatchCmdline(const std::string& cmdline, const TargetMeta& m, const std::string& pid);
#endif
};
