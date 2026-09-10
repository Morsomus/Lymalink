/////////////////////////////////////////////////////////
// File: DatabaseUtils.h
// Date: 2026-09-03
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Declares Lymalink specific DatabaseUtils
/////////////////////////////////////////////////////////

#pragma once

#include <QObject>
#include <QSqlError>
#include <QString>
#include <QVariantMap>

class DatabaseUtils : public QObject
{
    Q_OBJECT

public:
    explicit DatabaseUtils(QObject *parent = nullptr);
    ~DatabaseUtils();

    QVariantMap VerifyDatabaseLocation(const QString &folderPath, const QString &databaseFileName) const;
    bool IsDatabaseWriteAllowed(const QString &databasePath, QString *error, bool allowUnlockedInitWrite = false) const;
    bool ProbeRuntimeWriteAccess(const QString &connectionName, QString *error, bool *transientBusy = nullptr) const;

private:
    QString DatabaseLockError() const;
};
