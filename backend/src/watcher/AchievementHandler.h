/////////////////////////////////////////////////////////
// File: AchievementHandler.h
// Date: 2026-05-26
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Declares AchievementHandler which tracks
//              achievement files for active game sessions
/////////////////////////////////////////////////////////

#pragma once

#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <cstdint>
#include <unordered_map>
#include <sys/inotify.h>

struct AchievementData
{
    std::string key;
    bool achieved = false;
    int curProgress = 0;
    int maxProgress = 0;
    int64_t unlockTime  = 0;
    bool handled = false;
    bool newlyUnlocked = false;
};

struct WatchSession
{
    int targetId;
    std::string appIdDirPath;
    std::string emulatorType;
    int dirWd = -1;
    int fileWd = -1;
    bool initialReadDone = false;
    std::unordered_map<std::string, AchievementData> achievements;
};

/////////////////////////////////////////////////////////////////////

class AchievementParser;

class AchievementHandler
{
public:
    AchievementHandler();
    ~AchievementHandler();

    void Init();
    void Start();
    void Pause();
    void Resume();
    void Stop();

    // Add a target session to track. Parser is selected by emulatorType.
    void AddTarget(int targetId, const std::string& appIdDirPath, const std::string& emulatorType);
    void RemoveTarget(int targetId);

    // Called by Lymalinkd to collect achievement changes for DB sync.
    // Returns unhandled changes and marks them as handled.
    std::vector<AchievementData> PollUnhandled(int targetId);

private:
    std::thread m_thread;
    std::mutex m_mutex;
    std::mutex m_stateMutex;
    std::condition_variable m_stateCv;
    std::atomic<bool> m_running;
    std::atomic<bool> m_paused;

    int m_inotifyFd;

    // Keyed by targetId
    std::unordered_map<int, WatchSession> m_sessions;
    // Reverse lookup: inotify wd -> targetId
    std::unordered_map<int, int> m_wdToTarget;
    // Parsers: keyed by targetId
    std::unordered_map<int, AchievementParser*> m_parsers;

    void WatchLoop();
    void HandleInotifyEvent(const struct inotify_event* ev);
    void ReadInitial(WatchSession& session);
    void ReadAndDiff(WatchSession& session);
    void AddFileWatch(WatchSession& session);

    AchievementParser* CreateParser(const std::string& emulatorType);
};
