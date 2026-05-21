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

void SteamApiHydrationWorker::EnqueueTask(int appId, bool reloadAssets)
{
    if (appId <= 0)
    {
        qWarning() << "SteamApiHydrationWorker: invalid appId:" << appId;
        return;
    }

    const auto alreadyQueued = std::any_of(m_taskQueue.cbegin(), m_taskQueue.cend(), [appId](const HydrationTask &task) {
        return task.appId == appId;
    });
    if (alreadyQueued)
    {
        qDebug() << "SteamApiHydrationWorker: appId already in queue:" << appId;
        return;
    }

    qDebug() << "SteamApiHydrationWorker: enqueuing appId:" << appId << "reloadAssets:" << reloadAssets;
    m_taskQueue.enqueue({appId, reloadAssets});

    if (!m_running)
    {
        ProcessNext();
    }
}

/////////////////////////////////////////////////////////////////////

void SteamApiHydrationWorker::CancelAllEnqueueTasks()
{
    m_cancelled.storeRelease(1);
    m_taskQueue.clear();
    qDebug() << "SteamApiHydrationWorker: cancellation requested, queue cleared";
}

/////////////////////////////////////////////////////////////////////
///////////////////////////// PRIVATE ///////////////////////////////
/////////////////////////////////////////////////////////////////////

void SteamApiHydrationWorker::ProcessNext()
{
    if (m_taskQueue.isEmpty())
    {
        m_running = false;
        emit signalHydrationQueueFinished();
        return;
    }

    m_running = true;
    const HydrationTask task = m_taskQueue.dequeue();
    ProcessTask(task);

    // Proceed to next unless cancelled (Cancel() already cleared the queue)
    ProcessNext();
}

/////////////////////////////////////////////////////////////////////

void SteamApiHydrationWorker::ProcessTask(const HydrationTask &task)
{
    m_cancelled.storeRelease(0);

    const int appId = task.appId;
    qDebug() << "SteamApiHydrationWorker: starting task for appId:" << appId;
    emit signalHydrationTaskStarted(appId);

    if (!m_steamApi || !m_imageCache)
    {
        qCritical() << "SteamApiHydrationWorker: not initialized";
        emit signalHydrationTaskError(appId, "Asset reload failed", "Steam asset worker is not initialized.");
        emit signalHydrationTaskFinished(appId, false, false);
        return;
    }

    const QString appDataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (appDataPath.isEmpty())
    {
        qCritical() << "SteamApiHydrationWorker: failed to resolve app data location";
        emit signalHydrationTaskError(appId, "Asset reload failed", "Could not resolve application data location.");
        emit signalHydrationTaskFinished(appId, false, false);
        return;
    }

    const QString appIdStr  = QString::number(appId);
    const QString coversDir = QDir(appDataPath).filePath("Emulator/" + appIdStr + "/covers");
    const QString iconsDir  = QDir(appDataPath).filePath("Emulator/" + appIdStr + "/icons");

    if (task.reloadAssets)
    {
        emit signalHydrationTaskProgress(appId, "ClearingAssets", 0, 0);
        if (!ClearAssetDirectory(coversDir) || !ClearAssetDirectory(iconsDir))
        {
            emit signalHydrationTaskError(appId, "Asset reload failed", "Could not clear existing asset files.");
            emit signalHydrationTaskFinished(appId, false, false);
            return;
        }
    }

    // Fetch game info (cover + icon suffixes)
    emit signalHydrationTaskProgress(appId, "FetchingGameInfo", 0, 0);

    SteamGameInfo gameInfo;
    const Error gameInfoError = m_steamApi->SearchGameInfo(appId, gameInfo);
    if (gameInfoError != Error::NoError)
    {
        qWarning() << "SteamApiHydrationWorker: failed to fetch game info for appId:" << appId;
        emit signalHydrationTaskError(appId, "Asset reload failed", "Could not fetch Steam game info. Check internet connection and try again.");
        emit signalHydrationTaskFinished(appId, false, false);
        return;
    }

    if (m_cancelled.loadAcquire())
    {
        emit signalHydrationTaskFinished(appId, false, true);
        return;
    }

    // Download library capsule (cover)
    emit signalHydrationTaskProgress(appId, "DownloadingCover", 0, 0);

    QList<QString> lcUrls;
    m_steamApi->GetLibraryCapsuleUrls(appId, gameInfo.lcSuffix, gameInfo.assetUrlFormat, lcUrls);
    TryDownloadFirstWorking(lcUrls, coversDir, COVER_CARD_TARGET_SIZE, "cover_200x300");
    TryDownloadFirstWorking(lcUrls, coversDir, COVER_CARD_SMALL_TARGET_SIZE, "cover_150x225");
    TryDownloadFirstWorking(lcUrls, coversDir, COVER_ROW_DETAILED_TARGET_SIZE, "cover_80x120");
    TryDownloadFirstWorking(lcUrls, coversDir, COVER_TARGET_DETAILS_TARGET_SIZE, "cover_240x360");

    if (m_cancelled.loadAcquire())
    {
        emit signalHydrationTaskFinished(appId, false, true);
        return;
    }

    // Download community icon
    emit signalHydrationTaskProgress(appId, "DownloadingCommunityIcon", 0, 0);

    QList<QString> ciUrls;
    m_steamApi->GetCommunityIconUrls(appId, gameInfo.ciSuffix, ciUrls);
    TryDownloadFirstWorking(ciUrls, iconsDir, CI_TARGET_SIZE, "community_icon");

    if (m_cancelled.loadAcquire())
    {
        emit signalHydrationTaskFinished(appId, false, true);
        return;
    }

    // Fetch achievement data
    emit signalHydrationTaskProgress(appId, "FetchingAchievements", 0, 0);

    QList<SteamAchievementData> achievements;
    const Error achievementsError = m_steamApi->FetchAchievementDataPrimary(appId, achievements);
    if (achievementsError == Error::NoData)
    {
        qDebug() << "SteamApiHydrationWorker: no achievement data available for appId:" << appId;
        emit signalAchievementsReady(appId, QVariantList());
        emit signalHydrationTaskFinished(appId, true, false);
        return;
    }

    if (achievementsError != Error::NoError)
    {
        qWarning() << "SteamApiHydrationWorker: failed to fetch achievements for appId:" << appId;
        emit signalHydrationTaskError(appId, "Asset reload failed", "Could not fetch Steam achievements. Check internet connection and try again.");
        emit signalHydrationTaskFinished(appId, false, false);
        return;
    }

    if (m_cancelled.loadAcquire())
    {
        emit signalHydrationTaskFinished(appId, false, true);
        return;
    }

    // Resolve and download achievement icons
    QList<SteamAchievementIconUrls> achievementIconUrls;
    const Error iconUrlsError = m_steamApi->GetAchievementIconUrls(appId, achievements, achievementIconUrls);
    if (iconUrlsError != Error::NoError)
    {
        qWarning() << "SteamApiHydrationWorker: failed to resolve achievement icon urls for appId:" << appId;
        emit signalHydrationTaskError(appId, "Asset reload failed", "Could not resolve Steam achievement icons. Check internet connection and try again.");
        emit signalHydrationTaskFinished(appId, false, false);
        return;
    }

    const int total = achievementIconUrls.size();
    for (int i = 0; i < total; ++i)
    {
        if (m_cancelled.loadAcquire())
        {
            emit signalHydrationTaskFinished(appId, false, true);
            return;
        }

        emit signalHydrationTaskProgress(appId, "DownloadingAchievementIcons", i + 1, total);

        const SteamAchievementIconUrls &iconUrls = achievementIconUrls.at(i);
        TryDownloadFirstWorking(iconUrls.iconUrls,     iconsDir, ACH_ICON_TARGET_SIZE, iconUrls.achievementKey + "_icon");
        TryDownloadFirstWorking(iconUrls.iconGrayUrls, iconsDir, ACH_ICON_TARGET_SIZE, iconUrls.achievementKey + "_gray_icon");
    }

    if (m_cancelled.loadAcquire())
    {
        emit signalHydrationTaskFinished(appId, false, true);
        return;
    }

    // Package achievement data for DB write
    emit signalHydrationTaskProgress(appId, "PreparingData", 0, 0);

    const qint64 now = QDateTime::currentSecsSinceEpoch();

    QVariantList achievementList;
    for (const SteamAchievementData &achievement : achievements)
    {
        QVariantMap entry;
        entry["achievement_key"]   = achievement.achievementKey;
        entry["achievement_name"] = achievement.achievementName;
        entry["achievement_description"] = achievement.achievementDescription;
        entry["achievement_hidden"] = achievement.achievementHidden ? 1 : 0;
        entry["global_unlock_percentage"] = achievement.globalUnlockPercentage;
        entry["date_added"]        = now;
        entry["date_updated"]      = now;

        achievementList.append(entry);
    }

    emit signalAchievementsReady(appId, achievementList);
    emit signalHydrationTaskFinished(appId, true, false);

    qDebug() << "SteamApiHydrationWorker: task completed for appId:" << appId << "- achievements:" << achievements.size();
}

/////////////////////////////////////////////////////////////////////

bool SteamApiHydrationWorker::ClearAssetDirectory(const QString &directoryPath)
{
    QDir directory(directoryPath);
    if (!directory.exists())
    {
        return QDir().mkpath(directoryPath);
    }

    const QFileInfoList entries = directory.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QFileInfo &entry : entries)
    {
        if (entry.isDir())
        {
            QDir childDir(entry.absoluteFilePath());
            if (!childDir.removeRecursively())
            {
                qWarning() << "SteamApiHydrationWorker: failed to remove asset folder:" << entry.absoluteFilePath();
                return false;
            }
        }
        else if (!QFile::remove(entry.absoluteFilePath()))
        {
            qWarning() << "SteamApiHydrationWorker: failed to remove asset file:" << entry.absoluteFilePath();
            return false;
        }
    }

    return true;
}

/////////////////////////////////////////////////////////////////////

QString SteamApiHydrationWorker::TryDownloadFirstWorking(const QList<QString> &urls, const QString &savePath, const QSize &targetSize, const QString &newName)
{
    for (const QString &url : urls)
    {
        QString cachedPath;
        const Error err = m_imageCache->DownloadAndCache(url, savePath, targetSize, cachedPath, newName);
        if (err == Error::NoError && !cachedPath.isEmpty())
        {
            return cachedPath;
        }
    }

    qWarning() << "SteamApiHydrationWorker: all URLs failed for:" << newName << savePath;
    return QString();
}
