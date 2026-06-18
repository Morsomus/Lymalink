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
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QVariantList>
#include <QVariantMap>
#include <QString>
#include <QVector>

class DataTransporter : public QObject
{
    Q_OBJECT

public:
    explicit DataTransporter(QObject *parent = nullptr);
    ~DataTransporter();

    Q_INVOKABLE QVariantMap ExportAchievements(const QString &filePath);
    Q_INVOKABLE QVariantMap PreviewAchievementImport(const QString &filePath);
    Q_INVOKABLE QVariantMap ImportAchievements(const QString &filePath, const QVariantList &decisions);
    Q_INVOKABLE void ClearAchievementImportPreview();

private:
    struct ImportedAchievement
    {
        QString key;
        QString name;
        QString description;
        bool hidden = false;
        double globalUnlockPercentage = 0.0;
        int currentProgress = 0;
        int maxProgress = 0;
        qint64 dateUnlocked = 0;
        qint64 dateAdded = 0;
    };

    struct ImportedGame
    {
        int id = 0;
        QString name;
        QString executableLocation;
        QString prefixLocation;
        QString installationDir;
        bool hidden = false;
        int totalSecondsPlayed = 0;
        qint64 lastPlayedDate = 0;
        qint64 dateAdded = 0;
        QVector<ImportedAchievement> achievements;
    };

    SQLiteManager m_databaseManager;
    QString m_databaseConnectionName;
    QString m_databasePath;
    QString m_cachedImportFilePath;
    QVector<ImportedGame> m_cachedImportGames;
    bool m_hasCachedImport = false;

    QString DefaultDatabasePath() const;
    bool EnsureDatabaseOpen(QVariantMap &payload);
    bool ReadImportFile(const QString &filePath, QVector<ImportedGame> &games, QString &error) const;
    bool ParseImportDocument(const QJsonDocument &document, QVector<ImportedGame> &games, QString &error) const;
    void CacheImportPreview(const QString &filePath, const QVector<ImportedGame> &games);
    bool CachedImportPreview(const QString &filePath, QVector<ImportedGame> &games) const;
    void ClearCachedImportPreview();
    bool MergeImportedGame(const ImportedGame &game, qint64 now, int &addedAchievements, QString &error);
    bool ReplaceImportedGame(const ImportedGame &game, qint64 now, int &addedAchievements, QString &error);
    bool InsertImportedGame(const ImportedGame &game, qint64 now, int &addedAchievements, QString &error);
    bool InsertImportedAchievement(int gameId, const ImportedAchievement &achievement, qint64 now, QString &error);
    bool RefreshImportedGameCounts(const ImportedGame &game, qint64 now, QString &error);
    QVariantMap ImportedGameRow(const ImportedGame &game, qint64 now) const;
    QVariantMap ImportedAchievementRow(int gameId, const ImportedAchievement &achievement, qint64 now) const;
    int ImportedAchievementCount(const QVector<ImportedGame> &games) const;
    bool BuildExportJson(QJsonObject &exportJson, int &exportedGameCount, int &exportedAchievementCount, QString &error);
    QJsonObject BuildGameJson(const QVariantMap &row, const QVariantList &achievementRows, int &exportedAchievementCount) const;
    QJsonObject BuildAchievementJson(const QVariantMap &row) const;

    QVariantMap PreviewSuccessPayload(const QString &filePath, const QVector<ImportedGame> &games, const QVariantList &conflicts) const;
    QVariantMap ImportSuccessPayload(const QString &filePath, int importedGameCount, int importedAchievementCount, int addedGameCount, int mergedGameCount, int replacedGameCount, const QVariantList &addedTargets) const;
    QVariantMap SuccessPayload(const QString &filePath, int exportedGameCount, int exportedAchievementCount) const;
    QVariantMap ErrorPayload(const QString &error, const QString &filePath = QString()) const;

    int JsonIntValue(const QJsonValue &value) const;
    qint64 JsonDateValue(const QJsonValue &value) const;
    QJsonValue DateValue(const QVariantMap &row, const QString &key) const;
    QJsonValue NumberValue(const QVariantMap &row, const QString &key) const;
    bool JsonNonNegativeIntValue(const QJsonValue &value, int &result, const QString &fieldName, QString &error, bool allowNull = true) const;
    bool JsonNonNegativeDateValue(const QJsonValue &value, qint64 &result, const QString &fieldName, QString &error) const;
    bool JsonPercentageValue(const QJsonValue &value, double &result, const QString &fieldName, QString &error) const;
};
