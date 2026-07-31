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
#include "api/SteamImportAutoSyncWorker.h"
#include "database/SQLiteManager.h"
#include "tools/AppIdDirectoryFinder.h"
#include "tools/FileManager.h"

#include <QObject>
#include <QThread>
#include <QVariantList>
#include <QVariantMap>
#include <QString>
#include <QImage>
#include <QSize>

class Lymalink : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool steamHydrationBusy READ GetSteamHydrationBusy NOTIFY signalSteamHydrationBusyChanged)
    Q_PROPERTY(bool steamImportAutoSyncBusy READ GetSteamImportAutoSyncBusy NOTIFY signalSteamImportAutoSyncBusyChanged)

public:
    explicit Lymalink(QObject *parent = nullptr);
    ~Lymalink();

    Error Initialize();

    bool GetSteamHydrationBusy() const;
    bool GetSteamImportAutoSyncBusy() const;
    
    Q_INVOKABLE void SearchSteamAppIds(const QString &term);
    Q_INVOKABLE void CancelSteamAppIdSearch();
    Q_INVOKABLE void EnqueueSteamHydrationTask(int appId, bool reloadAssets = false, const QString &targetType = "Emulator");
    Q_INVOKABLE void CancelSteamHydration();
    Q_INVOKABLE QVariantMap InspectExecutableFolder(const QString &executablePath);
    Q_INVOKABLE void FindEmulatorAppIdFolders(const QString &rootPath = QString());
    Q_INVOKABLE bool CreateNewSteamEmuTarget(int appId, QString gameName, QString exePath, QString prefixPath, QString installationDir);
    Q_INVOKABLE QVariantMap ImportSteamGames(QVariantList games, const QString &steamId, const QString &apiKey);
    Q_INVOKABLE QVariantMap UpdateSteamImports(QVariantList games, const QString &steamId, const QString &apiKey);
    Q_INVOKABLE void StartSteamImportAutoSync(const QString &steamId, const QString &apiKey);
    Q_INVOKABLE bool SetTargetHidden(int appId, bool hidden, const QString &targetType = "Emulator");
    Q_INVOKABLE bool SetAllTargetsHidden(bool hidden, const QString &targetType = "Emulator");
    Q_INVOKABLE bool SetTargetPrefixLocation(int appId, const QString &prefixPath);
    Q_INVOKABLE bool SetTargetExecutableLocation(int appId, const QString &executablePath);
    Q_INVOKABLE bool SetTargetInstallationLocation(int appId, const QString &installationDir);
    Q_INVOKABLE bool SetTargetCoverImage(int appId, const QString &sourceImagePath, const QString &targetType = "Emulator");
    Q_INVOKABLE bool ClearTargetCoverImage(int appId, const QString &targetType = "Emulator");
    Q_INVOKABLE bool ResetTargetAchievementDataLocation(int appId);
    Q_INVOKABLE bool SetAchievementUnlocked(int appId, const QString &achievementKey, bool unlocked, qint64 unlockTimestamp);
    Q_INVOKABLE bool DeleteTarget(int appId, const QString &targetType = "Emulator");
    Q_INVOKABLE QString GetTargetTitle(int appId);
    Q_INVOKABLE QString GetTargetPrefixLocation(int appId);
    Q_INVOKABLE QString GetTargetExecutableLocation(int appId);
    Q_INVOKABLE QString GetTargetInstallationLocation(int appId);
    Q_INVOKABLE QString GetLastOperationError() const;
    Q_INVOKABLE QVariantList ReloadAllMissingMetadata();
    Q_INVOKABLE QVariantList FetchDashboardTargets();
    Q_INVOKABLE QVariantMap FetchTargetDetails(int appId, const QString &targetType = "Emulator");
    Q_INVOKABLE QVariantMap FetchSteamOwnedGames(const QString &steamId, const QString &apiKey);

signals:
    void signalSteamAppIdsSearchReady(bool success, bool cancelled, QVariantList results);
    void signalSteamHydrationTaskStarted(int appId, QString targetType);
    void signalSteamHydrationTaskProgress(int appId, QString targetType, QString stage, int current, int total);
    void signalSteamHydrationTaskFinished(int appId, QString targetType, bool success, bool cancelled);
    void signalSteamHydrationQueueFinished();
    void signalSteamHydrationBusyChanged();
    void signalSteamImportAutoSyncBusyChanged();
    void signalSteamImportAutoSyncFinished(QVariantMap payload);
    void signalAchievementMetadataReady(int appId, QString targetType, bool success);
    void signalEmulatorAppIdFolderFindFinished(bool success, QVariantList results, QString error);
    void signalErrorOccurred(QString title, QString message);

    // Internal - SteamApiSearchWorker
    void signalRequestSearchSteamAppIds(const QString &term);
    void signalRequestCancelSearchSteamAppIds();

    // Internal - SteamApiHydrationWorker
    void signalRequestEnqueueSteamHydrationTask(int appId, bool reloadAssets, QString targetType);
    void signalRequestCancelSteamHydration();
    
private:
    FileManager m_fileManager;
    SQLiteManager m_databaseManager;
    QString m_databaseConnectionName;
    QString m_databasePath;
    QString m_lastOperationError;

    QThread m_searchWorkerThread;
    SteamApiSearchWorker *m_steamApiSearchWorker;
    QThread m_hydrationWorkerThread;
    SteamApiHydrationWorker *m_steamApiHydrationWorker;
    bool m_steamHydrationBusy;
    QThread *m_steamImportAutoSyncThread;
    bool m_steamImportAutoSyncBusy;
    QThread *m_appIdFolderFindThread;
    bool m_appIdFolderFindBusy;

    Error DatabaseInit();
    Error FileSystemInit();
    bool EnsureColumn(const QString &tableName, const QString &columnName, const QString &columnDef);
    bool ApplyNewAchievements(int appId, QString targetType, QVariantList achievements);
    QString PlaytimeText(int hoursPlayed) const;
    QVariantMap LatestUnlockedAchievement(const QVariantList &achievements) const;
    QVariantList BuildAchievementDetails(int appId, const QString &iconsPath, const QString &targetType);
    QString CoverImageFilePath(const QString &coversPath, const QString &fileName) const;
    QString PreferredCoverImageFilePath(const QString &coversPath, const QString &fileName) const;
    bool TargetHasMissingMetadata(int appId, const QString &targetType);
    bool TargetHasMissingCoverAssets(const QString &coversPath) const;
    bool TargetHasMissingAchievementIcons(int appId, const QString &iconsPath, const QString &targetType);
    bool SaveCustomCoverVariant(const QImage &sourceImage, const QString &coversPath, const QString &fileName, const QSize &targetSize) const;
    QString CommunityIconFilePath(const QString &iconsPath) const;
    QString AchievementIconFilePath(const QString &iconsPath, const QVariantMap &achievement) const;
    QString ExecutableInstallationStatus(const QVariantMap &row) const;
    bool IsTargetExecutableLocationInUse(const QString &executablePath, int excludedAppId, bool *querySucceeded = nullptr);
    QString NormalizeTargetType(const QString &targetType) const;
    QString GameTableForTargetType(const QString &targetType) const;
    QString AchievementTableForTargetType(const QString &targetType) const;
    QString AssetFolderForTargetType(const QString &targetType) const;
    bool IsSteamTargetType(const QString &targetType) const;
};
