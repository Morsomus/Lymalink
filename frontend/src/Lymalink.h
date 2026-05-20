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
    Q_INVOKABLE void EnqueueHydrationTask(int appId, bool reloadAssets = false);
    Q_INVOKABLE void CancelHydration();
    Q_INVOKABLE bool CreateNewSteamEmuTarget(int appId, QString name, QString exePath, QString prefixPath);
    Q_INVOKABLE QVariantList FetchDashboardTargets();
    Q_INVOKABLE QVariantMap FetchTargetDetails(int appId);

signals:
    void signalSteamAppIdsReady(bool success, bool cancelled, QVariantList results);
    void signalHydrationTaskStarted(int appId);
    void signalHydrationTaskProgress(int appId, QString stage, int current, int total);
    void signalHydrationTaskFinished(int appId, bool success, bool cancelled);
    void signalHydrationQueueFinished();

    // Internal - SteamApiSearchWorker
    void signalRequestSearchSteamAppIds(const QString &term);
    void signalRequestCancel();

    // Internal - SteamApiHydrationWorker
    void signalRequestEnqueueHydrationTask(int appId, bool reloadAssets);
    void signalRequestCancelHydration();
    
private:
    SQLiteManager m_databaseManager;
    QString m_databaseConnectionName;
    QString m_databasePath;

    QThread m_searchWorkerThread;
    SteamApiSearchWorker *m_steamApiSearchWorker = nullptr;

    QThread m_hydrationWorkerThread;
    SteamApiHydrationWorker *m_steamApiHydrationWorker = nullptr;

    Error DatabaseInit();
    Error FileSystemInit();
    void OnAchievementsReady(int appId, QVariantList achievements);
    QString PlaytimeText(int hoursPlayed) const;
    QVariantMap LatestUnlockedAchievement(const QVariantList &achievements) const;
    QVariantList BuildAchievementDetails(int appId, const QString &iconsPath);
    QString CoverSource(const QString &coversPath, const QString &fileName) const;
    QString CommunityIconSource(const QString &iconsPath) const;
    QString AchievementIconSource(const QString &iconsPath, const QVariantMap &achievement) const;
    QString InstallationStatus(const QVariantMap &row) const;
};
