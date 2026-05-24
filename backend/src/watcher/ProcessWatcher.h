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
#include <sys/types.h>

struct WatchTarget {
    int targetId;
    std::string executablePath;   // full path, e.g. /home/user/Program/.../program.exe
};

struct ActiveProcess {
    int targetId;
    std::string executablePath;
    pid_t pid;
    time_t sessionStart;
};

class ProcessWatcher
{
public:
    ProcessWatcher();
    ~ProcessWatcher();

    void SetTargets(const std::vector<WatchTarget>& targets);

    // Callbacks
    std::function<void(int targetId, const std::string& executablePath)> onProcessStarted;
    std::function<void(int targetId, long secondsPlayed)> onProcessStopped;

    void Start();
    void Stop();

private:
    std::thread m_thread;
    std::mutex m_mutex;
    std::vector<WatchTarget> m_targets;
    std::vector<ActiveProcess> m_active;
    std::atomic<bool> m_running;
    uint8_t m_pollIntervalSec;

    void PollLoop();
    void ScanProc();
};
