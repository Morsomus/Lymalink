/////////////////////////////////////////////////////////
// File: SQLiteManager.h
// Date: 2026-05-14
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Declares SQLiteManager class
/////////////////////////////////////////////////////////

#pragma once

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
    ~SQLiteManager();

    // Connection methods
    Q_INVOKABLE bool openDatabase(const QString &connectionName, const QString &dbPath);
    Q_INVOKABLE void closeDatabase(const QString &connectionName);
    Q_INVOKABLE bool isDatabaseOpen(const QString &connectionName) const;

    // Database
    Q_INVOKABLE bool createDatabase(const QString &connectionName, const QString &dbPath);
    Q_INVOKABLE bool deleteDatabase(const QString &connectionName, const QString &dbPath);
    Q_INVOKABLE bool databaseFileExists(const QString &dbPath) const;

    // Schema / initialization
    Q_INVOKABLE bool createTable(const QString &connectionName, const QString &tableName, const QStringList &columnDefs);
    Q_INVOKABLE bool tableExists(const QString &connectionName, const QString &tableName) const;
    Q_INVOKABLE bool dropTable(const QString &connectionName, const QString &tableName);
    Q_INVOKABLE bool executeSql(const QString &connectionName, const QString &sql);

    // Write
    Q_INVOKABLE bool insert(const QString &connectionName, const QString &tableName, const QVariantMap &data);
    Q_INVOKABLE bool update(const QString &connectionName, const QString &tableName, const QVariantMap &data, const QString &whereClause, const QVariantList &whereValues = {});
    Q_INVOKABLE bool remove(const QString &connectionName, const QString &tableName, const QString &whereClause, const QVariantList &whereValues = {});

    // Read
    Q_INVOKABLE QVariantList selectAll(const QString &connectionName, const QString &tableName, const QStringList &columns = {});
    Q_INVOKABLE QVariantList selectWhere(const QString &connectionName, const QString &tableName, const QString &whereClause, const QVariantList &whereValues = {}, const QStringList &columns = {});
    Q_INVOKABLE QVariantMap  selectFirst(const QString &connectionName, const QString &tableName, const QString &whereClause = {}, const QVariantList &whereValues = {});
    Q_INVOKABLE int          count(const QString &connectionName, const QString &tableName, const QString &whereClause = {}, const QVariantList &whereValues = {});

    // Transaction helpers
    Q_INVOKABLE bool beginTransaction(const QString &connectionName);
    Q_INVOKABLE bool commitTransaction(const QString &connectionName);
    Q_INVOKABLE bool rollbackTransaction(const QString &connectionName);

    // Error handling
    Q_INVOKABLE QString lastError() const;

signals:
    void signalDatabaseError(const QString &error);
    void signalConnectionStatusChanged(bool connected, const QString &connectionName);

private:
    QMap<QString, QSqlDatabase> m_dbConnections;
    QString m_lastError;

    QString resolveConn(const QString &connectionName) const;
    QSqlDatabase getDb(const QString &connectionName) const;
    QVariantList fetchRows(QSqlQuery &query) const;
    void setLastError(const QString &error);
};
