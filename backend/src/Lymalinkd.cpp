/////////////////////////////////////////////////////////
// File: Lymalinkd.cpp
// Date: 2026-05-22
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Implementation of Lymalinkd backend service
/////////////////////////////////////////////////////////

#include "Lymalinkd.h"
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

/////////////////////////////////////////////////////////////////////

Lymalinkd::Lymalinkd() :
    m_achievementNotifications(m_database, m_freedesktopNotifications, m_notificationSound)
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
        Logger::Log("[Lymalinkd] sigprocmask failed: " + std::string(strerror(errno)));
        err = Error::UnknownError;
        return err;
    }

    // Start backend services before monitor loop
    err = Init();
    if (err != Error::NoError)
    {
        Logger::Log("[Lymalinkd] Init failed, exiting.");
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

    err = DatabaseInit();
    if (err != Error::NoError)
    {
        return err;
    }

    // Configure notification backends from resolved database/data paths
    std::filesystem::path databaseParent = std::filesystem::path(m_databasePath).parent_path();
    m_achievementNotifications.Configure(m_databaseConnectionName, databaseParent.string());
    err = m_freedesktopNotifications.Init();
    if (err != Error::NoError)
    {
        Logger::Log("[Lymalinkd] Desktop notifications unavailable.");
    }

    err = m_notificationSound.Init(ResolveInstalledNotificationSoundPath());
    m_notificationSound.SetFallbackSoundPath(ResolveInstalledNotificationSoundPath(false));
    if (err != Error::NoError)
    {
        Logger::Log("[Lymalinkd] Achievement sounds unavailable.");
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

	err = m_dbus.Init();
    if (err != Error::NoError)
    {
        Logger::Log("[Lymalinkd] DBusService init failed.");
        return err;
    }

    // ProcessWatcher callbacks
    m_processWatcher.onProcessStarted = [this](int targetId, const std::string& exe) { OnProcessStarted(targetId, exe); };
    m_processWatcher.onProcessStopped = [this](int targetId, long secs) { OnProcessStopped(targetId, secs); };

    // Start process watcher with executable paths from database
    m_processWatcher.SetTargets(LoadExeTargetsFromDatabase());
    m_processWatcher.Start();

    // Signal systemd that we are ready (no-op if not under systemd)
    m_notify.NotifyReady();
    m_notify.NotifyStatus("Running");

	Logger::Log("[Lymalinkd] Init complete.");
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
        Logger::Log("[Lymalinkd] Database path resolve failed.");
        err = Error::DatabaseError;
        return err;
    }

    if (!m_database.DatabaseFileExists(m_databasePath))
    {
        Logger::Log("[Lymalinkd] Database file not found: " + m_databasePath);
        err = Error::DatabaseError;
        return err;
    }

    if (!m_database.OpenDatabase(m_databaseConnectionName, m_databasePath))
    {
        Logger::Log("[Lymalinkd] Database open failed: " + m_database.LastError());
        err = Error::DatabaseError;
        return err;
    }

    Logger::Log("[Lymalinkd] Database opened: " + m_databasePath);
    return err;
}

/////////////////////////////////////////////////////////////////////

void Lymalinkd::Monitor()
{
    Logger::Log("[Lymalinkd] Entering main Monitor loop");

	while (m_running.load())
    {
        if (!m_processActive.load())
        {
            Logger::Log("[Lymalinkd][Monitor] No active processes, going to sleep...");

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
            
            // Reload after wakeup
            m_processWatcher.SetTargets(LoadExeTargetsFromDatabase());
        }

        Logger::Log("[Lymalinkd] Process active, orchestrating...");

        auto lastScanTime = std::chrono::steady_clock::now();
        while (m_running.load() && m_processActive.load())
        {
            auto currentTime = std::chrono::steady_clock::now();

            // Scan AppId dir for currently active executable (5 second interval)
            if (std::chrono::duration_cast<std::chrono::seconds>(currentTime - lastScanTime).count() >= 5)
            {
                if (HasCurrentActiveTargetsNeedingAppIdDirScan())
                {
                    const std::vector<AppIdDirPathScanTarget> currentActivePrefixPaths = LoadCurrentActivePrefixPaths();
                    m_pathScanner.SetTargets(currentActivePrefixPaths);

                    if (!currentActivePrefixPaths.empty())
                    {
                        Logger::Log("[Lymalinkd][Monitor] Scanning for AppId dir...");

                        const std::vector<AppIdDirPathScanResult> scanResults = m_pathScanner.ScanOnceForAppIdDir();
                        if (!scanResults.empty())
                        {
                            // Persist discovered AppId dirs and drop completed targets
                            SavePathScanResults(scanResults);

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
        Logger::Log("[Lymalinkd] signalfd failed: " + std::string(strerror(errno)));
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

            Logger::Log("[Lymalinkd] signalfd read failed: " + std::string(strerror(errno)));
            m_running.store(false);
            break;
        }

        if (bytes != sizeof(info))
        {
            Logger::Log("[Lymalinkd] signalfd read returned incomplete signal info");
            continue;
        }

        // Valid signal means daemon should exit main loop
        Logger::Log("[Lymalinkd] Signal received: " + std::to_string(info.ssi_signo));
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
    Logger::Log("[Lymalinkd] Shutdown initiated.");

    // Wait signal thread to shutdown
    if (m_signalThread.joinable())
    {
        m_signalThread.join();
    }

    m_notify.NotifyStopping();

    // Stop external services before closing database connection
    m_processWatcher.Stop();
    m_freedesktopNotifications.Stop();
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

    Logger::Log("[Lymalinkd] Shutdown complete.");
}


/////////////////////////////////////////////////////////////////////

void Lymalinkd::OnProcessStarted(int targetId, const std::string& executablePath)
{
    Logger::Log("[Lymalinkd] OnProcessStarted - targetId=" + std::to_string(targetId) + " exe=" + executablePath);

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
    m_dbus.EmitGameStateChanged(targetId, "Active");
    m_cv.notify_one();
}

/////////////////////////////////////////////////////////////////////

void Lymalinkd::OnProcessStopped(int targetId, long secondsPlayed)
{
    Logger::Log("[Lymalinkd] OnProcessStopped - targetId=" + std::to_string(targetId) + " playtime=" + std::to_string(secondsPlayed) + "s");

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
                Logger::Log("[Lymalinkd] No active processes for 60s, returning to sleep.");
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

    // Show local notification and forward event over DBus
    m_achievementNotifications.NotifyUnlocked(targetId, achievementKey);
    m_dbus.EmitAchievementUnlocked(targetId, achievementKey);
}

/////////////////////////////////////////////////////////////////////

void Lymalinkd::OnTestToast()
{
    // Reuse installed app icon for both notification image slots
    const std::string iconPath = ResolveInstalledAppIconPath();

    AchievementNotification notification;
    notification.achievementName = "Test toast";
    notification.achievementDescription = "Lymalink notification test";
    notification.iconPath = iconPath;
    notification.appIconPath = iconPath;

    // Display notification and play configured sound
    m_freedesktopNotifications.ShowAchievementToast(notification);
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
    Logger::Log("[Lymalinkd] Reloading all targets from database.");

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

    Logger::Log("[Lymalinkd] AppID dir scan targets loaded: " + std::to_string(targets.size()));
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
                Logger::Log("[Lymalinkd] Failed to save APPID dir result: targetId=" + std::to_string(result.targetId) + " error=" + m_database.LastError());
                continue;
            }

            savedTargetIds.push_back(result.targetId);
            Logger::Log("[Lymalinkd] APPID dir saved: targetId=" + std::to_string(result.targetId) + " emulator=" + result.emulatorType);
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
        Logger::Log("[Lymalinkd] Failed to save playtime: targetId=" + std::to_string(targetId) + " error=" + m_database.LastError());
    }
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
            return databasePath;
        }
    }

    // Fallback to default user data location
    const char* home = std::getenv("HOME");
    if (!home || *home == '\0')
    {
        return databasePath;
    }

    databasePath = (std::filesystem::path(home) / ".local" / "share" / "Lymalink" / DATABASE_FILE_NAME).string();
    return databasePath;
}

/////////////////////////////////////////////////////////////////////

std::string Lymalinkd::ResolveInstalledAppIconPath() const
{
    std::string appIconPath = "";

    // Use installed app icon only if the resolved file exists
    const std::filesystem::path iconPath = ResolveDataPath(LYMALINK_APP_ICON_PATH);
    if (std::filesystem::exists(iconPath))
    {
        appIconPath = iconPath.string();
    }

    return appIconPath;
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
                return notificationSoundPath;
            }
            Logger::Log("[Lymalinkd] Ignoring invalid LYMALINK_NOTIFICATION_SOUND_PATH override.");
        }
    }

    if (allowCustomSound)
    {
        if (const char* overridePath = std::getenv("LYMALINK_ACHIEVEMENT_SOUND_PATH"))
        {
            if (*overridePath != '\0' && IsSupportedCustomNotificationSound(std::filesystem::path(overridePath)))
            {
                notificationSoundPath = overridePath;
                return notificationSoundPath;
            }
            Logger::Log("[Lymalinkd] Ignoring invalid LYMALINK_ACHIEVEMENT_SOUND_PATH override.");
        }
    }

    // Resolve installed notification sounds directory
    const std::filesystem::path soundDir = ResolveDataPath("Lymalink/sounds");
    if (soundDir.empty())
    {
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
            Logger::Log("[Lymalinkd] Custom notification sound loaded from config: " + notificationSoundPath);
            return notificationSoundPath;
        }

        Logger::Log("[Lymalinkd] Custom notification sound unavailable, falling back to bundled sounds: " + customSoundPath);
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
                Logger::Log("[Lymalinkd] Notification sound loaded from config: " + notificationSoundPath);
                return notificationSoundPath;
            }

            Logger::Log("[Lymalinkd] Configured notification sound not found: " + installedSoundPath.string());
        }
        else
        {
            Logger::Log("[Lymalinkd] Ignoring invalid notification sound config value: " + bundledSound);
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
        Logger::Log("[Lymalinkd] Using default notification sound: " + notificationSoundPath);
        return notificationSoundPath;
    }

    if (!installedSounds.empty())
    {
        notificationSoundPath = installedSounds.front().string();
        Logger::Log("[Lymalinkd] Using default notification sound: " + notificationSoundPath);
        return notificationSoundPath;
    }

    Logger::Log("[Lymalinkd] No installed notification sounds found under: " + soundDir.string());
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
        }
    }

    if (configHome.empty())
    {
        // Fallback to ~/.config if XDG_CONFIG_HOME is unavailable
        const char* home = std::getenv("HOME");
        if (!home || *home == '\0')
        {
            return configPath;
        }
        configHome = std::filesystem::path(home) / ".config";
    }

    configPath = (configHome / ORGANIZATION / (std::string(APPLICATION) + ".ini")).string();
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
        }
    }

    // Fallback to ~/.local/share if XDG_DATA_HOME is unavailable
    if (dataHome.empty())
    {
        const char* home = std::getenv("HOME");
        if (!home || *home == '\0')
        {
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
        return;
    }

    std::ifstream configFile(configPath);
    if (!configFile.is_open())
    {
        return;
    }

    bool inBackgroundServiceGroup = false;

    // Read the config file line-by-line
    std::string line;
    while (std::getline(configFile, line))
    {
        line = Utils::TrimWhitespace(line);
        if (line.empty() || line[0] == ';' || line[0] == '#')
        {
            continue;
        }

        // Track whether we are inside the target config section
        if (line.front() == '[' && line.back() == ']')
        {
            inBackgroundServiceGroup = Utils::TrimWhitespace(line.substr(1, line.size() - 2)) == GROUP_BACKGROUND_SERVICE;
            continue;
        }

        if (!inBackgroundServiceGroup)
        {
            continue;
        }

        // Ignore malformed config entries without =
        const size_t separator = line.find('=');
        if (separator == std::string::npos)
        {
            continue;
        }

        const std::string key = Utils::TrimWhitespace(line.substr(0, separator));
        const std::string value = Utils::TrimWhitespace(line.substr(separator + 1));
        if (key == "NotificationSound")
        {
            outBundledSound = value;
        }
        else if (key == "CustomNotificationSound")
        {
            outUseCustomSound = ParseConfigBool(value);
        }
        else if (key == "CustomNotificationSoundPath")
        {
            outCustomSoundPath = value;
        }
    }
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
        return false;
    }

    // Confirm the path resolves to an actual file
    std::error_code ec;
    const bool isFile = std::filesystem::is_regular_file(soundPath, ec);
    return isFile;
}
