/////////////////////////////////////////////////////////
// File: DatabaseUtils.cpp
// Date: 2026-09-03
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Implements Lymalink specific DatabaseUtils
/////////////////////////////////////////////////////////

#include "DatabaseUtils.h"
#include "Defines.h"
#include "SQLiteManager.h"
#include "../tools/Logger.h"
#include "../tools/Utils.h"

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <vector>
#if defined(_WIN32)
    #include <windows.h>
#else
    #include <fcntl.h>
    #include <sys/types.h>
    #include <unistd.h>
#endif

namespace fs = std::filesystem;

#define COMPONENT "DatabaseUtils"

DatabaseUtils::DatabaseUtils()
{
    m_databaseLockPath = "";
    m_machineId = "";
}

DatabaseUtils::~DatabaseUtils()
{
    // Destructor
}

/////////////////////////////////////////////////////////////////////
////////////////////////////// PUBLIC ///////////////////////////////
/////////////////////////////////////////////////////////////////////

DatabasePathResult DatabaseUtils::ResolveDatabasePath(const std::string& configPath) const
{
    DatabasePathResult result;
    const std::string customPath = Utils::ReadIniValue(configPath, GROUP_DATABASE, "CustomDBPath");
    result.customPath = !customPath.empty();
    if (!customPath.empty())
    {
        result.path = (fs::path(customPath) / DATABASE_FILE_NAME).string();
        LOG_BE(Urgency::Info, "Database path loaded from config: %s", result.path.c_str());
        return result;
    }

    // Use default user data location
#if defined(_WIN32)
    const char* appData = std::getenv("APPDATA");
    if (!appData || *appData == '\0')
    {
        LOG_BE(Urgency::Critical, "APPDATA environment variable not set. Cannot resolve database path.");
        return result;
    }
    result.path = (fs::path(appData) / ORGANIZATION / DATABASE_FILE_NAME).string();
    fs::create_directories(fs::path(result.path).parent_path());
    return result;
#else
    const std::string appDataPath = Utils::ResolveAppDataPath(ORGANIZATION);
    if (appDataPath.empty())
    {
        LOG_BE(Urgency::Critical, "HOME environment variable not set or empty. Cannot resolve database path.");
        return result;
    }

    result.path = (fs::path(appDataPath) / DATABASE_FILE_NAME).string();
    LOG_BE(Urgency::Debug, "Database path resolved to default location: %s", result.path.c_str());
    return result;
#endif
}

/////////////////////////////////////////////////////////////////////

bool DatabaseUtils::AcquireDatabaseLock(const std::string& databasePath, const std::function<bool(uint64_t)>& isProcessAlive)
{
    m_machineId = Utils::ResolveMachineId();
    if (m_machineId.empty())
    {
        LOG_BE(Urgency::Critical, "Machine ID unavailable. Cannot acquire database lock.");
        return false;
    }

    m_databaseLockPath = (fs::path(databasePath).parent_path() / DATABASE_LOCK_FILE_NAME).string();
    const int64_t now = Utils::NowEpoch();
    if (fs::exists(m_databaseLockPath))
    {
        bool removeStaleLock = true;
        try
        {
            std::ifstream file(m_databaseLockPath);
            const nlohmann::json lock = nlohmann::json::parse(file, nullptr, false);
            const std::string lockMachineId = lock.value("machineId", "");
            const uint64_t lockProcessId = lock.value("processId", 0ULL);
            const int64_t lastHeartbeatAt = lock.value("lastHeartbeatAt", 0LL);
            const bool stale = now - lastHeartbeatAt > (DATABASE_DB_LOCK_LIFESPAN_SEC + 3);
            if (lockMachineId == m_machineId && lockProcessId > 0 && isProcessAlive(lockProcessId) && !stale)
            {
                LOG_BE(Urgency::Critical, "Database lock is already owned by this machine process: %llu", static_cast<unsigned long long>(lockProcessId));
                return false;
            }
            if (lockMachineId != m_machineId && !stale)
            {
                LOG_BE(Urgency::Critical, "Database lock is owned by another machine.");
                return false;
            }
        }
        catch (...)
        {
            removeStaleLock = true;
        }

        if (removeStaleLock)
        {
            std::error_code ec;
            fs::remove(m_databaseLockPath, ec);
            if (ec)
            {
                LOG_BE(Urgency::Critical, "Failed to remove stale database lock: %s", ec.message().c_str());
                return false;
            }
        }
    }

#if defined(_WIN32)
    HANDLE lockHandle = CreateFileA(
        m_databaseLockPath.c_str(),
        GENERIC_WRITE,
        0,
        nullptr,
        CREATE_NEW,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (lockHandle == INVALID_HANDLE_VALUE)
    {
        LOG_BE(Urgency::Critical, "Failed to create database lock file.");
        return false;
    }
    CloseHandle(lockHandle);
#else
    const int fd = open(m_databaseLockPath.c_str(), O_CREAT | O_EXCL | O_WRONLY, 0644);
    if (fd < 0)
    {
        LOG_BE(Urgency::Critical, "Failed to create database lock file: %s", strerror(errno));
        return false;
    }
    close(fd);
#endif

    RefreshDatabaseLock();
    return true;
}

/////////////////////////////////////////////////////////////////////

void DatabaseUtils::RefreshDatabaseLock()
{
    std::lock_guard<std::mutex> lock(m_databaseLockMutex);
    if (m_databaseLockPath.empty() || m_machineId.empty())
    {
        return;
    }

    const uint64_t processId =
#if defined(_WIN32)
        static_cast<uint64_t>(GetCurrentProcessId());
#else
        static_cast<uint64_t>(getpid());
#endif
    const int64_t now = Utils::NowEpoch();
    nlohmann::json data = {
        {"machineId", m_machineId},
        {"processId", processId},
        {"startedAt", now},
        {"lastHeartbeatAt", now}
    };

    if (fs::exists(m_databaseLockPath))
    {
        std::ifstream file(m_databaseLockPath);
        const nlohmann::json existing = nlohmann::json::parse(file, nullptr, false);
        if (existing.is_object())
        {
            if (existing.value("machineId", "") != m_machineId || existing.value("processId", 0ULL) != processId)
            {
                LOG_BE(Urgency::Warning, "Database lock ownership changed. Stopping heartbeat refresh.");
                m_databaseLockPath.clear();
                return;
            }
            data["startedAt"] = existing.value("startedAt", now);
        }
    }

    const fs::path lockPath(m_databaseLockPath);
    const fs::path tempPath = lockPath.parent_path() / (lockPath.filename().string() + ".tmp");
    {
        std::ofstream file(tempPath, std::ios::trunc);
        if (!file)
        {
            LOG_BE(Urgency::Warning, "Failed to open temporary database lock file for write.");
            return;
        }

        file << data.dump(4);
        file.flush();
        if (!file)
        {
            LOG_BE(Urgency::Warning, "Failed to write temporary database lock file.");
            std::error_code removeEc;
            fs::remove(tempPath, removeEc);
            return;
        }
    }

#if defined(_WIN32)
    if (!MoveFileExA(
            tempPath.string().c_str(),
            m_databaseLockPath.c_str(),
            MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
    {
        LOG_BE(Urgency::Warning, "Failed to replace database lock file atomically.");
        std::error_code removeEc;
        fs::remove(tempPath, removeEc);
    }
#else
    std::error_code ec;
    fs::rename(tempPath, lockPath, ec);
    if (ec)
    {
        LOG_BE(Urgency::Warning, "Failed to replace database lock file atomically: %s", ec.message().c_str());
        std::error_code removeEc;
        fs::remove(tempPath, removeEc);
    }
#endif
}

/////////////////////////////////////////////////////////////////////

void DatabaseUtils::ReleaseDatabaseLock()
{
    std::lock_guard<std::mutex> lock(m_databaseLockMutex);
    if (m_databaseLockPath.empty())
    {
        return;
    }

    const uint64_t processId =
#if defined(_WIN32)
        static_cast<uint64_t>(GetCurrentProcessId());
#else
        static_cast<uint64_t>(getpid());
#endif
    if (fs::exists(m_databaseLockPath))
    {
        std::ifstream file(m_databaseLockPath);
        const nlohmann::json existing = nlohmann::json::parse(file, nullptr, false);
        if (existing.is_object() && (existing.value("machineId", "") != m_machineId || existing.value("processId", 0ULL) != processId))
        {
            m_databaseLockPath.clear();
            return;
        }
    }

    std::error_code ec;
    fs::remove(m_databaseLockPath, ec);
    if (ec)
    {
        LOG_BE(Urgency::Warning, "Failed to remove database lock: %s", ec.message().c_str());
    }
    m_databaseLockPath.clear();
}

/////////////////////////////////////////////////////////////////////

bool DatabaseUtils::ProbeRuntimeWriteAccess(SQLiteManager& database, const std::string& connectionName, std::string& error, bool* transientBusy) const
{
    error.clear();
    if (transientBusy)
    {
        *transientBusy = false;
    }

    auto handleLockError = [&](int errorCode) -> bool {
        const int primaryCode = errorCode & 0xFF;
        if (primaryCode == SQLITE_BUSY || primaryCode == SQLITE_LOCKED)
        {
            if (transientBusy)
            {
                *transientBusy = true;
            }
            return true;
        }
        return false;
    };

    const std::vector<std::string> statements = {
        "BEGIN IMMEDIATE",
        "CREATE TABLE IF NOT EXISTS lymalink_write_probe(id INTEGER PRIMARY KEY)",
        "INSERT OR REPLACE INTO lymalink_write_probe(id) VALUES (1)",
        "DELETE FROM lymalink_write_probe WHERE id = 1"
    };

    bool transactionStarted = false;
    for (size_t i = 0; i < statements.size(); ++i)
    {
        const std::string& statement = statements[i];
        if (!database.ExecuteSql(connectionName, statement))
        {
            const int sqliteError = database.LastErrorCode();
            const std::string probeError = database.LastError();

            if (transactionStarted)
            {
                database.ExecuteSql(connectionName, "ROLLBACK");
            }

            if (handleLockError(sqliteError))
            {
                return false;
            }

            error = probeError;
            return false;
        }

        if (i == 0)
        {
            transactionStarted = true;
        }
    }

    if (!database.ExecuteSql(connectionName, "ROLLBACK"))
    {
        const int sqliteError = database.LastErrorCode();
        if (handleLockError(sqliteError))
        {
            return false;
        }

        error = database.LastError();
        return false;
    }

    return true;
}

/////////////////////////////////////////////////////////////////////
///////////////////////////// PRIVATE ///////////////////////////////
/////////////////////////////////////////////////////////////////////
