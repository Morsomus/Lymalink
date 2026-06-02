/////////////////////////////////////////////////////////
// File: Lymalinkd.cpp
// Date: 2026-05-22
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Implementation of Lymalinkd backend service
/////////////////////////////////////////////////////////

#include "Lymalinkd.h"
#include "Defines.h"
#include "tools/Logger.h"
#include "tools/Utils.h"

#include <cerrno>
#include <cctype>
#include <chrono>
#include <cstring>
#include <cstdlib>
#include <fstream>
#include <algorithm>
#include <sys/signalfd.h>
#include <unistd.h>

#define COMPONENT "Lymalinkd"

/////////////////////////////////////////////////////////////////////

Lymalinkd::Lymalinkd() :
    m_achievementNotifications(m_database, m_overlayNotifications, m_notificationSound)
{
    m_processActive.store(false);
    m_activeCount.store(0);
    m_sleepTimerGeneration.store(0);
    m_running.store(true);
    m_activeTargetsIds = {};
    m_targetIdsRequiringDirScan = {};
    m_databaseConnectionName = DATABASE_CONNECTION_NAME;
    m_databasePath = "";
    m_databaseEmuGamesTable = DATABASE_TABLE_EMU_GAMES;
}

Lymalinkd::~Lymalinkd()
{
    // Destructor
}

/////////////////////////////////////////////////////////////////////
////////////////////////////// PUBLIC ///////////////////////////////
/////////////////////////////////////////////////////////////////////

Error Lymalinkd::Main()
{
    Error err = Error::NoError;

	// Block SIGTERM and SIGINT from normal delivery, signal thread will read them
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGTERM);
    sigaddset(&mask, SIGINT);
    if (sigprocmask(SIG_BLOCK, &mask, nullptr) < 0)
    {
        LOG_BE(Urgency::Critical, "sigprocmask failed: %s", strerror(errno));
        err = Error::UnknownError;
        return err;
    }

    // Start backend services before monitor loop
    err = Init();
    if (err != Error::NoError)
    {
        LOG_BE(Urgency::Fatal, "Init failed, exiting.");
        return err;
    }

    m_running.store(true);
    m_signalThread = std::thread(&Lymalinkd::SignalThread, this, mask);

    Monitor();  // Main Monitor Loop

    Shutdown();
    return err;
}

/////////////////////////////////////////////////////////////////////
///////////////////////////// PRIVATE ///////////////////////////////
/////////////////////////////////////////////////////////////////////

Error Lymalinkd::Init()
{
    Error err = Error::NoError;

    // Reset monitor state before services start
    m_processActive.store(false);

    // Enable Vulkan based Overlay loading to games by tweaking manifest
    SetVulkanOverlayManifestEnableEnvironment(false);

    err = DatabaseInit();
    if (err != Error::NoError)
    {
        return err;
    }

    // Configure notification backends from resolved database/data paths
    std::filesystem::path databaseParent = std::filesystem::path(m_databasePath).parent_path();
    m_achievementNotifications.Configure(m_databaseConnectionName, databaseParent.string());

    if (!m_overlayNotifications.Init())
    {
        LOG_BE(Urgency::Warning, "Overlay notifications unavailable.");
    }

    err = m_notificationSound.Init(ResolveInstalledNotificationSoundPath());
    m_notificationSound.SetFallbackSoundPath(ResolveInstalledNotificationSoundPath(false));
    if (err != Error::NoError)
    {
        LOG_BE(Urgency::Warning, "Achievement sounds unavailable.");
    }

    // Cache targets still requiring AppId dir scan
    std::unordered_map<int, std::string> targetsMissingAppIdDir = LoadAppIdDirScanTargetsFromDatabase();
    {
        std::lock_guard<std::mutex> lock(m_targetIdsRequiringDirScanMutex);
        m_targetIdsRequiringDirScan = targetsMissingAppIdDir;
    }

    // Wire DBus requests to daemon handlers
    m_dbus.onRequestActiveTargets = [this]() { OnRequestActiveTargets(); };
    m_dbus.onReloadAllTargets = [this]() { OnReloadAllTargets(); };
    m_dbus.onReloadConfig = [this]() { OnReloadConfig(); };
    m_dbus.onTestToast = [this]() { OnTestToast(); };
    m_dbus.onTestSound = [this]() { OnTestSound(); };

    err = m_dbus.Init();
    if (err != Error::NoError)
    {
        LOG_BE(Urgency::Critical, "DBusService init failed.");
        return err;
    }

    // ProcessWatcher callbacks
    m_processWatcher.onProcessStarted = [this](int targetId, const std::string& exe) { OnProcessStarted(targetId, exe); };
    m_processWatcher.onProcessStopped = [this](int targetId, long secs) { OnProcessStopped(targetId, secs); };

    m_achievementHandler.Init();
    m_achievementHandler.Start();

    // Start process watcher with executable paths from database
    m_processWatcher.SetTargets(LoadExeTargetsFromDatabase());
    m_processWatcher.Start();

    // Signal systemd that we are ready (no-op if not under systemd)
    m_notify.NotifyReady();
    m_notify.NotifyStatus("Running");

    LOG_BE(Urgency::Debug, "Init complete.");
    return err;
}

/////////////////////////////////////////////////////////////////////

Error Lymalinkd::DatabaseInit()
{
    Error err = Error::NoError;

    // Resolve DB path before opening persistent connection
    m_databasePath = ResolveDatabasePath();
    if (m_databasePath.empty())
    {
        LOG_BE(Urgency::Critical, "Database path resolve failed.");
        err = Error::DatabaseError;
        return err;
    }

    if (!m_database.DatabaseFileExists(m_databasePath))
    {
        LOG_BE(Urgency::Critical, "Database file not found: %s", m_databasePath.c_str());
        err = Error::DatabaseError;
        return err;
    }

    if (!m_database.OpenDatabase(m_databaseConnectionName, m_databasePath))
    {
        LOG_BE(Urgency::Fatal, "Database open failed: %s", m_database.LastError().c_str());
        err = Error::DatabaseError;
        return err;
    }

    LOG_BE(Urgency::Debug, "Database opened: %s", m_databasePath.c_str());
    return err;
}

/////////////////////////////////////////////////////////////////////

void Lymalinkd::Monitor()
{
    LOG_BE(Urgency::Debug, "Entering main Monitor loop");

    while (m_running.load())
    {
        if (!m_processActive.load())
        {
            LOG_BE(Urgency::Info, "No active processes, going to sleep...");
            m_overlayNotifications.SetSocketPaused(true);
            m_achievementHandler.Pause();

            // Sleep until process watcher reports activity or shutdown starts
            std::unique_lock<std::mutex> lock(m_cvMutex);
            m_cv.wait(lock, [this]() {
                return m_processActive.load() || !m_running.load();
            });
            lock.unlock();

            if (!m_running.load())
            {
                break;
            }
            if (!m_processActive.load())
            {
                continue;
            }

            m_overlayNotifications.SetSocketPaused(false);
            m_achievementHandler.Resume();
            
            // Reload after wakeup
            m_processWatcher.SetTargets(LoadExeTargetsFromDatabase());
        }

        LOG_BE(Urgency::Info, "Process active, orchestrating...");

        auto lastScanTime = std::chrono::steady_clock::now();
        while (m_running.load() && m_processActive.load())
        {
            auto currentTime = std::chrono::steady_clock::now();

            std::vector<std::pair<int, int>> activeTargets;
            {
                std::lock_guard<std::mutex> lock(m_activeTargetsMutex);
                activeTargets = m_activeTargetsIds;
            }

            for (const auto& activeTarget : activeTargets)
            {
                const std::vector<AchievementData> unhandled = m_achievementHandler.PollUnhandled(activeTarget.first);
                for (const AchievementData& achievement : unhandled)
                {
                    const bool saved = SaveAchievementState(activeTarget.first, achievement);
                    if (saved && achievement.newlyUnlocked && achievement.achieved)
                    {
                        // OnAchievementUnlocked is only done IF achievement is actually new AND it was marked as achieved
                        OnAchievementUnlocked(activeTarget.first, achievement.key);
                    }
                }
            }

            // Scan AppId dir for currently active executable (5 second interval)
            if (std::chrono::duration_cast<std::chrono::seconds>(currentTime - lastScanTime).count() >= 5)
            {
                if (HasCurrentActiveTargetsNeedingAppIdDirScan())
                {
                    const std::vector<AppIdDirPathScanTarget> currentActivePrefixPaths = LoadCurrentActivePrefixPaths();
                    m_pathScanner.SetTargets(currentActivePrefixPaths);

                    if (!currentActivePrefixPaths.empty())
                    {
                        LOG_BE(Urgency::Debug, "Scanning for AppId dir...");

                        const std::vector<AppIdDirPathScanResult> scanResults = m_pathScanner.ScanOnceForAppIdDir();
                        if (!scanResults.empty())
                        {
                            // Persist discovered AppId dirs and drop completed targets
                            SavePathScanResults(scanResults);
                            for (const AppIdDirPathScanResult& result : scanResults)
                            {
                                m_achievementHandler.AddTarget(result.targetId, result.appidDirLocation, result.emulatorType);
                            }

                            if (!HasCurrentActiveTargetsNeedingAppIdDirScan())
                            {
                                m_pathScanner.SetTargets({});
                            }
                        }
                    }
                }

                lastScanTime = currentTime;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
}

/////////////////////////////////////////////////////////////////////

void Lymalinkd::SignalThread(sigset_t mask)
{
    // Read blocked process signals through signalfd
    int sfd = signalfd(-1, &mask, SFD_CLOEXEC);
    if (sfd < 0)
    {
        LOG_BE(Urgency::Critical, "signalfd failed: %s", strerror(errno));
        m_running.store(false);
        return;
    }

    while (m_running.load())
    {
        struct signalfd_siginfo info{};
        const ssize_t bytes = read(sfd, &info, sizeof(info));

        // Ignore interrupted reads, shutdown on real signalfd errors
        if (bytes < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }

            LOG_BE(Urgency::Critical, "signalfd read failed: %s", strerror(errno));
            m_running.store(false);
            break;
        }

        if (bytes != sizeof(info))
        {
            LOG_BE(Urgency::Warning, "signalfd read returned incomplete signal info");
            continue;
        }

        // Valid signal means daemon should exit main loop
        LOG_BE(Urgency::Debug, "Signal received: %u", info.ssi_signo);
        m_running.store(false);
        break;
    }

    close(sfd);

    // Wake up Monitor()
    m_cv.notify_one();
}

/////////////////////////////////////////////////////////////////////

void Lymalinkd::Shutdown()
{
    LOG_BE(Urgency::Debug, "Shutdown initiated.");

    // Wait signal thread to shutdown
    if (m_signalThread.joinable())
    {
        m_signalThread.join();
    }

    m_notify.NotifyStopping();

    // Stop external services before closing database connection
    m_processWatcher.Stop();
    m_overlayNotifications.Shutdown();
    m_notificationSound.Stop();
    m_dbus.Stop();

    // Cancel pending sleep timer and wait for thread exit
    m_sleepTimerGeneration.fetch_add(1);
    if (m_sleepTimerThread.joinable())
    {
        m_sleepTimerThread.join();
    }

    // Close shared database connection last
    std::lock_guard<std::mutex> lock(m_databaseMutex);
    if (m_database.IsDatabaseOpen(m_databaseConnectionName))
    {
        m_database.CloseDatabase(m_databaseConnectionName);
    }

    // Disable Vulkan based Overlay loading to games by tweaking manifest
    SetVulkanOverlayManifestEnableEnvironment(true);
    LOG_BE(Urgency::Debug, "Shutdown complete.");
}

/////////////////////////////////////////////////////////////////////

void Lymalinkd::OnProcessStarted(int targetId, const std::string& executablePath)
{
    LOG_BE(Urgency::Debug, "OnProcessStarted - targetId=%d exe=%s", targetId, executablePath.c_str());

    // Mark daemon active and wake monitor loop
    m_activeCount.fetch_add(1);
    {
        std::lock_guard<std::mutex> lock(m_cvMutex);
        m_processActive.store(true);
    }

    // Track active target once for state reporting and AppId scans
    {
        std::lock_guard<std::mutex> lock(m_activeTargetsMutex);
        const auto it = std::find_if(m_activeTargetsIds.begin(), m_activeTargetsIds.end(), [targetId](const auto& activeTarget) {
            return activeTarget.first == targetId;
        });

        if (it == m_activeTargetsIds.end())
        {
            m_activeTargetsIds.push_back({targetId, 0});
        }
    }

    DbRecord target;
    {
        std::lock_guard<std::mutex> lock(m_databaseMutex);
        target = m_database.SelectFirst(m_databaseConnectionName, m_databaseEmuGamesTable, "id = ?", {static_cast<int64_t>(targetId)});
    }

    const std::string appIdDirPath = SQLiteManager::RowString(target, "appid_dir_location");
    const std::string emulatorType = SQLiteManager::RowString(target, "emulator_type");
    if (!appIdDirPath.empty() && !emulatorType.empty())
    {
        m_achievementHandler.AddTarget(targetId, appIdDirPath, emulatorType);
    }

    m_dbus.EmitGameStateChanged(targetId, "Active");
    m_cv.notify_one();
}

/////////////////////////////////////////////////////////////////////

void Lymalinkd::OnProcessStopped(int targetId, long secondsPlayed)
{
    LOG_BE(Urgency::Debug, "OnProcessStopped - targetId=%d playtime=%lds", targetId, secondsPlayed);

    // Persist playtime before removing active state
    SavePlaytime(targetId, secondsPlayed);
    {
        std::lock_guard<std::mutex> lock(m_activeTargetsMutex);
        const auto removeIt = std::remove_if(m_activeTargetsIds.begin(), m_activeTargetsIds.end(), [targetId](const auto& activeTarget) {
            return activeTarget.first == targetId;
        });
        m_activeTargetsIds.erase(removeIt, m_activeTargetsIds.end());
    }
    m_dbus.EmitGameStateChanged(targetId, "Inactive");
    m_achievementHandler.RemoveTarget(targetId);

    if (m_activeCount.fetch_sub(1) - 1 <= 0)
    {
        // Start delayed sleep timer after last active process exits
        const uint64_t generation = m_sleepTimerGeneration.fetch_add(1) + 1;
        if (m_sleepTimerThread.joinable())
        {
            m_sleepTimerThread.join();
        }
        
        m_sleepTimerThread = std::thread([this, generation]() {
            for (int i = 0; i < 60; ++i)
            {
                // Cancel stale timer when process state changes
                if (!m_running.load() || generation != m_sleepTimerGeneration.load())
                {
                    return;
                }
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }

            if (m_running.load() && generation == m_sleepTimerGeneration.load() && m_activeCount.load() <= 0)
            {
                LOG_BE(Urgency::Debug, "No active processes for 60s, returning to sleep.");
                {
                    std::lock_guard<std::mutex> lock(m_cvMutex);
                    m_processActive.store(false);
                }
            }
        });
    }
}

/////////////////////////////////////////////////////////////////////

void Lymalinkd::OnAchievementUnlocked(int targetId, const std::string& achievementKey)
{
    // Ignore invalid achievement events
    if (targetId <= 0 || achievementKey.empty())
    {
        return;
    }

    LOG_BE(Urgency::Debug, "Achievement unlocked for targetId=%d: %s", targetId, achievementKey.c_str());

    // Show local notification and forward event over DBus
    m_achievementNotifications.NotifyUnlocked(targetId, achievementKey);
    m_dbus.EmitAchievementUnlocked(targetId, achievementKey);
}

/////////////////////////////////////////////////////////////////////

void Lymalinkd::OnTestToast()
{
    LOG_BE(Urgency::Debug, "Test toast requested.");

    std::string appIconPath = "";

    // Use installed test icon if found
    const std::filesystem::path iconPath = ResolveDataPath(LYMALINK_TEST_ICON_PATH);
    if (std::filesystem::exists(iconPath))
    {
        appIconPath = iconPath.string();
    }

    AchievementNotification notification;
    notification.achievementName = "Scientific Overlay Experiment";
    notification.achievementDescription = "If you can see this, the overlay survived another day";
    notification.iconPath = appIconPath;
    notification.appIconPath = appIconPath;

    // Display notification and play configured sound
    m_overlayNotifications.ShowAchievementToast(notification);
    m_notificationSound.PlayNotificationSound();
}

/////////////////////////////////////////////////////////////////////

void Lymalinkd::OnTestSound()
{
    LOG_BE(Urgency::Debug, "Test sound requested.");

    // Play configured notification sound only, no overlay toast.
    m_notificationSound.PlayNotificationSound();
}

/////////////////////////////////////////////////////////////////////

void Lymalinkd::OnReloadConfig()
{
    // Refresh notification sound without restarting daemon
    m_notificationSound.SetSoundPath(ResolveInstalledNotificationSoundPath());
    m_notificationSound.SetFallbackSoundPath(ResolveInstalledNotificationSoundPath(false));
}

/////////////////////////////////////////////////////////////////////

void Lymalinkd::OnRequestActiveTargets()
{
    std::vector<int32_t> activeTargetIds;
    {
        std::lock_guard<std::mutex> lock(m_activeTargetsMutex);
        activeTargetIds.reserve(m_activeTargetsIds.size());

        // Collect active IDs and mark them as reported
        for (auto& activeTarget : m_activeTargetsIds)
        {
            activeTargetIds.push_back(activeTarget.first);
            activeTarget.second = 1;
        }
    }

    if (!activeTargetIds.empty())
    {
        m_dbus.EmitGameStateChanged(activeTargetIds, "Active");
    }
}

/////////////////////////////////////////////////////////////////////

void Lymalinkd::OnReloadAllTargets()
{
    LOG_BE(Urgency::Debug, "Reloading all targets from database.");

    // Reload watched executables from current database state
    m_processWatcher.SetTargets(LoadExeTargetsFromDatabase());

    // Refresh pending AppId dir scan targets
    const std::unordered_map<int, std::string> targetsMissingAppIdDir = LoadAppIdDirScanTargetsFromDatabase();
    {
        std::lock_guard<std::mutex> lock(m_targetIdsRequiringDirScanMutex);
        m_targetIdsRequiringDirScan = targetsMissingAppIdDir;
    }
}

/////////////////////////////////////////////////////////////////////

std::vector<WatchTarget> Lymalinkd::LoadExeTargetsFromDatabase()
{
    DbRows rows;
    {
        std::lock_guard<std::mutex> lock(m_databaseMutex);
        // Load executable watch targets from games table
        rows = m_database.SelectWhere(
            m_databaseConnectionName,
            m_databaseEmuGamesTable,
            "executable_location IS NOT NULL AND executable_location != ''",
            {},
            {"id", "executable_location"}
        );
    }

    std::vector<WatchTarget> targets;
    targets.reserve(rows.size());

    // Convert database rows to process watcher targets
    for (const auto& row : rows)
    {
        const std::string executable = SQLiteManager::RowString(row, "executable_location");
        if (executable.empty())
        {
            continue;
        }
        targets.push_back(WatchTarget{static_cast<int>(SQLiteManager::RowInt(row, "id")), executable});
    }

    LOG_BE(Urgency::Debug, "Loaded %zu executable watch targets.", targets.size());
    return targets;
}

/////////////////////////////////////////////////////////////////////

// Load Targets which are missing AppId Dir paths
std::unordered_map<int, std::string> Lymalinkd::LoadAppIdDirScanTargetsFromDatabase()
{
    DbRows rows;
    {
        std::lock_guard<std::mutex> lock(m_databaseMutex);
        // Load targets that still need AppId dir discovery
        rows = m_database.SelectWhere(
            m_databaseConnectionName,
            m_databaseEmuGamesTable,
            "appid_dir_found = 0 AND prefix_location IS NOT NULL AND prefix_location != ''",
            {},
            {"id", "prefix_location"}
        );
    }

    std::unordered_map<int, std::string> targets;
    targets.reserve(rows.size());

    // Map target ID to prefix path for future AppId dir scans
    for (const auto& row : rows)
    {
        targets.emplace(
            static_cast<int>(SQLiteManager::RowInt(row, "id")),
            SQLiteManager::RowString(row, "prefix_location")
        );
    }

    LOG_BE(Urgency::Debug, "AppID dir scan targets loaded: %zu", targets.size());
    return targets;
}

/////////////////////////////////////////////////////////////////////

// Check if any active (currently played) target requires finding missing AppId path
bool Lymalinkd::HasCurrentActiveTargetsNeedingAppIdDirScan()
{
    bool hasActiveTargetsNeedingAppIdDirScan = false;

    // Copy active target IDs before checking AppId scan map
    std::vector<std::pair<int, int>> ids;
    {
        std::lock_guard<std::mutex> lock(m_activeTargetsMutex);
        ids = m_activeTargetsIds;
    }

    if (ids.empty())
    {
        return hasActiveTargetsNeedingAppIdDirScan;
    }

    // Check active targets against the pending AppId dir scan list
    std::lock_guard<std::mutex> lock(m_targetIdsRequiringDirScanMutex);
    for (const auto& activeTarget : ids)
    {
        if (m_targetIdsRequiringDirScan.contains(activeTarget.first))
        {
            LOG_BE(Urgency::Debug, "Active target %d requires AppId dir scan.", activeTarget.first);
            hasActiveTargetsNeedingAppIdDirScan = true;
            return hasActiveTargetsNeedingAppIdDirScan;
        }
    }

    return hasActiveTargetsNeedingAppIdDirScan;
}

/////////////////////////////////////////////////////////////////////

// Get vector of active (currently played) targets which are missing AppId path 
std::vector<AppIdDirPathScanTarget> Lymalinkd::LoadCurrentActivePrefixPaths()
{
    std::vector<AppIdDirPathScanTarget> targets = {};

    // Copy active targets before matching against scan requirements
    std::vector<std::pair<int, int>> activeTargets;
    {
        std::lock_guard<std::mutex> lock(m_activeTargetsMutex);
        activeTargets = m_activeTargetsIds;
    } 

    if (activeTargets.empty())
    {
        return targets;
    }

    {
        std::lock_guard<std::mutex> lock(m_targetIdsRequiringDirScanMutex);
        targets.reserve(activeTargets.size());

        // Build scan targets for active games missing AppId dir paths
        for (const auto& activeTarget : activeTargets)
        {
            const int targetId = activeTarget.first;
            const auto it = m_targetIdsRequiringDirScan.find(targetId);
            if (it == m_targetIdsRequiringDirScan.end())
            {
                continue;
            }
            targets.push_back(AppIdDirPathScanTarget{targetId, std::to_string(targetId), it->second});
        }
    }

    if (!targets.empty())
    {
        LOG_BE(Urgency::Debug, "Found %zu active targets requiring prefix path scanning.", targets.size());
    }

    return targets;
}

/////////////////////////////////////////////////////////////////////

// Save AppId path, emulator type to DB for future use 
void Lymalinkd::SavePathScanResults(const std::vector<AppIdDirPathScanResult>& results)
{
    std::vector<int> savedTargetIds;
    savedTargetIds.reserve(results.size());

    {
        std::lock_guard<std::mutex> lock(m_databaseMutex);

        // Save discovered AppId metadata for each successful scan result
        for (const auto& result : results)
        {
            DbRecord data{
                {"appid_dir_found", int64_t{1}},
                {"appid_dir_location", result.appidDirLocation},
                {"emulator_type", result.emulatorType},
                {"date_updated", Utils::NowEpoch()}
            };

            if (!m_database.Update(m_databaseConnectionName, m_databaseEmuGamesTable, data, "id = ?", {static_cast<int64_t>(result.targetId)}))
            {
                LOG_BE(Urgency::Critical, "Failed to save APPID dir result: targetId=%d error=%s", result.targetId, m_database.LastError().c_str());
                continue;
            }

            savedTargetIds.push_back(result.targetId);
            LOG_BE(Urgency::Debug, "APPID dir saved: targetId=%d emulator=%s", result.targetId, result.emulatorType.c_str());
        }
    }

    if (!savedTargetIds.empty())
    {
        // Remove saved targets from pending scan cache
        std::lock_guard<std::mutex> lock(m_targetIdsRequiringDirScanMutex);
        for (const int targetId : savedTargetIds)
        {
            m_targetIdsRequiringDirScan.erase(targetId);
        }
    }
}

/////////////////////////////////////////////////////////////////////

void Lymalinkd::SavePlaytime(int targetId, long secondsPlayed)
{
    std::lock_guard<std::mutex> lock(m_databaseMutex);

    // Add this session time to stored total playtime
    const DbRecord row = m_database.SelectFirst(m_databaseConnectionName, m_databaseEmuGamesTable, "id = ?", {static_cast<int64_t>(targetId)});
    const int64_t totalSeconds = SQLiteManager::RowInt(row, "total_seconds_played") + static_cast<int64_t>(secondsPlayed);

    DbRecord data{
        {"total_seconds_played", totalSeconds},
        {"last_played_date", Utils::NowEpoch()},
        {"date_updated", Utils::NowEpoch()}
    };

    if (!m_database.Update(m_databaseConnectionName, m_databaseEmuGamesTable, data, "id = ?", {static_cast<int64_t>(targetId)}))
    {
        LOG_BE(Urgency::Critical, "Failed to save playtime: targetId=%d error=%s", targetId, m_database.LastError().c_str());
    }
    else
    {
        LOG_BE(Urgency::Debug, "Playtime updated for targetId=%d (+%llds, total: %llds)", targetId, static_cast<long long>(secondsPlayed), static_cast<long long>(totalSeconds));
    }
}

/////////////////////////////////////////////////////////////////////

bool Lymalinkd::SaveAchievementState(int targetId, const AchievementData& achievement)
{
    bool achievementStateUpdated = false;

    if (targetId <= 0 || achievement.key.empty())
    {
        return achievementStateUpdated;
    }

    const int64_t now = Utils::NowEpoch();

    std::lock_guard<std::mutex> lock(m_databaseMutex);

    const DbRecord existingAchievement = m_database.SelectFirst(
        m_databaseConnectionName,
        DATABASE_TABLE_EMU_ACHIEVEMENTS,
        "id = ? AND achievement_key = ?",
        {static_cast<int64_t>(targetId), achievement.key}
    );

    if (existingAchievement.empty())
    {
        LOG_BE(Urgency::Warning, "Achievement not found for DB update: targetId=%d key=%s", targetId, achievement.key.c_str());
        return achievementStateUpdated;
    }

    const int64_t existingUnlockTime = SQLiteManager::RowInt(existingAchievement, "date_unlocked");
    if (achievement.achieved && existingUnlockTime > 0)
    {
        LOG_BE(Urgency::Debug, "Achievement already unlocked in DB, skipping update and notification: targetId=%d key=%s", targetId, achievement.key.c_str());
        return achievementStateUpdated;
    }

    if ((achievement.hasCurProgress && achievement.curProgress < 0) || (achievement.hasMaxProgress && achievement.maxProgress < 0))
    {
        LOG_BE(Urgency::Warning, "Rejecting negative achievement progress: targetId=%d key=%s progress=%d/%d", targetId, achievement.key.c_str(), achievement.curProgress, achievement.maxProgress);
        return achievementStateUpdated;
    }

    const int64_t dbCurProgress = SQLiteManager::RowInt(existingAchievement, "cur_progress");
    const int64_t dbMaxProgress = SQLiteManager::RowInt(existingAchievement, "max_progress");
    const int64_t effectiveMaxProgress = achievement.maxProgress > 0 ? achievement.maxProgress : dbMaxProgress;
    int64_t currentProgress = static_cast<int64_t>(achievement.curProgress);
    bool shouldUpdateCurrentProgress = achievement.hasCurProgress;
    if (achievement.achieved && effectiveMaxProgress > 0 && currentProgress < effectiveMaxProgress)
    {
        currentProgress = effectiveMaxProgress;
        shouldUpdateCurrentProgress = true;
    }

    const bool currentProgressChanged = shouldUpdateCurrentProgress && currentProgress != dbCurProgress;
    const bool maxProgressChanged = achievement.hasMaxProgress && achievement.maxProgress != dbMaxProgress;
    int64_t updatedUnlockTime = existingUnlockTime;
    if (achievement.achieved)
    {
        updatedUnlockTime = achievement.unlockTime > 0 ? achievement.unlockTime : now;
    }
    const bool unlockTimeChanged = updatedUnlockTime != existingUnlockTime;

    if (!currentProgressChanged && !maxProgressChanged && !unlockTimeChanged)
    {
        LOG_BE(Urgency::Debug, "Achievement state unchanged, skipping DB update: targetId=%d key=%s", targetId, achievement.key.c_str());
        return achievementStateUpdated;
    }

    DbRecord achievementUpdate{
        {"date_updated", now}
    };

    if (currentProgressChanged)
    {
        achievementUpdate["cur_progress"] = currentProgress;
    }

    if (maxProgressChanged)
    {
        achievementUpdate["max_progress"] = static_cast<int64_t>(achievement.maxProgress);
    }

    if (unlockTimeChanged)
    {
        achievementUpdate["date_unlocked"] = updatedUnlockTime;
    }

    if (!m_database.Update(
        m_databaseConnectionName,
        DATABASE_TABLE_EMU_ACHIEVEMENTS,
        achievementUpdate,
        "id = ? AND achievement_key = ?",
        {static_cast<int64_t>(targetId), achievement.key}))
    {
        LOG_BE(Urgency::Critical, "Failed to update achievement state: targetId=%d key=%s error=%s", targetId, achievement.key.c_str(), m_database.LastError().c_str());
        return achievementStateUpdated;
    }

    if (currentProgressChanged || maxProgressChanged)
    {
        const int64_t updatedCurProgress = currentProgressChanged ? currentProgress : dbCurProgress;
        const int64_t updatedMaxProgress = achievement.hasMaxProgress ? achievement.maxProgress : dbMaxProgress;
        LOG_BE(Urgency::Info, "Achievement progress updated: targetId=%d key=%s progress=%lld/%lld", targetId, achievement.key.c_str(), static_cast<long long>(updatedCurProgress), static_cast<long long>(updatedMaxProgress));
    }

    const int64_t unlockedCount = m_database.Count(
        m_databaseConnectionName,
        DATABASE_TABLE_EMU_ACHIEVEMENTS,
        "id = ? AND date_unlocked > 0",
        {static_cast<int64_t>(targetId)}
    );

    if (unlockedCount < 0)
    {
        LOG_BE(Urgency::Critical, "Failed to count unlocked achievements: targetId=%d error=%s", targetId, m_database.LastError().c_str());
        return achievementStateUpdated;
    }

    DbRecord targetUpdate{
        {"total_unlocked_amount_achievements", unlockedCount},
        {"date_updated", now}
    };

    achievementStateUpdated = m_database.Update(
        m_databaseConnectionName,
        m_databaseEmuGamesTable,
        targetUpdate,
        "id = ?",
        {static_cast<int64_t>(targetId)}
    );

    if (!achievementStateUpdated)
    {
        LOG_BE(Urgency::Critical, "Failed to update target achievement count: targetId=%d error=%s", targetId, m_database.LastError().c_str());
    }
    else
    {
        LOG_BE(Urgency::Debug, "Achievement state saved successfully: targetId=%d key=%s (Total unlocked: %lld)", targetId, achievement.key.c_str(), static_cast<long long>(unlockedCount));
    }

    return achievementStateUpdated;
}

/////////////////////////////////////////////////////////////////////

std::string Lymalinkd::ResolveDatabasePath() const
{
    std::string databasePath = "";

    // Allow developers/users to override the database path via environment variable
    if (const char* overridePath = std::getenv("LYMALINK_DATABASE_PATH"))
    {
        if (*overridePath != '\0')
        {
            databasePath = overridePath;
            LOG_BE(Urgency::Info, "Database path overridden via environment variable: %s", databasePath.c_str());
            return databasePath;
        }
    }

    // Fallback to default user data location
    const char* home = std::getenv("HOME");
    if (!home || *home == '\0')
    {
        LOG_BE(Urgency::Critical, "HOME environment variable not set or empty. Cannot resolve database path.");
        return databasePath;
    }

    databasePath = (std::filesystem::path(home) / ".local" / "share" / "Lymalink" / DATABASE_FILE_NAME).string();
    LOG_BE(Urgency::Debug, "Database path resolved to default location: %s", databasePath.c_str());
    return databasePath;
}

/////////////////////////////////////////////////////////////////////

std::string Lymalinkd::ResolveInstalledVulkanOverlayManifestPath() const
{
    // Check for an environment variable override
    if (const char* overridePath = std::getenv("LYMALINK_OVERLAY_MANIFEST_PATH"))
    {
        if (*overridePath != '\0')
        {
            LOG_BE(Urgency::Info, "Vulkan overlay manifest path overridden via environment variable: %s", overridePath);
            return std::string(overridePath);
        }
    }

    std::string defaultPath = ResolveDataPath("vulkan/implicit_layer.d/lymalink_overlay.json");
    LOG_BE(Urgency::Debug, "Vulkan overlay manifest path resolved to: %s", defaultPath.c_str());
    return defaultPath;
}

/////////////////////////////////////////////////////////////////////

std::vector<std::string> Lymalinkd::ResolveInstalledFlatpakVulkanOverlayManifestPaths() const
{
    std::vector<std::string> manifestPaths;

    // Check for a Flatpak-specific environment variable override
    if (const char* overridePath = std::getenv("LYMALINK_FLATPAK_OVERLAY_MANIFEST_PATH"))
    {
        if (*overridePath != '\0')
        {
            manifestPaths.push_back(overridePath);
            LOG_BE(Urgency::Info, "Flatpak Vulkan overlay manifest path overridden via environment variable: %s", overridePath);
            return manifestPaths;
        }
    }

    // Verify the target Flatpak runtime directory exists
    const std::filesystem::path runtimeDir = ResolveDataPath("flatpak/runtime/org.freedesktop.Platform.VulkanLayer.lymalink/x86_64/25.08");
    if (runtimeDir.empty() || !std::filesystem::exists(runtimeDir))
    {
        LOG_BE(Urgency::Debug, "Flatpak runtime directory not found or empty: %s", runtimeDir.string().c_str());
        return manifestPaths;
    }

    LOG_BE(Urgency::Debug, "Scanning Flatpak runtime directory for manifests: %s", runtimeDir.string().c_str());

    // Recursively scan the directory for the overlay manifest
    std::error_code ec;
    std::filesystem::recursive_directory_iterator it(runtimeDir, std::filesystem::directory_options::skip_permission_denied, ec);
    const std::filesystem::recursive_directory_iterator end;
    while (!ec && it != end)
    {
        const std::filesystem::directory_entry& entry = *it;

        // Skip directories and inaccessible files
        if (!entry.is_regular_file(ec) || ec)
        {
            ec.clear();
            it.increment(ec);
            continue;
        }

        // Collect paths matching the specific filename
        if (entry.path().filename() == "lymalink_overlay.x86_64.json")
        {
            manifestPaths.push_back(entry.path().string());
            LOG_BE(Urgency::Info, "Found Flatpak Vulkan overlay manifest: %s", entry.path().string().c_str());
        }

        it.increment(ec);
    }

    if (ec)
    {
        LOG_BE(Urgency::Warning, "Error occurred during Flatpak manifest directory scan: %s", ec.message().c_str());
    }

    return manifestPaths;
}

/////////////////////////////////////////////////////////////////////

std::vector<std::string> Lymalinkd::ResolveInstalledVulkanOverlayManifestPaths() const
{
    std::vector<std::string> manifestPaths;

    // Collect the primary host manifest path
    const std::string hostManifestPath = ResolveInstalledVulkanOverlayManifestPath();
    if (!hostManifestPath.empty())
    {
        manifestPaths.push_back(hostManifestPath);
    }

    // Append all discovered Flatpak manifest paths
    const std::vector<std::string> flatpakManifestPaths = ResolveInstalledFlatpakVulkanOverlayManifestPaths();
    manifestPaths.insert(manifestPaths.end(), flatpakManifestPaths.begin(), flatpakManifestPaths.end());

    LOG_BE(Urgency::Debug, "Resolved total of %zu Vulkan overlay manifest paths.", manifestPaths.size());
    return manifestPaths;
}

/////////////////////////////////////////////////////////////////////

void Lymalinkd::SetVulkanOverlayManifestEnableEnvironment(bool enabled)
{
    // Apply the toggle state to every discovered manifest path
    for (const std::string& manifestPath : ResolveInstalledVulkanOverlayManifestPaths())
    {
        SetVulkanOverlayManifestEnableEnvironment(manifestPath, enabled);
    }
}

/////////////////////////////////////////////////////////////////////

void Lymalinkd::SetVulkanOverlayManifestEnableEnvironment(const std::string& manifestPath, bool enabled)
{
    if (manifestPath.empty() || !std::filesystem::exists(manifestPath))
    {
        LOG_BE(Urgency::Warning, "Manifest path empty or does not exist: %s", manifestPath.c_str());
        return;
    }

    // Open the manifest file for reading
    std::ifstream in(manifestPath);
    if (!in.is_open())
    {
        LOG_BE(Urgency::Critical, "Failed to open overlay manifest for read: %s", manifestPath.c_str());
        return;
    }

    // Read the entire file content into memory and close the stream
    std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    in.close();

    const std::string enableKey  = "\"enable_environment\"";
    const std::string disableKey = "\"disable_environment\"";
    const bool hasBlock = (content.find(enableKey) != std::string::npos);
    bool changed = false;

    if (!enabled)
    {
        // Remove the "enable_environment" block if it exists
        if (hasBlock)
        {
            const size_t keyPos = content.find(enableKey);

            // Find the start of the line containing the key
            size_t lineStart = content.rfind('\n', keyPos);
            lineStart = (lineStart == std::string::npos) ? 0 : lineStart + 1;

            // Find the closing brace+comma that ends this block
            const size_t braceClose = content.find("},", keyPos);
            if (braceClose == std::string::npos)
            {
                LOG_BE(Urgency::Critical, "Invalid overlay manifest enable_environment block: %s", manifestPath.c_str());
                return;
            }

            // Erase from line start through the closing brace+comma and its trailing newline
            size_t eraseEnd = braceClose + 2; // include "},"
            if (eraseEnd < content.size() && content[eraseEnd] == '\n')
            {
                ++eraseEnd; // include trailing newline
            }

            content.erase(lineStart, eraseEnd - lineStart);
            changed = true;
        }
    }
    else
    {
        // Insert the "enable_environment" block if it's missing
        if (!hasBlock)
        {
            const size_t markerPos = content.find(disableKey);
            if (markerPos == std::string::npos)
            {
                LOG_BE(Urgency::Critical, "disable_environment missing in overlay manifest: %s", manifestPath.c_str());
                return;
            }

            // Detect the indentation of the disable_environment line and match it
            size_t lineStart = content.rfind('\n', markerPos);
            lineStart = (lineStart == std::string::npos) ? 0 : lineStart + 1;
            const std::string indent(content.begin() + lineStart, content.begin() + markerPos);

            // Detect the indentation used one level deeper (first non-whitespace char inside the block)
            const size_t blockOpen = content.find('{', markerPos);
            size_t innerLineStart = content.find('\n', blockOpen);
            innerLineStart = (innerLineStart == std::string::npos) ? blockOpen + 1 : innerLineStart + 1;
            size_t innerContentStart = content.find_first_not_of(" \t", innerLineStart);
            if (innerContentStart == std::string::npos)
            {
                innerContentStart = innerLineStart;
            }
            const std::string innerIndent(content.begin() + innerLineStart, content.begin() + innerContentStart);

            // Build the block using the same indentation as the surrounding JSON
            const std::string block =
                indent + "\"enable_environment\": {\n" +
                innerIndent + "\"LYMALINK_OVERLAY_ENABLE\": \"1\"\n" +
                indent + "},\n";

            content.insert(lineStart, block);
            changed = true;
        }
    }

    // Skip writing if no modifications were made
    if (!changed)
    {
        LOG_BE(Urgency::Debug, "Manifest already in desired state (%s): %s", enabled ? "enabled" : "disabled", manifestPath.c_str());
        return;
    }

    const std::filesystem::path targetPath = manifestPath;
    const std::filesystem::path tempPath = targetPath.string() + ".tmp." + std::to_string(getpid());
    std::error_code ec;
    const std::filesystem::perms targetPerms = std::filesystem::status(targetPath, ec).permissions();

    // Write through a temp file and rename so Flatpak hardlinked OSTree files are not modified in place.
    std::ofstream out(tempPath, std::ios::trunc);
    if (!out.is_open())
    {
        LOG_BE(Urgency::Critical, "Failed to open overlay manifest for write: %s", manifestPath.c_str());
        return;
    }

    out << content;
    if (!out.good())
    {
        LOG_BE(Urgency::Critical, "Failed to write overlay manifest: %s", manifestPath.c_str());
        out.close();
        std::filesystem::remove(tempPath, ec);
        return;
    }
    out.close();

    if (targetPerms != std::filesystem::perms::unknown)
    {
        std::filesystem::permissions(tempPath, targetPerms, ec);
        ec.clear();
    }

    std::filesystem::rename(tempPath, targetPath, ec);
    if (ec)
    {
        LOG_BE(Urgency::Critical, "Failed to replace overlay manifest: %s: %s", manifestPath.c_str(), ec.message().c_str());
        std::filesystem::remove(tempPath, ec);
        return;
    }

    LOG_BE(Urgency::Debug, "Overlay manifest updated: %s (%s)", manifestPath.c_str(), enabled ? "enable_environment restored" : "enable_environment removed");
}

/////////////////////////////////////////////////////////////////////

std::string Lymalinkd::ResolveInstalledNotificationSoundPath(bool allowCustomSound) const
{
    std::string notificationSoundPath = "";

    // Allow developers/users to override the sound path via environment variable
    if (allowCustomSound)
    {
        if (const char* overridePath = std::getenv("LYMALINK_NOTIFICATION_SOUND_PATH"))
        {
            if (*overridePath != '\0' && IsSupportedCustomNotificationSound(std::filesystem::path(overridePath)))
            {
                notificationSoundPath = overridePath;
                LOG_BE(Urgency::Info, "Notification sound path overridden via LYMALINK_NOTIFICATION_SOUND_PATH: %s", notificationSoundPath.c_str());
                return notificationSoundPath;
            }
            LOG_BE(Urgency::Warning, "Ignoring invalid LYMALINK_NOTIFICATION_SOUND_PATH override.");
        }
    }

    if (allowCustomSound)
    {
        if (const char* overridePath = std::getenv("LYMALINK_ACHIEVEMENT_SOUND_PATH"))
        {
            if (*overridePath != '\0' && IsSupportedCustomNotificationSound(std::filesystem::path(overridePath)))
            {
                notificationSoundPath = overridePath;
                LOG_BE(Urgency::Info, "Notification sound path overridden via LYMALINK_ACHIEVEMENT_SOUND_PATH: %s", notificationSoundPath.c_str());
                return notificationSoundPath;
            }
            LOG_BE(Urgency::Warning, "Ignoring invalid LYMALINK_ACHIEVEMENT_SOUND_PATH override.");
        }
    }

    // Resolve installed notification sounds directory
    const std::filesystem::path soundDir = ResolveDataPath("Lymalink/sounds");
    if (soundDir.empty())
    {
        LOG_BE(Urgency::Warning, "Notification sounds directory path is empty.");
        return notificationSoundPath;
    }

    // Try loading a custom notification sound - Fall back to bundled sounds if invalid.
    bool useCustomSound = false;
    std::string customSoundPath;
    std::string bundledSound;
    LoadNotificationSoundConfig(useCustomSound, customSoundPath, bundledSound);

    if (allowCustomSound && useCustomSound)
    {
        if (!customSoundPath.empty() && IsSupportedCustomNotificationSound(std::filesystem::path(customSoundPath)))
        {
            notificationSoundPath = customSoundPath;
            LOG_BE(Urgency::Info, "Custom notification sound loaded from config: %s", notificationSoundPath.c_str());
            return notificationSoundPath;
        }

        LOG_BE(Urgency::Warning, "Custom notification sound unavailable, falling back to bundled sounds: %s", customSoundPath.c_str());
    }

    // Try loading a user-configured bundled achievement sound
    if (!bundledSound.empty())
    {
        const std::filesystem::path configuredSoundPath(bundledSound);

        // Only allow plain .ogg filenames
        if (configuredSoundPath.filename() == configuredSoundPath && configuredSoundPath.extension() == ".ogg")
        {
            const std::filesystem::path installedSoundPath = soundDir / configuredSoundPath;

            // Use the configured sound if the file exists
            if (std::filesystem::exists(installedSoundPath))
            {
                notificationSoundPath = installedSoundPath.string();
                LOG_BE(Urgency::Debug, "Notification sound loaded from config: %s", notificationSoundPath.c_str());
                return notificationSoundPath;
            }

            LOG_BE(Urgency::Warning, "Configured notification sound not found: %s", installedSoundPath.string().c_str());
        }
        else
        {
            LOG_BE(Urgency::Warning, "Ignoring invalid notification sound config value: %s", bundledSound.c_str());
        }
    }

    // Scan installed .ogg files as fallback sounds
    std::vector<std::filesystem::path> installedSounds;
    std::error_code ec;
    if (std::filesystem::exists(soundDir, ec))
    {
        for (const auto& entry : std::filesystem::directory_iterator(soundDir, ec))
        {
            if (entry.is_regular_file(ec) && entry.path().extension() == ".ogg")
            {
                installedSounds.push_back(entry.path());
            }
        }
    }

    // Sort sounds to ensure deterministic fallback selection
    std::sort(installedSounds.begin(), installedSounds.end());
    const std::filesystem::path defaultSoundPath = soundDir / DEFAULT_NOTIFICATION_SOUND;
    if (std::filesystem::exists(defaultSoundPath))
    {
        notificationSoundPath = defaultSoundPath.string();
        LOG_BE(Urgency::Debug, "Using default notification sound: %s", notificationSoundPath.c_str());
        return notificationSoundPath;
    }

    if (!installedSounds.empty())
    {
        notificationSoundPath = installedSounds.front().string();
        LOG_BE(Urgency::Debug, "Using default notification sound: %s", notificationSoundPath.c_str());
        return notificationSoundPath;
    }

    LOG_BE(Urgency::Critical, "No installed notification sounds found under: %s", soundDir.string().c_str());
    return notificationSoundPath;
}

/////////////////////////////////////////////////////////////////////

std::string Lymalinkd::ResolveConfigPath() const
{
    std::string configPath = "";

    // Prefer XDG_CONFIG_HOME if defined
    std::filesystem::path configHome;
    if (const char* xdgConfigHome = std::getenv("XDG_CONFIG_HOME"))
    {
        if (*xdgConfigHome != '\0')
        {
            configHome = xdgConfigHome;
            LOG_BE(Urgency::Debug, "XDG_CONFIG_HOME detected: %s", xdgConfigHome);
        }
    }

    if (configHome.empty())
    {
        // Fallback to ~/.config if XDG_CONFIG_HOME is unavailable
        const char* home = std::getenv("HOME");
        if (!home || *home == '\0')
        {
            LOG_BE(Urgency::Critical, "HOME environment variable not set or empty. Cannot resolve config path.");
            return configPath;
        }
        configHome = std::filesystem::path(home) / ".config";
    }

    configPath = (configHome / ORGANIZATION / (std::string(APPLICATION) + ".ini")).string();
    LOG_BE(Urgency::Debug, "Config path resolved to: %s", configPath.c_str());
    return configPath;
}

/////////////////////////////////////////////////////////////////////

std::string Lymalinkd::ResolveDataPath(const std::string& relativePath) const
{
    std::string dataPath = "";

    // Prefer XDG_DATA_HOME if defined
    std::filesystem::path dataHome;
    if (const char* xdgDataHome = std::getenv("XDG_DATA_HOME"))
    {
        if (*xdgDataHome != '\0')
        {
            dataHome = xdgDataHome;
            LOG_BE(Urgency::Debug, "XDG_DATA_HOME detected: %s", xdgDataHome);
        }
    }

    // Fallback to ~/.local/share if XDG_DATA_HOME is unavailable
    if (dataHome.empty())
    {
        const char* home = std::getenv("HOME");
        if (!home || *home == '\0')
        {
            LOG_BE(Urgency::Critical, "HOME environment variable not set or empty. Cannot resolve data path.");
            return dataPath;
        }
        dataHome = std::filesystem::path(home) / ".local" / "share";
    }

    dataPath = (dataHome / relativePath).string();
    return dataPath;
}

/////////////////////////////////////////////////////////////////////

void Lymalinkd::LoadNotificationSoundConfig(bool& outUseCustomSound, std::string& outCustomSoundPath, std::string& outBundledSound) const
{
    // Reset all output values before parsing
    outUseCustomSound = false;
    outCustomSoundPath = {};
    outBundledSound = {};

    // Resolve the application's config file path
    const std::string configPath = ResolveConfigPath();
    if (configPath.empty())
    {
        LOG_BE(Urgency::Warning, "Cannot load sound config because config path is empty.");
        return;
    }

    outBundledSound = Utils::ReadIniValue(configPath, GROUP_BACKGROUND_SERVICE, "NotificationSound");
    outUseCustomSound = ParseConfigBool(Utils::ReadIniValue(configPath, GROUP_BACKGROUND_SERVICE, "CustomNotificationSound"));
    outCustomSoundPath = Utils::ReadIniValue(configPath, GROUP_BACKGROUND_SERVICE, "CustomNotificationSoundPath");

    LOG_BE(Urgency::Debug, "Sound config loaded. Bundled: %s, Use custom: %d, Custom path: %s", 
        outBundledSound.empty() ? "none" : outBundledSound.c_str(), outUseCustomSound, outCustomSoundPath.empty() ? "none" : outCustomSoundPath.c_str());
}

/////////////////////////////////////////////////////////////////////

bool Lymalinkd::ParseConfigBool(const std::string& value) const
{
    // Normalize to lowercase before comparing against accepted truthy values
    std::string normalized = Utils::TrimWhitespace(value);
    std::transform(normalized.begin(), normalized.end(), normalized.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });

    const bool isTruthy = normalized == "true" || normalized == "1" || normalized == "yes" || normalized == "on";
    return isTruthy;
}

/////////////////////////////////////////////////////////////////////

bool Lymalinkd::IsSupportedCustomNotificationSound(const std::filesystem::path& soundPath) const
{
    // Normalize extension to lowercase before comparing against supported formats
    std::string extension = soundPath.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });

    if (extension != ".ogg" && extension != ".wav")
    {
        LOG_BE(Urgency::Debug, "Unsupported extension '%s' for sound path: %s", extension.c_str(), soundPath.c_str());
        return false;
    }

    // Confirm the path resolves to an actual file
    std::error_code ec;
    const bool isFile = std::filesystem::is_regular_file(soundPath, ec);
    
    if (!isFile)
    {
        LOG_BE(Urgency::Warning, "Sound path does not resolve to a regular file: %s", soundPath.c_str());
    }
    else
    {
        LOG_BE(Urgency::Debug, "Validated supported custom sound file: %s", soundPath.c_str());
    }

    return isFile;
}
