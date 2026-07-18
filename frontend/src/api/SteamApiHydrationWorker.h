/////////////////////////////////////////////////////////
// File: SteamApiHydrationWorker.h
// Date: 2026-05-20
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Declares Hydration Worker
//              Processes a queue of Steam app IDs sequentially.
//              Each task fetches cover art, community icon,
//              achievement data and icons, then writes results
//              to the caller via signals.
/////////////////////////////////////////////////////////

#pragma once

#include "../Error.h"
#include "../tools/ImageCacheManager.h"
#include "SteamApi.h"

#include <QAtomicInt>
#include <QObject>
#include <QQueue>
#include <QString>
#include <QStringList>
#include <QVariantMap>

class SteamApiHydrationWorker : public QObject
{
    Q_OBJECT
public:
    explicit SteamApiHydrationWorker(QObject *parent = nullptr);
    ~SteamApiHydrationWorker();

public slots:
    void Init();
    void EnqueueTask(int appId, bool reloadAssets = false, QString targetType = "Emulator");
    void CancelAllEnqueueTasks();

signals:
    // Progress reporting
    void signalHydrationTaskStarted(int appId, QString targetType);
    void signalHydrationTaskProgress(int appId, QString targetType, QString stage, int current, int total);
    void signalHydrationTaskFinished(int appId, QString targetType, bool success, bool cancelled);
    void signalHydrationQueueFinished();
    void signalHydrationTaskError(int appId, QString title, QString message);

    // Lymalink write signal
    void signalAchievementsReady(int appId, QString targetType, QVariantList achievements);

private:
    struct HydrationTask
    {
        int appId = 0;
        bool reloadAssets = false;
        QString targetType = "Emulator";
    };

    SteamApi *m_steamApi;
    ImageCacheManager *m_imageCache;
    QQueue<HydrationTask> m_taskQueue;
    QAtomicInt m_cancelled;
    bool m_running = false;
    QStringList m_benchmarkedAchievementIconUrlFormats;

    static const QSize COVER_CARD_TARGET_SIZE;
    static const QSize COVER_CARD_SMALL_TARGET_SIZE;
    static const QSize COVER_ROW_DETAILED_TARGET_SIZE;
    static const QSize COVER_TARGET_DETAILS_TARGET_SIZE;
    static const QSize CI_TARGET_SIZE;
    static const QSize ACH_ICON_TARGET_SIZE;

    void ProcessNext();
    void ProcessTask(const HydrationTask &task);
    void BenchmarkAchievementIconCdn();
    bool ClearAssetDirectory(const QString &directoryPath, int appId);
    QString TryDownloadFirstWorking(const QList<QString> &urls, const QString &savePath, const QSize &targetSize, const QString &newName = QString());
};
