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

SQLiteManager::SQLiteManager(QObject *parent) : QObject(parent)
{
    // Constructor
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

bool SQLiteManager::openDatabase(const QString &connectionName, const QString &dbPath)
{
    const QString conn = resolveConn(connectionName);
    if (m_dbConnections.contains(conn))
    {
        if (m_dbConnections[conn].isOpen())
        {
            return true;
        }
        closeDatabase(conn);
    }

    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", conn);
    db.setDatabaseName(dbPath);
    if (!db.open())
    {
        setLastError("openDatabase: " + db.lastError().text());
        emit signalDatabaseError(m_lastError);
        return false;
    }

    // Enable WAL mode + foreign keys
    QSqlQuery q(db);
    q.exec("PRAGMA journal_mode=WAL");
    q.exec("PRAGMA foreign_keys=ON");

    m_dbConnections[conn] = db;
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
    return m_dbConnections.contains(conn) && m_dbConnections[conn].isOpen();
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
            setLastError("createDatabase: failed to create directory: " + dir.absolutePath());
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
            setLastError("deleteDatabase: failed to remove file: " + dbPath);
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
    return executeSql(connectionName, sql);
}

/////////////////////////////////////////////////////////////////////

bool SQLiteManager::tableExists(const QString &connectionName, const QString &tableName) const
{
    QSqlDatabase db = getDb(connectionName);
    if (!db.isOpen())
    {
        qWarning() << "SQLiteManager::tableExists: database not open:" << connectionName;
        return false;
    }
    return db.tables().contains(tableName, Qt::CaseInsensitive);
}

/////////////////////////////////////////////////////////////////////

bool SQLiteManager::dropTable(const QString &connectionName, const QString &tableName)
{
    return executeSql(connectionName, QString("DROP TABLE IF EXISTS %1").arg(tableName));
}

/////////////////////////////////////////////////////////////////////

bool SQLiteManager::executeSql(const QString &connectionName, const QString &sql)
{
    QSqlDatabase db = getDb(connectionName);
    if (!db.isOpen())
    {
        setLastError("executeSql: database not open");
        return false;
    }

    QSqlQuery q(db);
    if (!q.exec(sql))
    {
        setLastError("executeSql: " + q.lastError().text() + " | SQL: " + sql);
        emit signalDatabaseError(m_lastError);
        return false;
    }
    return true;
}

/////////////////////////////////////////////////////////////////////

bool SQLiteManager::insert(const QString &connectionName, const QString &tableName, const QVariantMap &data)
{
    if (data.isEmpty())
    {
        setLastError("insert: data map is empty");
        return false;
    }

    QSqlDatabase db = getDb(connectionName);
    if (!db.isOpen())
    {
        setLastError("insert: database not open");
        return false;
    }

    const QStringList cols   = data.keys();
    const QStringList placeholders(cols.size(), "?");
    const QString sql = QString("INSERT INTO %1 (%2) VALUES (%3)").arg(tableName, cols.join(", "), placeholders.join(", "));

    QSqlQuery q(db);
    q.prepare(sql);
    for (const QString &col : cols)
    {
        q.addBindValue(data[col]);
    }
        
    if (!q.exec())
    {
        setLastError("insert: " + q.lastError().databaseText());
        emit signalDatabaseError(m_lastError);
        return false;
    }
    return true;
}

/////////////////////////////////////////////////////////////////////

bool SQLiteManager::update(const QString &connectionName, const QString &tableName, const QVariantMap &data, const QString &whereClause, const QVariantList &whereValues)
{
    if (data.isEmpty())
    {
        setLastError("update: data map is empty");
        return false;
    }

    QSqlDatabase db = getDb(connectionName);
    if (!db.isOpen())
    {
        setLastError("update: database not open");
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
        setLastError("update: " + q.lastError().text());
        emit signalDatabaseError(m_lastError);
        return false;
    }
    return true;
}

/////////////////////////////////////////////////////////////////////

bool SQLiteManager::remove(const QString &connectionName, const QString &tableName, const QString &whereClause, const QVariantList &whereValues)
{
    QSqlDatabase db = getDb(connectionName);
    if (!db.isOpen())
    {
        setLastError("remove: database not open");
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
        setLastError("remove: " + q.lastError().text());
        emit signalDatabaseError(m_lastError);
        return false;
    }
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
    QSqlDatabase db = getDb(connectionName);
    if (!db.isOpen())
    {
        setLastError("selectWhere: database not open");
        return {};
    }

    const QString cols = columns.isEmpty() ? "*" : columns.join(", ");
    QString sql = QString("SELECT %1 FROM %2").arg(cols, tableName);
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
        setLastError("selectWhere: " + q.lastError().text());
        emit signalDatabaseError(m_lastError);
        return {};
    }
    return fetchRows(q);
}

/////////////////////////////////////////////////////////////////////

QVariantMap SQLiteManager::selectFirst(const QString &connectionName, const QString &tableName, const QString &whereClause, const QVariantList &whereValues)
{
    QSqlDatabase db = getDb(connectionName);
    if (!db.isOpen())
    {
        setLastError("selectFirst: database not open");
        return {};
    }

    QString sql = QString("SELECT * FROM %1").arg(tableName);
    if (!whereClause.isEmpty())
    {
        sql += " WHERE " + whereClause;
    }
    sql += " LIMIT 1";

    QSqlQuery q(db);
    q.prepare(sql);
    for (const QVariant &v : whereValues)
    {
        q.addBindValue(v);
    }
        
    if (!q.exec() || !q.next())
    {
        if (q.lastError().isValid())
        {
            setLastError("selectFirst: " + q.lastError().text());
            emit signalDatabaseError(m_lastError);
        }
        return {};
    }

    QVariantMap row;
    const QSqlRecord rec = q.record();
    for (int i = 0; i < rec.count(); ++i)
    {
        row[rec.fieldName(i)] = q.value(i);
    }
    return row;
}

/////////////////////////////////////////////////////////////////////

int SQLiteManager::count(const QString &connectionName, const QString &tableName, const QString &whereClause, const QVariantList &whereValues)
{
    QSqlDatabase db = getDb(connectionName);
    if (!db.isOpen())
    {
        setLastError("count: database not open");
        return -1;
    }

    QString sql = QString("SELECT COUNT(*) FROM %1").arg(tableName);
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

    if (!q.exec() || !q.next())
    {
        setLastError("count: " + q.lastError().text());
        emit signalDatabaseError(m_lastError);
        return -1;
    }
    return q.value(0).toInt();
}

/////////////////////////////////////////////////////////////////////

bool SQLiteManager::beginTransaction(const QString &connectionName)
{
    QSqlDatabase db = getDb(connectionName);
    if (!db.isOpen())
    {
        setLastError("beginTransaction: not open");
        return false;
    }
    
    if (!db.transaction())
    {
        setLastError("beginTransaction: " + db.lastError().text());
        emit signalDatabaseError(m_lastError);
        return false;
    }
    return true;
}

/////////////////////////////////////////////////////////////////////

bool SQLiteManager::commitTransaction(const QString &connectionName)
{
    QSqlDatabase db = getDb(connectionName);
    if (!db.isOpen())
    {
        setLastError("commitTransaction: not open");
        return false;
    }
    
    if (!db.commit())
    {
        setLastError("commitTransaction: " + db.lastError().text());
        emit signalDatabaseError(m_lastError);
        return false;
    }
    return true;
}

/////////////////////////////////////////////////////////////////////

bool SQLiteManager::rollbackTransaction(const QString &connectionName)
{
    QSqlDatabase db = getDb(connectionName);
    if (!db.isOpen())
    {
        setLastError("rollbackTransaction: not open");
        return false;
    }

    if (!db.rollback())
    {
        setLastError("rollbackTransaction: " + db.lastError().text());
        emit signalDatabaseError(m_lastError);
        return false;
    }
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

void SQLiteManager::setLastError(const QString &error)
{
    m_lastError = error;
    qWarning() << "SQLiteManager::setLastError:" << error;
}
