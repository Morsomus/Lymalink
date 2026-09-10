/////////////////////////////////////////////////////////
// File: DatabaseUtils.h
// Date: 2026-09-03
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Declares Lymalink specific DatabaseUtils
/////////////////////////////////////////////////////////

#pragma once

#include <cstdint>
#include <functional>
#include <mutex>
#include <string>

class SQLiteManager;

struct DatabasePathResult
{
    std::string path;
    bool customPath = false;
};

class DatabaseUtils
{
public:
    DatabaseUtils();
    ~DatabaseUtils();

    DatabasePathResult ResolveDatabasePath(const std::string& configPath) const;
    bool AcquireDatabaseLock(const std::string& databasePath, const std::function<bool(uint64_t)>& isProcessAlive);
    void RefreshDatabaseLock();
    void ReleaseDatabaseLock();
    bool ProbeRuntimeWriteAccess(SQLiteManager& database, const std::string& connectionName, std::string& error, bool* transientBusy = nullptr) const;

private:
    std::mutex m_databaseLockMutex;
    std::string m_databaseLockPath;
    std::string m_machineId;
};
