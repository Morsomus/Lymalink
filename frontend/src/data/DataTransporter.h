/////////////////////////////////////////////////////////
// File: DataTransporter.h
// Date: 2026-06-17
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Declares achievement import/export helper
/////////////////////////////////////////////////////////

#pragma once

#include "../database/SQLiteManager.h"

#include <QObject>
#include <QJsonObject>
#include <QVariantMap>
#include <QString>

class DataTransporter : public QObject
{
    Q_OBJECT

public:
    explicit DataTransporter(QObject *parent = nullptr);
    ~DataTransporter();

    Q_INVOKABLE QVariantMap ExportAchievements(const QString &filePath);

private:
    SQLiteManager m_databaseManager;
    QString m_databaseConnectionName;
    QString m_databasePath;

    QString DefaultDatabasePath() const;
    bool EnsureDatabaseOpen(QVariantMap &payload);
    QJsonObject BuildExportJson(int &exportedGameCount, int &exportedAchievementCount);
    QJsonObject BuildGameJson(const QVariantMap &row, const QVariantList &achievementRows, int &exportedAchievementCount) const;
    QJsonObject BuildAchievementJson(const QVariantMap &row) const;
    QJsonValue DateValue(const QVariantMap &row, const QString &key) const;
    QJsonValue NumberValue(const QVariantMap &row, const QString &key) const;
    QVariantMap SuccessPayload(const QString &filePath, int exportedGameCount, int exportedAchievementCount) const;
    QVariantMap ErrorPayload(const QString &error, const QString &filePath = QString()) const;
};
