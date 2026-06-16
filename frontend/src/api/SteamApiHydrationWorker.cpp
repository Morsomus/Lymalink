/////////////////////////////////////////////////////////
// File: SteamApiHydrationWorker.cpp
// Date: 2026-05-20
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Implements Hydration Worker
/////////////////////////////////////////////////////////

#include "SteamApiHydrationWorker.h"

#include <algorithm>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>

/////////////////////////////////////////////////////////////////////

const QSize SteamApiHydrationWorker::COVER_CARD_TARGET_SIZE = QSize(200, 300);
const QSize SteamApiHydrationWorker::COVER_CARD_SMALL_TARGET_SIZE = QSize(150, 225);
const QSize SteamApiHydrationWorker::COVER_ROW_DETAILED_TARGET_SIZE = QSize(80, 120);
const QSize SteamApiHydrationWorker::COVER_TARGET_DETAILS_TARGET_SIZE = QSize(240, 360);
const QSize SteamApiHydrationWorker::CI_TARGET_SIZE = QSize(44, 44);
const QSize SteamApiHydrationWorker::ACH_ICON_TARGET_SIZE = QSize(64, 64);

/////////////////////////////////////////////////////////////////////

SteamApiHydrationWorker::SteamApiHydrationWorker(QObject *parent) : QObject(parent)
{
    m_steamApi = nullptr;
    m_imageCache = nullptr;
    m_taskQueue = {};
    m_cancelled.storeRelease(0);
    m_running = false;
}

SteamApiHydrationWorker::~SteamApiHydrationWorker()
{
    // Destructor
}

/////////////////////////////////////////////////////////////////////
////////////////////////////// PUBLIC ///////////////////////////////
/////////////////////////////////////////////////////////////////////

void SteamApiHydrationWorker::Init()
{
    m_steamApi = new SteamApi(this);
    m_imageCache = new ImageCacheManager(this);
}

/////////////////////////////////////////////////////////////////////

void SteamApiHydrationWorker::EnqueueTask(int appId, bool reloadAssets, QString targetType)
{
    if (appId <= 0)
    {
        qWarning() << "SteamApiHydrationWorker::EnqueueTask: invalid appId:" << appId;
        return;
    }

    targetType = targetType.trimmed();
    if (targetType.compare("Steam", Qt::CaseInsensitive) == 0)
    {
        targetType = "Steam";
    }
    else
    {
        targetType = "Emulator";
    }

    // Avoid duplicate queued work for same app
    const auto alreadyQueued = std::any_of(m_taskQueue.cbegin(), m_taskQueue.cend(), [appId, targetType](const HydrationTask &task) {
        return task.appId == appId && task.targetType == targetType;
    });
    if (alreadyQueued)
    {
        qDebug() << "SteamApiHydrationWorker::EnqueueTask: appId already in queue:" << appId;
        return;
    }

    qDebug() << "SteamApiHydrationWorker::EnqueueTask: enqueuing appId:" << appId << "targetType:" << targetType << "reloadAssets:" << reloadAssets;

    HydrationTask task = {};
    task.appId = appId;
    task.reloadAssets = reloadAssets;
    task.targetType = targetType;
    m_taskQueue.enqueue(task);

    if (!m_running)
    {
        // Start queue processing immediately when worker is idle
        ProcessNext();
    }
}

/////////////////////////////////////////////////////////////////////

void SteamApiHydrationWorker::CancelAllEnqueueTasks()
{
    // Mark current task cancelled and drop queued tasks
    m_cancelled.storeRelease(1);
    m_taskQueue.clear();
    qDebug() << "SteamApiHydrationWorker::CancelAllEnqueueTasks: cancellation requested, queue cleared";
}

/////////////////////////////////////////////////////////////////////
///////////////////////////// PRIVATE ///////////////////////////////
/////////////////////////////////////////////////////////////////////

void SteamApiHydrationWorker::ProcessNext()
{
    // Finish queue when no pending tasks remain
    if (m_taskQueue.isEmpty())
    {
        m_running = false;
        emit signalHydrationQueueFinished();
        return;
    }

    // Process tasks sequentially in worker thread
    m_running = true;
    const HydrationTask task = m_taskQueue.dequeue();
    ProcessTask(task);

    // Proceed to next unless cancelled (Cancel() already cleared the queue)
    ProcessNext();
}

/////////////////////////////////////////////////////////////////////

void SteamApiHydrationWorker::ProcessTask(const HydrationTask &task)
{
    // Reset cancellation state for current task
    m_cancelled.storeRelease(0);

    const int appId = task.appId;
    const QString targetType = task.targetType.compare("Steam", Qt::CaseInsensitive) == 0 ? "Steam" : "Emulator";
    qDebug() << "SteamApiHydrationWorker::ProcessTask: starting task for appId:" << appId << "targetType:" << targetType;
    emit signalHydrationTaskStarted(appId, targetType);

    // Ensure Init() ran in this worker thread
    if (!m_steamApi || !m_imageCache)
    {
        qCritical() << "SteamApiHydrationWorker::ProcessTask: not initialized";
        emit signalHydrationTaskError(appId, "Asset reload failed", "Steam asset worker is not initialized.");
        emit signalHydrationTaskFinished(appId, targetType, false, false);
        return;
    }

    // Resolve app data location for target asset folders
    const QString appDataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (appDataPath.isEmpty())
    {
        qCritical() << "SteamApiHydrationWorker::ProcessTask: failed to resolve app data location";
        emit signalHydrationTaskError(appId, "Asset reload failed", "Could not resolve application data location.");
        emit signalHydrationTaskFinished(appId, targetType, false, false);
        return;
    }

    // Build per-target cover and icon directories
    const QString appIdStr  = QString::number(appId);
    const QString assetRoot = targetType == "Steam" ? "Steam" : "Emulator";
    const QString coversDir = QDir(appDataPath).filePath(assetRoot + "/" + appIdStr + "/covers");
    const QString iconsDir  = QDir(appDataPath).filePath(assetRoot + "/" + appIdStr + "/icons");

    if (task.reloadAssets)
    {
        // Remove stale files before downloading replacement assets
        emit signalHydrationTaskProgress(appId, targetType, "ClearingAssets", 0, 0);
        if (!ClearAssetDirectory(coversDir, appId) || !ClearAssetDirectory(iconsDir, appId))
        {
            emit signalHydrationTaskError(appId, "Asset reload failed", "Could not clear existing asset files.");
            emit signalHydrationTaskFinished(appId, targetType, false, false);
            return;
        }
    }

    // Fetch game info (cover + icon suffixes)
    emit signalHydrationTaskProgress(appId, targetType, "FetchingGameInfo", 0, 0);

    SteamGameInfo gameInfo = {};
    const Error gameInfoError = m_steamApi->SearchGameInfo(appId, gameInfo);
    if (gameInfoError != Error::NoError)
    {
        qWarning() << "SteamApiHydrationWorker::ProcessTask: failed to fetch game info for appId:" << appId;
        emit signalHydrationTaskError(appId, "Asset reload failed", "Could not fetch Steam game info. Check internet connection and try again.");
        emit signalHydrationTaskFinished(appId, targetType, false, false);
        return;
    }

    if (m_cancelled.loadAcquire())
    {
        emit signalHydrationTaskFinished(appId, targetType, false, true);
        return;
    }

    // Download library capsule (cover)
    emit signalHydrationTaskProgress(appId, targetType, "DownloadingCover", 0, 0);

    // Resolve cover CDN URLs and download required scaled cover variants
    QList<QString> lcUrls;
    m_steamApi->GetLibraryCapsuleUrls(appId, gameInfo.lcSuffix, gameInfo.assetUrlFormat, lcUrls);
    TryDownloadFirstWorking(lcUrls, coversDir, COVER_CARD_TARGET_SIZE, "cover_200x300");
    TryDownloadFirstWorking(lcUrls, coversDir, COVER_CARD_SMALL_TARGET_SIZE, "cover_150x225");
    TryDownloadFirstWorking(lcUrls, coversDir, COVER_ROW_DETAILED_TARGET_SIZE, "cover_80x120");
    TryDownloadFirstWorking(lcUrls, coversDir, COVER_TARGET_DETAILS_TARGET_SIZE, "cover_240x360");
    m_imageCache->ClearMemoryCache();

    if (m_cancelled.loadAcquire())
    {
        emit signalHydrationTaskFinished(appId, targetType, false, true);
        return;
    }

    // Download community icon
    emit signalHydrationTaskProgress(appId, targetType, "DownloadingCommunityIcon", 0, 0);

    // Resolve community icon CDN URLs and cache icon
    QList<QString> ciUrls;
    m_steamApi->GetCommunityIconUrls(appId, gameInfo.ciSuffix, ciUrls);
    TryDownloadFirstWorking(ciUrls, iconsDir, CI_TARGET_SIZE, "community_icon");

    if (m_cancelled.loadAcquire())
    {
        emit signalHydrationTaskFinished(appId, targetType, false, true);
        return;
    }

    // Fetch achievement data
    emit signalHydrationTaskProgress(appId, targetType, "FetchingAchievements", 0, 0);

    // Fetch public achievement payload from Steam
    QList<SteamAchievementData> achievements;
    const Error achievementsError = m_steamApi->FetchAchievementDataPrimary(appId, achievements);
    if (achievementsError == Error::NoData)
    {
        qDebug() << "SteamApiHydrationWorker::ProcessTask: no achievement data available for appId:" << appId;
        emit signalAchievementsReady(appId, targetType, QVariantList());
        emit signalHydrationTaskFinished(appId, targetType, true, false);
        return;
    }

    if (achievementsError != Error::NoError)
    {
        qWarning() << "SteamApiHydrationWorker::ProcessTask: failed to fetch achievements for appId:" << appId;
        emit signalHydrationTaskError(appId, "Asset reload failed", "Could not fetch Steam achievements. Check internet connection and try again.");
        emit signalHydrationTaskFinished(appId, targetType, false, false);
        return;
    }

    if (m_cancelled.loadAcquire())
    {
        emit signalHydrationTaskFinished(appId, targetType, false, true);
        return;
    }

    // Resolve active and locked achievement icon URLs
    QList<SteamAchievementIconUrls> achievementIconUrls;
    const Error iconUrlsError = m_steamApi->GetAchievementIconUrls(appId, achievements, achievementIconUrls);
    if (iconUrlsError != Error::NoError)
    {
        qWarning() << "SteamApiHydrationWorker::ProcessTask: failed to resolve achievement icon urls for appId:" << appId;
        emit signalHydrationTaskError(appId, "Asset reload failed", "Could not resolve Steam achievement icons. Check internet connection and try again.");
        emit signalHydrationTaskFinished(appId, targetType, false, false);
        return;
    }

    // Download both active and locked achievement icons
    const int total = achievementIconUrls.size();
    for (int i = 0; i < total; ++i)
    {
        if (m_cancelled.loadAcquire())
        {
            emit signalHydrationTaskFinished(appId, targetType, false, true);
            return;
        }

        emit signalHydrationTaskProgress(appId, targetType, "DownloadingAchievementIcons", i + 1, total);

        const SteamAchievementIconUrls &iconUrls = achievementIconUrls.at(i);
        TryDownloadFirstWorking(iconUrls.iconUrls,     iconsDir, ACH_ICON_TARGET_SIZE, iconUrls.achievementKey + "_icon");
        TryDownloadFirstWorking(iconUrls.iconGrayUrls, iconsDir, ACH_ICON_TARGET_SIZE, iconUrls.achievementKey + "_gray_icon");
    }

    if (m_cancelled.loadAcquire())
    {
        emit signalHydrationTaskFinished(appId, targetType, false, true);
        return;
    }

    // Package achievement data for DB write
    emit signalHydrationTaskProgress(appId, targetType, "PreparingData", 0, 0);

    const qint64 now = QDateTime::currentSecsSinceEpoch();

    QVariantList achievementList;
    for (const SteamAchievementData &achievement : achievements)
    {
        QVariantMap entry = {};
        entry["achievement_key"]   = achievement.achievementKey;
        entry["achievement_name"] = achievement.achievementName;
        entry["achievement_description"] = achievement.achievementDescription;
        entry["achievement_hidden"] = achievement.achievementHidden ? 1 : 0;
        entry["global_unlock_percentage"] = achievement.globalUnlockPercentage;
        entry["cur_progress"] = achievement.minProgress;
        entry["max_progress"] = achievement.maxProgress;
        entry["date_added"] = now;
        entry["date_updated"] = now;

        achievementList.append(entry);
    }

    emit signalAchievementsReady(appId, targetType, achievementList);
    emit signalHydrationTaskFinished(appId, targetType, true, false);

    qInfo() << "SteamApiHydrationWorker::ProcessTask: task completed for appId:" << appId << "- achievements:" << achievements.size();
}

/////////////////////////////////////////////////////////////////////

bool SteamApiHydrationWorker::ClearAssetDirectory(const QString &directoryPath, int appId)
{
    bool directoryCleared = true;

    // Create missing asset directory instead of treating it as failure
    QDir directory(directoryPath);
    if (!directory.exists())
    {
        directoryCleared = QDir().mkpath(directoryPath);
        if (directoryCleared)
        {
            qDebug() << "SteamApiHydrationWorker::ClearAssetDirectory: created directory for appId" << appId << directoryPath;
        }
        else
        {
            qWarning() << "SteamApiHydrationWorker::ClearAssetDirectory: failed to create directory for appId" << appId << directoryPath;
        }
        return directoryCleared;
    }

    // Remove existing asset files and nested directories
    const QFileInfoList entries = directory.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QFileInfo &entry : entries)
    {
        if (entry.isDir())
        {
            QDir childDir(entry.absoluteFilePath());
            if (!childDir.removeRecursively())
            {
                qWarning() << "SteamApiHydrationWorker::ClearAssetDirectory: failed to remove asset folder for appId" << appId << entry.absoluteFilePath();
                directoryCleared = false;
                return directoryCleared;
            }
        }
        else if (!QFile::remove(entry.absoluteFilePath()))
        {
            qWarning() << "SteamApiHydrationWorker::ClearAssetDirectory: failed to remove asset file for appId" << appId << entry.absoluteFilePath();
            directoryCleared = false;
            return directoryCleared;
        }
    }

    if (entries.size() > 0)
    {
        qDebug() << "SteamApiHydrationWorker::ClearAssetDirectory: cleared" << entries.size() << "entries for appId" << appId << directoryPath;
    }
    else
    {
        qDebug() << "SteamApiHydrationWorker::ClearAssetDirectory: directory already empty for appId" << appId << directoryPath;
    }

    return directoryCleared;
}

/////////////////////////////////////////////////////////////////////

QString SteamApiHydrationWorker::TryDownloadFirstWorking(const QList<QString> &urls, const QString &savePath, const QSize &targetSize, const QString &newName)
{
    QString downloadedPath = "";

    // Try CDN URLs in priority order until one downloads and caches
    const int urlCount = urls.size();
    for (int i = 0; i < urlCount; ++i)
    {
        const QString &url = urls.at(i);
        QString cachedPath = "";
        const Error err = m_imageCache->DownloadAndCache(url, savePath, targetSize, cachedPath, newName);
        if (err == Error::NoError && !cachedPath.isEmpty())
        {
            downloadedPath = cachedPath;
            qDebug() << "SteamApiHydrationWorker::TryDownloadFirstWorking: downloaded" << newName << "from URL (" << i + 1 << "/" << urlCount << ")" << url << "to" << downloadedPath;
            return downloadedPath;
        }
    }

    qWarning() << "SteamApiHydrationWorker::TryDownloadFirstWorking: failed to download" << newName << "from" << urlCount << "URLs for savePath" << savePath;
    return downloadedPath;
}
