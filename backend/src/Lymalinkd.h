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
#include "notification/CanberraSoundService.h"
#include "notification/AchievementNotificationService.h"
#include "overlay/OverlayNotifier.h"
#include "database/SQLiteManager.h"
#include "watcher/PathScanner.h"
#include "watcher/ProcessWatcher.h"
#include "watcher/AchievementHandler.h"

#include <signal.h>
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <filesystem>
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
    OverlayNotifier m_overlayNotifications;
    CanberraSoundService m_notificationSound;
    AchievementNotificationService m_achievementNotifications;
    PathScanner m_pathScanner;
    ProcessWatcher m_processWatcher;
    AchievementHandler m_achievementHandler;

    std::mutex m_cvMutex;
    std::mutex m_activeTargetsMutex;
    std::mutex m_targetIdsRequiringDirScanMutex;
    std::mutex m_databaseMutex;
    std::mutex m_startupNotificationThreadsMutex;

    std::thread m_sleepTimerThread;
    std::thread m_signalThread;
    std::vector<std::thread> m_startupNotificationThreads;

    std::condition_variable m_cv;

    std::atomic_bool m_processActive;
    std::atomic_int m_activeCount;
    std::atomic_uint64_t m_sleepTimerGeneration;
    std::atomic_bool m_running;
    std::atomic_bool m_startupNotificationEnabled;

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
    void  OnAchievementUnlocked(int targetId, const std::string& achievementKey);
    void  OnTestToast();
    void  OnTestSound();
    void  OnRequestActiveTargets();
    void  OnReloadAllTargets();
    void  OnReloadConfig();

    std::vector<WatchTarget> LoadExeTargetsFromDatabase();
    std::unordered_map<int, std::string> LoadAppIdDirScanTargetsFromDatabase();
    bool HasCurrentActiveTargetsNeedingAppIdDirScan();
    std::vector<AppIdDirPathScanTarget> LoadCurrentActivePrefixPaths();
    void SavePathScanResults(const std::vector<AppIdDirPathScanResult>& results);
    void SavePlaytime(int targetId, long secondsPlayed);
    bool SaveAchievementState(int targetId, const AchievementData& achievement);
    void ScheduleStartupNotification(int targetId, std::string gameName);
    bool IsTargetActive(int targetId);
    std::string ResolveDatabasePath() const;
    std::vector<std::string> ResolveInstalledVulkanOverlayManifestPaths() const;
    std::vector<std::string> ResolveInstalledFlatpakVulkanOverlayManifestPaths() const;
    void SetVulkanOverlayManifestEnableEnvironment(bool enabled);
    void SetVulkanOverlayManifestEnableEnvironment(const std::string& manifestPath, bool enabled);
    std::string ResolveInstalledNotificationSoundPath(bool allowCustomSound = true) const;
    std::string ResolveConfigPath() const;
    std::string ResolveDataPath(const std::string& relativePath) const;
    void LoadNotificationSoundConfig(bool& outUseCustomSound, std::string& outCustomSoundPath, std::string& outBundledSound) const;
    bool LoadStartupNotificationConfig() const;
    bool ParseConfigBool(const std::string& value) const;
    bool IsSupportedCustomNotificationSound(const std::filesystem::path& soundPath) const;
};
