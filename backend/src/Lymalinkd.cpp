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
#include <format>
#include <algorithm>
#include <unordered_set>
#if defined(_WIN32)
    #include <QCoreApplication>
    #include <QMetaObject>
    #include <windows.h>
    #include <tlhelp32.h>
#else
    #include <sys/signalfd.h>
    #include <unistd.h>
#endif

#if defined(_WIN32)
    #define WINDOWS_OVERLAY_INJECTION_DELAY_MS 100
#endif

#define COMPONENT "Lymalinkd"

/////////////////////////////////////////////////////////////////////

Lymalinkd::Lymalinkd() :
    m_achievementKeyResolver(m_database, DATABASE_CONNECTION_NAME),
    m_achievementNotifications(m_database, m_overlayNotifications, m_notificationSound)
{
    m_processActive.store(false);
    m_activeCount.store(0);
    m_sleepTimerGeneration.store(0);
    m_running.store(true);
    m_startupNotificationEnabled.store(true);
    m_manualScanActive.store(false);
    m_manualScanCancelRequested.store(false);
    m_activeTargetsIds = {};
    m_targetIdsRequiringDirScan = {};
    m_manualScanTargetId = 0;
    m_manualScanCancelReason = "";
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

#if defined(_WIN32)
    err = Init();
    if (err != Error::NoError)
    {
        LOG_BE(Urgency::Fatal, "Init failed, exiting.");
        return err;
    }

    m_running.store(true);
    m_monitorThread = std::thread(&Lymalinkd::Monitor, this);

    QCoreApplication::exec();
    m_running.store(false);
    m_cv.notify_all();

    if (m_monitorThread.joinable())
    {
        m_monitorThread.join();
    }

    Shutdown();
    return err;
#else
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
#endif
}

/////////////////////////////////////////////////////////////////////
///////////////////////////// PRIVATE ///////////////////////////////
/////////////////////////////////////////////////////////////////////

Error Lymalinkd::Init()
{
    Error err = Error::NoError;

    // Reset monitor state before services start
    m_processActive.store(false);

#if !defined(_WIN32)
    // Enable Vulkan based Overlay loading to games by tweaking manifest
    SetVulkanOverlayManifestEnableEnvironment(false);
#endif

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
#if !defined(_WIN32)
    else
    {
        // Start in paused, until running executable is detected
        m_overlayNotifications.SetSocketPaused(true);
    }
#endif

    err = m_notificationSound.Init(ResolveInstalledNotificationSoundPath());
    m_notificationSound.SetFallbackSoundPath(ResolveInstalledNotificationSoundPath(false));
    if (err != Error::NoError)
    {
        LOG_BE(Urgency::Warning, "Achievement sounds unavailable.");
    }
    m_startupNotificationEnabled.store(LoadStartupNotificationConfig());

    // Cache targets still requiring AppId dir scan
    std::unordered_map<int, AppIdDirPathScanTarget> targetsMissingAppIdDir = LoadAppIdDirScanTargetsFromDatabase();
    {
        std::lock_guard<std::mutex> lock(m_targetIdsRequiringDirScanMutex);
        m_targetIdsRequiringDirScan = targetsMissingAppIdDir;
    }

#if defined(_WIN32)
    m_ipc.onRequestActiveTargets = [this]() { OnRequestActiveTargets(); };
    m_ipc.onReloadTarget = [this](int) { OnReloadAllTargets(); };
    m_ipc.onReloadAllTargets = [this]() { OnReloadAllTargets(); };
    m_ipc.onReloadConfig = [this]() { OnReloadConfig(); };
    m_ipc.onStartManualAchievementDataScan = [this](int targetId) { OnStartManualAchievementDataScan(targetId); };
    m_ipc.onCancelManualAchievementDataScan = [this](int targetId) { OnCancelManualAchievementDataScan(targetId); };
    m_ipc.onTestToast = [this]() { OnTestToast(); };
    m_ipc.onTestSound = [this]() { OnTestSound(); };
    m_ipc.onShutdown = [this]() { OnShutdown(); };
    if (!m_ipc.Start())
    {
        return Error::UnknownError;
    }
#else
    // Wire DBus requests to daemon handlers
    m_dbus.onRequestActiveTargets = [this]() { OnRequestActiveTargets(); };
    m_dbus.onReloadAllTargets = [this]() { OnReloadAllTargets(); };
    m_dbus.onReloadConfig = [this]() { OnReloadConfig(); };
    m_dbus.onStartManualAchievementDataScan = [this](int32_t targetId) { OnStartManualAchievementDataScan(static_cast<int>(targetId)); };
    m_dbus.onCancelManualAchievementDataScan = [this](int32_t targetId) { OnCancelManualAchievementDataScan(static_cast<int>(targetId)); };
    m_dbus.onTestToast = [this]() { OnTestToast(); };
    m_dbus.onTestSound = [this]() { OnTestSound(); };

    err = m_dbus.Init();
    if (err != Error::NoError)
    {
        LOG_BE(Urgency::Critical, "DBusService init failed.");
        return err;
    }
#endif

    m_trayIcon.onQuitBackend = [this]() { OnShutdown(); };
    m_trayIcon.Start(ResolveDataPath(LYMALINKD_TRAY_ICON_PATH));

    // ProcessWatcher callbacks
    m_processWatcher.onProcessStarted = [this](int targetId, const std::string& exe, uint32_t pid) { OnProcessStarted(targetId, exe, pid); };
    m_processWatcher.onProcessStopped = [this](int targetId, long secs) { OnProcessStopped(targetId, secs); };
    m_achievementHandler.onAppIdDirUnavailable = [this](int targetId, const std::string& appIdDirPath) { OnAppIdDirUnavailable(targetId, appIdDirPath); };

    m_achievementHandler.Init();
    m_achievementHandler.Start();

    // Start process watcher with executable paths from database
    m_processWatcher.SetTargets(LoadExeTargetsFromDatabase());
    m_processWatcher.Start();

#if !defined(_WIN32)
    // Signal systemd that we are ready (no-op if not under systemd)
    m_notify.NotifyReady();
    m_notify.NotifyStatus("Running");
#endif

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

    // Execute migrates for updated version of Lymalink
    bool achievementDataStatusColumnAdded = false;
    if (!EnsureColumn(m_databaseEmuGamesTable, "installation_dir", "installation_dir TEXT") ||
        !EnsureColumn(m_databaseEmuGamesTable, "data_opt", "data_opt TEXT") ||
        !EnsureColumn(m_databaseEmuGamesTable, "achievement_data_status", "achievement_data_status INTEGER DEFAULT 0", &achievementDataStatusColumnAdded))
    {
        LOG_BE(Urgency::Critical, "Database migration failed: %s", m_database.LastError().c_str());
        err = Error::DatabaseError;
        return err;
    }
    if (achievementDataStatusColumnAdded)
    {
        if (!m_database.ExecuteSql(m_databaseConnectionName, std::format("UPDATE {} SET achievement_data_status = 1 WHERE appid_dir_found = 1 AND achievement_data_status = 0", m_databaseEmuGamesTable)))
        {
            LOG_BE(Urgency::Critical, "Database achievement data status sync failed: %s", m_database.LastError().c_str());
            err = Error::DatabaseError;
            return err;
        }
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
#if !defined(_WIN32)
            m_overlayNotifications.SetSocketPaused(true);
#endif
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

#if !defined(_WIN32)
            m_overlayNotifications.SetSocketPaused(false);
#endif
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
                    // SmartSteamEmu stores achievement keys as CRC32 values, so resolve them before DB updates
                    AchievementData resolvedAchievement = achievement;
                    bool shouldIgnoreUnresolvedKey = false;
                    {
                        std::lock_guard<std::mutex> lock(m_databaseMutex);
                        resolvedAchievement.key = m_achievementKeyResolver.ResolveKey(activeTarget.first, achievement.key);
                        shouldIgnoreUnresolvedKey = resolvedAchievement.key == achievement.key && m_achievementKeyResolver.ShouldIgnoreUnresolvedKey(activeTarget.first, achievement.key);
                    }
                    if (shouldIgnoreUnresolvedKey)
                    {
                        LOG_BE(Urgency::Debug, "Ignoring unresolved CRC entry: targetId=%d key=%s", activeTarget.first, achievement.key.c_str());
                        continue;
                    }

                    const bool saved = SaveAchievementState(activeTarget.first, resolvedAchievement);
                    if (saved && resolvedAchievement.newlyUnlocked && resolvedAchievement.achieved)
                    {
                        // OnAchievementUnlocked is only done IF achievement is actually new AND it was marked as achieved
                        OnAchievementUnlocked(activeTarget.first, resolvedAchievement.key);
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
                                if (result.appidDirFound)
                                {
                                    if (result.emulatorType == "SmartSteamEmu")
                                    {
                                        // SmartSteamEmu stat rows cannot be mapped safely, so hide stale DB progress
                                        bool progressChanged = false;
                                        {
                                            std::lock_guard<std::mutex> lock(m_databaseMutex);
                                            progressChanged = DisableAchievementProgress(result.targetId);
                                            m_achievementKeyResolver.PrepareTargetKeys(result.targetId);
                                        }
                                        if (progressChanged)
                                        {
                                            EmitTargetDataChanged(result.targetId);
                                        }
                                    }
#if defined(_WIN32)
                                    std::filesystem::file_time_type processStartedAt{};
                                    bool hasProcessStartTime = false;
                                    {
                                        std::lock_guard<std::mutex> lock(m_activeTargetsMutex);
                                        const auto it = m_processStartedAt.find(result.targetId);
                                        if (it != m_processStartedAt.end())
                                        {
                                            processStartedAt = it->second;
                                            hasProcessStartTime = true;
                                        }
                                    }

                                    m_achievementHandler.AddTarget(result.targetId, result.appidDirLocation, result.emulatorType, hasProcessStartTime ? std::optional{processStartedAt} : std::nullopt);
#else
                                    m_achievementHandler.AddTarget(result.targetId, result.appidDirLocation, result.emulatorType);
#endif
                                }
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

#if !defined(_WIN32)
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
#endif

/////////////////////////////////////////////////////////////////////

void Lymalinkd::Shutdown()
{
    LOG_BE(Urgency::Debug, "Shutdown initiated.");

#if !defined(_WIN32)
    // Wait signal thread to shutdown
    if (m_signalThread.joinable())
    {
        m_signalThread.join();
    }

    m_notify.NotifyStopping();
#endif

    // Stop external services before closing database connection
    m_trayIcon.Stop();
    RequestManualAchievementDataScanCancel(0, "cancelled");
    if (m_manualScanThread.joinable())
    {
        m_manualScanThread.join();
    }

    m_processWatcher.Stop();
    m_overlayNotifications.Shutdown();
    m_notificationSound.Stop();
#if defined(_WIN32)
    m_ipc.Stop();
#else
    m_dbus.Stop();
#endif

    {
        std::lock_guard<std::mutex> lock(m_startupNotificationThreadsMutex);
        for (std::thread& thread : m_startupNotificationThreads)
        {
            if (thread.joinable())
            {
                thread.join();
            }
        }
        m_startupNotificationThreads.clear();
    }

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

#if !defined(_WIN32)
    // Disable Vulkan based Overlay loading to games by tweaking manifest
    SetVulkanOverlayManifestEnableEnvironment(true);
#endif
    LOG_BE(Urgency::Debug, "Shutdown complete.");
}

/////////////////////////////////////////////////////////////////////

#if defined(_WIN32)
bool Lymalinkd::IsWindowsProcessAlive(uint32_t pid) const
{
    // Open a lightweight handle and check whether the process has exited
    HANDLE process = OpenProcess(SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!process)
    {
        return false;
    }

    const DWORD wait = WaitForSingleObject(process, 0);
    CloseHandle(process);
    return wait == WAIT_TIMEOUT;
}

/////////////////////////////////////////////////////////////////////

std::vector<uint32_t> Lymalinkd::CollectWindowsProcessTree(uint32_t rootPid) const
{
    std::vector<uint32_t> pids;
    if (rootPid == 0)
    {
        return pids;
    }

    // Always inject the watched root process first
    pids.push_back(rootPid);

    const HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE)
    {
        LOG_BE(Urgency::Warning, "Process tree snapshot failed for root pid=%u.", rootPid);
        return pids;
    }

    // Cache snapshot rows so parent/child expansion can run without holding OS resources
    std::vector<PROCESSENTRY32> entries;
    PROCESSENTRY32 entry{sizeof(entry)};
    for (BOOL ok = Process32First(snapshot, &entry); ok; ok = Process32Next(snapshot, &entry))
    {
        entries.push_back(entry);
    }
    CloseHandle(snapshot);

    // Add direct and nested child processes below the watched root
    bool added = true;
    while (added)
    {
        added = false;
        for (const PROCESSENTRY32& process : entries)
        {
            const uint32_t pid = static_cast<uint32_t>(process.th32ProcessID);
            const uint32_t parentPid = static_cast<uint32_t>(process.th32ParentProcessID);
            if (std::find(pids.begin(), pids.end(), pid) != pids.end())
            {
                continue;
            }
            if (std::find(pids.begin(), pids.end(), parentPid) == pids.end())
            {
                continue;
            }

            pids.push_back(pid);
            added = true;
        }
    }

    return pids;
}

/////////////////////////////////////////////////////////////////////

void Lymalinkd::InjectWindowsOverlayProcessTree(int targetId, uint32_t rootPid)
{
    LOG_BE(Urgency::Debug, "Delaying starting Windows overlay injection for stability for targetId=%d rootPid=%u by %dms.", targetId, rootPid, 3000);
    std::this_thread::sleep_for(std::chrono::milliseconds(3000));

    // Retry briefly because some games spawn the real render process after the launcher process
    auto worker = [this, targetId, rootPid]() {
        std::unordered_set<uint32_t> injected;
        constexpr int retrySeconds = 45;

        for (int second = 0; second < retrySeconds && m_running.load(); ++second)
        {
            // Stop polling once the watched root process has exited
            if (!IsWindowsProcessAlive(rootPid))
            {
                LOG_BE(Urgency::Debug, "Windows overlay process-tree injection stopped: root pid=%u exited.", rootPid);
                return;
            }

            // Inject each newly discovered process in the root process tree once
            const std::vector<uint32_t> pids = CollectWindowsProcessTree(rootPid);
            for (uint32_t pid : pids)
            {
                if (pid == 0 || injected.count(pid))
                {
                    continue;
                }

                LOG_BE(Urgency::Info, "Windows overlay process-tree injection targetId=%d rootPid=%u pid=%u.", targetId, rootPid, pid);
                const bool overlayMappingReady = m_overlayNotifications.RegisterProcess(targetId, pid);
                if (!overlayMappingReady)
                {
                    LOG_BE(Urgency::Warning, "Overlay mapping unavailable for targetId=%d pid=%u.", targetId, pid);
                    injected.insert(pid);
                    continue;
                }

                LOG_BE(Urgency::Debug, "Delaying Windows overlay injection for stability for targetId=%d pid=%u by %dms.", targetId, pid, WINDOWS_OVERLAY_INJECTION_DELAY_MS);
                std::this_thread::sleep_for(std::chrono::milliseconds(WINDOWS_OVERLAY_INJECTION_DELAY_MS));

                // After delay, check if process is still alive - If not, do not try to inject
                if (!m_running.load() || !IsWindowsProcessAlive(rootPid) || !IsWindowsProcessAlive(pid))
                {
                    LOG_BE(Urgency::Debug, "Windows overlay injection skipped after delay: rootPid=%u pid=%u exited.", rootPid, pid);
                    injected.insert(pid);
                    continue;
                }

                if (!m_overlayInjector.InjectOpenGL(pid))
                {
                    LOG_BE(Urgency::Warning, "OpenGL overlay injection unavailable for targetId=%d pid=%u.", targetId, pid);
                }
                if (!m_overlayInjector.InjectDirect3D9(pid))
                {
                    LOG_BE(Urgency::Warning, "Direct3D9 overlay injection unavailable for targetId=%d pid=%u.", targetId, pid);
                }
                if (!m_overlayInjector.InjectDirect3D10(pid))
                {
                    LOG_BE(Urgency::Warning, "Direct3D10 overlay injection unavailable for targetId=%d pid=%u.", targetId, pid);
                }
                if (!m_overlayInjector.InjectDirect3D11(pid))
                {
                    LOG_BE(Urgency::Warning, "Direct3D11 overlay injection unavailable for targetId=%d pid=%u.", targetId, pid);
                }
                if (!m_overlayInjector.InjectDirect3D12(pid))
                {
                    LOG_BE(Urgency::Warning, "Direct3D12 overlay injection unavailable for targetId=%d pid=%u.", targetId, pid);
                }

                injected.insert(pid);
            }

            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    };

    std::lock_guard<std::mutex> lock(m_startupNotificationThreadsMutex);
    m_startupNotificationThreads.emplace_back(std::move(worker));
}
#endif

/////////////////////////////////////////////////////////////////////

void Lymalinkd::OnProcessStarted(int targetId, const std::string& executablePath, uint32_t pid)
{
    LOG_BE(Urgency::Debug, "OnProcessStarted - targetId=%d exe=%s", targetId, executablePath.c_str());
    RequestManualAchievementDataScanCancel(0, "game_started");

#if defined(_WIN32)
    InjectWindowsOverlayProcessTree(targetId, pid);
#else
    (void)pid;
#endif

    // Mark daemon active and wake monitor loop
    m_activeCount.fetch_add(1);
    {
        std::lock_guard<std::mutex> lock(m_cvMutex);
        m_processActive.store(true);
        m_processWatcher.SetPollIntervalSec(2);
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
#if defined(_WIN32)
            m_processStartedAt[targetId] = std::filesystem::file_time_type::clock::now();
#endif
        }
    }

    DbRecord target;
    {
        std::lock_guard<std::mutex> lock(m_databaseMutex);
        target = m_database.SelectFirst(m_databaseConnectionName, m_databaseEmuGamesTable, "id = ?", {static_cast<int64_t>(targetId)});
    }

    const std::string appIdDirPath = SQLiteManager::RowString(target, "appid_dir_location");
    const std::string emulatorType = SQLiteManager::RowString(target, "emulator_type");
    const std::string gameName = SQLiteManager::RowString(target, "game_name");
    if (!appIdDirPath.empty() && !emulatorType.empty())
    {
        if (emulatorType == "SmartSteamEmu")
        {
            // Prepare CRC key matching before the achievement watcher starts polling stats.bin
            bool progressChanged = false;
            {
                std::lock_guard<std::mutex> lock(m_databaseMutex);
                progressChanged = DisableAchievementProgress(targetId);
                m_achievementKeyResolver.PrepareTargetKeys(targetId);
            }
            if (progressChanged)
            {
                EmitTargetDataChanged(targetId);
            }
        }
        m_achievementHandler.AddTarget(targetId, appIdDirPath, emulatorType);
    }

    ScheduleStartupNotification(targetId, gameName);

    EmitGameStateChanged({targetId}, "Active");
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
#if defined(_WIN32)
        m_processStartedAt.erase(targetId);
#endif
    }
#if defined(_WIN32)
    m_overlayNotifications.UnregisterProcess(targetId);
#endif
    EmitGameStateChanged({targetId}, "Inactive");
    m_achievementHandler.RemoveTarget(targetId);

    if (m_activeCount.fetch_sub(1) - 1 <= 0)
    {
#if !defined(_WIN32)
        m_overlayNotifications.ClearSharedMemoryNotification();
#endif

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
                    m_processWatcher.SetPollIntervalSec(5);
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
    EmitAchievementUnlocked(targetId, achievementKey);
}

/////////////////////////////////////////////////////////////////////

void Lymalinkd::OnAppIdDirUnavailable(int targetId, const std::string& appIdDirPath)
{
    if (targetId <= 0)
    {
        return;
    }

    LOG_BE(Urgency::Warning, "AppID dir unavailable, resetting scan state: targetId=%d path=%s", targetId, appIdDirPath.c_str());

    {
        std::lock_guard<std::mutex> lock(m_databaseMutex);
        DbRecord data{
            {"appid_dir_found", int64_t{0}},
            {"achievement_data_status", int64_t{0}},
            {"appid_dir_location", std::string{}},
            {"date_updated", Utils::NowEpoch()}
        };

        if (!m_database.Update(m_databaseConnectionName, m_databaseEmuGamesTable, data, "id = ?", {static_cast<int64_t>(targetId)}))
        {
            LOG_BE(Urgency::Critical, "Failed to reset AppID dir scan state: targetId=%d error=%s", targetId, m_database.LastError().c_str());
            return;
        }
    }

    const std::unordered_map<int, AppIdDirPathScanTarget> targetsMissingAppIdDir = LoadAppIdDirScanTargetsFromDatabase();
    {
        std::lock_guard<std::mutex> lock(m_targetIdsRequiringDirScanMutex);
        m_targetIdsRequiringDirScan = targetsMissingAppIdDir;
    }
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
#if defined(_WIN32)
    // Windows mappings are per game PID - Route the manual test toast to the first active configured target
    {
        std::lock_guard<std::mutex> lock(m_activeTargetsMutex);
        if (m_activeTargetsIds.empty())
        {
            LOG_BE(Urgency::Warning, "No active target available for Windows overlay test toast.");
            return;
        }
        notification.targetId = m_activeTargetsIds.front().first;
    }
#endif
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

void Lymalinkd::OnShutdown()
{
    LOG_BE(Urgency::Debug, "Shutdown backend requested.");

    m_running.store(false);
    m_cv.notify_all();
#if defined(_WIN32)
    QCoreApplication::quit();
#else
    kill(getpid(), SIGTERM);
#endif
}

/////////////////////////////////////////////////////////////////////

void Lymalinkd::OnReloadConfig()
{
    // Refresh notification sound without restarting daemon
    m_notificationSound.SetSoundPath(ResolveInstalledNotificationSoundPath());
    m_notificationSound.SetFallbackSoundPath(ResolveInstalledNotificationSoundPath(false));
    m_startupNotificationEnabled.store(LoadStartupNotificationConfig());
}

/////////////////////////////////////////////////////////////////////

void Lymalinkd::OnStartManualAchievementDataScan(int targetId)
{
    LOG_BE(Urgency::Debug, "Manual achievement data scan requested: targetId=%d", targetId);

    // Reject invalid or currently active targets before starting manual filesystem traversal
    if (targetId <= 0)
    {
        LOG_BE(Urgency::Warning, "Manual achievement data scan rejected: invalid targetId=%d", targetId);
        EmitManualAchievementDataScanFinished(targetId, false, "invalid");
        return;
    }

    {
        std::lock_guard<std::mutex> lock(m_activeTargetsMutex);
        if (!m_activeTargetsIds.empty())
        {
            LOG_BE(Urgency::Info, "Manual achievement data scan rejected because a target is active: targetId=%d", targetId);
            EmitManualAchievementDataScanFinished(targetId, false, "game_started");
            return;
        }
    }

    // Load current scan metadata after frontend reset cleared cached AppID dir state
    AppIdDirPathScanTarget target;
    if (!LoadAppIdDirScanTargetFromDatabase(targetId, target))
    {
        LOG_BE(Urgency::Warning, "Manual achievement data scan rejected because target metadata is unavailable: targetId=%d", targetId);
        EmitManualAchievementDataScanFinished(targetId, false, "invalid");
        return;
    }

#if defined(_WIN32)
    if (target.executableLocation.empty())
#else
    if (target.prefixLocation.empty() || target.executableLocation.empty())
#endif
    {
        LOG_BE(Urgency::Warning, "Manual achievement data scan rejected because required paths are missing: targetId=%d prefix=%s executable=%s", targetId, target.prefixLocation.c_str(), target.executableLocation.c_str());
        EmitManualAchievementDataScanFinished(targetId, false, "invalid");
        return;
    }

    {
        // Only one manual scan is allowed at a time - automatic active-game scans remain separate
        std::lock_guard<std::mutex> lock(m_manualScanMutex);
        if (m_manualScanActive.load())
        {
            LOG_BE(Urgency::Info, "Manual achievement data scan rejected because another scan is active: targetId=%d activeTargetId=%d", targetId, m_manualScanTargetId);
            EmitManualAchievementDataScanFinished(targetId, false, "invalid");
            return;
        }

        if (m_manualScanThread.joinable())
        {
            m_manualScanThread.join();
        }

        m_manualScanActive.store(true);
        m_manualScanCancelRequested.store(false);
        m_manualScanTargetId = targetId;
        m_manualScanCancelReason = "";
    }

    // Run one cancellable scan pass off the IPC thread
    m_manualScanThread = std::thread([this, target]() {
        const int targetId = target.targetId;
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
        PathScanner scanner;
        scanner.SetTargets({target});

        // Cancellation is cooperative so recursive filesystem walks can stop between entries
        auto shouldStopScanning = [this, targetId, deadline]() {
            if (!m_running.load())
            {
                return true;
            }
            if (m_manualScanCancelRequested.load())
            {
                return true;
            }
            if (std::chrono::steady_clock::now() >= deadline)
            {
                RequestManualAchievementDataScanCancel(targetId, "timeout");
                return true;
            }
            return false;
        };

        const std::vector<AppIdDirPathScanResult> results = scanner.ScanOnceForAppIdDir(shouldStopScanning);
        bool found = false;
        for (const AppIdDirPathScanResult& result : results)
        {
            if (result.appidDirFound)
            {
                found = true;
                break;
            }
        }

        // If any game starts while scan is finishing, let normal active-game scanning own the result
        bool anyTargetActive = false;
        {
            std::lock_guard<std::mutex> lock(m_activeTargetsMutex);
            anyTargetActive = !m_activeTargetsIds.empty();
        }
        if (anyTargetActive)
        {
            RequestManualAchievementDataScanCancel(0, "game_started");
        }

        if (!results.empty() && !m_manualScanCancelRequested.load())
        {
            SavePathScanResults(results, false); // Set false - Using EmitManualAchievementDataScanFinished instead of EmitTargetDataChanged

            // After finding AppID data manually, do the initial achievement state sync
            for (const AppIdDirPathScanResult& result : results)
            {
                if (!result.appidDirFound || m_manualScanCancelRequested.load())
                {
                    continue;
                }

                int updatedAchievements = 0;
                if (result.emulatorType == "SmartSteamEmu")
                {
                    // Manual scans use the same CRC resolving and progress hiding rules as runtime polling
                    bool progressChanged = false;
                    {
                        std::lock_guard<std::mutex> lock(m_databaseMutex);
                        progressChanged = DisableAchievementProgress(result.targetId);
                        m_achievementKeyResolver.PrepareTargetKeys(result.targetId);
                    }
                    if (progressChanged)
                    {
                        EmitTargetDataChanged(result.targetId);
                    }
                }
                const std::vector<AchievementData> achievements = m_achievementHandler.ReadAchievementFileOnce(result.targetId, result.appidDirLocation, result.emulatorType);
                for (const AchievementData& achievement : achievements)
                {
                    if (achievement.key.empty())
                    {
                        continue;
                    }

                    // Reuse normal runtime DB update path - unknown parser keys are ignored by SaveAchievementState
                    // CRC-only SmartSteamEmu keys must resolve first so notifications can find the real DB row
                    AchievementData resolvedAchievement = achievement;
                    bool shouldIgnoreUnresolvedKey = false;
                    {
                        std::lock_guard<std::mutex> lock(m_databaseMutex);
                        resolvedAchievement.key = m_achievementKeyResolver.ResolveKey(result.targetId, achievement.key);
                        shouldIgnoreUnresolvedKey = resolvedAchievement.key == achievement.key && m_achievementKeyResolver.ShouldIgnoreUnresolvedKey(result.targetId, achievement.key);
                    }
                    if (shouldIgnoreUnresolvedKey)
                    {
                        LOG_BE(Urgency::Debug, "Ignoring unresolved CRC entry during manual scan: targetId=%d key=%s", result.targetId, achievement.key.c_str());
                        continue;
                    }
                    if (SaveAchievementState(result.targetId, resolvedAchievement))
                    {
                        ++updatedAchievements;
                    }
                }

                LOG_BE(Urgency::Debug, "Manual achievement scan state sync saved: targetId=%d updated=%d", result.targetId, updatedAchievements);
            }
        }
        if (!found && !m_manualScanCancelRequested.load())
        {
            LOG_BE(Urgency::Debug, "Not found appIdDir: targetId=%d", targetId);
            std::lock_guard<std::mutex> lock(m_databaseMutex);
            DbRecord data{
                {"appid_dir_found", int64_t{0}},
                {"achievement_data_status", int64_t{0}},
                {"appid_dir_location", std::string{}},
                {"emulator_type", std::string{}},
                {"date_updated", Utils::NowEpoch()}
            };
            if (!m_database.Update(m_databaseConnectionName, m_databaseEmuGamesTable, data, "id = ?", {static_cast<int64_t>(targetId)}))
            {
                LOG_BE(Urgency::Critical, "Failed to save missing APPID dir result: targetId=%d error=%s", targetId, m_database.LastError().c_str());
            }
            else
            {
                std::lock_guard<std::mutex> scanLock(m_targetIdsRequiringDirScanMutex);
                m_targetIdsRequiringDirScan[targetId] = target;
            }
        }

        std::string reason = found ? "found" : "not_found";
        if (m_manualScanCancelRequested.load())
        {
            std::lock_guard<std::mutex> lock(m_manualScanMutex);
            reason = m_manualScanCancelReason.empty() ? "cancelled" : m_manualScanCancelReason;
            found = false;
        }

        FinishManualAchievementDataScan(targetId, found, reason);
    });
}

/////////////////////////////////////////////////////////////////////

void Lymalinkd::OnCancelManualAchievementDataScan(int targetId)
{
    LOG_BE(Urgency::Debug, "Manual achievement data scan cancel requested: targetId=%d", targetId);
    RequestManualAchievementDataScanCancel(targetId, "cancelled");
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
        EmitGameStateChanged(activeTargetIds, "Active");
    }
}

/////////////////////////////////////////////////////////////////////

void Lymalinkd::OnReloadAllTargets()
{
    LOG_BE(Urgency::Debug, "Reloading all targets from database.");

    // Reload watched executables from current database state
    m_processWatcher.SetTargets(LoadExeTargetsFromDatabase());

    // Refresh pending AppId dir scan targets
    const std::unordered_map<int, AppIdDirPathScanTarget> targetsMissingAppIdDir = LoadAppIdDirScanTargetsFromDatabase();
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
std::unordered_map<int, AppIdDirPathScanTarget> Lymalinkd::LoadAppIdDirScanTargetsFromDatabase()
{
    DbRows rows;
    {
        std::lock_guard<std::mutex> lock(m_databaseMutex);
        // Load targets that still need AppId dir discovery
        rows = m_database.SelectWhere(
            m_databaseConnectionName,
            m_databaseEmuGamesTable,
#if defined(_WIN32)
            "appid_dir_found = 0 AND executable_location IS NOT NULL AND executable_location != ''",
#else
            "appid_dir_found = 0 AND prefix_location IS NOT NULL AND prefix_location != ''",
#endif
            {},
            {"id", "prefix_location", "executable_location", "installation_dir", "data_opt"}
        );
    }

    std::unordered_map<int, AppIdDirPathScanTarget> targets;
    targets.reserve(rows.size());

    // Map target ID to scan metadata for future AppId dir scans
    for (const auto& row : rows)
    {
        const int targetId = static_cast<int>(SQLiteManager::RowInt(row, "id"));
        targets.emplace(targetId, AppIdDirPathScanTarget{
            targetId,
            std::to_string(targetId),
            SQLiteManager::RowString(row, "prefix_location"),
            SQLiteManager::RowString(row, "executable_location"),
            SQLiteManager::RowString(row, "installation_dir"),
            SQLiteManager::RowString(row, "data_opt")
        });
    }

    LOG_BE(Urgency::Debug, "AppID dir scan targets loaded: %zu", targets.size());
    return targets;
}

/////////////////////////////////////////////////////////////////////

bool Lymalinkd::LoadAppIdDirScanTargetFromDatabase(int targetId, AppIdDirPathScanTarget& target)
{
    if (targetId <= 0)
    {
        return false;
    }

    DbRecord row;
    {
        std::lock_guard<std::mutex> lock(m_databaseMutex);
        row = m_database.SelectFirst(
            m_databaseConnectionName,
            m_databaseEmuGamesTable,
            "id = ?",
            {static_cast<int64_t>(targetId)}
        );
    }

    if (row.empty())
    {
        LOG_BE(Urgency::Warning, "Manual AppID dir scan target not found: targetId=%d", targetId);
        return false;
    }

    target = AppIdDirPathScanTarget{
        targetId,
        std::to_string(targetId),
        SQLiteManager::RowString(row, "prefix_location"),
        SQLiteManager::RowString(row, "executable_location"),
        SQLiteManager::RowString(row, "installation_dir"),
        SQLiteManager::RowString(row, "data_opt")
    };
    return true;
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
            targets.push_back(it->second);
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
void Lymalinkd::SavePathScanResults(const std::vector<AppIdDirPathScanResult>& results, bool emitTargetDataChanged)
{
    std::vector<int> savedTargetIds;
    savedTargetIds.reserve(results.size());
    std::vector<std::pair<int, std::string>> savedDataOpt;

    {
        std::lock_guard<std::mutex> lock(m_databaseMutex);

        // Save discovered AppId metadata for each successful scan result
        for (const auto& result : results)
        {
            DbRecord data{
                {"date_updated", Utils::NowEpoch()}
            };

            if (!result.dataOpt.empty())
            {
                data.emplace("data_opt", result.dataOpt);
            }

            if (result.appidDirFound)
            {
                data.emplace("appid_dir_found", int64_t{1});
                data.emplace("achievement_data_status", int64_t{1});
                data.emplace("appid_dir_location", result.appidDirLocation);
                data.emplace("emulator_type", result.emulatorType);
            }

            if (!m_database.Update(m_databaseConnectionName, m_databaseEmuGamesTable, data, "id = ?", {static_cast<int64_t>(result.targetId)}))
            {
                LOG_BE(Urgency::Critical, "Failed to save APPID dir result: targetId=%d error=%s", result.targetId, m_database.LastError().c_str());
                continue;
            }

            if (result.appidDirFound)
            {
                savedTargetIds.push_back(result.targetId);
                LOG_BE(Urgency::Debug, "APPID dir saved: targetId=%d emulator=%s", result.targetId, result.emulatorType.c_str());
            }
            else if (!result.dataOpt.empty())
            {
                savedDataOpt.push_back({result.targetId, result.dataOpt});
            }
        }
    }

    if (!savedTargetIds.empty() || !savedDataOpt.empty())
    {
        // Remove completed targets and refresh partial metadata in pending scan cache
        std::lock_guard<std::mutex> lock(m_targetIdsRequiringDirScanMutex);
        for (const int targetId : savedTargetIds)
        {
            m_targetIdsRequiringDirScan.erase(targetId);
        }
        for (const auto& [targetId, dataOpt] : savedDataOpt)
        {
            if (auto it = m_targetIdsRequiringDirScan.find(targetId); it != m_targetIdsRequiringDirScan.end())
            {
                it->second.dataOpt = dataOpt;
            }
        }
    }

    // Manual scan mostly uses this variable - During manual scan everything will be parsed and so
    // 3000ms delay is not needed for refreshing the view at frontend, which EmitTargetDataChanged adds
    if (!emitTargetDataChanged)
    {
        return;
    }

    for (const int targetId : savedTargetIds)
    {
        EmitTargetDataChanged(targetId);
    }
}

/////////////////////////////////////////////////////////////////////

bool Lymalinkd::EnsureColumn(const std::string& tableName, const std::string& columnName, const std::string& columnDef, bool *columnAdded)
{
    if (columnAdded)
    {
        *columnAdded = false;
    }

    std::string escapedTableName;
    escapedTableName.reserve(tableName.size());
    for (const char c : tableName)
    {
        escapedTableName += c;
        if (c == '\'')
        {
            escapedTableName += '\'';
        }
    }

    const DbRecord row = m_database.SelectFirst(
        m_databaseConnectionName,
        std::format("pragma_table_info('{}')", escapedTableName),
        "name = ?",
        {columnName}
    );
    if (row.contains("name"))
    {
        return true;
    }

    if (!m_database.ExecuteSql(m_databaseConnectionName, std::format("ALTER TABLE {} ADD COLUMN {}", tableName, columnDef)))
    {
        return false;
    }

    LOG_BE(Urgency::Info, "EnsureColumn altered table=%s added column=%s", tableName.c_str(), columnName.c_str());
    if (columnAdded)
    {
        *columnAdded = true;
    }
    return true;
}

/////////////////////////////////////////////////////////////////////

bool Lymalinkd::DisableAchievementProgress(int targetId)
{
    if (targetId <= 0)
    {
        return false;
    }

    const DbRecord progressRow = m_database.SelectFirst(
        m_databaseConnectionName,
        DATABASE_TABLE_EMU_ACHIEVEMENTS,
        "id = ? AND (cur_progress != 0 OR max_progress != 0)",
        {static_cast<int64_t>(targetId)}
    );
    if (progressRow.empty())
    {
        return false;
    }

    const bool updated = m_database.Update(
        m_databaseConnectionName,
        DATABASE_TABLE_EMU_ACHIEVEMENTS,
        {
            {"cur_progress", int64_t{0}},
            {"max_progress", int64_t{0}},
            {"date_updated", Utils::NowEpoch()}
        },
        "id = ?",
        {static_cast<int64_t>(targetId)}
    );

    if (!updated)
    {
        LOG_BE(Urgency::Warning, "Failed to disable achievement progress: targetId=%d error=%s", targetId, m_database.LastError().c_str());
        return false;
    }

    LOG_BE(Urgency::Debug, "Disabled achievement progress: targetId=%d", targetId);
    return true;
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
        m_database.Update(m_databaseConnectionName,
            m_databaseEmuGamesTable,
            {{"achievement_data_status", int64_t{2}},
            {"date_updated", now}},
            "id = ? AND achievement_data_status != 2",
            {static_cast<int64_t>(targetId)}
        );
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
    if (achievement.achieved)
    {
        targetUpdate["achievement_data_status"] = int64_t{2};
    }

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

void Lymalinkd::ScheduleStartupNotification(int targetId, std::string gameName)
{
    if (!m_startupNotificationEnabled.load())
    {
        return;
    }

    std::lock_guard<std::mutex> lock(m_startupNotificationThreadsMutex);
    m_startupNotificationThreads.emplace_back([this, targetId, gameName]() {
#if defined(_WIN32)
        constexpr int STARTUP_NOTIFICATION_DELAY_MS = 5000 + WINDOWS_OVERLAY_INJECTION_DELAY_MS;
#else
        constexpr int STARTUP_NOTIFICATION_DELAY_MS = 5000;
#endif
        constexpr int SOCKET_NOTIFICATION_TIMEOUT_MS = 30000;
        constexpr int POLL_INTERVAL_MS = 100;

        auto shouldAbort = [this, targetId]() {
            return !m_running.load() || !m_startupNotificationEnabled.load() || !IsTargetActive(targetId);
        };

        // Wait before first startup notification so short process probes do not show a toast
        for (int elapsedMs = 0; elapsedMs < STARTUP_NOTIFICATION_DELAY_MS; elapsedMs += POLL_INTERVAL_MS)
        {
            if (shouldAbort())
            {
                return;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(POLL_INTERVAL_MS));
        }

        if (shouldAbort())
        {
            return;
        }

        // Use installed test icon when available so startup toast has same app identity as test toast
        std::string appIconPath = "";
        const std::filesystem::path iconPath = ResolveDataPath(LYMALINK_TEST_ICON_PATH);
        if (std::filesystem::exists(iconPath))
        {
            appIconPath = iconPath.string();
        }

        AchievementNotification notification;
        notification.targetId = targetId;
        notification.achievementName = "Lymalink";
        notification.achievementDescription = "Notifications activated for " + gameName;
        notification.iconPath = appIconPath;
        notification.appIconPath = appIconPath;

#if defined(_WIN32)
        // Windows has only its target-PID shared-memory mapping - The injected overlay claims the same single pending slot used for Linux SHM
        const bool sharedMemorySent = m_overlayNotifications.ShowAchievementToast(notification);
        LOG_BE(Urgency::Debug, "Startup SHM notification completed. Sent successfully: %s targetId=%d exe=%s", sharedMemorySent ? "true" : "false", targetId, gameName.c_str());
        return;
#else
        // SHM is written even if game overlay is not ready yet; native overlay reads latest SHM state when it starts
        const bool sharedMemorySent = m_overlayNotifications.ShowAchievementToastSharedMemory(notification);
        LOG_BE(Urgency::Debug, "Startup SHM notification completed. Sent successfully: %s targetId=%d exe=%s", sharedMemorySent ? "true" : "false", targetId, gameName.c_str());

        // Socket transport needs a live overlay client. Poll only until 30s total from process detection.
        for (int elapsedMs = STARTUP_NOTIFICATION_DELAY_MS; elapsedMs < SOCKET_NOTIFICATION_TIMEOUT_MS; elapsedMs += POLL_INTERVAL_MS)
        {
            if (shouldAbort())
            {
                return;
            }

            if (m_overlayNotifications.HasSocketClient())
            {
                const bool socketSent = m_overlayNotifications.ShowAchievementToastSocket(notification);
                LOG_BE(Urgency::Debug, "Startup socket notification completed. Sent successfully: %s targetId=%d exe=%s", socketSent ? "true" : "false", targetId, gameName.c_str());
                if (socketSent)
                {
                    m_overlayNotifications.ClearSharedMemoryNotification();
                }
                return;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(POLL_INTERVAL_MS));
        }

        LOG_BE(Urgency::Debug, "Startup socket notification timed out: targetId=%d exe=%s", targetId, gameName.c_str());
#endif
    });
}

/////////////////////////////////////////////////////////////////////

bool Lymalinkd::IsTargetActive(int targetId)
{
    std::lock_guard<std::mutex> lock(m_activeTargetsMutex);
    const auto it = std::find_if(m_activeTargetsIds.begin(), m_activeTargetsIds.end(), [targetId](const auto& activeTarget) {
        return activeTarget.first == targetId;
    });
    return it != m_activeTargetsIds.end();
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
#if defined(_WIN32)
    const char* appData = std::getenv("APPDATA");
    if (!appData || *appData == '\0')
    {
        LOG_BE(Urgency::Critical, "APPDATA environment variable not set. Cannot resolve database path.");
        return databasePath;
    }
    databasePath = (std::filesystem::path(appData) / "Lymalink" / DATABASE_FILE_NAME).string();
    std::filesystem::create_directories(std::filesystem::path(databasePath).parent_path());
    return databasePath;
#else
    const char* home = std::getenv("HOME");
    if (!home || *home == '\0')
    {
        LOG_BE(Urgency::Critical, "HOME environment variable not set or empty. Cannot resolve database path.");
        return databasePath;
    }

    databasePath = (std::filesystem::path(home) / ".local" / "share" / "Lymalink" / DATABASE_FILE_NAME).string();
    LOG_BE(Urgency::Debug, "Database path resolved to default location: %s", databasePath.c_str());
    return databasePath;
#endif
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

        // Collect paths matching the Flatpak VulkanLayer architecture manifests.
        const std::string filename = entry.path().filename().string();
        if (filename == "lymalink_overlay.x86_64.json" || filename == "lymalink_overlay.x86.json")
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

    const std::filesystem::path hostManifestDir = ResolveDataPath("vulkan/implicit_layer.d");
    for (const char* filename : {"lymalink_overlay.x86_64.json", "lymalink_overlay.x86.json"})
    {
        const std::filesystem::path manifestPath = hostManifestDir / filename;
        if (std::filesystem::exists(manifestPath))
        {
            manifestPaths.push_back(manifestPath.string());
            LOG_BE(Urgency::Info, "Found host Vulkan overlay manifest: %s", manifestPath.string().c_str());
        }
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
    const std::filesystem::path tempPath = targetPath.string() + ".tmp." + std::to_string(
#if defined(_WIN32)
        GetCurrentProcessId()
#else
        getpid()
#endif
    );
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

#if defined(_WIN32)
    // QSettings::UserScope location: %APPDATA%\Lymalink\config.ini.
    const char* appData = std::getenv("APPDATA");
    if (!appData || *appData == '\0')
    {
        return configPath;
    }

    configPath = (std::filesystem::path(appData) / ORGANIZATION / (std::string(APPLICATION) + ".ini")).string();
    return configPath;
#else

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
#endif
}

/////////////////////////////////////////////////////////////////////

std::string Lymalinkd::ResolveDataPath(const std::string& relativePath) const
{
    std::string dataPath = "";

#if defined(_WIN32)
    char executablePath[MAX_PATH]{};
    const DWORD length = GetModuleFileNameA(nullptr, executablePath, MAX_PATH);
    if (length == 0 || length == MAX_PATH)
    {
        return dataPath;
    }

    // Windows packages assets directly beside lymalinkd.exe, so preserve callers while dropping that prefix
    std::filesystem::path relative(relativePath);
    if (relative.begin() != relative.end() && *relative.begin() == "Lymalink")
    {
        relative = relative.lexically_relative("Lymalink");
    }
    
    return (std::filesystem::path(executablePath).parent_path() / relative).string();
#else

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
#endif
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

bool Lymalinkd::LoadStartupNotificationConfig() const
{
    const std::string configPath = ResolveConfigPath();
    if (configPath.empty())
    {
        LOG_BE(Urgency::Warning, "Cannot load startup notification config because config path is empty.");
        return true;
    }

    std::string value = Utils::TrimWhitespace(Utils::ReadIniValue(configPath, GROUP_BACKGROUND_SERVICE, "StartupNotification"));
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });

    const bool enabled = value != "false" && value != "0" && value != "no" && value != "off";
    LOG_BE(Urgency::Debug, "Startup notification config loaded. Enabled: %s", enabled ? "true" : "false");
    return enabled;
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

#if defined(_WIN32)
    if (extension != ".ogg" && extension != ".wav" && extension != ".mp3" && extension != ".flac")
#else
    if (extension != ".ogg" && extension != ".wav")
#endif
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

/////////////////////////////////////////////////////////////////////

void Lymalinkd::RequestManualAchievementDataScanCancel(int targetId, const std::string& reason)
{
    // targetId 0 means cancel whichever manual scan is active
    std::lock_guard<std::mutex> lock(m_manualScanMutex);
    if (!m_manualScanActive.load())
    {
        return;
    }
    if (targetId > 0 && m_manualScanTargetId != targetId)
    {
        return;
    }

    m_manualScanCancelRequested.store(true);
    m_manualScanCancelReason = reason.empty() ? "cancelled" : reason;
    LOG_BE(Urgency::Debug, "Manual achievement data scan cancellation marked: targetId=%d reason=%s", m_manualScanTargetId, m_manualScanCancelReason.c_str());
}

/////////////////////////////////////////////////////////////////////

void Lymalinkd::FinishManualAchievementDataScan(int targetId, bool found, const std::string& reason)
{
    {
        // Clear scan state before notifying frontend so a follow-up scan can start immediately
        std::lock_guard<std::mutex> lock(m_manualScanMutex);
        if (m_manualScanTargetId == targetId)
        {
            m_manualScanActive.store(false);
            m_manualScanCancelRequested.store(false);
            m_manualScanTargetId = 0;
            m_manualScanCancelReason = "";
        }
    }

    EmitManualAchievementDataScanFinished(targetId, found, reason);
    LOG_BE(Urgency::Debug, "Manual achievement data scan finished: targetId=%d found=%d reason=%s", targetId, found, reason.c_str());
}

/////////////////////////////////////////////////////////////////////

void Lymalinkd::EmitAchievementUnlocked(int targetId, const std::string& achievementKey)
{
#if defined(_WIN32)
    QMetaObject::invokeMethod(&m_ipc, [this, targetId, achievementKey] {
        m_ipc.EmitAchievementUnlocked(targetId, achievementKey);
    }, Qt::QueuedConnection);
#else
    m_dbus.EmitAchievementUnlocked(targetId, achievementKey);
#endif
}

/////////////////////////////////////////////////////////////////////

void Lymalinkd::EmitGameStateChanged(const std::vector<int>& targetIds, const std::string& state)
{
#if defined(_WIN32)
    QMetaObject::invokeMethod(&m_ipc, [this, targetIds, state] {
        m_ipc.EmitGameStateChanged(targetIds, state);
    }, Qt::QueuedConnection);
#else
    std::vector<int32_t> ids(targetIds.begin(), targetIds.end());
    m_dbus.EmitGameStateChanged(ids, state);
#endif
}

/////////////////////////////////////////////////////////////////////

void Lymalinkd::EmitTargetDataChanged(int targetId)
{
#if defined(_WIN32)
    QMetaObject::invokeMethod(&m_ipc, [this, targetId] {
        m_ipc.EmitTargetDataChanged(targetId);
    }, Qt::QueuedConnection);
#else
    m_dbus.EmitTargetDataChanged(targetId);
#endif
}

/////////////////////////////////////////////////////////////////////

void Lymalinkd::EmitManualAchievementDataScanFinished(int targetId, bool found, const std::string& reason)
{
#if defined(_WIN32)
    QMetaObject::invokeMethod(&m_ipc, [this, targetId, found, reason] {
        m_ipc.EmitManualAchievementDataScanFinished(targetId, found, reason);
    }, Qt::QueuedConnection);
#else
    m_dbus.EmitManualAchievementDataScanFinished(targetId, found, reason);
#endif
}
