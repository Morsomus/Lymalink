/////////////////////////////////////////////////////////
// File: SQLiteManager.h
// Date: 2026-05-22
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Declares SQLiteManager class (C++20, libsqlite3)
/////////////////////////////////////////////////////////
#pragma once

#include <sqlite3.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

// Value type used in rows: NULL | int64 | double | string | blob (bytes)
using DbNull = std::monostate;
using DbBlob = std::vector<std::byte>;
using DbValue = std::variant<DbNull, int64_t, double, std::string, DbBlob>;

using DbRow = std::unordered_map<std::string, DbValue>;
using DbRows = std::vector<DbRow>;
using DbRecord = std::unordered_map<std::string, DbValue>;  // alias for single-row result

class SQLiteManager
{
public:
    SQLiteManager();
    ~SQLiteManager();

    SQLiteManager(const SQLiteManager &) = delete;
    SQLiteManager &operator=(const SQLiteManager &) = delete;
    SQLiteManager(SQLiteManager &&) = default;
    SQLiteManager &operator=(SQLiteManager &&) = default;

    // Connection
    bool OpenDatabase(const std::string &connectionName, const std::string &dbPath);
    void CloseDatabase(const std::string &connectionName);
    bool IsDatabaseOpen(const std::string &connectionName) const;

    // Database
    bool CreateDatabase(const std::string &connectionName, const std::string &dbPath);
    bool DeleteDatabase(const std::string &connectionName, const std::string &dbPath);
    bool DatabaseFileExists(const std::string &dbPath) const;

    // Schema / initialization
    bool CreateTable(const std::string &connectionName, const std::string &tableName, const std::vector<std::string> &columnDefs);
    bool TableExists(const std::string &connectionName, const std::string &tableName) const;
    bool DropTable(const std::string &connectionName, const std::string &tableName);
    bool ExecuteSql(const std::string &connectionName, const std::string &sql);

    // Write
    bool Insert(const std::string &connectionName, const std::string &tableName, const DbRecord &data);
    bool Update(const std::string &connectionName, const std::string &tableName, const DbRecord &data, const std::string &whereClause, const std::vector<DbValue> &whereValues = {});
    bool Remove(const std::string &connectionName, const std::string &tableName, const std::string &whereClause, const std::vector<DbValue> &whereValues = {});

    // Read
    DbRows SelectAll(const std::string &connectionName, const std::string &tableName, const std::vector<std::string> &columns = {});
    DbRows SelectWhere(const std::string &connectionName, const std::string &tableName, const std::string &whereClause, const std::vector<DbValue> &whereValues = {}, const std::vector<std::string> &columns = {});
    DbRecord SelectFirst(const std::string &connectionName, const std::string &tableName, const std::string &whereClause = {}, const std::vector<DbValue> &whereValues = {});
    int64_t Count(const std::string &connectionName, const std::string &tableName, const std::string &whereClause = {}, const std::vector<DbValue> &whereValues = {});

    // Transactions
    bool BeginTransaction(const std::string &connectionName);
    bool CommitTransaction(const std::string &connectionName);
    bool RollbackTransaction(const std::string &connectionName);

    // Error handling
    [[nodiscard]] std::string LastError() const { return m_lastError; }

    // Helpers
    static int64_t RowInt(const DbRow &row, const std::string &key, int64_t fallback = 0);
    static std::string RowString(const DbRow &row, const std::string &key);

private:
    struct Conn {
        sqlite3 *db = nullptr;
        bool isOpen() const { return db != nullptr; }
    };

    std::unordered_map<std::string, Conn> m_dbConnections;
    std::string m_lastError;

    std::string ResolveConn(const std::string &name) const;
    sqlite3 *GetDb(const std::string &name) const;  // nullptr if not open
    void SetLastError(const std::string &err);
    bool BindValues(sqlite3_stmt *stmt, const std::vector<DbValue> &vals, int startIdx = 1);
    DbRows FetchRows(sqlite3_stmt *stmt) const;
    DbValue ColumnValue(sqlite3_stmt *stmt, int col) const;
    static std::string Join(const std::vector<std::string> &v, const std::string &sep);
};
