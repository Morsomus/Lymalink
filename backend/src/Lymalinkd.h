/////////////////////////////////////////////////////////
// File: Lymalinkd.h
// Date: 2026-05-22
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Declaration of Lymalinkd backend service
/////////////////////////////////////////////////////////

#pragma once

#include "Error.h"
#include "service/SystemdNotify.h"
#include "ipc/DBusService.h"
#include "database/SQLiteManager.h"
#include "watcher/PathScanner.h"
#include "watcher/ProcessWatcher.h"

#include <signal.h>
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

class Lymalinkd
{
public:
    Lymalinkd();
    ~Lymalinkd();

    Error Main();

private:
    SystemdNotify m_notify;
    DBusService m_dbus;
    SQLiteManager m_database;
    PathScanner m_pathScanner;
    ProcessWatcher m_processWatcher;

    std::mutex m_cvMutex;
    std::mutex m_activeTargetsMutex;
    std::mutex m_targetIdsRequiringDirScanMutex;
    std::mutex m_databaseMutex;

    std::thread m_sleepTimerThread;
    std::thread m_signalThread;

    std::condition_variable m_cv;

    std::atomic_bool m_processActive;
    std::atomic_int m_activeCount;
    std::atomic_uint64_t m_sleepTimerGeneration;
    std::atomic_bool m_running;

    std::vector<std::pair<int, int>> m_activeTargetsIds; // id, notification (sent to frontend)
    std::unordered_map<int, std::string> m_targetIdsRequiringDirScan;
    std::string m_databaseConnectionName;
    std::string m_databasePath;
    std::string m_databaseEmuGamesTable;

    Error Init();
    Error DatabaseInit();
    void  Monitor();
    void  SignalThread(sigset_t mask);
    void  Shutdown();
    
    void  OnProcessStarted(int targetId, const std::string& executablePath);
    void  OnProcessStopped(int targetId, long secondsPlayed);
    void  OnRequestActiveTargets();
    void  OnReloadAllTargets();

    std::vector<WatchTarget> LoadExeTargetsFromDatabase();
    std::unordered_map<int, std::string> LoadAppIdDirScanTargetsFromDatabase();
    bool HasCurrentActiveTargetsNeedingAppIdDirScan();
    std::vector<AppIdDirPathScanTarget> LoadCurrentActivePrefixPaths();
    void  SavePathScanResults(const std::vector<AppIdDirPathScanResult>& results);
    void  SavePlaytime(int targetId, long secondsPlayed);
    std::string ResolveDatabasePath() const;
};
