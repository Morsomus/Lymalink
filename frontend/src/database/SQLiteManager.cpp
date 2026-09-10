/////////////////////////////////////////////////////////
// File: SQLiteManager.cpp
// Date: 2026-05-14
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Implements SQLiteManager class
/////////////////////////////////////////////////////////

#include "SQLiteManager.h"

#include <QSqlRecord>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QDebug>
#include <QCoreApplication>

/////////////////////////////////////////////////////////////////////

SQLiteManager::SQLiteManager(QObject *parent) : SQLiteManager(nullptr, parent)
{
    m_databaseUtils = nullptr;
    m_lastError = "";
}

SQLiteManager::SQLiteManager(DatabaseUtils *databaseUtils, QObject *parent) : QObject(parent)
{
    m_databaseUtils = databaseUtils;
    m_lastError = "";
}

SQLiteManager::~SQLiteManager()
{
    // Close all open database connections
    for (const QString &conn : m_dbConnections.keys())
    {
        closeDatabase(conn);
    }
}

/////////////////////////////////////////////////////////////////////
////////////////////////////// PUBLIC ///////////////////////////////
/////////////////////////////////////////////////////////////////////

bool SQLiteManager::openDatabase(const QString &connectionName, const QString &dbPath, bool createMissingDb)
{
    const QString conn = resolveConn(connectionName);
    if (!createMissingDb && !QFileInfo::exists(dbPath))
    {
        setLastError("Database file is not available: " + dbPath);
        emit signalDatabaseError(m_lastError);
        return false;
    }

    if (m_dbConnections.contains(conn))
    {
        if (isDatabaseOpen(conn) && m_dbConnections.value(conn).databaseName() == dbPath)
        {
            m_lastError.clear();
            return true;
        }
        closeDatabase(conn);
    }

    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", conn);
    db.setDatabaseName(dbPath);
    if (!db.open())
    {
        setLastError(db.lastError().text());
        emit signalDatabaseError(m_lastError);
        db = QSqlDatabase();
        QSqlDatabase::removeDatabase(conn);
        return false;
    }

    // Enable WAL mode + foreign keys
    QSqlQuery q(db);
    q.exec("PRAGMA journal_mode=WAL");
    q.exec("PRAGMA busy_timeout=5000");
    q.exec("PRAGMA foreign_keys=ON");

    m_dbConnections[conn] = db;
    m_lastError.clear();
    emit signalConnectionStatusChanged(true, conn);
    return true;
}

/////////////////////////////////////////////////////////////////////

void SQLiteManager::closeDatabase(const QString &connectionName)
{
    if (QCoreApplication::instance() == nullptr)
    {
        m_dbConnections.remove(resolveConn(connectionName));
        return;
    }

    const QString conn = resolveConn(connectionName);
    if (!m_dbConnections.contains(conn))
    {
        qWarning() << "SQLiteManager::closeDatabase: connection not found:" << conn;
        return;
    }

    if (m_dbConnections[conn].isOpen())
    {
        m_dbConnections[conn].close();
    }

    m_dbConnections.remove(conn);
    QSqlDatabase::removeDatabase(conn);
    emit signalConnectionStatusChanged(false, conn);
}

/////////////////////////////////////////////////////////////////////

bool SQLiteManager::isDatabaseOpen(const QString &connectionName) const
{
    const QString conn = resolveConn(connectionName);
    auto it = m_dbConnections.constFind(conn);
    return it != m_dbConnections.constEnd() && it.value().isOpen();
}

/////////////////////////////////////////////////////////////////////

bool SQLiteManager::databaseAvailable(const QString &connectionName)
{
    const QString path = getDb(connectionName).databaseName();
    if (path.isEmpty())
    {
        setLastError("Database path is empty");
        emit signalDatabaseError(m_lastError);
        return false;
    }

    if (!QFileInfo::exists(path))
    {
        setLastError("Database file is not available: " + path);
        emit signalDatabaseError(m_lastError);
        return false;
    }

    if (isDatabaseOpen(connectionName))
    {
        m_lastError.clear();
        return true;
    }

    setLastError("Database not open");
    emit signalDatabaseError(m_lastError);
    return false;
}

/////////////////////////////////////////////////////////////////////

bool SQLiteManager::createDatabase(const QString &connectionName, const QString &dbPath)
{
    // Ensure the target directory exists, create it if necessary
    const QFileInfo fi(dbPath);
    const QDir dir = fi.absoluteDir();
    if (!dir.exists())
    {
        if (!dir.mkpath("."))
        {
            setLastError("Failed to create directory: " + dir.absolutePath());
            emit signalDatabaseError(m_lastError);
            return false;
        }
    }

    // SQLite creates the file automatically on first open
    if (!openDatabase(connectionName, dbPath))
    {
        return false;
    }

    qDebug() << "SQLiteManager::createDatabase: database created at" << dbPath;
    return true;
}

/////////////////////////////////////////////////////////////////////

bool SQLiteManager::deleteDatabase(const QString &connectionName, const QString &dbPath)
{
    // Close the connection first (and WAL/SHM side-files too)
    if (isDatabaseOpen(connectionName))
    {
        closeDatabase(connectionName);
    }

    bool allRemoved = true;

    // Remove the main database file
    if (QFile::exists(dbPath))
    {
        if (!QFile::remove(dbPath))
        {
            setLastError("Failed to remove file: " + dbPath);
            emit signalDatabaseError(m_lastError);
            allRemoved = false;
        }
    }

    // Remove WAL and shared-memory side-files if present
    for (const QString &suffix : {QString("-wal"), QString("-shm")})
    {
        const QString sidePath = dbPath + suffix;
        if (QFile::exists(sidePath))
        {
            if (!QFile::remove(sidePath))
            {
                qWarning() << "SQLiteManager::deleteDatabase: failed to remove side file:" << sidePath;
            }
        }
    }

    if (allRemoved)
    {
        qDebug() << "SQLiteManager::deleteDatabase: database deleted:" << dbPath;
    }
        
    return allRemoved;
}

/////////////////////////////////////////////////////////////////////

bool SQLiteManager::databaseFileExists(const QString &dbPath) const
{
    return QFileInfo::exists(dbPath);
}

/////////////////////////////////////////////////////////////////////

bool SQLiteManager::createTable(const QString &connectionName, const QString &tableName, const QStringList &columnDefs)
{
    // columnDefs example: {"id INTEGER PRIMARY KEY AUTOINCREMENT", "name TEXT NOT NULL"}
    const QString sql = QString("CREATE TABLE IF NOT EXISTS %1 (%2)").arg(tableName, columnDefs.join(", "));
    return executeSql(connectionName, sql, true);
}

/////////////////////////////////////////////////////////////////////

bool SQLiteManager::tableExists(const QString &connectionName, const QString &tableName)
{
    QSqlDatabase db = getDb(connectionName);
    if (!db.isOpen())
    {
        setLastError("Database not open");
        return false;
    }

    QSqlQuery q(db);
    q.prepare("SELECT 1 FROM sqlite_master WHERE type='table' AND name=? LIMIT 1");
    q.addBindValue(tableName);
    if (!q.exec())
    {
        setLastError(q.lastError().text());
        emit signalDatabaseError(m_lastError);
        return false;
    }
    if (q.next())
    {
        m_lastError.clear();
        return true;
    }
    if (q.lastError().isValid())
    {
        setLastError(q.lastError().text());
        emit signalDatabaseError(m_lastError);
        return false;
    }

    m_lastError.clear();
    return false;
}

/////////////////////////////////////////////////////////////////////

bool SQLiteManager::dropTable(const QString &connectionName, const QString &tableName)
{
    return executeSql(connectionName, QString("DROP TABLE IF EXISTS %1").arg(tableName));
}

/////////////////////////////////////////////////////////////////////

bool SQLiteManager::executeSql(const QString &connectionName, const QString &sql, bool allowUnlockedInitWrite)
{
    if (!databaseAvailable(connectionName))
    {
        return false;
    }

    const QString trimmedSql = sql.trimmed().toUpper();
    const bool writeSql = trimmedSql.startsWith("INSERT") || trimmedSql.startsWith("UPDATE") ||
        trimmedSql.startsWith("DELETE") || trimmedSql.startsWith("CREATE") ||
        trimmedSql.startsWith("DROP") || trimmedSql.startsWith("ALTER") ||
        trimmedSql.startsWith("REPLACE");
    if (writeSql && !isDatabaseWriteAllowed(connectionName, allowUnlockedInitWrite))
    {
        return false;
    }

    QSqlDatabase db = getDb(connectionName);
    if (!db.isOpen())
    {
        setLastError("Database not open");
        return false;
    }

    QSqlQuery q(db);
    if (!q.exec(sql))
    {
        setLastError(q.lastError().text() + " | SQL: " + sql);
        emit signalDatabaseError(m_lastError);
        return false;
    }
    m_lastError.clear();
    return true;
}

/////////////////////////////////////////////////////////////////////

bool SQLiteManager::insert(const QString &connectionName, const QString &tableName, const QVariantMap &data)
{
    if (!databaseAvailable(connectionName) || !isDatabaseWriteAllowed(connectionName))
    {
        return false;
    }

    if (data.isEmpty())
    {
        setLastError("Data map is empty");
        return false;
    }

    QSqlDatabase db = getDb(connectionName);
    if (!db.isOpen())
    {
        setLastError("Database not open");
        return false;
    }

    const QStringList cols   = data.keys();
    const QStringList placeholders(cols.size(), "?");
    const QString sql = QString("INSERT INTO %1 (%2) VALUES (%3)").arg(tableName, cols.join(", "), placeholders.join(", "));

    QSqlQuery q(db);
    if (!q.prepare(sql))
    {
        QString errorText = q.lastError().text();
        if (errorText.trimmed().isEmpty())
        {
            errorText = QString("driver='%1' database='%2'").arg(q.lastError().driverText(), q.lastError().databaseText());
        }
        setLastError(QString("Prepare insert into %1 failed: %2 | SQL: %3").arg(tableName, errorText, sql));
        emit signalDatabaseError(m_lastError);
        return false;
    }
    for (const QString &col : cols)
    {
        q.addBindValue(data[col]);
    }
        
    if (!q.exec())
    {
        setLastError(q.lastError().databaseText());
        emit signalDatabaseError(m_lastError);
        return false;
    }
    m_lastError.clear();
    return true;
}

/////////////////////////////////////////////////////////////////////

bool SQLiteManager::update(const QString &connectionName, const QString &tableName, const QVariantMap &data, const QString &whereClause, const QVariantList &whereValues)
{
    if (!databaseAvailable(connectionName) || !isDatabaseWriteAllowed(connectionName))
    {
        return false;
    }

    if (data.isEmpty())
    {
        setLastError("Data map is empty");
        return false;
    }

    QSqlDatabase db = getDb(connectionName);
    if (!db.isOpen())
    {
        setLastError("Database not open");
        return false;
    }

    QStringList setClauses;
    for (const QString &col : data.keys())
    {
        setClauses << QString("%1 = ?").arg(col);
    }
        
    QString sql = QString("UPDATE %1 SET %2").arg(tableName, setClauses.join(", "));
    if (!whereClause.isEmpty())
    {
        sql += " WHERE " + whereClause;
    }
        
    QSqlQuery q(db);
    q.prepare(sql);
    for (const QString &col : data.keys())
    {
        q.addBindValue(data[col]);
    }   
    for (const QVariant &v : whereValues)
    {
        q.addBindValue(v);
    }

    if (!q.exec())
    {
        setLastError(q.lastError().text());
        emit signalDatabaseError(m_lastError);
        return false;
    }
    m_lastError.clear();
    return true;
}

/////////////////////////////////////////////////////////////////////

bool SQLiteManager::remove(const QString &connectionName, const QString &tableName, const QString &whereClause, const QVariantList &whereValues)
{
    if (!databaseAvailable(connectionName) || !isDatabaseWriteAllowed(connectionName))
    {
        return false;
    }

    QSqlDatabase db = getDb(connectionName);
    if (!db.isOpen())
    {
        setLastError("Database not open");
        return false;
    }

    QString sql = QString("DELETE FROM %1").arg(tableName);
    if (!whereClause.isEmpty())
    {
        sql += " WHERE " + whereClause;
    }
        
    QSqlQuery q(db);
    q.prepare(sql);
    for (const QVariant &v : whereValues)
    {
        q.addBindValue(v);
    }

    if (!q.exec())
    {
        setLastError(q.lastError().text());
        emit signalDatabaseError(m_lastError);
        return false;
    }
    m_lastError.clear();
    return true;
}

/////////////////////////////////////////////////////////////////////

QVariantList SQLiteManager::selectAll(const QString &connectionName, const QString &tableName, const QStringList &columns)
{
    return selectWhere(connectionName, tableName, {}, {}, columns);
}

/////////////////////////////////////////////////////////////////////

QVariantList SQLiteManager::selectWhere(const QString &connectionName, const QString &tableName, const QString &whereClause, const QVariantList &whereValues, const QStringList &columns)
{
    if (!databaseAvailable(connectionName))
    {
        return {};
    }

    const QString cols = columns.isEmpty() ? "*" : columns.join(", ");
    QString sql = QString("SELECT %1 FROM %2").arg(cols, tableName);
    if (!whereClause.isEmpty())
    {
        sql += " WHERE " + whereClause;
    }

    QSqlDatabase db = getDb(connectionName);
    if (!db.isOpen())
    {
        setLastError("Database not open");
        emit signalDatabaseError(m_lastError);
        return {};
    }

    QSqlQuery q(db);
    q.prepare(sql);
    for (const QVariant &v : whereValues)
    {
        q.addBindValue(v);
    }

    if (!q.exec())
    {
        setLastError(q.lastError().text());
        emit signalDatabaseError(m_lastError);
        return {};
    }

    QVariantList rows = fetchRows(q);
    if (q.lastError().isValid())
    {
        setLastError(q.lastError().text());
        emit signalDatabaseError(m_lastError);
        return {};
    }

    m_lastError.clear();
    return rows;
}

/////////////////////////////////////////////////////////////////////

QVariantMap SQLiteManager::selectFirst(const QString &connectionName, const QString &tableName, const QString &whereClause, const QVariantList &whereValues)
{
    if (!databaseAvailable(connectionName))
    {
        return {};
    }

    QString sql = QString("SELECT * FROM %1").arg(tableName);
    if (!whereClause.isEmpty())
    {
        sql += " WHERE " + whereClause;
    }
    sql += " LIMIT 1";

    QSqlDatabase db = getDb(connectionName);
    if (!db.isOpen())
    {
        setLastError("Database not open");
        emit signalDatabaseError(m_lastError);
        return {};
    }

    QSqlQuery q(db);
    q.prepare(sql);
    for (const QVariant &v : whereValues)
    {
        q.addBindValue(v);
    }

    if (!q.exec())
    {
        setLastError(q.lastError().text());
        emit signalDatabaseError(m_lastError);
        return {};
    }

    if (!q.next())
    {
        if (q.lastError().isValid())
        {
            setLastError(q.lastError().text());
            emit signalDatabaseError(m_lastError);
            return {};
        }
        m_lastError.clear();
        return {};
    }

    QVariantMap row;
    const QSqlRecord rec = q.record();
    for (int i = 0; i < rec.count(); ++i)
    {
        row[rec.fieldName(i)] = q.value(i);
    }
    m_lastError.clear();
    return row;
}

/////////////////////////////////////////////////////////////////////

int SQLiteManager::count(const QString &connectionName, const QString &tableName, const QString &whereClause, const QVariantList &whereValues)
{
    if (!databaseAvailable(connectionName))
    {
        return -1;
    }

    QString sql = QString("SELECT COUNT(*) FROM %1").arg(tableName);
    if (!whereClause.isEmpty())
    {
        sql += " WHERE " + whereClause;
    }

    QSqlDatabase db = getDb(connectionName);
    if (!db.isOpen())
    {
        setLastError("Database not open");
        emit signalDatabaseError(m_lastError);
        return -1;
    }

    QSqlQuery q(db);
    q.prepare(sql);
    for (const QVariant &v : whereValues)
    {
        q.addBindValue(v);
    }

    if (!q.exec() || !q.next())
    {
        setLastError(q.lastError().text());
        emit signalDatabaseError(m_lastError);
        return -1;
    }

    const int result = q.value(0).toInt();
    m_lastError.clear();
    return result;
}

/////////////////////////////////////////////////////////////////////

bool SQLiteManager::beginTransaction(const QString &connectionName)
{
    if (!databaseAvailable(connectionName) || !isDatabaseWriteAllowed(connectionName))
    {
        return false;
    }

    QSqlDatabase db = getDb(connectionName);
    if (!db.isOpen())
    {
        setLastError("Database not open");
        return false;
    }
    
    if (!db.transaction())
    {
        setLastError(db.lastError().text());
        emit signalDatabaseError(m_lastError);
        return false;
    }
    m_lastError.clear();
    return true;
}

/////////////////////////////////////////////////////////////////////

bool SQLiteManager::commitTransaction(const QString &connectionName)
{
    if (!databaseAvailable(connectionName))
    {
        return false;
    }

    QSqlDatabase db = getDb(connectionName);
    if (!db.isOpen())
    {
        setLastError("Database not open");
        return false;
    }
    
    if (!db.commit())
    {
        setLastError(db.lastError().text());
        emit signalDatabaseError(m_lastError);
        return false;
    }
    m_lastError.clear();
    return true;
}

/////////////////////////////////////////////////////////////////////

bool SQLiteManager::rollbackTransaction(const QString &connectionName)
{
    if (!databaseAvailable(connectionName))
    {
        return false;
    }

    QSqlDatabase db = getDb(connectionName);
    if (!db.isOpen())
    {
        setLastError("Database not open");
        return false;
    }

    if (!db.rollback())
    {
        // setLastError(db.lastError().text());
        qCritical() << "SQLiteManager::rollbackTransaction:" << db.lastError().text();
        // emit signalDatabaseError(m_lastError);
        return false;
    }
    m_lastError.clear();
    return true;
}

/////////////////////////////////////////////////////////////////////

QString SQLiteManager::lastError() const
{
    return m_lastError;
}

/////////////////////////////////////////////////////////////////////
///////////////////////////// PRIVATE ///////////////////////////////
/////////////////////////////////////////////////////////////////////

QString SQLiteManager::resolveConn(const QString &connectionName) const
{
    return connectionName.isEmpty() ? "default_sqlite_connection" : connectionName;
}

/////////////////////////////////////////////////////////////////////

QSqlDatabase SQLiteManager::getDb(const QString &connectionName) const
{
    const QString conn = resolveConn(connectionName);
    if (m_dbConnections.contains(conn))
    {
        return m_dbConnections[conn];
    }
    return QSqlDatabase(); // invalid / not-open db
}

/////////////////////////////////////////////////////////////////////

QVariantList SQLiteManager::fetchRows(QSqlQuery &query) const
{
    QVariantList rows;
    while (query.next())
    {
        QVariantMap row;
        const QSqlRecord rec = query.record();
        for (int i = 0; i < rec.count(); ++i)
        {
            row[rec.fieldName(i)] = query.value(i);
        }
        rows << row;
    }
    return rows;
}

/////////////////////////////////////////////////////////////////////

bool SQLiteManager::isDatabaseWriteAllowed(const QString &connectionName, bool allowUnlockedInitWrite)
{
    if (m_databaseUtils == nullptr)
    {
        return true;
    }

    QSqlDatabase db = getDb(connectionName);
    QString error;
    if (!m_databaseUtils->IsDatabaseWriteAllowed(db.databaseName(), &error, allowUnlockedInitWrite))
    {
        setLastError(error);
        emit signalDatabaseError(m_lastError);
        return false;
    }

    return true;
}

/////////////////////////////////////////////////////////////////////

void SQLiteManager::setLastError(const QString &error)
{
    m_lastError = error;
    qWarning() << "SQLiteManager::setLastError:" << error;
}
