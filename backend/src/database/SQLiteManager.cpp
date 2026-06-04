/////////////////////////////////////////////////////////
// File: SQLiteManager.cpp
// Date: 2026-05-22
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Implements SQLiteManager class (C++20, libsqlite3)
/////////////////////////////////////////////////////////

#include "SQLiteManager.h"
#include "tools/Utils.h"

#include <filesystem>
#include <iostream>
#include <sstream>
#include <type_traits>

namespace fs = std::filesystem;

/////////////////////////////////////////////////////////////////////

SQLiteManager::SQLiteManager()
{
    m_dbConnections = {};
    m_lastError = "";
}

SQLiteManager::~SQLiteManager()
{
    for (auto &[name, conn] : m_dbConnections)
    {
        if (conn.isOpen())
        {
            sqlite3_close_v2(conn.db);
            conn.db = nullptr;
        }
    }
    m_dbConnections.clear();
}

/////////////////////////////////////////////////////////////////////
////////////////////////////// PUBLIC ///////////////////////////////
/////////////////////////////////////////////////////////////////////

bool SQLiteManager::OpenDatabase(const std::string &connectionName, const std::string &dbPath)
{
    const std::string conn = ResolveConn(connectionName);

    if (auto it = m_dbConnections.find(conn); it != m_dbConnections.end())
    {
        if (it->second.isOpen()) return true;
        // stale entry - clean up before re-opening
        sqlite3_close_v2(it->second.db);
        m_dbConnections.erase(it);
    }

    sqlite3 *db = nullptr;
    if (sqlite3_open(dbPath.c_str(), &db) != SQLITE_OK)
    {
        SetLastError(Utils::BuildString({"openDatabase: ", sqlite3_errmsg(db)}));
        sqlite3_close_v2(db);
        return false;
    }

    // WAL mode + foreign keys
    sqlite3_exec(db, "PRAGMA journal_mode=WAL;", nullptr, nullptr, nullptr);
    sqlite3_exec(db, "PRAGMA foreign_keys=ON;", nullptr, nullptr, nullptr);

    m_dbConnections[conn] = Conn{db};
    return true;
}

/////////////////////////////////////////////////////////////////////

void SQLiteManager::CloseDatabase(const std::string &connectionName)
{
    const std::string conn = ResolveConn(connectionName);
    auto it = m_dbConnections.find(conn);
    if (it == m_dbConnections.end())
    {
        std::cerr << "SQLiteManager - CloseDatabase: connection not found\n";
        return;
    }

    if (it->second.isOpen())
    {
        sqlite3_close_v2(it->second.db);
        it->second.db = nullptr;
    }

    m_dbConnections.erase(it);
}

/////////////////////////////////////////////////////////////////////

bool SQLiteManager::IsDatabaseOpen(const std::string &connectionName) const
{
    const std::string conn = ResolveConn(connectionName);
    auto it = m_dbConnections.find(conn);
    return it != m_dbConnections.end() && it->second.isOpen();
}

/////////////////////////////////////////////////////////////////////

bool SQLiteManager::CreateDatabase(const std::string &connectionName, const std::string &dbPath)
{
    const fs::path p(dbPath);
    const fs::path dir = p.parent_path();

    if (!dir.empty() && !fs::exists(dir))
    {
        std::error_code ec;
        fs::create_directories(dir, ec);
        if (ec)
        {
            SetLastError(Utils::BuildString({"createDatabase: failed to create directory: ", dir.string(), " (", ec.message(), ")"}));
            return false;
        }
    }

    if (!OpenDatabase(connectionName, dbPath))
    {
        return false;
    }

    std::cerr << "SQLiteManager: database created at " << dbPath << '\n';
    return true;
}

/////////////////////////////////////////////////////////////////////

bool SQLiteManager::DeleteDatabase(const std::string &connectionName, const std::string &dbPath)
{
    if (IsDatabaseOpen(connectionName)) 
    {
        CloseDatabase(connectionName);
    }

    bool allRemoved = true;
    std::error_code ec;

    if (fs::exists(dbPath))
    {
        fs::remove(fs::path(dbPath), ec);
        if (ec)
        {
            SetLastError(Utils::BuildString({"deleteDatabase: failed to remove file: ", dbPath, " (", ec.message(), ")"}));
            allRemoved = false;
        }
    }

    // WAL / SHM side-files - best-effort
    for (const auto &suffix : {std::string("-wal"), std::string("-shm")})
    {
        const std::string side = dbPath + suffix;
        if (fs::exists(side))
        {
            fs::remove(side, ec); // ignore error
        }
    }

    if (allRemoved)
    {
        std::cerr << "SQLiteManager: database deleted: " << dbPath << '\n';
    }

    return allRemoved;
}

/////////////////////////////////////////////////////////////////////

bool SQLiteManager::DatabaseFileExists(const std::string &dbPath) const
{
    return fs::exists(dbPath);
}

/////////////////////////////////////////////////////////////////////

bool SQLiteManager::CreateTable(const std::string &connectionName, const std::string &tableName, const std::vector<std::string> &columnDefs)
{
    const std::string sql = Utils::BuildString({"CREATE TABLE IF NOT EXISTS ", tableName, " (", Join(columnDefs, ", "), ")"});
    return ExecuteSql(connectionName, sql);
}

/////////////////////////////////////////////////////////////////////

bool SQLiteManager::TableExists(const std::string &connectionName, const std::string &tableName) const
{
    sqlite3 *db = GetDb(connectionName);
    if (!db)
    {
        std::cerr << "SQLiteManager - tableExists: database not open\n";
        return false;
    }

    const std::string sql = "SELECT 1 FROM sqlite_master WHERE type='table' AND name=? LIMIT 1";

    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
    {
        return false;
    }

    sqlite3_bind_text(stmt, 1, tableName.c_str(), -1, SQLITE_STATIC);
    const bool found = (sqlite3_step(stmt) == SQLITE_ROW);
    sqlite3_finalize(stmt);
    return found;
}

/////////////////////////////////////////////////////////////////////

bool SQLiteManager::DropTable(const std::string &connectionName, const std::string &tableName)
{
    return ExecuteSql(connectionName, Utils::BuildString({"DROP TABLE IF EXISTS ", tableName}));
}

/////////////////////////////////////////////////////////////////////

bool SQLiteManager::ExecuteSql(const std::string &connectionName, const std::string &sql)
{
    sqlite3 *db = GetDb(connectionName);
    if (!db)
    {
        SetLastError("ExecuteSql: database not open");
        return false;
    }

    char *errmsg = nullptr;
    if (sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &errmsg) != SQLITE_OK)
    {
        SetLastError(Utils::BuildString({"ExecuteSql: ", errmsg ? errmsg : "?", " | SQL: ", sql}));
        sqlite3_free(errmsg);
        return false;
    }
    return true;
}

/////////////////////////////////////////////////////////////////////

bool SQLiteManager::Insert(const std::string &connectionName, const std::string &tableName, const DbRecord &data)
{
    if (data.empty())
    {
        SetLastError("insert: data map is empty");
        return false;
    }

    sqlite3 *db = GetDb(connectionName);
    if (!db)
    {
        SetLastError("insert: database not open");
        return false;
    }

    std::vector<std::string> cols;
    std::vector<DbValue> vals;
    cols.reserve(data.size());
    vals.reserve(data.size());
    for (const auto &[k, v] : data)
    {
        cols.push_back(k); vals.push_back(v);
    }

    const std::string placeholders = Join(std::vector<std::string>(cols.size(), "?"), ", ");
    const std::string sql = Utils::BuildString({"INSERT INTO ", tableName, " (", Join(cols, ", "), ") VALUES (", placeholders, ")"});

    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
    {
        SetLastError(Utils::BuildString({"insert (prepare): ", sqlite3_errmsg(db)}));
        return false;
    }

    if (!BindValues(stmt, vals))
    {
        sqlite3_finalize(stmt);
        return false;
    }

    const bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    if (!ok)
    {
        SetLastError(Utils::BuildString({"insert: ", sqlite3_errmsg(db)}));
    }

    sqlite3_finalize(stmt);
    return ok;
}

/////////////////////////////////////////////////////////////////////

bool SQLiteManager::Update(const std::string &connectionName, const std::string &tableName, const DbRecord &data, const std::string &whereClause, const std::vector<DbValue> &whereValues)
{
    if (data.empty())
    {
        SetLastError("update: data map is empty");
        return false;
    }

    sqlite3 *db = GetDb(connectionName);
    if (!db)
    {
        SetLastError("update: database not open");
        return false;
    }

    std::vector<std::string> setClauses;
    std::vector<DbValue> vals;
    setClauses.reserve(data.size());
    vals.reserve(data.size() + whereValues.size());

    for (const auto &[k, v] : data)
    {
        setClauses.push_back(Utils::BuildString({k, " = ?"}));
        vals.push_back(v);
    }
    for (const auto &v : whereValues)
    {
        vals.push_back(v);
    }

    std::string sql = Utils::BuildString({"UPDATE ", tableName, " SET ", Join(setClauses, ", ")});
    if (!whereClause.empty()) 
    {
        sql += " WHERE " + whereClause;
    }

    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
    {
        SetLastError(Utils::BuildString({"update (prepare): ", sqlite3_errmsg(db)}));
        return false;
    }

    if (!BindValues(stmt, vals))
    {
        sqlite3_finalize(stmt);
        return false;
    }

    const bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    if (!ok)
    {
        SetLastError(Utils::BuildString({"update: ", sqlite3_errmsg(db)}));
    }
    
    sqlite3_finalize(stmt);
    return ok;
}

/////////////////////////////////////////////////////////////////////

bool SQLiteManager::Remove(const std::string &connectionName, const std::string &tableName, const std::string &whereClause, const std::vector<DbValue> &whereValues)
{
    sqlite3 *db = GetDb(connectionName);
    if (!db)
    {
        SetLastError("remove: database not open");
        return false;
    }

    std::string sql = Utils::BuildString({"DELETE FROM ", tableName});
    if (!whereClause.empty())
    {
        sql += " WHERE " + whereClause;
    }

    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
    {
        SetLastError(Utils::BuildString({"remove (prepare): ", sqlite3_errmsg(db)}));
        return false;
    }

    if (!BindValues(stmt, whereValues))
    {
        sqlite3_finalize(stmt);
        return false;
    }

    const bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    if (!ok)
    {
        SetLastError(Utils::BuildString({"remove: ", sqlite3_errmsg(db)}));
    }

    sqlite3_finalize(stmt);
    return ok;
}

/////////////////////////////////////////////////////////////////////

DbRows SQLiteManager::SelectAll(const std::string &connectionName, const std::string &tableName, const std::vector<std::string> &columns)
{
    return SelectWhere(connectionName, tableName, {}, {}, columns);
}

/////////////////////////////////////////////////////////////////////

DbRows SQLiteManager::SelectWhere(const std::string &connectionName, const std::string &tableName, const std::string &whereClause, const std::vector<DbValue> &whereValues, const std::vector<std::string> &columns)
{
    sqlite3 *db = GetDb(connectionName);
    if (!db)
    {
        SetLastError("SelectWhere: database not open");
        return {};
    }

    const std::string cols = columns.empty() ? "*" : Join(columns, ", ");
    std::string sql = Utils::BuildString({"SELECT ", cols, " FROM ", tableName});
    if (!whereClause.empty())
    {
        sql += " WHERE " + whereClause;
    }

    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
    {
        SetLastError(Utils::BuildString({"SelectWhere (prepare): ", sqlite3_errmsg(db)}));
        return {};
    }

    if (!BindValues(stmt, whereValues))
    {
        sqlite3_finalize(stmt);
        return {};
    }

    auto rows = FetchRows(stmt);
    sqlite3_finalize(stmt);
    return rows;
}

/////////////////////////////////////////////////////////////////////

DbRecord SQLiteManager::SelectFirst(const std::string &connectionName, const std::string &tableName, const std::string &whereClause, const std::vector<DbValue> &whereValues)
{
    sqlite3 *db = GetDb(connectionName);
    if (!db)
    {
        SetLastError("selectFirst: database not open");
        return {};
    }

    std::string sql = Utils::BuildString({"SELECT * FROM ", tableName});
    if (!whereClause.empty())
    {
        sql += " WHERE " + whereClause;
    }
    sql += " LIMIT 1";

    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
    {
        SetLastError(Utils::BuildString({"selectFirst (prepare): ", sqlite3_errmsg(db)}));
        return {};
    }

    if (!BindValues(stmt, whereValues))
    {
        sqlite3_finalize(stmt);
        return {};
    }

    DbRecord row;
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const int nCols = sqlite3_column_count(stmt);
        for (int i = 0; i < nCols; ++i)
        {
            const char *name = sqlite3_column_name(stmt, i);
            row[name ? name : Utils::BuildString({"col", std::to_string(i)})] = ColumnValue(stmt, i);
        }
    }

    sqlite3_finalize(stmt);
    return row;
}

/////////////////////////////////////////////////////////////////////

int64_t SQLiteManager::Count(const std::string &connectionName, const std::string &tableName, const std::string &whereClause, const std::vector<DbValue> &whereValues)
{
    sqlite3 *db = GetDb(connectionName);
    if (!db)
    {
        SetLastError("count: database not open");
        return -1;
    }

    std::string sql = Utils::BuildString({"SELECT COUNT(*) FROM ", tableName});
    if (!whereClause.empty())
    {
        sql += " WHERE " + whereClause;
    }

    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
    {
        SetLastError(Utils::BuildString({"count (prepare): ", sqlite3_errmsg(db)}));
        return -1;
    }

    if (!BindValues(stmt, whereValues))
    {
        sqlite3_finalize(stmt);
        return -1;
    }

    int64_t result = -1;
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        result = static_cast<int64_t>(sqlite3_column_int64(stmt, 0));
    }

    sqlite3_finalize(stmt);
    return result;
}

/////////////////////////////////////////////////////////////////////

bool SQLiteManager::BeginTransaction(const std::string &connectionName)
{
    return ExecuteSql(connectionName, "BEGIN");
}

/////////////////////////////////////////////////////////////////////

bool SQLiteManager::CommitTransaction(const std::string &connectionName)
{
    return ExecuteSql(connectionName, "COMMIT");
}

/////////////////////////////////////////////////////////////////////

bool SQLiteManager::RollbackTransaction(const std::string &connectionName)
{
    return ExecuteSql(connectionName, "ROLLBACK");
}

/////////////////////////////////////////////////////////////////////
/////////////////////////// PUBLIC STATIC ///////////////////////////
/////////////////////////////////////////////////////////////////////

int64_t SQLiteManager::RowInt(const DbRow &row, const std::string &key, int64_t fallback)
{
    const auto it = row.find(key);
    if (it == row.end())
    {
        return fallback;
    }

    if (const auto *value = std::get_if<int64_t>(&it->second))
    {
        return *value;
    }

    return fallback;
}

/////////////////////////////////////////////////////////////////////

std::string SQLiteManager::RowString(const DbRow &row, const std::string &key)
{
    const auto it = row.find(key);
    if (it == row.end())
    {
        return {};
    }

    if (const auto *value = std::get_if<std::string>(&it->second))
    {
        return *value;
    }

    return {};
}

/////////////////////////////////////////////////////////////////////
///////////////////////////// PRIVATE ///////////////////////////////
/////////////////////////////////////////////////////////////////////

std::string SQLiteManager::ResolveConn(const std::string &name) const
{
    return name.empty() ? "default_sqlite_connection" : name;
}

/////////////////////////////////////////////////////////////////////

sqlite3 *SQLiteManager::GetDb(const std::string &connectionName) const
{
    const std::string conn = ResolveConn(connectionName);
    auto it = m_dbConnections.find(conn);
    if (it != m_dbConnections.end() && it->second.isOpen())
    {
        return it->second.db;
    }

    return nullptr;
}

/////////////////////////////////////////////////////////////////////

void SQLiteManager::SetLastError(const std::string &err)
{
    m_lastError = err;
    std::cerr << "SQLiteManager - SetLastError: " << err << '\n';
}

/////////////////////////////////////////////////////////////////////

bool SQLiteManager::BindValues(sqlite3_stmt *stmt, const std::vector<DbValue> &vals, int startIdx)
{
    const int expected = sqlite3_bind_parameter_count(stmt);
    if (startIdx < 1 || startIdx + static_cast<int>(vals.size()) - 1 != expected)
    {
        SetLastError(Utils::BuildString({"BindValues: expected ", std::to_string(expected), " parameter values, got ", std::to_string(vals.size())}));
        return false;
    }

    int idx = startIdx;
    for (const auto &v : vals)
    {
        // Bind variant value to SQLite statement based on its actual type
        int rc = std::visit([&](const auto &val) -> int
        {
            using T = std::decay_t<decltype(val)>;
            if constexpr (std::is_same_v<T, DbNull>)
                return sqlite3_bind_null(stmt, idx);
            else if constexpr (std::is_same_v<T, int64_t>)
                return sqlite3_bind_int64(stmt, idx, val);
            else if constexpr (std::is_same_v<T, double>)
                return sqlite3_bind_double(stmt, idx, val);
            else if constexpr (std::is_same_v<T, std::string>)
                return sqlite3_bind_text64(stmt, idx, val.data(), static_cast<sqlite3_uint64>(val.size()), SQLITE_TRANSIENT, SQLITE_UTF8);
            else if constexpr (std::is_same_v<T, DbBlob>)
            {
                if (val.empty())
                {
                    return sqlite3_bind_zeroblob64(stmt, idx, 0);
                }
                return sqlite3_bind_blob64(stmt, idx, val.data(), static_cast<sqlite3_uint64>(val.size()), SQLITE_TRANSIENT);
            }
            else
            {
                static_assert(!sizeof(T), "Unhandled DbValue type in BindValues");
            }
            return SQLITE_OK;
        }, v);

        if (rc != SQLITE_OK)
        {
            SetLastError(Utils::BuildString({"BindValues: bind parameter ", std::to_string(idx), " failed (", sqlite3_errstr(rc), ")"}));
            return false;
        }
        ++idx;
    }
    return true;
}

/////////////////////////////////////////////////////////////////////

DbRows SQLiteManager::FetchRows(sqlite3_stmt *stmt) const
{
    DbRows rows;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        DbRow row;
        const int nCols = sqlite3_column_count(stmt);
        for (int i = 0; i < nCols; ++i)
        {
            const char *name = sqlite3_column_name(stmt, i);
            row[name ? name : Utils::BuildString({"col", std::to_string(i)})] = ColumnValue(stmt, i);
        }
        rows.push_back(std::move(row));
    }
    return rows;
}

/////////////////////////////////////////////////////////////////////

DbValue SQLiteManager::ColumnValue(sqlite3_stmt *stmt, int col) const
{
    switch (sqlite3_column_type(stmt, col))
    {
        case SQLITE_INTEGER:
        {
            return static_cast<int64_t>(sqlite3_column_int64(stmt, col));
        }

        case SQLITE_FLOAT: 
        {
            return sqlite3_column_double(stmt, col);
        }

        case SQLITE_TEXT:
        {
            const auto *text = reinterpret_cast<const char *>(sqlite3_column_text(stmt, col));
            const int sz = sqlite3_column_bytes(stmt, col);
            return text ? std::string(text, static_cast<std::size_t>(sz)) : std::string{};
        }

        case SQLITE_BLOB:
        {
            const auto *data = static_cast<const std::byte *>(sqlite3_column_blob(stmt, col));
            const int sz = sqlite3_column_bytes(stmt, col);
            if (sz <= 0 || !data) return DbBlob{};
            return DbBlob(data, data + sz);
        }
        case SQLITE_NULL:
        default: return DbNull{};
    }
}

/////////////////////////////////////////////////////////////////////

std::string SQLiteManager::Join(const std::vector<std::string> &v, const std::string &sep)
{
    if (v.empty()) return {};
    std::ostringstream oss;
    for (std::size_t i = 0; i < v.size(); ++i)
    {
        if (i) oss << sep;
        oss << v[i];
    }
    return oss.str();
}
