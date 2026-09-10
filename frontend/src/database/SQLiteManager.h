/////////////////////////////////////////////////////////
// File: SQLiteManager.h
// Date: 2026-05-14
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Declares SQLiteManager class
/////////////////////////////////////////////////////////

#pragma once

#include "DatabaseUtils.h"

#include <QObject>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QVariantList>
#include <QVariantMap>
#include <QString>
#include <QStringList>
#include <QMap>
#include <QFileInfo>
#include <QDir>

class SQLiteManager : public QObject
{
    Q_OBJECT
public:
    explicit SQLiteManager(QObject *parent = nullptr);
    explicit SQLiteManager(DatabaseUtils *databaseUtils, QObject *parent = nullptr);
    ~SQLiteManager();

    // Connection methods
    bool openDatabase(const QString &connectionName, const QString &dbPath, bool createMissingDb = true);
    void closeDatabase(const QString &connectionName);
    bool isDatabaseOpen(const QString &connectionName) const;
    bool databaseAvailable(const QString &connectionName);

    // Database
    bool createDatabase(const QString &connectionName, const QString &dbPath);
    bool deleteDatabase(const QString &connectionName, const QString &dbPath);
    bool databaseFileExists(const QString &dbPath) const;

    // Schema / initialization
    bool createTable(const QString &connectionName, const QString &tableName, const QStringList &columnDefs);
    bool tableExists(const QString &connectionName, const QString &tableName);
    bool dropTable(const QString &connectionName, const QString &tableName);
    bool executeSql(const QString &connectionName, const QString &sql, bool allowUnlockedInitWrite = false);

    // Write
    bool insert(const QString &connectionName, const QString &tableName, const QVariantMap &data);
    bool update(const QString &connectionName, const QString &tableName, const QVariantMap &data, const QString &whereClause, const QVariantList &whereValues = {});
    bool remove(const QString &connectionName, const QString &tableName, const QString &whereClause, const QVariantList &whereValues = {});

    // Read
    QVariantList selectAll(const QString &connectionName, const QString &tableName, const QStringList &columns = {});
    QVariantList selectWhere(const QString &connectionName, const QString &tableName, const QString &whereClause, const QVariantList &whereValues = {}, const QStringList &columns = {});
    QVariantMap  selectFirst(const QString &connectionName, const QString &tableName, const QString &whereClause = {}, const QVariantList &whereValues = {});
    int          count(const QString &connectionName, const QString &tableName, const QString &whereClause = {}, const QVariantList &whereValues = {});

    // Transaction helpers
    bool beginTransaction(const QString &connectionName);
    bool commitTransaction(const QString &connectionName);
    bool rollbackTransaction(const QString &connectionName);

    // Error handling
    QString lastError() const;

signals:
    void signalDatabaseError(const QString &error);
    void signalConnectionStatusChanged(bool connected, const QString &connectionName);

private:
    QMap<QString, QSqlDatabase> m_dbConnections;
    DatabaseUtils *m_databaseUtils;
    QString m_lastError;

    QString resolveConn(const QString &connectionName) const;
    QSqlDatabase getDb(const QString &connectionName) const;
    QVariantList fetchRows(QSqlQuery &query) const;
    bool isDatabaseWriteAllowed(const QString &connectionName, bool allowUnlockedInitWrite = false);
    void setLastError(const QString &error);
};
