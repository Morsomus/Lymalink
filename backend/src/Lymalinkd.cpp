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
#include <chrono>
#include <cstring>
#include <cstdlib>
#include <filesystem>
#include <algorithm>
#include <sys/signalfd.h>
#include <unistd.h>

/////////////////////////////////////////////////////////////////////

Lymalinkd::Lymalinkd()
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
	// Block SIGTERM and SIGINT from normal delivery, signal thread will read them
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGTERM);
    sigaddset(&mask, SIGINT);
    if (sigprocmask(SIG_BLOCK, &mask, nullptr) < 0)
    {
        Logger::Log("[Lymalinkd] sigprocmask failed: " + std::string(strerror(errno)));
        return Error::UnknownError;
    }

    Error err = Init();
    if (err != Error::NoError)
    {
        Logger::Log("[Lymalinkd] Init failed, exiting.");
        return err;
    }

    m_running.store(true);
    m_signalThread = std::thread(&Lymalinkd::SignalThread, this, mask);

    Monitor();  // Main Monitor Loop

    Shutdown();
    return Error::NoError;
}

/////////////////////////////////////////////////////////////////////
///////////////////////////// PRIVATE ///////////////////////////////
/////////////////////////////////////////////////////////////////////

Error Lymalinkd::Init()
{
	Error err = Error::NoError;

    m_processActive.store(false);

    err = DatabaseInit();
    if (err != Error::NoError)
    {
        return err;
    }

    const std::unordered_map<int, std::string> targetsMissingAppIdDir = LoadAppIdDirScanTargetsFromDatabase();
    {
        std::lock_guard<std::mutex> lock(m_targetIdsRequiringDirScanMutex);
        m_targetIdsRequiringDirScan = targetsMissingAppIdDir;
    }

    m_dbus.onRequestActiveTargets = [this]() { OnRequestActiveTargets(); };

	err = m_dbus.Init();
    if (err != Error::NoError)
    {
        Logger::Log("[Lymalinkd] DBusService init failed.");
        return err;
    }

    // ProcessWatcher callbacks
    m_processWatcher.onProcessStarted = [this](int targetId, const std::string& exe) { OnProcessStarted(targetId, exe); };
    m_processWatcher.onProcessStopped = [this](int targetId, long secs) { OnProcessStopped(targetId, secs); };

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
    m_databasePath = ResolveDatabasePath();
    if (m_databasePath.empty())
    {
        Logger::Log("[Lymalinkd] Database path resolve failed.");
        return Error::DatabaseError;
    }

    if (!m_database.DatabaseFileExists(m_databasePath))
    {
        Logger::Log("[Lymalinkd] Database file not found: " + m_databasePath);
        return Error::DatabaseError;
    }

    if (!m_database.OpenDatabase(m_databaseConnectionName, m_databasePath))
    {
        Logger::Log("[Lymalinkd] Database open failed: " + m_database.LastError());
        return Error::DatabaseError;
    }

    Logger::Log("[Lymalinkd] Database opened: " + m_databasePath);
    return Error::NoError;
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

    m_processWatcher.Stop();
	m_dbus.Stop();
    m_sleepTimerGeneration.fetch_add(1);
    if (m_sleepTimerThread.joinable())
    {
        m_sleepTimerThread.join();
    }

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
    m_activeCount.fetch_add(1);
    {
        std::lock_guard<std::mutex> lock(m_cvMutex);
        m_processActive.store(true);
    }
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
        const uint64_t generation = m_sleepTimerGeneration.fetch_add(1) + 1;
        if (m_sleepTimerThread.joinable())
        {
            m_sleepTimerThread.join();
        }
        
        m_sleepTimerThread = std::thread([this, generation]() {
            for (int i = 0; i < 60; ++i)
            {
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

void Lymalinkd::OnRequestActiveTargets()
{
    std::vector<int32_t> activeTargetIds;
    {
        std::lock_guard<std::mutex> lock(m_activeTargetsMutex);
        activeTargetIds.reserve(m_activeTargetsIds.size());
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

std::vector<WatchTarget> Lymalinkd::LoadExeTargetsFromDatabase()
{
    DbRows rows;
    {
        std::lock_guard<std::mutex> lock(m_databaseMutex);
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
    std::vector<std::pair<int, int>> ids;
    {
        std::lock_guard<std::mutex> lock(m_activeTargetsMutex);
        ids = m_activeTargetsIds;
    }

    if (ids.empty())
    {
        return false;
    }

    std::lock_guard<std::mutex> lock(m_targetIdsRequiringDirScanMutex);
    for (const auto& activeTarget : ids)
    {
        if (m_targetIdsRequiringDirScan.contains(activeTarget.first))
        {
            return true;
        }
    }

    return false;
}

/////////////////////////////////////////////////////////////////////

// Get vector of active (currently played) targets which are missing AppId path 
std::vector<AppIdDirPathScanTarget> Lymalinkd::LoadCurrentActivePrefixPaths()
{
    std::vector<AppIdDirPathScanTarget> targets = {};

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
    if (const char* overridePath = std::getenv("LYMALINK_DATABASE_PATH"))
    {
        if (*overridePath != '\0')
        {
            return overridePath;
        }
    }

    const char* home = std::getenv("HOME");
    if (!home || *home == '\0')
    {
        return {};
    }

    return (std::filesystem::path(home) / ".local" / "share" / "Lymalink" / DATABASE_FILE_NAME).string();
}
