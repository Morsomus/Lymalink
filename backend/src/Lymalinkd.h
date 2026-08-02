/////////////////////////////////////////////////////////
// File: Lymalinkd.h
// Date: 2026-05-22
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Declaration of Lymalinkd backend service
/////////////////////////////////////////////////////////

#pragma once

#include "Error.h"
#if defined(_WIN32)
    #include "ipc/WinSocketServer.h"
    #include "notification/WinSoundService.h"
    #include "overlay/WinOverlayInjector.h"
    #include "overlay/WinOverlayNotifier.h"
#else
    #include "service/SystemdNotify.h"
    #include "ipc/DBusService.h"
    #include "notification/CanberraSoundService.h"
    #include "overlay/OverlayNotifier.h"
#endif
#include "notification/AchievementNotificationService.h"
#include "tray/BackendTrayIcon.h"
#include "database/SQLiteManager.h"
#include "tools/AchievementKeyResolver.h"
#include "watcher/PathScanner.h"
#include "watcher/ProcessWatcher.h"
#include "watcher/AchievementHandler.h"

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
#if !defined(_WIN32)
    #include <signal.h>
#endif

class Lymalinkd
{
public:
    Lymalinkd();
    ~Lymalinkd();

    Error Main();

private:
#if defined(_WIN32)
    WinSocketServer m_ipc;
    WinSoundService m_notificationSound;
    WinOverlayNotifier m_overlayNotifications;
    WinOverlayInjector m_overlayInjector;
#else
    SystemdNotify m_notify;
    DBusService m_dbus;
    OverlayNotifier m_overlayNotifications;
    CanberraSoundService m_notificationSound;
#endif
    SQLiteManager m_database;
    AchievementKeyResolver m_achievementKeyResolver;
    AchievementNotificationService m_achievementNotifications;
    BackendTrayIcon m_trayIcon;
    PathScanner m_pathScanner;
    ProcessWatcher m_processWatcher;
    AchievementHandler m_achievementHandler;

    std::mutex m_cvMutex;
    std::mutex m_activeTargetsMutex;
    std::mutex m_targetIdsRequiringDirScanMutex;
    std::mutex m_databaseMutex;
    std::mutex m_startupNotificationThreadsMutex;
    std::mutex m_manualScanMutex;

    std::thread m_sleepTimerThread;
    std::thread m_manualScanThread;
#if defined(_WIN32)
    std::thread m_monitorThread;
#endif
#if !defined(_WIN32)
    std::thread m_signalThread;
#endif
    std::vector<std::thread> m_startupNotificationThreads;

    std::condition_variable m_cv;

    std::atomic_bool m_processActive;
    std::atomic_int m_activeCount;
    std::atomic_uint64_t m_sleepTimerGeneration;
    std::atomic_bool m_running;
    std::atomic_bool m_startupNotificationEnabled;
    std::atomic_bool m_manualScanActive;
    std::atomic_bool m_manualScanCancelRequested;

    std::vector<std::pair<int, int>> m_activeTargetsIds; // id, notification (sent to frontend)
#if defined(_WIN32)
    std::unordered_map<int, std::filesystem::file_time_type> m_processStartedAt;
#endif
    std::unordered_map<int, AppIdDirPathScanTarget> m_targetIdsRequiringDirScan;
    int m_manualScanTargetId;
    std::string m_manualScanCancelReason;
    std::string m_databaseConnectionName;
    std::string m_databasePath;
    std::string m_databaseEmuGamesTable;

    Error Init();
    Error DatabaseInit();
    void  Monitor();
#if !defined(_WIN32)
    void  SignalThread(sigset_t mask);
#endif
    void  Shutdown();

#if defined(_WIN32)
    void  InjectWindowsOverlayProcessTree(int targetId, uint32_t rootPid);
    bool  IsWindowsProcessAlive(uint32_t pid) const;
    std::vector<uint32_t> CollectWindowsProcessTree(uint32_t rootPid) const;
#endif
    void  OnProcessStarted(int targetId, const std::string& executablePath, uint32_t pid);
    void  OnProcessStopped(int targetId, long secondsPlayed);
    void  OnAchievementUnlocked(int targetId, const std::string& achievementKey);
    void  OnAppIdDirUnavailable(int targetId, const std::string& appIdDirPath);
    void  OnTestToast();
    void  OnTestSound();
    void  OnShutdown();
    void  OnRequestActiveTargets();
    void  OnReloadAllTargets();
    void  OnReloadConfig();
    void  OnStartManualAchievementDataScan(int targetId);
    void  OnCancelManualAchievementDataScan(int targetId);

    std::vector<WatchTarget> LoadExeTargetsFromDatabase();
    std::unordered_map<int, AppIdDirPathScanTarget> LoadAppIdDirScanTargetsFromDatabase();
    bool LoadAppIdDirScanTargetFromDatabase(int targetId, AppIdDirPathScanTarget& target);
    bool HasCurrentActiveTargetsNeedingAppIdDirScan();
    std::vector<AppIdDirPathScanTarget> LoadCurrentActivePrefixPaths();
    void SavePathScanResults(const std::vector<AppIdDirPathScanResult>& results, bool emitTargetDataChanged = true);
    bool EnsureColumn(const std::string& tableName, const std::string& columnName, const std::string& columnDef, bool *columnAdded = nullptr);
    bool EmulatorAchievementProgressNotImplemented(const std::string& emulatorType) const;
    bool DisableAchievementProgress(int targetId);
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
    void RequestManualAchievementDataScanCancel(int targetId, const std::string& reason);
    void FinishManualAchievementDataScan(int targetId, bool found, const std::string& reason);
    void EmitAchievementUnlocked(int targetId, const std::string& achievementKey);
    void EmitGameStateChanged(const std::vector<int>& targetIds, const std::string& state);
    void EmitTargetDataChanged(int targetId);
    void EmitManualAchievementDataScanFinished(int targetId, bool found, const std::string& reason);
};
