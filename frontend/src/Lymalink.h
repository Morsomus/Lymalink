/////////////////////////////////////////////////////////
// File: Lymalink.h
// Date: 2026-05-14
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Declares Lymalink backend orchestrator 
/////////////////////////////////////////////////////////

#pragma once

#include "Error.h"
#include "api/SteamApiSearchWorker.h"
#include "api/SteamApiHydrationWorker.h"
#include "database/SQLiteManager.h"
#include "tools/FileManager.h"

#include <QObject>
#include <QThread>
#include <QVariantList>
#include <QVariantMap>
#include <QString>

class Lymalink : public QObject
{
    Q_OBJECT

public:
    explicit Lymalink(QObject *parent = nullptr);
    ~Lymalink();

    Error Initialize();
    
    Q_INVOKABLE void SearchSteamAppIds(const QString &term);
    Q_INVOKABLE void CancelSteamAppIdSearch();
    Q_INVOKABLE void EnqueueSteamHydrationTask(int appId, bool reloadAssets = false);
    Q_INVOKABLE void CancelSteamHydration();
    Q_INVOKABLE bool CreateNewSteamEmuTarget(int appId, QString gameName, QString exePath, QString prefixPath);
    Q_INVOKABLE bool SetTargetHidden(int appId, bool hidden);
    Q_INVOKABLE bool SetAchievementUnlocked(int appId, const QString &achievementKey, bool unlocked, qint64 unlockTimestamp);
    Q_INVOKABLE bool DeleteTarget(int appId);
    Q_INVOKABLE QVariantList FetchDashboardTargets();
    Q_INVOKABLE QVariantMap FetchTargetDetails(int appId);

signals:
    void signalSteamAppIdsSearchReady(bool success, bool cancelled, QVariantList results);
    void signalSteamHydrationTaskStarted(int appId);
    void signalSteamHydrationTaskProgress(int appId, QString stage, int current, int total);
    void signalSteamHydrationTaskFinished(int appId, bool success, bool cancelled);
    void signalSteamHydrationQueueFinished();
    void signalErrorOccurred(QString title, QString message);

    // Internal - SteamApiSearchWorker
    void signalRequestSearchSteamAppIds(const QString &term);
    void signalRequestCancelSearchSteamAppIds();

    // Internal - SteamApiHydrationWorker
    void signalRequestEnqueueSteamHydrationTask(int appId, bool reloadAssets);
    void signalRequestCancelSteamHydration();
    
private:
    FileManager m_fileManager;
    SQLiteManager m_databaseManager;
    QString m_databaseConnectionName;
    QString m_databasePath;

    QThread m_searchWorkerThread;
    SteamApiSearchWorker *m_steamApiSearchWorker = nullptr;
    QThread m_hydrationWorkerThread;
    SteamApiHydrationWorker *m_steamApiHydrationWorker = nullptr;

    Error DatabaseInit();
    Error FileSystemInit();
    void ApplyNewAchievements(int appId, QVariantList achievements);
    QString PlaytimeText(int hoursPlayed) const;
    QVariantMap LatestUnlockedAchievement(const QVariantList &achievements) const;
    QVariantList BuildAchievementDetails(int appId, const QString &iconsPath);
    QString CoverImageFilePath(const QString &coversPath, const QString &fileName) const;
    QString CommunityIconFilePath(const QString &iconsPath) const;
    QString AchievementIconFilePath(const QString &iconsPath, const QVariantMap &achievement) const;
    QString ExecutableInstallationStatus(const QVariantMap &row) const;
};
