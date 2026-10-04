/////////////////////////////////////////////////////////
// File: Lymalink.cpp
// Date: 2026-05-14
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Implements Lymalink backend orchestrator
//              which handles api calls, database etc.
/////////////////////////////////////////////////////////

#include "Lymalink.h"
#include "Defines.h"
#include "Settings.h"
#include "api/SteamApi.h"
#include "tools/Utils.h"

#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QImageReader>
#include <QLocale>
#include <QMetaObject>
#include <QPair>
#include <QPainter>
#include <QProcess>
#include <QRegularExpression>
#include <QSet>
#include <QStringList>
#include <QStandardPaths>
#include <QVariantMap>

#include <algorithm>
#include <numeric>

/////////////////////////////////////////////////////////////////////

Lymalink::Lymalink(Settings *settings, QObject *parent) :
    QObject(parent),
    m_databaseManager(settings && !settings->GetDatabaseCustomPath().trimmed().isEmpty() ? &m_databaseUtils : nullptr)
{
    m_settings = settings;
    m_useDefaultDbPath = m_settings == nullptr || m_settings->GetDatabaseCustomPath().trimmed().isEmpty();
    m_useDbWalMode = true;
    m_databaseConnectionName = DATABASE_CONNECTION_NAME;
    m_databasePath = "";
    m_steamApiSearchWorker = nullptr;
    m_steamApiHydrationWorker = nullptr;
    m_steamHydrationBusy = false;
    m_steamImportAutoSyncThread = nullptr;
    m_steamImportAutoSyncBusy = false;
    m_appIdFolderFindThread = nullptr;
    m_appIdFolderFindBusy = false;
}

Lymalink::~Lymalink()
{
    if (m_steamApiHydrationWorker && m_hydrationWorkerThread.isRunning())
    {
        QMetaObject::invokeMethod(m_steamApiHydrationWorker, &SteamApiHydrationWorker::CancelAllEnqueueTasks, Qt::BlockingQueuedConnection);
    }

    if (m_appIdFolderFindThread)
    {
        m_appIdFolderFindThread->wait();
    }

    m_searchWorkerThread.quit();
    m_searchWorkerThread.wait();

    m_hydrationWorkerThread.quit();
    m_hydrationWorkerThread.wait();

    if (m_steamImportAutoSyncThread)
    {
        m_steamImportAutoSyncThread->quit();
        m_steamImportAutoSyncThread->wait();
    }
}

/////////////////////////////////////////////////////////////////////
////////////////////////////// PUBLIC ///////////////////////////////
/////////////////////////////////////////////////////////////////////

Error Lymalink::Initialize()
{
    Error initResult = Error::NoError;

    // Initialize required folders before database and worker setup
    const Error fileSystemError = FileSystemInit();
    if (fileSystemError != Error::NoError)
    {
        initResult = fileSystemError;
        return initResult;
    }

    // Wire database errors to frontend
    connect(&m_databaseManager, &SQLiteManager::signalDatabaseError, this,
        [this](const QString &error) {
            emit signalErrorOccurred(tr("Database Error"), error);
        });

    // Run Steam search API work on dedicated worker thread
    m_steamApiSearchWorker = new SteamApiSearchWorker();
    m_steamApiSearchWorker->moveToThread(&m_searchWorkerThread);
    connect(&m_searchWorkerThread, &QThread::started,  m_steamApiSearchWorker, &SteamApiSearchWorker::Init);
    connect(&m_searchWorkerThread, &QThread::finished, m_steamApiSearchWorker, &QObject::deleteLater);
    connect(this, &Lymalink::signalRequestSearchSteamAppIds,        m_steamApiSearchWorker, &SteamApiSearchWorker::SearchAppIds);
    connect(this, &Lymalink::signalRequestCancelSearchSteamAppIds,  m_steamApiSearchWorker, &SteamApiSearchWorker::CancelSearchAppIds);
    connect(m_steamApiSearchWorker, &SteamApiSearchWorker::signalSearchAppIdsFinished, this, &Lymalink::signalSteamAppIdsSearchReady);
    m_searchWorkerThread.start();

    // Run asset and achievement hydration on dedicated worker thread
    m_steamApiHydrationWorker = new SteamApiHydrationWorker();
    m_steamApiHydrationWorker->moveToThread(&m_hydrationWorkerThread);
    connect(&m_hydrationWorkerThread, &QThread::started,  m_steamApiHydrationWorker, &SteamApiHydrationWorker::Init);
    connect(&m_hydrationWorkerThread, &QThread::finished, m_steamApiHydrationWorker, &QObject::deleteLater);
    connect(this, &Lymalink::signalRequestEnqueueSteamHydrationTask, m_steamApiHydrationWorker, &SteamApiHydrationWorker::EnqueueTask);
    connect(this, &Lymalink::signalRequestContinueSteamHydration,    m_steamApiHydrationWorker, &SteamApiHydrationWorker::ContinueQueue);
    connect(this, &Lymalink::signalRequestCancelSteamHydration,      m_steamApiHydrationWorker, &SteamApiHydrationWorker::CancelAllEnqueueTasks);
    connect(m_steamApiHydrationWorker, &SteamApiHydrationWorker::signalHydrationTaskProgress,  this, &Lymalink::signalSteamHydrationTaskProgress);
    connect(m_steamApiHydrationWorker, &SteamApiHydrationWorker::signalHydrationTaskFinished,  this, &Lymalink::signalSteamHydrationTaskFinished);
    connect(m_steamApiHydrationWorker, &SteamApiHydrationWorker::signalHydrationTaskStarted, this,
        [this](int appId, const QString &targetType) {
            if (!m_steamHydrationBusy)
            {
                m_steamHydrationBusy = true;
                emit signalSteamHydrationBusyChanged();
            }
            emit signalSteamHydrationTaskStarted(appId, targetType);
        });
    connect(m_steamApiHydrationWorker, &SteamApiHydrationWorker::signalHydrationQueueFinished, this,
        [this]() {
            if (m_steamHydrationBusy)
            {
                m_steamHydrationBusy = false;
                emit signalSteamHydrationBusyChanged();
            }
            emit signalSteamHydrationQueueFinished();
        });
    connect(m_steamApiHydrationWorker, &SteamApiHydrationWorker::signalHydrationTaskError, this,
        [this](int appId, const QString &title, const QString &message) {
            emit signalErrorOccurred(title, QString("%1\n\nApp ID: %2").arg(message).arg(appId));
        });
    connect(m_steamApiHydrationWorker, &SteamApiHydrationWorker::signalAchievementsReady, this,
        [this](int appId, QString targetType, QVariantList achievements) {
            const bool success = ApplyNewAchievements(appId, targetType, achievements);
            emit signalAchievementMetadataReady(appId, NormalizeTargetType(targetType), success);
        });
    m_hydrationWorkerThread.start();

    initResult = DatabaseInit(QString(), m_useDefaultDbPath);

    return initResult;
}

/////////////////////////////////////////////////////////////////////

bool Lymalink::GetSteamHydrationBusy() const
{
    return m_steamHydrationBusy;
}

/////////////////////////////////////////////////////////////////////

bool Lymalink::GetSteamImportAutoSyncBusy() const
{
    return m_steamImportAutoSyncBusy;
}

/////////////////////////////////////////////////////////////////////

void Lymalink::SearchSteamAppIds(const QString &term)
{
    emit signalRequestSearchSteamAppIds(term);
}

/////////////////////////////////////////////////////////////////////

void Lymalink::CancelSteamAppIdSearch()
{
    emit signalRequestCancelSearchSteamAppIds();
}

/////////////////////////////////////////////////////////////////////

void Lymalink::EnqueueSteamHydrationTask(int appId, bool reloadAssets, const QString &targetType)
{
    emit signalRequestEnqueueSteamHydrationTask(appId, reloadAssets, NormalizeTargetType(targetType));
}

/////////////////////////////////////////////////////////////////////

void Lymalink::CancelSteamHydration()
{
    emit signalRequestCancelSteamHydration();
}

/////////////////////////////////////////////////////////////////////

void Lymalink::ContinueSteamHydration()
{
    emit signalRequestContinueSteamHydration();
}

/////////////////////////////////////////////////////////////////////

QVariantMap Lymalink::InspectExecutableFolder(const QString &executablePath)
{
    QVariantList steamAppIds;
    QSet<QString> detectedIds;
    bool hasGogConfig = false;

    const QFileInfo executableInfo(executablePath.trimmed());
    const QDir installDir = executableInfo.absoluteDir();
    if (executablePath.trimmed().isEmpty() || !installDir.exists())
    {
        return {{"steamAppIds", steamAppIds}, {"hasGogConfig", hasGogConfig}};
    }

    const auto addSteamAppId = [&detectedIds, &steamAppIds](const QString &rawId) {
        bool ok = false;
        const qlonglong appId = rawId.trimmed().toLongLong(&ok);
        if (!ok || appId <= 0)
        {
            return;
        }

        const QString normalizedId = QString::number(appId);
        if (!detectedIds.contains(normalizedId))
        {
            detectedIds.insert(normalizedId);
            steamAppIds.append(normalizedId);
        }
    };

    const QRegularExpression appIdLineRegex(QStringLiteral("^\\s*AppId\\s*=\\s*(\\d+)\\s*$"), QRegularExpression::MultilineOption);
    const QRegularExpression tenokeIdLineRegex(QStringLiteral("^\\s*id\\s*=\\s*(\\d+)\\s*(?:#.*)?$"), QRegularExpression::MultilineOption);
    const QRegularExpression numericTextRegex(QStringLiteral("^\\d+$"));
    const QFileInfoList entries = installDir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot, QDir::Name);
    for (const QFileInfo &entry : entries)
    {
        const QString fileName = entry.fileName();
        const QString lowerName = fileName.toLower();
        if (lowerName == QStringLiteral("galaxyconfig.json") || lowerName.contains(QStringLiteral("goggame")))
        {
            hasGogConfig = true;
        }

        if (lowerName == QStringLiteral("steam_appid.txt"))
        {
            QFile file(entry.absoluteFilePath());
            if (file.open(QIODevice::ReadOnly | QIODevice::Text))
            {
                const QString text = QString::fromUtf8(file.readAll()).trimmed();
                if (numericTextRegex.match(text).hasMatch())
                {
                    addSteamAppId(text);
                }
            }
        }
        else if (lowerName == QStringLiteral("steam_api.ini") || lowerName == QStringLiteral("steam_emu.ini"))
        {
            QFile file(entry.absoluteFilePath());
            if (file.open(QIODevice::ReadOnly | QIODevice::Text))
            {
                const QString text = QString::fromUtf8(file.readAll());
                const QRegularExpressionMatch match = appIdLineRegex.match(text);
                if (match.hasMatch())
                {
                    addSteamAppId(match.captured(1));
                }
            }
        }
        else if (lowerName == QStringLiteral("tenoke.ini"))
        {
            QFile file(entry.absoluteFilePath());
            if (file.open(QIODevice::ReadOnly | QIODevice::Text))
            {
                const QString text = QString::fromUtf8(file.readAll());
                const QRegularExpressionMatch match = tenokeIdLineRegex.match(text);
                if (match.hasMatch())
                {
                    addSteamAppId(match.captured(1));
                }
            }
        }
    }

    return {{"steamAppIds", steamAppIds}, {"hasGogConfig", hasGogConfig}};
}

/////////////////////////////////////////////////////////////////////

void Lymalink::FindEmulatorAppIdFolders(const QString &rootPath)
{
    if (m_appIdFolderFindBusy)
    {
        qWarning() << "Lymalink::FindEmulatorAppIdFolders: search already running";
        emit signalEmulatorAppIdFolderFindFinished(false, {}, tr("AppId directory search is already running."));
        return;
    }

    m_appIdFolderFindBusy = true;
    qDebug() << "Lymalink::FindEmulatorAppIdFolders: starting search root=" << rootPath;

    m_appIdFolderFindThread = QThread::create([this, rootPath]() {
        bool timedOut = false;
        QString error;
        AppIdDirectoryFinder finder;
        QVariantList results = finder.Search(rootPath, &timedOut, &error);
        const bool success = error.isEmpty() || timedOut;
        if (timedOut && error.isEmpty())
        {
            error = tr("Search stopped after 30 seconds. Displaying partial results.");
        }

        // Enrich discovered AppIds with names when available
        if (!results.isEmpty())
        {
            QList<int> appIds = {};
            QSet<int> seenAppIds = {};
            for (const QVariant &resultValue : results)
            {
                const QVariantMap result = resultValue.toMap();
                bool appIdOk = false;
                const int appId = result.value(QStringLiteral("appId")).toString().toInt(&appIdOk);
                if (appIdOk && appId > 0 && !seenAppIds.contains(appId))
                {
                    seenAppIds.insert(appId);
                    appIds.append(appId);
                }
            }

            QMap<int, QString> gameNames = {};
            SteamApi steamApi;
            const Error namesError = steamApi.FetchAppNames(appIds, gameNames);
            if (namesError == Error::NoError)
            {
                for (QVariant &resultValue : results)
                {
                    QVariantMap result = resultValue.toMap();
                    const int appId = result.value(QStringLiteral("appId")).toString().toInt();
                    if (gameNames.contains(appId))
                    {
                        result[QStringLiteral("gameName")] = gameNames.value(appId);
                        resultValue = result;
                    }
                }
            }
            else
            {
                qWarning() << "Lymalink::FindEmulatorAppIdFolders: Names unavailable, keeping APPID-only results. error=" << static_cast<int>(namesError);
            }
        }

        // Sort by resolved game name when possible, then by AppId
        std::sort(results.begin(), results.end(), Utils::CreateVariantMapComparator(QStringLiteral("gameName"), QStringLiteral("appId"), QStringLiteral("path")));

        QMetaObject::invokeMethod(this, [this, success, results, error]() mutable {
            // Mark already-created emulator targets after returning to database thread
            for (QVariant &resultValue : results)
            {
                QVariantMap result = resultValue.toMap();
                const int appId = result.value(QStringLiteral("appId")).toString().toInt();
                const int existingRows = m_databaseManager.count(m_databaseConnectionName, DATABASE_TABLE_EMU_GAMES, "id = ?", {appId});
                result[QStringLiteral("targetExists")] = existingRows > 0;
                resultValue = result;
            }

            qDebug() << "Lymalink::FindEmulatorAppIdFolders: finished count=" << results.size() << "error=" << error;
            m_appIdFolderFindBusy = false;
            if (m_appIdFolderFindThread)
            {
                m_appIdFolderFindThread->deleteLater();
                m_appIdFolderFindThread = nullptr;
            }
            emit signalEmulatorAppIdFolderFindFinished(success, results, error);
        }, Qt::QueuedConnection);
    });

    m_appIdFolderFindThread->start();
}

/////////////////////////////////////////////////////////////////////

bool Lymalink::CreateNewSteamEmuTarget(int appId, QString gameName, QString exePath, QString locationPath, QString installationDir, bool customAchievementLocation, QString emulatorType)
{
    bool targetCreated = false;
    m_lastOperationError.clear();

    // Normalize user-provided paths before validation/database write
    gameName = gameName.trimmed();
    exePath = exePath.trimmed();
    locationPath = locationPath.trimmed();

    // Keep only optional fields used by selected achievement source mode
    if (customAchievementLocation)
    {
        // Custom fields
        installationDir.clear();
        emulatorType = emulatorType.trimmed();
    }
    else
    {
        // Default fields
        emulatorType.clear();
        installationDir = installationDir.trimmed();
    }

#if defined(Q_OS_WIN)
    const bool automaticAchLocationInvalid = false;
#else
    const bool automaticAchLocationInvalid = locationPath.isEmpty();
#endif
    const bool customAchLocationInvalid = emulatorType.isEmpty() || locationPath.isEmpty();
    if (appId <= 0 || gameName.isEmpty() || exePath.isEmpty() || (customAchievementLocation ? customAchLocationInvalid : automaticAchLocationInvalid))
    {
        m_lastOperationError = tr("Invalid emulator target data.");
        qWarning() << "Lymalink::CreateNewSteamEmuTarget: invalid emulator target data";
        return targetCreated;
    }

    const QString appDataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (appDataPath.isEmpty())
    {
        m_lastOperationError = tr("Couldn't resolve app data location.");
        qCritical() << "Lymalink::CreateNewSteamEmuTarget: failed to resolve app data location for emulator target";
        return targetCreated;
    }

    // Open database lazily for QML calls made after startup
    if (!m_databaseManager.isDatabaseOpen(m_databaseConnectionName) && !m_databaseManager.openDatabase(m_databaseConnectionName, m_databasePath, m_useDefaultDbPath, m_useDbWalMode))
    {
        m_lastOperationError = tr("Couldn't open target database.");
        qCritical() << "Lymalink::CreateNewSteamEmuTarget: failed to open database for emulator target:" << m_databaseManager.lastError();
        return targetCreated;
    }

    const QString appIdText = QString::number(appId);
    QDir emulatorDir(QDir(appDataPath).filePath("Emulator"));
    const QString targetPath = emulatorDir.filePath(appIdText);

    // Check database entry to avoid duplicate target rows
    const int existingGameRows = m_databaseManager.count(m_databaseConnectionName, DATABASE_TABLE_EMU_GAMES, "id = ?", {appId});
    if (existingGameRows < 0)
    {
        m_lastOperationError = tr("Couldn't check existing targets.");
        qCritical() << "Lymalink::CreateNewSteamEmuTarget: failed to check emulator target database row:" << m_databaseManager.lastError();
        return targetCreated;
    }

    if (existingGameRows > 0)
    {
        m_lastOperationError = tr("Emulator target with ID %1 already exists.").arg(appId);
        qWarning() << "Lymalink::CreateNewSteamEmuTarget: emulator target already exists in database, skipping creation:" << appId;
        return targetCreated;
    }

    bool executableQuerySucceeded = false;
    if (IsTargetExecutableLocationInUse(exePath, 0, &executableQuerySucceeded))
    {
        m_lastOperationError = tr("Game executable is already set for another target.");
        qWarning() << "Lymalink::CreateNewSteamEmuTarget: executable location already in use, skipping creation:" << exePath;
        return targetCreated;
    }
    if (!executableQuerySucceeded)
    {
        m_lastOperationError = tr("Couldn't check existing target executables.");
        qCritical() << "Lymalink::CreateNewSteamEmuTarget: failed to check executable location:" << m_databaseManager.lastError();
        return targetCreated;
    }

    // If DB row missing but stale folder exists, remove old folder and recreate target cleanly
    if (QFileInfo::exists(targetPath))
    {
        if (!m_fileManager.DeleteFolder(targetPath))
        {
            m_lastOperationError = tr("Couldn't remove stale target folder.");
            qCritical() << "Lymalink::CreateNewSteamEmuTarget: stale emulator target folder exists and couldn't be removed:" << targetPath;
            return targetCreated;
        }
    }

    // Create per-target asset folders before database insert
    if (!emulatorDir.mkpath(appIdText) || !emulatorDir.mkpath(appIdText + "/icons") || !emulatorDir.mkpath(appIdText + "/covers"))
    {
        m_lastOperationError = tr("Couldn't create target folders.");
        qCritical() << "Lymalink::CreateNewSteamEmuTarget: failed to create emulator target folders:" << targetPath;
        return targetCreated;
    }

    // Insert target metadata after filesystem is ready
    const QVariantMap data = {
        {"id", appId},
        {"game_name", gameName},
        {"executable_location", exePath},
        {"prefix_location", customAchievementLocation ? "" : locationPath},
        {"installation_dir", installationDir},
        {"custom_achievement_location", customAchievementLocation ? 1 : 0},
        {"emulator_type", emulatorType},
        {"appid_dir_location", customAchievementLocation ? locationPath : ""},
        {"date_added", QDateTime::currentSecsSinceEpoch()}
    };

    if (!m_databaseManager.insert(m_databaseConnectionName, DATABASE_TABLE_EMU_GAMES, data))
    {
        // Roll back folder creation when database insert fails
        QDir cleanupDir(targetPath);
        cleanupDir.removeRecursively();
        m_lastOperationError = tr("Couldn't save target.");
        qCritical() << "Lymalink::CreateNewSteamEmuTarget: failed to insert emulator target:" << m_databaseManager.lastError();
        return targetCreated;
    }

    qDebug() << "Lymalink::CreateNewSteamEmuTarget: emulator target created:" << appId << targetPath;
    targetCreated = true;
    return targetCreated;
}

/////////////////////////////////////////////////////////////////////

QVariantMap Lymalink::ImportSteamGames(QVariantList games, const QString &steamId, const QString &apiKey)
{
    QVariantMap payload = {
        {"success", false},
        {"importedCount", 0},
        {"skippedCount", 0},
        {"errors", QVariantList()},
        {"importedAppIds", QVariantList()}
    };

    if (games.isEmpty())
    {
        m_lastOperationError = tr("No Steam games selected for import.");
        payload["errors"] = QVariantList{m_lastOperationError};
        return payload;
    }

    if (!m_databaseManager.isDatabaseOpen(m_databaseConnectionName) && !m_databaseManager.openDatabase(m_databaseConnectionName, m_databasePath, m_useDefaultDbPath, m_useDbWalMode))
    {
        m_lastOperationError = tr("Couldn't open target database.");
        payload["errors"] = QVariantList{m_lastOperationError};
        return payload;
    }

    SteamApi steamApi;
    QVariantList errors;
    QVariantList importedAppIds;
    int importedCount = 0;
    int skippedCount = 0;
    bool profilePrivacyErrorReported = false;
    bool cancelRemainingFetches = false;
    const qint64 now = QDateTime::currentSecsSinceEpoch();

    // Iterate through each game in the provided list
    for (const QVariant &gameValue : games)
    {
        if (cancelRemainingFetches)
        {
            break;
        }

        // Extract game metadata from the input map
        const QVariantMap game = gameValue.toMap();
        const int appId = Utils::MapIntValue(game, "appId");
        const QString gameName = Utils::MapStringValue(game, "name").trimmed();
        const qint64 totalSecondsPlayed = game.value("playtimeSeconds").toLongLong();
        const qint64 lastPlayedDate = game.value("lastPlayedTimestamp").toLongLong();
        const QString errorPrefix = gameName.isEmpty()
            ? tr("App ID %1").arg(appId)
            : QString("%1 (%2)").arg(gameName).arg(appId);

        // Skip entries with invalid or missing core data
        if (appId <= 0 || gameName.isEmpty())
        {
            ++skippedCount;
            errors << tr("%1: invalid game data.").arg(errorPrefix);
            continue;
        }

        // Check if the game already exists in the database
        const int existingSteamRows = m_databaseManager.count(m_databaseConnectionName, DATABASE_TABLE_GAMES, "id = ?", {appId});
        if (existingSteamRows < 0)
        {
            ++skippedCount;
            errors << tr("%1: couldn't check existing Steam import.").arg(errorPrefix);
            continue;
        }
        if (existingSteamRows > 0)
        {
            // Game already exists, skip to avoid duplicates
            ++skippedCount;
            continue;
        }

        QList<SteamAchievementData> publicAchievements;
        Error publicError = steamApi.FetchAchievementDataPrimary(appId, publicAchievements);
        if (publicError != Error::NoError && publicError != Error::NoData)
        {
            publicAchievements.clear();
            publicError = steamApi.FetchAchievementDataSecondary(appId, publicAchievements, SteamApi::English, apiKey);
        }

        if (publicError != Error::NoError && publicError != Error::NoData)
        {
            ++skippedCount;
            errors << tr("%1: couldn't fetch achievement metadata.").arg(errorPrefix);
            continue;
        }

        // Fetch player-specific achievement progress
        QList<SteamPlayerAchievementData> playerAchievements;
        const bool hasPublicAchievements = publicError == Error::NoError && !publicAchievements.isEmpty();
        const Error playerError = hasPublicAchievements
            ? steamApi.FetchPlayerAchievements(appId, steamId, playerAchievements, apiKey)
            : Error::NoData;
        
        // Check for errors
        if (hasPublicAchievements && playerError != Error::NoError && playerError != Error::NoData)
        {
            ++skippedCount;
            if (playerError == Error::ProfileNotPublic)
            {
                const QString errorText = tr("%1: Steam profile is not public. Make your Steam profile and game details public, then retry.").arg(errorPrefix);
                errors << errorText;
                if (!profilePrivacyErrorReported)
                {
                    profilePrivacyErrorReported = true;
                    emit signalErrorOccurred(
                        tr("Steam Profile Is Private"),
                        tr("Steam rejected player achievement data request because your profile is not public. Make your Steam profile and game details public, then retry.")
                    );
                }
                cancelRemainingFetches = true;
            }
            else
            {
                errors << tr("%1: couldn't fetch player achievements.").arg(errorPrefix);
            }
            continue;
        }
        if (hasPublicAchievements && playerError == Error::NoData)
        {
            errors << tr("%1: player achievement unlock data is unavailable; imported achievements as locked.").arg(errorPrefix);
        }

        // Map player achievements by key for quick lookup during iteration
        QMap<QString, SteamPlayerAchievementData> playerByKey;
        for (const SteamPlayerAchievementData &achievement : playerAchievements)
        {
            if (!achievement.achievementKey.isEmpty())
            {
                playerByKey.insert(achievement.achievementKey, achievement);
            }
        }

        // Prepare rows for database insertion
        QVariantList achievementRows;
        int unlockedCount = 0;
        for (const SteamAchievementData &achievement : publicAchievements)
        {
            if (achievement.achievementKey.isEmpty())
            {
                continue;
            }

            const SteamPlayerAchievementData playerAchievement = playerByKey.value(achievement.achievementKey);
            const qint64 dateUnlocked = playerAchievement.dateUnlocked;
            if (dateUnlocked > 0)
            {
                ++unlockedCount;
            }

            // Construct achievement row with fallback names/descriptions to player data if public is missing
            QVariantMap achievementRow = {
                {"id", appId},
                {"achievement_key", achievement.achievementKey},
                {"achievement_name", achievement.achievementName.isEmpty() ? playerAchievement.achievementName : achievement.achievementName},
                {"achievement_description", achievement.achievementDescription.isEmpty() ? playerAchievement.achievementDescription : achievement.achievementDescription},
                {"achievement_hidden", achievement.achievementHidden ? 1 : 0},
                {"global_unlock_percentage", achievement.globalUnlockPercentage},
                {"date_unlocked", dateUnlocked},
                {"date_updated", now},
                {"date_added", now}
            };
            achievementRows << achievementRow;
        }

        // Begin a database transaction to ensure atomicity (game + achievements insert together)
        if (!m_databaseManager.beginTransaction(m_databaseConnectionName))
        {
            ++skippedCount;
            errors << tr("%1: couldn't start database transaction.").arg(errorPrefix);
            continue;
        }

        // Construct game row with computed achievement statistics
        const QVariantMap gameRow = {
            {"id", appId},
            {"game_name", gameName},
            {"target_hidden", 0},
            {"total_amount_achievements", achievementRows.size()},
            {"total_unlocked_amount_achievements", unlockedCount},
            {"total_seconds_played", totalSecondsPlayed},
            {"last_played_date", lastPlayedDate},
            {"date_updated", now},
            {"date_added", now}
        };

        // Insert game row and then all associated achievement rows
        bool imported = m_databaseManager.insert(m_databaseConnectionName, DATABASE_TABLE_GAMES, gameRow);
        if (imported)
        {
            for (const QVariant &achievementValue : achievementRows)
            {
                if (!m_databaseManager.insert(m_databaseConnectionName, DATABASE_TABLE_ACHIEVEMENTS, achievementValue.toMap()))
                {
                    imported = false;
                    break;
                }
            }
        }

        // Commit transaction on success, otherwise rollback to maintain database integrity
        if (imported && m_databaseManager.commitTransaction(m_databaseConnectionName))
        {
            ++importedCount;
            importedAppIds << appId;
        }
        else
        {
            m_databaseManager.rollbackTransaction(m_databaseConnectionName);
            ++skippedCount;
            errors << tr("%1: couldn't write Steam import to database.").arg(errorPrefix);
        }
    }

    // Finalize the result payload with operation statistics
    payload["success"] = importedCount > 0;
    payload["importedCount"] = importedCount;
    payload["skippedCount"] = skippedCount;
    payload["errors"] = errors;
    payload["importedAppIds"] = importedAppIds;
    m_lastOperationError = errors.isEmpty() ? QString() : errors.first().toString();
    return payload;
}

/////////////////////////////////////////////////////////////////////

QVariantMap Lymalink::UpdateSteamImports(QVariantList games, const QString &steamId, const QString &apiKey)
{
    QVariantMap payload = {
        {"success", false},
        {"updatedCount", 0},
        {"skippedCount", 0},
        {"assetRefreshAppIds", QVariantList()},
        {"errors", QVariantList()}
    };

    if (games.isEmpty())
    {
        m_lastOperationError = tr("No imported Steam games found to update.");
        payload["errors"] = QVariantList{m_lastOperationError};
        return payload;
    }

    if (!m_databaseManager.isDatabaseOpen(m_databaseConnectionName) && !m_databaseManager.openDatabase(m_databaseConnectionName, m_databasePath, m_useDefaultDbPath, m_useDbWalMode))
    {
        m_lastOperationError = tr("Couldn't open target database.");
        payload["errors"] = QVariantList{m_lastOperationError};
        return payload;
    }

    SteamApi steamApi;
    QVariantList errors;
    QVariantList assetRefreshAppIds;
    int updatedCount = 0;
    int skippedCount = 0;
    bool profilePrivacyErrorReported = false;
    bool cancelRemainingFetches = false;

    // Iterate through each game provided in the import list
    for (const QVariant &gameValue : games)
    {
        if (cancelRemainingFetches)
        {
            break;
        }

        // Extract and validate core game metadata
        const QVariantMap game = gameValue.toMap();
        const int appId = Utils::MapIntValue(game, "appId");
        const QString gameName = Utils::MapStringValue(game, "name").trimmed();
        const qint64 totalSecondsPlayed = game.value("playtimeSeconds").toLongLong();
        const qint64 lastPlayedDate = game.value("lastPlayedTimestamp").toLongLong();
        const QString errorPrefix = gameName.isEmpty() ? tr("App ID %1").arg(appId) : QString("%1 (%2)").arg(gameName).arg(appId);

        if (appId <= 0 || gameName.isEmpty())
        {
            ++skippedCount;
            errors << tr("%1: invalid game data.").arg(errorPrefix);
            continue;
        }

        // Skip if the game doesn't already exist in the local database
        const QVariantMap existingGame = m_databaseManager.selectFirst(m_databaseConnectionName, DATABASE_TABLE_GAMES, "id = ?", {appId});
        if (existingGame.isEmpty())
        {
            ++skippedCount;
            continue;
        }

        // Fetch existing achievement keys for this game to track structure changes
        const QVariantList existingAchievementRows = m_databaseManager.selectWhere(
            m_databaseConnectionName,
            DATABASE_TABLE_ACHIEVEMENTS,
            "id = ?",
            {appId},
            QStringList{"achievement_key"}
        );

        QSet<QString> existingKeys;
        for (const QVariant &rowValue : existingAchievementRows)
        {
            const QString achievementKey = rowValue.toMap().value("achievement_key").toString();
            if (!achievementKey.isEmpty())
            {
                existingKeys.insert(achievementKey);
            }
        }

        // Fetch public achievement metadata from API
        // Attempts primary endpoint first, falls back to secondary with API key on failure
        QList<SteamAchievementData> publicAchievements;
        Error publicError = steamApi.FetchAchievementDataPrimary(appId, publicAchievements);
        if (publicError != Error::NoError && publicError != Error::NoData)
        {
            publicAchievements.clear();
            publicError = steamApi.FetchAchievementDataSecondary(appId, publicAchievements, SteamApi::English, apiKey);
        }

        // Skip if public metadata fetch fails unexpectedly
        if (publicError != Error::NoError && publicError != Error::NoData)
        {
            ++skippedCount;
            errors << tr("%1: couldn't fetch achievement metadata.").arg(errorPrefix);
            continue;
        }

        // Determine if player-specific achievement data is needed
        QList<SteamPlayerAchievementData> playerAchievements;
        const bool hasPublicAchievements = publicError == Error::NoError && !publicAchievements.isEmpty();
        const bool hasExistingAchievements = !existingKeys.isEmpty();
        const bool shouldFetchPlayerAchievements = hasPublicAchievements || hasExistingAchievements;
        const Error playerError = shouldFetchPlayerAchievements
            ? steamApi.FetchPlayerAchievements(appId, steamId, playerAchievements, apiKey)
            : Error::NoData;
        const bool hasPlayerAchievements = playerError == Error::NoError;

        // Handle player data fetch errors, with special handling for private profiles
        if (shouldFetchPlayerAchievements && playerError != Error::NoError && playerError != Error::NoData)
        {
            ++skippedCount;
            if (playerError == Error::ProfileNotPublic)
            {
                const QString errorText = tr("%1: Steam profile is not public. Make your Steam profile and game details public, then retry.").arg(errorPrefix);
                errors << errorText;
                if (!profilePrivacyErrorReported)
                {
                    profilePrivacyErrorReported = true;
                    emit signalErrorOccurred(
                        tr("Steam Profile Is Private"),
                        tr("Steam rejected player achievement data request because your profile is not public. Make your Steam profile and game details public, then retry.")
                    );
                }
                cancelRemainingFetches = true;
            }
            else
            {
                errors << tr("%1: couldn't fetch player achievements.").arg(errorPrefix);
            }
            continue;
        }

        // Map player achievements by key and count total unlocked
        QMap<QString, SteamPlayerAchievementData> playerByKey;
        int playerUnlockedCount = 0;
        for (const SteamPlayerAchievementData &achievement : playerAchievements)
        {
            if (!achievement.achievementKey.isEmpty())
            {
                playerByKey.insert(achievement.achievementKey, achievement);
                if (achievement.dateUnlocked > 0)
                {
                    ++playerUnlockedCount;
                }
            }
        }

        // Calculate unlocked count and track which achievements exist in public data
        QSet<QString> fetchedKeys;
        int unlockedCount = 0;
        for (const SteamAchievementData &achievement : publicAchievements)
        {
            if (achievement.achievementKey.isEmpty())
            {
                continue;
            }

            // Check if the player has unlocked this specific achievement
            fetchedKeys.insert(achievement.achievementKey);
            if (playerByKey.value(achievement.achievementKey).dateUnlocked > 0)
            {
                ++unlockedCount;
            }
        }

        // Detect if achievements have been added or removed since last sync
        const bool achievementStructureChanged = hasPublicAchievements && existingKeys != fetchedKeys;
        const qint64 now = QDateTime::currentSecsSinceEpoch();

        if (!m_databaseManager.beginTransaction(m_databaseConnectionName))
        {
            ++skippedCount;
            errors << tr("%1: couldn't start database transaction.").arg(errorPrefix);
            continue;
        }

        // Prepare updated game record with latest metadata & playtime stats
        QVariantMap gameRow = {
            {"game_name", gameName},
            {"total_seconds_played", totalSecondsPlayed},
            {"last_played_date", lastPlayedDate},
            {"date_updated", now}
        };
        if (hasPublicAchievements)
        {
            gameRow["total_amount_achievements"] = fetchedKeys.size();
            if (hasPlayerAchievements)
            {
                gameRow["total_unlocked_amount_achievements"] = unlockedCount;
            }
        }
        else if (hasPlayerAchievements)
        {
            // Fallback: use player data counts when public metadata is unavailable
            gameRow["total_unlocked_amount_achievements"] = playerUnlockedCount;
        }

        // Apply game record update
        bool updated = m_databaseManager.update(
            m_databaseConnectionName,
            DATABASE_TABLE_GAMES,
            gameRow,
            "id = ?",
            {appId}
        );

        // Remove achievements that no longer exist in the public dataset
        if (updated && hasPublicAchievements)
        {
            const QSet<QString> removedKeys = existingKeys - fetchedKeys;
            for (const QString &achievementKey : removedKeys)
            {
                if (!m_databaseManager.remove(m_databaseConnectionName, DATABASE_TABLE_ACHIEVEMENTS, "id = ? AND achievement_key = ?", {appId, achievementKey}))
                {
                    updated = false;
                    break;
                }
            }
        }

        // Insert or update achievement rows based on fetched public data
        if (updated && hasPublicAchievements)
        {
            for (const SteamAchievementData &achievement : publicAchievements)
            {
                if (achievement.achievementKey.isEmpty())
                {
                    continue;
                }

                const SteamPlayerAchievementData playerAchievement = playerByKey.value(achievement.achievementKey);
                QVariantMap achievementRow = {
                    {"achievement_name", achievement.achievementName.isEmpty() ? playerAchievement.achievementName : achievement.achievementName},
                    {"achievement_description", achievement.achievementDescription.isEmpty() ? playerAchievement.achievementDescription : achievement.achievementDescription},
                    {"achievement_hidden", achievement.achievementHidden ? 1 : 0},
                    {"global_unlock_percentage", achievement.globalUnlockPercentage},
                    {"date_updated", now}
                };
                if (hasPlayerAchievements)
                {
                    achievementRow["date_unlocked"] = playerAchievement.dateUnlocked;
                }

                // Update existing achievement record
                if (existingKeys.contains(achievement.achievementKey))
                {
                    if (!m_databaseManager.update(
                        m_databaseConnectionName,
                        DATABASE_TABLE_ACHIEVEMENTS,
                        achievementRow,
                        "id = ? AND achievement_key = ?",
                        {appId, achievement.achievementKey}))
                    {
                        updated = false;
                        break;
                    }
                }
                else
                {
                    // Insert new achievement record
                    achievementRow["id"] = appId;
                    achievementRow["achievement_key"] = achievement.achievementKey;
                    achievementRow["date_added"] = now;
                    if (!m_databaseManager.insert(m_databaseConnectionName, DATABASE_TABLE_ACHIEVEMENTS, achievementRow))
                    {
                        updated = false;
                        break;
                    }
                }
            }
        }

        // Update existing achievements with player unlock dates when public data is unavailable
        if (updated && !hasPublicAchievements && playerError == Error::NoError)
        {
            for (const QString &achievementKey : existingKeys)
            {
                if (!playerByKey.contains(achievementKey))
                {
                    continue;
                }

                const SteamPlayerAchievementData playerAchievement = playerByKey.value(achievementKey);
                if (!m_databaseManager.update(
                    m_databaseConnectionName,
                    DATABASE_TABLE_ACHIEVEMENTS,
                    {{"date_unlocked", playerAchievement.dateUnlocked}, {"date_updated", now}},
                    "id = ? AND achievement_key = ?",
                    {appId, achievementKey}))
                {
                    updated = false;
                    break;
                }
            }
        }

        // Commit on success, rollback on failure
        if (updated && m_databaseManager.commitTransaction(m_databaseConnectionName))
        {
            ++updatedCount;
            // Track apps that need external asset refresh due to structure changes
            if (achievementStructureChanged)
            {
                assetRefreshAppIds << appId;
            }
        }
        else
        {
            m_databaseManager.rollbackTransaction(m_databaseConnectionName);
            ++skippedCount;
            errors << tr("%1: couldn't write Steam update to database.").arg(errorPrefix);
        }
    }

    // Compile final results and update global error state
    payload["success"] = updatedCount > 0;
    payload["updatedCount"] = updatedCount;
    payload["skippedCount"] = skippedCount;
    payload["assetRefreshAppIds"] = assetRefreshAppIds;
    payload["errors"] = errors;
    m_lastOperationError = errors.isEmpty() ? QString() : errors.first().toString();
    return payload;
}

/////////////////////////////////////////////////////////////////////

void Lymalink::StartSteamImportAutoSync(const QString &steamId, const QString &apiKey, qint64 expiresAt)
{
    if (m_steamImportAutoSyncBusy)
    {
        qWarning() << "Lymalink::StartSteamImportAutoSync: Steam progress sync already running";
        return;
    }


    if (expiresAt <= QDateTime::currentSecsSinceEpoch())
    {
        emit signalErrorOccurred(tr("Steam progress sync failed"), tr("Automatic Steam progress sync activation expired. Re-enable it in Settings."));
        emit signalSteamImportAutoSyncFinished({
            {"success", false},
            {"errors", QVariantList{tr("Automatic Steam progress sync activation expired.")}},
            {"assetRefreshAppIds", QVariantList()}
        });
        return;
    }

    if (m_databasePath.trimmed().isEmpty())
    {
        emit signalErrorOccurred(tr("Steam progress sync failed"), tr("Target database is unavailable."));
        emit signalSteamImportAutoSyncFinished({
            {"success", false},
            {"errors", QVariantList{tr("Target database is unavailable.")}},
            {"assetRefreshAppIds", QVariantList()}
        });
        return;
    }

    m_steamImportAutoSyncBusy = true;
    emit signalSteamImportAutoSyncBusyChanged();

    QThread *workerThread = new QThread(this);
    SteamImportAutoSyncWorker *worker = new SteamImportAutoSyncWorker();
    worker->moveToThread(workerThread);
    m_steamImportAutoSyncThread = workerThread;
    const bool useCustomDatabasePath = m_settings && !m_settings->GetDatabaseCustomPath().trimmed().isEmpty();

    connect(workerThread, &QThread::started, worker,
        [worker, databasePath = m_databasePath, steamId, apiKey, useCustomDatabasePath]() {
            worker->Run(databasePath, steamId, apiKey, useCustomDatabasePath);
        });
    connect(worker, &SteamImportAutoSyncWorker::signalError, this,
        [this](const QString &title, const QString &message) {
            emit signalErrorOccurred(title, message);
        });
    connect(worker, &SteamImportAutoSyncWorker::signalFinished, this,
        [this, workerThread](QVariantMap payload) {
            emit signalSteamImportAutoSyncFinished(payload);
            if (m_steamImportAutoSyncBusy)
            {
                m_steamImportAutoSyncBusy = false;
                emit signalSteamImportAutoSyncBusyChanged();
            }
            workerThread->quit();
        });
    connect(workerThread, &QThread::finished, worker, &QObject::deleteLater);
    connect(workerThread, &QThread::finished, this,
        [this, workerThread]() {
            if (m_steamImportAutoSyncThread == workerThread)
            {
                m_steamImportAutoSyncThread = nullptr;
            }
            workerThread->deleteLater();
        });

    workerThread->start();
}

/////////////////////////////////////////////////////////////////////

bool Lymalink::SetTargetHidden(int appId, bool hidden, const QString &targetType)
{
    bool targetUpdated = false;
    const QString normalizedTargetType = NormalizeTargetType(targetType);
    const QString gameTable = GameTableForTargetType(normalizedTargetType);

    if (appId <= 0)
    {
        qWarning() << "Lymalink::SetTargetHidden: invalid appId for target hidden update:" << appId;
        return targetUpdated;
    }

    // Ensure database is open before toggling visibility
    if (!m_databaseManager.isDatabaseOpen(m_databaseConnectionName) && !m_databaseManager.openDatabase(m_databaseConnectionName, m_databasePath, m_useDefaultDbPath, m_useDbWalMode))
    {
        qCritical() << "Lymalink::SetTargetHidden: failed to open database for target hidden update:" << m_databaseManager.lastError();
        return targetUpdated;
    }

    targetUpdated = m_databaseManager.update(
        m_databaseConnectionName,
        gameTable,
        {{"target_hidden", hidden ? 1 : 0}},
        "id = ?",
        {appId}
    );

    if (!targetUpdated)
    {
        qCritical() << "Lymalink::SetTargetHidden: failed to update target hidden state:" << m_databaseManager.lastError();
    }

    return targetUpdated;
}

/////////////////////////////////////////////////////////////////////

bool Lymalink::SetAllTargetsHidden(bool hidden, const QString &targetType)
{
    bool targetsUpdated = false;
    const QString normalizedTargetType = NormalizeTargetType(targetType);
    const QString gameTable = GameTableForTargetType(normalizedTargetType);

    if (!m_databaseManager.isDatabaseOpen(m_databaseConnectionName) && !m_databaseManager.openDatabase(m_databaseConnectionName, m_databasePath, m_useDefaultDbPath, m_useDbWalMode))
    {
        qCritical() << "Lymalink::SetAllTargetsHidden: failed to open database for target hidden update:" << m_databaseManager.lastError();
        return targetsUpdated;
    }

    targetsUpdated = m_databaseManager.update(
        m_databaseConnectionName,
        gameTable,
        {{"target_hidden", hidden ? 1 : 0}},
        "target_hidden = ?",
        {hidden ? 0 : 1}
    );

    if (!targetsUpdated)
    {
        qCritical() << "Lymalink::SetAllTargetsHidden: failed to update target hidden states:" << m_databaseManager.lastError();
    }

    return targetsUpdated;
}

/////////////////////////////////////////////////////////////////////

bool Lymalink::SetTargetPrefixLocation(int appId, const QString &prefixPath)
{
    bool targetUpdated = false;
    m_lastOperationError.clear();

    // Prefix changes invalidate discovered Steam appid directory cache
    const QString trimmedPrefixPath = prefixPath.trimmed();
#if defined(Q_OS_WIN)
    if (appId <= 0)
#else
    if (appId <= 0 || trimmedPrefixPath.isEmpty())
#endif
    {
        qWarning() << "Lymalink::SetTargetPrefixLocation: invalid target prefix location update:" << appId << trimmedPrefixPath;
        return targetUpdated;
    }

    if (!m_databaseManager.isDatabaseOpen(m_databaseConnectionName) && !m_databaseManager.openDatabase(m_databaseConnectionName, m_databasePath, m_useDefaultDbPath, m_useDbWalMode))
    {
        m_lastOperationError = tr("Couldn't open target database.");
        qCritical() << "Lymalink::SetTargetPrefixLocation: failed to open database for target prefix location update:" << m_databaseManager.lastError();
        return targetUpdated;
    }

    // Custom targets must keep their configured achievement path and parser
    const QVariantMap target = m_databaseManager.selectFirst(m_databaseConnectionName, DATABASE_TABLE_EMU_GAMES, "id = ?", {appId});
    if (target.isEmpty() || target.value("custom_achievement_location").toInt() == 1)
    {
        m_lastOperationError = tr("Prefix location can only be edited for automatic detection.");
        qWarning() << "Lymalink::SetTargetPrefixLocation: custom or missing target rejected:" << appId;
        return targetUpdated;
    }

    targetUpdated = m_databaseManager.update(
        m_databaseConnectionName,
        DATABASE_TABLE_EMU_GAMES,
        {
            {"prefix_location", trimmedPrefixPath},
            {"appid_dir_found", 0},
            {"achievement_data_status", 0},
            {"appid_dir_location", ""},
            {"emulator_type", ""},
            {"date_updated", QDateTime::currentSecsSinceEpoch()}
        },
        "id = ?",
        {appId}
    );

    if (!targetUpdated)
    {
        m_lastOperationError = tr("Couldn't save target prefix location.");
        qCritical() << "Lymalink::SetTargetPrefixLocation: failed to update target prefix location:" << m_databaseManager.lastError();
    }

    return targetUpdated;
}

/////////////////////////////////////////////////////////////////////

bool Lymalink::SetTargetExecutableLocation(int appId, const QString &executablePath)
{
    bool targetUpdated = false;
    m_lastOperationError.clear();

    // Store trimmed executable path and update modification timestamp
    const QString trimmedExecutablePath = executablePath.trimmed();
    if (appId <= 0 || trimmedExecutablePath.isEmpty())
    {
        m_lastOperationError = tr("Invalid target executable location.");
        qWarning() << "Lymalink::SetTargetExecutableLocation: invalid target executable location update:" << appId << trimmedExecutablePath;
        return targetUpdated;
    }

    if (!m_databaseManager.isDatabaseOpen(m_databaseConnectionName) && !m_databaseManager.openDatabase(m_databaseConnectionName, m_databasePath, m_useDefaultDbPath, m_useDbWalMode))
    {
        m_lastOperationError = tr("Couldn't open target database.");
        qCritical() << "Lymalink::SetTargetExecutableLocation: failed to open database for target executable location update:" << m_databaseManager.lastError();
        return targetUpdated;
    }

    bool executableQuerySucceeded = false;
    if (IsTargetExecutableLocationInUse(trimmedExecutablePath, appId, &executableQuerySucceeded))
    {
        m_lastOperationError = tr("Game executable is already set for another target.");
        qWarning() << "Lymalink::SetTargetExecutableLocation: executable location already in use, skipping update:" << trimmedExecutablePath;
        return targetUpdated;
    }
    if (!executableQuerySucceeded)
    {
        m_lastOperationError = tr("Couldn't check existing target executables.");
        qCritical() << "Lymalink::SetTargetExecutableLocation: failed to check executable location:" << m_databaseManager.lastError();
        return targetUpdated;
    }

    targetUpdated = m_databaseManager.update(
        m_databaseConnectionName,
        DATABASE_TABLE_EMU_GAMES,
        {
            {"executable_location", trimmedExecutablePath},
            {"date_updated", QDateTime::currentSecsSinceEpoch()}
        },
        "id = ?",
        {appId}
    );

    if (!targetUpdated)
    {
        m_lastOperationError = tr("Couldn't save target executable location.");
        qCritical() << "Lymalink::SetTargetExecutableLocation: failed to update target executable location:" << m_databaseManager.lastError();
    }

    return targetUpdated;
}

/////////////////////////////////////////////////////////////////////

bool Lymalink::SetTargetInstallationLocation(int appId, const QString &installationDir)
{
    bool targetUpdated = false;
    m_lastOperationError.clear();

    const QString trimmedInstallationDir = installationDir.trimmed();
    if (appId <= 0)
    {
        m_lastOperationError = tr("Invalid target installation directory.");
        qWarning() << "Lymalink::SetTargetInstallationLocation: invalid target installation directory update:" << appId << trimmedInstallationDir;
        return targetUpdated;
    }

    if (!m_databaseManager.isDatabaseOpen(m_databaseConnectionName) && !m_databaseManager.openDatabase(m_databaseConnectionName, m_databasePath, m_useDefaultDbPath, m_useDbWalMode))
    {
        m_lastOperationError = tr("Couldn't open target database.");
        qCritical() << "Lymalink::SetTargetInstallationLocation: failed to open database for target installation directory update:" << m_databaseManager.lastError();
        return targetUpdated;
    }

    // Custom targets do not use installation-directory based detection
    const QVariantMap target = m_databaseManager.selectFirst(m_databaseConnectionName, DATABASE_TABLE_EMU_GAMES, "id = ?", {appId});
    if (target.isEmpty() || target.value("custom_achievement_location").toInt() == 1)
    {
        m_lastOperationError = tr("Installation directory can only be edited for automatic detection.");
        qWarning() << "Lymalink::SetTargetInstallationLocation: custom or missing target rejected:" << appId;
        return targetUpdated;
    }

    targetUpdated = m_databaseManager.update(
        m_databaseConnectionName,
        DATABASE_TABLE_EMU_GAMES,
        {
            {"installation_dir", trimmedInstallationDir},
            {"appid_dir_found", 0},
            {"achievement_data_status", 0},
            {"appid_dir_location", ""},
            {"emulator_type", ""},
            {"date_updated", QDateTime::currentSecsSinceEpoch()}
        },
        "id = ?",
        {appId}
    );

    if (!targetUpdated)
    {
        m_lastOperationError = tr("Couldn't save target installation directory.");
        qCritical() << "Lymalink::SetTargetInstallationLocation: failed to update target installation directory:" << m_databaseManager.lastError();
    }

    return targetUpdated;
}

/////////////////////////////////////////////////////////////////////

QVariantMap Lymalink::GetTargetLocationSettings(int appId)
{
    QVariantMap settings;
    m_lastOperationError.clear();

    if (appId <= 0)
    {
        m_lastOperationError = tr("Invalid target.");
        return settings;
    }

    if (!m_databaseManager.isDatabaseOpen(m_databaseConnectionName) && !m_databaseManager.openDatabase(m_databaseConnectionName, m_databasePath, m_useDefaultDbPath, m_useDbWalMode))
    {
        m_lastOperationError = tr("Couldn't open target database.");
        return settings;
    }

    const QVariantMap target = m_databaseManager.selectFirst(m_databaseConnectionName, DATABASE_TABLE_EMU_GAMES, "id = ?", {appId});
    if (target.isEmpty())
    {
        m_lastOperationError = tr("Couldn't find target.");
        return settings;
    }

    const bool customAchievementLocation = target.value("custom_achievement_location").toInt() == 1;
    const QString achievementDataLocation = customAchievementLocation ? Utils::MapStringValue(target, "appid_dir_location") : QString{};
    const bool achievementLocationIsFolder = achievementDataLocation.isEmpty() || achievementDataLocation.endsWith('/') || achievementDataLocation.endsWith('\\');

    // Return one consistent snapshot for the staged location editor
    settings = {
        {"executableLocation", Utils::MapStringValue(target, "executable_location")},
        {"prefixLocation", Utils::MapStringValue(target, "prefix_location")},
        {"installationLocation", Utils::MapStringValue(target, "installation_dir")},
        {"customAchievementLocation", customAchievementLocation},
        {"emulatorType", Utils::MapStringValue(target, "emulator_type")},
        {"achievementDataLocation", achievementDataLocation},
        {"achievementLocationIsFolder", achievementLocationIsFolder}
    };
    return settings;
}

/////////////////////////////////////////////////////////////////////

bool Lymalink::SetTargetLocationSettings(int appId, QString executablePath, QString prefixPath, QString installationDir, bool customAchievementLocation, QString emulatorType, QString achievementDataLocation, bool achievementLocationIsFolder)
{
    bool targetUpdated = false;
    m_lastOperationError.clear();

    executablePath = executablePath.trimmed();
    prefixPath = prefixPath.trimmed();
    installationDir = installationDir.trimmed();
    emulatorType = emulatorType.trimmed();
    achievementDataLocation = achievementDataLocation.trimmed();

    bool requiredSettingsMissing = appId <= 0 || executablePath.isEmpty();
    if (customAchievementLocation)
    {
        requiredSettingsMissing = requiredSettingsMissing || emulatorType.isEmpty() || achievementDataLocation.isEmpty();
    }
#if !defined(Q_OS_WIN)
    else
    {
        requiredSettingsMissing = requiredSettingsMissing || prefixPath.isEmpty();
    }
#endif
    if (requiredSettingsMissing)
    {
        m_lastOperationError = tr("Required target location settings are missing.");
        return targetUpdated;
    }

    if (!m_databaseManager.isDatabaseOpen(m_databaseConnectionName) && !m_databaseManager.openDatabase(m_databaseConnectionName, m_databasePath, m_useDefaultDbPath, m_useDbWalMode))
    {
        m_lastOperationError = tr("Couldn't open target database.");
        return targetUpdated;
    }

    const QVariantMap target = m_databaseManager.selectFirst(m_databaseConnectionName, DATABASE_TABLE_EMU_GAMES, "id = ?", {appId});
    if (target.isEmpty())
    {
        m_lastOperationError = tr("Couldn't find target.");
        return targetUpdated;
    }

    bool executableQuerySucceeded = false;
    if (IsTargetExecutableLocationInUse(executablePath, appId, &executableQuerySucceeded))
    {
        m_lastOperationError = tr("Game executable is already set for another target.");
        return targetUpdated;
    }
    if (!executableQuerySucceeded)
    {
        m_lastOperationError = tr("Couldn't check existing target executables.");
        return targetUpdated;
    }

    const bool currentCustomLocation = target.value("custom_achievement_location").toInt() == 1;
    const QString currentPrefixPath = Utils::MapStringValue(target, "prefix_location");
    const QString currentInstallationDir = Utils::MapStringValue(target, "installation_dir");
    const QString currentAchievementLocation = Utils::MapStringValue(target, "appid_dir_location");
    const QString currentEmulatorType = Utils::MapStringValue(target, "emulator_type");

    QVariantMap data{
        {"executable_location", executablePath},
        {"custom_achievement_location", customAchievementLocation ? 1 : 0},
        {"date_updated", QDateTime::currentSecsSinceEpoch()}
    };
    bool detectionSettingsChanged = false;

    // Manual mode stores an exact file or normalized folder and clears automatic-only paths.
    if (customAchievementLocation)
    {
        if (achievementLocationIsFolder)
        {
            achievementDataLocation = QDir::cleanPath(QDir::fromNativeSeparators(achievementDataLocation));
            if (!achievementDataLocation.endsWith('/'))
            {
                achievementDataLocation += '/';
            }
        }

        data.insert("prefix_location", "");
        data.insert("installation_dir", "");
        data.insert("appid_dir_location", achievementDataLocation);
        data.insert("emulator_type", emulatorType);
        detectionSettingsChanged = !currentCustomLocation || !currentPrefixPath.isEmpty() || !currentInstallationDir.isEmpty() || currentAchievementLocation != achievementDataLocation || currentEmulatorType != emulatorType;
    }
    else
    {
#if defined(Q_OS_WIN)
        prefixPath.clear();
#endif
        data.insert("prefix_location", prefixPath);
        data.insert("installation_dir", installationDir);
        detectionSettingsChanged = currentCustomLocation || currentPrefixPath != prefixPath || currentInstallationDir != installationDir;
        if (detectionSettingsChanged)
        {
            data.insert("appid_dir_location", "");
            data.insert("emulator_type", "");
        }
    }

    if (detectionSettingsChanged)
    {
        data.insert("appid_dir_found", 0);
        data.insert("achievement_data_status", 0);
    }

    // Apply the complete location configuration in one database update.
    targetUpdated = m_databaseManager.update(m_databaseConnectionName, DATABASE_TABLE_EMU_GAMES, data, "id = ?", {appId});
    if (!targetUpdated)
    {
        m_lastOperationError = tr("Couldn't save target location settings.");
    }
    return targetUpdated;
}

/////////////////////////////////////////////////////////////////////

bool Lymalink::SetTargetCoverImage(int appId, const QString &sourceImagePath, const QString &targetType, bool useBlackBars)
{
    bool coverUpdated = false;
    m_lastOperationError.clear();

    const QString trimmedSourceImagePath = sourceImagePath.trimmed();
    const QString normalizedTargetType = NormalizeTargetType(targetType);
    const QString assetFolder = AssetFolderForTargetType(normalizedTargetType);

    if (appId <= 0 || trimmedSourceImagePath.isEmpty())
    {
        m_lastOperationError = tr("Invalid cover image.");
        qWarning() << "Lymalink::SetTargetCoverImage: invalid cover image update:" << appId << trimmedSourceImagePath;
        return coverUpdated;
    }

    QFileInfo sourceInfo(trimmedSourceImagePath);
    if (!sourceInfo.exists() || !sourceInfo.isFile())
    {
        m_lastOperationError = tr("Cover image file was not found.");
        qWarning() << "Lymalink::SetTargetCoverImage: source image not found:" << trimmedSourceImagePath;
        return coverUpdated;
    }

    QImageReader reader(trimmedSourceImagePath);
    reader.setAutoTransform(true);
    reader.setDecideFormatFromContent(true);

    QImage sourceImage = reader.read();
    if (sourceImage.isNull())
    {
        m_lastOperationError = tr("Selected file is not a compatible image.");
        qWarning() << "Lymalink::SetTargetCoverImage: failed to decode source image:" << reader.errorString() << trimmedSourceImagePath;
        return coverUpdated;
    }

    const QString appDataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (appDataPath.isEmpty())
    {
        m_lastOperationError = tr("Couldn't resolve application data folder.");
        qCritical() << "Lymalink::SetTargetCoverImage: failed to resolve app data location";
        return coverUpdated;
    }

    const QString coversPath = QDir(appDataPath).filePath(assetFolder + "/" + QString::number(appId) + "/covers");
    if (!QDir().mkpath(coversPath))
    {
        m_lastOperationError = tr("Couldn't create target cover folder.");
        qWarning() << "Lymalink::SetTargetCoverImage: failed to create covers folder:" << coversPath;
        return coverUpdated;
    }

    const QList<QPair<QString, QSize>> coverVariants = {
        {"custom_cover_80x120.jpg", QSize(80, 120)},
        {"custom_cover_152x228.jpg", QSize(152, 228)},
        {"custom_cover_200x300.jpg", QSize(200, 300)},
        {"custom_cover_240x360.jpg", QSize(240, 360)},
        {"custom_cover_304x456.jpg", QSize(304, 456)},
        {"custom_cover_360x540.jpg", QSize(360, 540)},
        {"custom_cover_600x900.jpg", QSize(600, 900)}
    };

    for (const auto &variant : coverVariants)
    {
        if (!SaveCustomCoverVariant(sourceImage, coversPath, variant.first, variant.second, useBlackBars))
        {
            m_lastOperationError = tr("Couldn't save custom cover image.");
            return coverUpdated;
        }
    }

    coverUpdated = true;
    qDebug() << "Lymalink::SetTargetCoverImage: custom cover saved:" << appId << normalizedTargetType << coversPath;
    return coverUpdated;
}

/////////////////////////////////////////////////////////////////////

bool Lymalink::ClearTargetCoverImage(int appId, const QString &targetType)
{
    bool coverCleared = false;
    m_lastOperationError.clear();

    const QString normalizedTargetType = NormalizeTargetType(targetType);
    const QString assetFolder = AssetFolderForTargetType(normalizedTargetType);

    if (appId <= 0)
    {
        m_lastOperationError = tr("Invalid target.");
        qWarning() << "Lymalink::ClearTargetCoverImage: invalid appId:" << appId;
        return coverCleared;
    }

    const QString appDataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (appDataPath.isEmpty())
    {
        m_lastOperationError = tr("Couldn't resolve application data folder.");
        qCritical() << "Lymalink::ClearTargetCoverImage: failed to resolve app data location";
        return coverCleared;
    }

    const QString coversPath = QDir(appDataPath).filePath(assetFolder + "/" + QString::number(appId) + "/covers");
    QDir coversDir(coversPath);
    if (!coversDir.exists())
    {
        coverCleared = true;
        return coverCleared;
    }

    const QFileInfoList customCoverFiles = coversDir.entryInfoList({"custom_cover_*.jpg"}, QDir::Files, QDir::Name | QDir::IgnoreCase);
    for (const QFileInfo &customCoverFile : customCoverFiles)
    {
        if (!QFile::remove(customCoverFile.absoluteFilePath()))
        {
            m_lastOperationError = tr("Couldn't clear custom cover image.");
            qWarning() << "Lymalink::ClearTargetCoverImage: failed to remove custom cover:" << customCoverFile.absoluteFilePath();
            return coverCleared;
        }
    }

    coverCleared = true;
    qDebug() << "Lymalink::ClearTargetCoverImage: custom cover cleared:" << appId << normalizedTargetType;
    return coverCleared;
}

/////////////////////////////////////////////////////////////////////

bool Lymalink::ResetTargetAchievementDataLocation(int appId)
{
    bool targetUpdated = false;
    m_lastOperationError.clear();

    if (appId <= 0)
    {
        m_lastOperationError = tr("Invalid target.");
        qWarning() << "Lymalink::ResetTargetAchievementDataLocation: invalid target:" << appId;
        return targetUpdated;
    }

    if (!m_databaseManager.isDatabaseOpen(m_databaseConnectionName) && !m_databaseManager.openDatabase(m_databaseConnectionName, m_databasePath, m_useDefaultDbPath, m_useDbWalMode))
    {
        m_lastOperationError = tr("Couldn't open target database.");
        qCritical() << "Lymalink::ResetTargetAchievementDataLocation: failed to open database:" << m_databaseManager.lastError();
        return targetUpdated;
    }

    const QVariantMap target = m_databaseManager.selectFirst(m_databaseConnectionName, DATABASE_TABLE_EMU_GAMES, "id = ?", {appId});
    if (target.isEmpty())
    {
        m_lastOperationError = tr("Couldn't find target.");
        qWarning() << "Lymalink::ResetTargetAchievementDataLocation: target not found:" << appId;
        return targetUpdated;
    }

    // Custom targets retain their configured location and parser during refresh.
    const bool customAchievementLocation = target.value("custom_achievement_location").toInt() == 1;
    QVariantMap data = {
        {"appid_dir_found", 0},
        {"achievement_data_status", 0},
        {"date_updated", QDateTime::currentSecsSinceEpoch()}
    };
    if (!customAchievementLocation)
    {
        data.insert("appid_dir_location", "");
        data.insert("emulator_type", "");
    }

    targetUpdated = m_databaseManager.update(
        m_databaseConnectionName,
        DATABASE_TABLE_EMU_GAMES,
        data,
        "id = ?",
        {appId}
    );

    if (!targetUpdated)
    {
        m_lastOperationError = tr("Couldn't reset achievement data location.");
        qCritical() << "Lymalink::ResetTargetAchievementDataLocation: failed to update target:" << m_databaseManager.lastError();
    }

    return targetUpdated;
}

/////////////////////////////////////////////////////////////////////

bool Lymalink::SetAchievementUnlocked(int appId, const QString &achievementKey, bool unlocked, qint64 unlockTimestamp)
{
    bool achievementStateUpdated = false;

    // Trim achievement key before database lookup
    const QString trimmedKey = achievementKey.trimmed();
    if (appId <= 0 || trimmedKey.isEmpty())
    {
        qWarning() << "Lymalink::SetAchievementUnlocked: invalid achievement unlock update:" << appId << trimmedKey;
        return achievementStateUpdated;
    }

    if (!m_databaseManager.isDatabaseOpen(m_databaseConnectionName) && !m_databaseManager.openDatabase(m_databaseConnectionName, m_databasePath, m_useDefaultDbPath, m_useDbWalMode))
    {
        qCritical() << "Lymalink::SetAchievementUnlocked: failed to open database for achievement unlock update:" << m_databaseManager.lastError();
        return achievementStateUpdated;
    }

    if (!m_databaseManager.beginTransaction(m_databaseConnectionName))
    {
        qCritical() << "Lymalink::SetAchievementUnlocked: failed to begin achievement unlock transaction:" << m_databaseManager.lastError();
        return achievementStateUpdated;
    }

    // Validate achievement exists before updating counts
    const QVariantMap existingAchievement = m_databaseManager.selectFirst(
        m_databaseConnectionName,
        DATABASE_TABLE_EMU_ACHIEVEMENTS,
        "id = ? AND achievement_key = ?",
        {appId, trimmedKey}
    );
    if (existingAchievement.isEmpty())
    {
        const QString selectError = m_databaseManager.lastError();
        m_databaseManager.rollbackTransaction(m_databaseConnectionName);
        if (!selectError.isEmpty())
        {
            qCritical() << "Lymalink::SetAchievementUnlocked: failed to fetch achievement for unlock update:" << selectError;
            return achievementStateUpdated;
        }
        qWarning() << "Lymalink::SetAchievementUnlocked: achievement not found for unlock update:" << appId << trimmedKey;
        return achievementStateUpdated;
    }

    // Use provided unlock time when present; otherwise current time
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    const qint64 normalizedUnlockTimestamp = unlocked
        ? (unlockTimestamp > 0 ? unlockTimestamp : now)
        : 0;

    QVariantMap achievementUpdate = {
        {"date_unlocked", normalizedUnlockTimestamp},
        {"date_updated", now}
    };

    // Set current progress to zero or max depending on unlock status
    const int maxProgress = Utils::MapIntValue(existingAchievement, "max_progress");
    if (unlocked && maxProgress > 0)
    {
        achievementUpdate["cur_progress"] = maxProgress;
    }
    else if (!unlocked)
    {
        achievementUpdate["cur_progress"] = 0;
    }

    const bool achievementUpdated = m_databaseManager.update(
        m_databaseConnectionName,
        DATABASE_TABLE_EMU_ACHIEVEMENTS,
        achievementUpdate,
        "id = ? AND achievement_key = ?",
        {appId, trimmedKey}
    );
    if (!achievementUpdated)
    {
        m_databaseManager.rollbackTransaction(m_databaseConnectionName);
        qCritical() << "Lymalink::SetAchievementUnlocked: failed to update achievement unlock state:" << m_databaseManager.lastError();
        return achievementStateUpdated;
    }

    // Recount unlocked achievements to keep target summary in sync
    const int unlockedCount = m_databaseManager.count(
        m_databaseConnectionName,
        DATABASE_TABLE_EMU_ACHIEVEMENTS,
        "id = ? AND date_unlocked > 0",
        {appId}
    );
    if (unlockedCount < 0)
    {
        m_databaseManager.rollbackTransaction(m_databaseConnectionName);
        qCritical() << "Lymalink::SetAchievementUnlocked: failed to count unlocked achievements:" << m_databaseManager.lastError();
        return achievementStateUpdated;
    }

    achievementStateUpdated = m_databaseManager.update(
        m_databaseConnectionName,
        DATABASE_TABLE_EMU_GAMES,
        {
            {"total_unlocked_amount_achievements", unlockedCount},
            {"date_updated", now}
        },
        "id = ?",
        {appId}
    );
    if (!achievementStateUpdated)
    {
        m_databaseManager.rollbackTransaction(m_databaseConnectionName);
        qCritical() << "Lymalink::SetAchievementUnlocked: failed to update target achievement count:" << m_databaseManager.lastError();
        return false;
    }

    if (!m_databaseManager.commitTransaction(m_databaseConnectionName))
    {
        const QString commitError = m_databaseManager.lastError();
        m_databaseManager.rollbackTransaction(m_databaseConnectionName);
        qCritical() << "Lymalink::SetAchievementUnlocked: failed to commit achievement unlock transaction:" << commitError;
        return false;
    }

    return achievementStateUpdated;
}

/////////////////////////////////////////////////////////////////////

bool Lymalink::DeleteTarget(int appId, const QString &targetType)
{
    bool targetDeleted = false;
    const QString normalizedTargetType = NormalizeTargetType(targetType);
    const QString gameTable = GameTableForTargetType(normalizedTargetType);
    const QString assetFolder = AssetFolderForTargetType(normalizedTargetType);

    if (appId <= 0)
    {
        qWarning() << "Lymalink::DeleteTarget: invalid appId for target delete:" << appId;
        return targetDeleted;
    }

    if (!m_databaseManager.isDatabaseOpen(m_databaseConnectionName) && !m_databaseManager.openDatabase(m_databaseConnectionName, m_databasePath, m_useDefaultDbPath, m_useDbWalMode))
    {
        qCritical() << "Lymalink::DeleteTarget: failed to open database for target delete:" << m_databaseManager.lastError();
        return targetDeleted;
    }

    // Require target row before deleting assets/database data
    const QVariantMap row = m_databaseManager.selectFirst(m_databaseConnectionName, gameTable, "id = ?", {appId});
    if (row.isEmpty())
    {
        qWarning() << "Lymalink::DeleteTarget: target delete row not found:" << appId;
        return targetDeleted;
    }

    const QString appDataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (appDataPath.isEmpty())
    {
        qCritical() << "Lymalink::DeleteTarget: failed to resolve app data location for target delete";
        return targetDeleted;
    }

    const QString targetPath = QDir(appDataPath).filePath(assetFolder + "/" + QString::number(appId));

    if (!m_databaseManager.beginTransaction(m_databaseConnectionName))
    {
        qCritical() << "Lymalink::DeleteTarget: failed to begin target delete transaction:" << m_databaseManager.lastError();
        return targetDeleted;
    }

    // Delete game row first; achievement rows cascade through foreign key
    const bool targetRemoved = m_databaseManager.remove(
        m_databaseConnectionName,
        gameTable,
        "id = ?",
        {appId}
    );

    if (!targetRemoved)
    {
        m_databaseManager.rollbackTransaction(m_databaseConnectionName);
        qCritical() << "Lymalink::DeleteTarget: failed to delete target:" << appId << m_databaseManager.lastError();
        return targetDeleted;
    }

    if (!m_databaseManager.commitTransaction(m_databaseConnectionName))
    {
        m_databaseManager.rollbackTransaction(m_databaseConnectionName);
        qCritical() << "Lymalink::DeleteTarget: failed to commit target delete:" << m_databaseManager.lastError();
        return targetDeleted;
    }

    // Remove assets after database commit so UI no longer references target
    const bool assetsRemoved = !QFileInfo::exists(targetPath) || m_fileManager.DeleteFolder(targetPath);
    if (!assetsRemoved)
    {
        qWarning() << "Lymalink::DeleteTarget: target deleted from database but asset removal failed:" << targetPath;
        return targetDeleted;
    }

    qDebug() << "Lymalink::DeleteTarget: target deleted:" << appId;
    targetDeleted = true;
    return targetDeleted;
}

/////////////////////////////////////////////////////////////////////

QString Lymalink::GetTargetTitle(int appId)
{
    QString targetTitle = "";

    if (appId <= 0)
    {
        return targetTitle;
    }

    // Fetch only target title for lightweight UI bindings
    const QVariantMap row = m_databaseManager.selectFirst(m_databaseConnectionName, DATABASE_TABLE_EMU_GAMES, "id = ?", {appId});
    targetTitle = Utils::MapStringValue(row, "game_name");
    return targetTitle;
}

/////////////////////////////////////////////////////////////////////

QString Lymalink::GetTargetPrefixLocation(int appId)
{
    QString prefixLocation = "";

    if (appId <= 0)
    {
        return prefixLocation;
    }

    // Fetch current prefix path for settings editor
    const QVariantMap row = m_databaseManager.selectFirst(m_databaseConnectionName, DATABASE_TABLE_EMU_GAMES, "id = ?", {appId});
    prefixLocation = Utils::MapStringValue(row, "prefix_location");
    return prefixLocation;
}

/////////////////////////////////////////////////////////////////////

QString Lymalink::GetTargetExecutableLocation(int appId)
{
    QString executableLocation = "";

    if (appId <= 0)
    {
        return executableLocation;
    }

    // Fetch current executable path for settings editor
    const QVariantMap row = m_databaseManager.selectFirst(m_databaseConnectionName, DATABASE_TABLE_EMU_GAMES, "id = ?", {appId});
    executableLocation = Utils::MapStringValue(row, "executable_location");
    return executableLocation;
}

/////////////////////////////////////////////////////////////////////

QString Lymalink::GetTargetInstallationLocation(int appId)
{
    QString installationLocation = "";

    if (appId <= 0)
    {
        return installationLocation;
    }

    // Fetch current installation directory for settings editor
    const QVariantMap row = m_databaseManager.selectFirst(m_databaseConnectionName, DATABASE_TABLE_EMU_GAMES, "id = ?", {appId});
    installationLocation = Utils::MapStringValue(row, "installation_dir");
    return installationLocation;
}

/////////////////////////////////////////////////////////////////////

QString Lymalink::GetLastOperationError() const
{
    return m_lastOperationError;
}

/////////////////////////////////////////////////////////////////////

bool Lymalink::RestartApplication() const
{
#if !defined(Q_OS_WIN)
    if (qEnvironmentVariableIsSet("INVOCATION_ID"))
    {
        const bool serviceRestarted = QProcess::startDetached(QStringLiteral("systemctl"), QStringList{
            QStringLiteral("--user"),
            QStringLiteral("restart"),
            QStringLiteral("lymalink-ui.service")
        });
        if (serviceRestarted)
        {
            QCoreApplication::quit();
        }
        return serviceRestarted;
    }
#endif

    QCoreApplication::instance()->setProperty("lymalink.restartRequested", true);
    QCoreApplication::quit();
    return true;
}

/////////////////////////////////////////////////////////////////////

bool Lymalink::InitializeCustomDatabasePath(const QString &folderPath)
{
    const QString databasePath = QDir(folderPath.trimmed()).filePath(DATABASE_FILE_NAME);
    bool customDbInitSuccess = DatabaseInit(databasePath, true) == Error::NoError;
    if (customDbInitSuccess)
    {
        m_useDefaultDbPath = false;
    }
    else
    {
        m_useDefaultDbPath = true;
    }

    return customDbInitSuccess;
}

/////////////////////////////////////////////////////////////////////

bool Lymalink::ProbeDatabaseRuntimeWriteAccess()
{
    if (!m_databaseManager.isDatabaseOpen(m_databaseConnectionName) && !m_databaseManager.openDatabase(m_databaseConnectionName, m_databasePath, false, m_useDbWalMode))
    {
        const QString error = tr("Database check failed: %1").arg(m_databaseManager.lastError());
        emit signalErrorOccurred(tr("Database Error"), error);
        return false;
    }

    const bool useCustomDatabasePath = m_settings && !m_settings->GetDatabaseCustomPath().trimmed().isEmpty();
    QString writeProbeError;
    bool transientBusy = false;
    if (!m_databaseUtils.ProbeRuntimeWriteAccess(m_databaseConnectionName, &writeProbeError, &transientBusy))
    {
        if (transientBusy)
        {
            qDebug() << "Lymalink::ProbeDatabaseRuntimeWriteAccess: database is temporarily busy";
            return true;
        }

        const QString error = useCustomDatabasePath
            ? tr("Database check failed - please check custom database location settings and connection.")
            : tr("Database check failed.");
        const QString logError = useCustomDatabasePath && !writeProbeError.isEmpty()
            ? QStringLiteral("Database check failed - please check custom database location settings and connection: %1").arg(writeProbeError)
            : writeProbeError.isEmpty()
                ? error
                : QStringLiteral("Database check failed: %1").arg(writeProbeError);
        qCritical() << "Lymalink::ProbeDatabaseRuntimeWriteAccess:" << logError;
        emit signalErrorOccurred(tr("Database Error"), error);
        return false;
    }

    return true;
}

/////////////////////////////////////////////////////////////////////

QVariantList Lymalink::ReloadAllMissingMetadata()
{
    QVariantList queuedTargets;

    if (!m_databaseManager.isDatabaseOpen(m_databaseConnectionName) && !m_databaseManager.openDatabase(m_databaseConnectionName, m_databasePath, m_useDefaultDbPath, m_useDbWalMode))
    {
        qCritical() << "Lymalink::ReloadAllMissingMetadata: failed to open database:" << m_databaseManager.lastError();
        return queuedTargets;
    }

    const auto queueMissingTargets = [this, &queuedTargets](const QString &targetType) {
        const QString gameTable = GameTableForTargetType(targetType);
        const QVariantList rows = m_databaseManager.selectAll(m_databaseConnectionName, gameTable, QStringList{"id"});
        if (!m_databaseManager.lastError().isEmpty())
        {
            qWarning() << "Lymalink::ReloadAllMissingMetadata: failed to fetch target rows:" << gameTable << m_databaseManager.lastError();
            return;
        }

        for (const QVariant &rowValue : rows)
        {
            const QVariantMap row = rowValue.toMap();
            const int appId = Utils::MapIntValue(row, "id");
            if (appId <= 0 || !TargetHasMissingMetadata(appId, targetType))
            {
                continue;
            }

            EnqueueSteamHydrationTask(appId, true, targetType);
            queuedTargets.append(QVariantMap{
                {"id", appId},
                {"targetType", NormalizeTargetType(targetType)}
            });
        }
    };

    queueMissingTargets("Emulator");
    queueMissingTargets("Steam");
    return queuedTargets;
}

/////////////////////////////////////////////////////////////////////

QVariantList Lymalink::FetchDashboardTargets()
{
    QVariantList targets;

    if (!m_databaseManager.isDatabaseOpen(m_databaseConnectionName) && !m_databaseManager.openDatabase(m_databaseConnectionName, m_databasePath, m_useDefaultDbPath, m_useDbWalMode))
    {
        qCritical() << "Lymalink::FetchDashboardTargets: failed to open database for dashboard targets:" << m_databaseManager.lastError();
        return targets;
    }

    // Build full dashboard model from game rows and related achievement state
    const QVariantList rows = m_databaseManager.selectAll(m_databaseConnectionName, DATABASE_TABLE_EMU_GAMES);

    const QString appDataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    for (const QVariant &rowValue : rows)
    {
        const QVariantMap row = rowValue.toMap();
        const int appId = Utils::MapIntValue(row, "id");
        const QString appIdText = QString::number(appId);
        const QDir emulatorAppDir(QDir(appDataPath).filePath("Emulator/" + appIdText));
        const QString coversPath = emulatorAppDir.filePath("covers");
        const QString iconsPath = emulatorAppDir.filePath("icons");
        const QString coverSourceCard = PreferredCoverImageFilePath(coversPath, "cover_200x300.jpg");
        const QString coverSourceCardSmall = PreferredCoverImageFilePath(coversPath, "cover_152x228.jpg");
        const QString coverSourceCardLarge = PreferredCoverImageFilePath(coversPath, "cover_240x360.jpg");
        const QString coverSourceRowDetailed = PreferredCoverImageFilePath(coversPath, "cover_80x120.jpg");
        const QString coverSourceTargetDetails = PreferredCoverImageFilePath(coversPath, "cover_240x360.jpg");
        const QString coverSourceTargetDetailsMedium = PreferredCoverImageFilePath(coversPath, "cover_304x456.jpg");
        const QString coverSourceTargetDetailsLarge = PreferredCoverImageFilePath(coversPath, "cover_360x540.jpg");
        const QString coverSourceScaled = PreferredCoverImageFilePath(coversPath, "cover_600x900.jpg");

        // Fetch achievements only enough to compute latest unlock preview
        const QVariantList achievements = m_databaseManager.selectWhere(
            m_databaseConnectionName,
            DATABASE_TABLE_EMU_ACHIEVEMENTS,
            "id = ?",
            {appId},
            {
                "achievement_key",
                "achievement_name",
                "achievement_description",
                "date_unlocked"
            }
        );
        const QVariantMap latestAchievement = LatestUnlockedAchievement(achievements);
        const qint64 latestUnlockTimestamp = latestAchievement.value("date_unlocked").toLongLong();
        const int achievementCount = Utils::MapIntValue(row, "total_unlocked_amount_achievements");
        const int achievementTotal = Utils::MapIntValue(row, "total_amount_achievements");
        
        // Package QML dashboard target data with image fallbacks
        QVariantMap target = {
            {"id", appId},
            {"title", Utils::MapStringValue(row, "game_name")},
            {"coverSource", coverSourceCard},
            {"coverSourceCard", coverSourceCard},
            {"coverSourceCardLarge", coverSourceCardLarge},
            {"coverSourceCardSmall", coverSourceCardSmall},
            {"coverSourceRowDetailed", coverSourceRowDetailed},
            {"coverSourceTargetDetails", coverSourceTargetDetails},
            {"coverSourceTargetDetailsMedium", coverSourceTargetDetailsMedium},
            {"coverSourceTargetDetailsLarge", coverSourceTargetDetailsLarge},
            {"coverSourceScaled", coverSourceScaled},
            {"logoSource", CommunityIconFilePath(iconsPath)},
            {"achievementCount", achievementCount},
            {"achievementTotal", achievementTotal},
            {"targetType", "Emulator"},
            {"status", ExecutableInstallationStatus(row)},
            {"lastPlayed", Utils::LocalDate(row.value("last_played_date").toLongLong())},
            {"recentUnlock", Utils::LocalDate(latestUnlockTimestamp)},
            {"progressValue", achievementTotal > 0 ? static_cast<double>(achievementCount) / achievementTotal : 0.0},
            {"targetHidden", row.value("target_hidden").toInt() == 1},
            {"playtimeSeconds", Utils::MapIntValue(row, "total_seconds_played")},
            {"lastPlayedTimestamp", row.value("last_played_date").toLongLong()},
            {"recentUnlockTimestamp", latestUnlockTimestamp},
            {"dateAddedTimestamp", row.value("date_added").toLongLong()},
            {"lastAchievementIcon", AchievementIconFilePath(iconsPath, latestAchievement)},
            {"lastAchievementName", Utils::MapStringValue(latestAchievement, "achievement_name")},
            {"lastAchievementDesc", Utils::MapStringValue(latestAchievement, "achievement_description")}
        };

        targets << target;
    }

    const QVariantList steamRows = m_databaseManager.selectAll(m_databaseConnectionName, DATABASE_TABLE_GAMES);
    for (const QVariant &rowValue : steamRows)
    {
        const QVariantMap row = rowValue.toMap();
        const int appId = Utils::MapIntValue(row, "id");
        const QString appIdText = QString::number(appId);
        const QDir steamAppDir(QDir(appDataPath).filePath("Steam/" + appIdText));
        const QString coversPath = steamAppDir.filePath("covers");
        const QString iconsPath = steamAppDir.filePath("icons");
        const QString coverSourceCard = PreferredCoverImageFilePath(coversPath, "cover_200x300.jpg");
        const QString coverSourceCardSmall = PreferredCoverImageFilePath(coversPath, "cover_152x228.jpg");
        const QString coverSourceCardLarge = PreferredCoverImageFilePath(coversPath, "cover_240x360.jpg");
        const QString coverSourceRowDetailed = PreferredCoverImageFilePath(coversPath, "cover_80x120.jpg");
        const QString coverSourceTargetDetails = PreferredCoverImageFilePath(coversPath, "cover_240x360.jpg");
        const QString coverSourceTargetDetailsMedium = PreferredCoverImageFilePath(coversPath, "cover_304x456.jpg");
        const QString coverSourceTargetDetailsLarge = PreferredCoverImageFilePath(coversPath, "cover_360x540.jpg");
        const QString coverSourceScaled = PreferredCoverImageFilePath(coversPath, "cover_600x900.jpg");

        const QVariantList achievements = m_databaseManager.selectWhere(
            m_databaseConnectionName,
            DATABASE_TABLE_ACHIEVEMENTS,
            "id = ?",
            {appId},
            {
                "achievement_key",
                "achievement_name",
                "achievement_description",
                "date_unlocked"
            }
        );
        const QVariantMap latestAchievement = LatestUnlockedAchievement(achievements);
        const qint64 latestUnlockTimestamp = latestAchievement.value("date_unlocked").toLongLong();
        const int achievementCount = Utils::MapIntValue(row, "total_unlocked_amount_achievements");
        const int achievementTotal = Utils::MapIntValue(row, "total_amount_achievements");

        QVariantMap target = {
            {"id", appId},
            {"title", Utils::MapStringValue(row, "game_name")},
            {"coverSource", coverSourceCard},
            {"coverSourceCard", coverSourceCard},
            {"coverSourceCardLarge", coverSourceCardLarge},
            {"coverSourceCardSmall", coverSourceCardSmall},
            {"coverSourceRowDetailed", coverSourceRowDetailed},
            {"coverSourceTargetDetails", coverSourceTargetDetails},
            {"coverSourceTargetDetailsMedium", coverSourceTargetDetailsMedium},
            {"coverSourceTargetDetailsLarge", coverSourceTargetDetailsLarge},
            {"coverSourceScaled", coverSourceScaled},
            {"logoSource", CommunityIconFilePath(iconsPath)},
            {"achievementCount", achievementCount},
            {"achievementTotal", achievementTotal},
            {"targetType", "Steam"},
            {"status", tr("Installed")},
            {"lastPlayed", Utils::LocalDate(row.value("last_played_date").toLongLong())},
            {"recentUnlock", Utils::LocalDate(latestUnlockTimestamp)},
            {"progressValue", achievementTotal > 0 ? static_cast<double>(achievementCount) / achievementTotal : 0.0},
            {"targetHidden", row.value("target_hidden").toInt() == 1},
            {"playtimeSeconds", Utils::MapIntValue(row, "total_seconds_played")},
            {"lastPlayedTimestamp", row.value("last_played_date").toLongLong()},
            {"recentUnlockTimestamp", latestUnlockTimestamp},
            {"dateAddedTimestamp", row.value("date_added").toLongLong()},
            {"lastAchievementIcon", AchievementIconFilePath(iconsPath, latestAchievement)},
            {"lastAchievementName", Utils::MapStringValue(latestAchievement, "achievement_name")},
            {"lastAchievementDesc", Utils::MapStringValue(latestAchievement, "achievement_description")}
        };

        targets << target;
    }

    return targets;
}

/////////////////////////////////////////////////////////////////////

QVariantMap Lymalink::FetchTargetDetails(int appId, const QString &targetType)
{
    QVariantMap targetDetails = {};
    const QString normalizedTargetType = NormalizeTargetType(targetType);
    const QString gameTable = GameTableForTargetType(normalizedTargetType);
    const QString achievementTable = AchievementTableForTargetType(normalizedTargetType);
    const QString assetFolder = AssetFolderForTargetType(normalizedTargetType);

    if (appId <= 0)
    {
        qWarning() << "Lymalink::FetchTargetDetails: invalid appId for target details:" << appId;
        return targetDetails;
    }

    if (!m_databaseManager.isDatabaseOpen(m_databaseConnectionName) && !m_databaseManager.openDatabase(m_databaseConnectionName, m_databasePath, m_useDefaultDbPath, m_useDbWalMode))
    {
        qCritical() << "Lymalink::FetchTargetDetails: failed to open database for target details:" << m_databaseManager.lastError();
        return targetDetails;
    }

    // Fetch target row before building heavy details payload
    const QVariantMap row = m_databaseManager.selectFirst(m_databaseConnectionName, gameTable, "id = ?", {appId});
    if (row.isEmpty())
    {
        qWarning() << "Lymalink::FetchTargetDetails: target details not found for appId:" << appId;
        return targetDetails;
    }

    const QString appDataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    const QString appIdText = QString::number(appId);
    const QDir targetAppDir(QDir(appDataPath).filePath(assetFolder + "/" + appIdText));
    const QString coversPath = targetAppDir.filePath("covers");
    const QString iconsPath = targetAppDir.filePath("icons");

    const QString coverSourceTargetDetails = PreferredCoverImageFilePath(coversPath, "cover_240x360.jpg");
    const QString coverSourceTargetDetailsMedium = PreferredCoverImageFilePath(coversPath, "cover_304x456.jpg");
    const QString coverSourceTargetDetailsLarge = PreferredCoverImageFilePath(coversPath, "cover_360x540.jpg");
    const QString coverSourceScaled = PreferredCoverImageFilePath(coversPath, "cover_600x900.jpg");
    const QVariantList achievements = BuildAchievementDetails(appId, iconsPath, normalizedTargetType);
    const QVariantMap latestAchievement = LatestUnlockedAchievement(m_databaseManager.selectWhere(
        m_databaseConnectionName,
        achievementTable,
        "id = ?",
        {appId},
        {"achievement_key", "achievement_name", "achievement_description", "date_unlocked"}
    ));
    const QString emulatorType = Utils::MapStringValue(row, "emulator_type");

    targetDetails = {
        {"id", appId},
        {"title", Utils::MapStringValue(row, "game_name")},
        {"coverSource", coverSourceTargetDetails},
        {"coverSourceTargetDetails", coverSourceTargetDetails},
        {"coverSourceTargetDetailsMedium", coverSourceTargetDetailsMedium},
        {"coverSourceTargetDetailsLarge", coverSourceTargetDetailsLarge},
        {"coverSourceScaled", coverSourceScaled},
        {"achievementCount", Utils::MapIntValue(row, "total_unlocked_amount_achievements")},
        {"achievementTotal", Utils::MapIntValue(row, "total_amount_achievements")},
        {"targetType", normalizedTargetType},
        {"installationStatus", IsSteamTargetType(normalizedTargetType) ? tr("") : ExecutableInstallationStatus(row)},
        {"lastPlayed", Utils::LocalDate(row.value("last_played_date").toLongLong())},
        {"recentUnlock", Utils::LocalDate(latestAchievement.value("date_unlocked").toLongLong())},
        {"playtime", PlaytimeText(Utils::MapIntValue(row, "total_seconds_played"))},
        {"appIdDirFound", IsSteamTargetType(normalizedTargetType) ? false : row.value("appid_dir_found").toInt() == 1},
        {"achievementDataStatus", IsSteamTargetType(normalizedTargetType) ? 0 : Utils::MapIntValue(row, "achievement_data_status")},
        {"emulatorType", emulatorType},
        {"customAchievementLocation", !IsSteamTargetType(normalizedTargetType) && row.value("custom_achievement_location").toInt() == 1},
        {"targetHidden", row.value("target_hidden").toInt() == 1},
        {"achievements", achievements}
    };
    return targetDetails;
}

/////////////////////////////////////////////////////////////////////

QVariantMap Lymalink::FetchSteamOwnedGames(const QString &steamId, const QString &apiKey)
{
    QVariantMap payload = {
        {"success", false},
        {"error", QString()},
        {"games", QVariantList()}
    };

    SteamApi steamApi;
    QList<SteamOwnedGameData> ownedGames;
    const Error fetchError = steamApi.FetchOwnedGames(steamId, ownedGames, apiKey);
    if (fetchError != Error::NoError && fetchError != Error::NoData)
    {
        QString errorText;
        switch (fetchError)
        {
            case Error::InvalidParameter:
                errorText = tr("Steam ID or Steam Web API key is invalid.");
                break;
            case Error::AccessDenied:
                errorText = tr("Steam profile is private or Steam Web API key was rejected.");
                break;
            case Error::NotFound:
                errorText = tr("Games could not be loaded. Check API credentials.");
                break;
            case Error::NoData:
                errorText = tr("Steam library is empty.");
                break;
            case Error::ParseError:
                errorText = tr("Games response could not be parsed.");
                break;
            default:
                errorText = tr("Games could not be loaded. Check API credentials.");
                break;
        }

        m_lastOperationError = errorText;
        payload["error"] = errorText;
        return payload;
    }

    if (!m_databaseManager.isDatabaseOpen(m_databaseConnectionName) && !m_databaseManager.openDatabase(m_databaseConnectionName, m_databasePath, m_useDefaultDbPath, m_useDbWalMode))
    {
        m_lastOperationError = tr("Failed to open database for Steam import state.");
        payload["error"] = m_lastOperationError;
        return payload;
    }

    const QVariantList importedRows = m_databaseManager.selectAll(m_databaseConnectionName, DATABASE_TABLE_GAMES, QStringList{"id"});
    QSet<int> importedIds;
    for (const QVariant &rowValue : importedRows)
    {
        const QVariantMap row = rowValue.toMap();
        const int appId = row.value("id").toInt();
        if (appId > 0)
        {
            importedIds.insert(appId);
        }
    }

    QVariantList games;
    games.reserve(ownedGames.size());
    for (const SteamOwnedGameData &ownedGame : ownedGames)
    {
        const bool imported = importedIds.contains(ownedGame.appId);
            QVariantMap game = {
                {"appId", ownedGame.appId},
                {"name", ownedGame.gameName},
                {"imported", imported},
                {"selected", imported},
                {"playtimeSeconds", ownedGame.totalSecondsPlayed},
            {"lastPlayedTimestamp", ownedGame.lastPlayedDate}
        };
        games.append(game);
    }

    payload["success"] = true;
    payload["games"] = games;
    m_lastOperationError.clear();
    return payload;
}

/////////////////////////////////////////////////////////////////////
///////////////////////////// PRIVATE ///////////////////////////////
/////////////////////////////////////////////////////////////////////

Error Lymalink::DatabaseInit(const QString &databasePath, bool createMissingDatabase)
{
    Error databaseResult = Error::NoError;

    // Default DB path uses WAL, missing DB is fatal error
    const bool useWal = m_useDbWalMode;
    const bool missingDbIsFatalErr = m_useDefaultDbPath;

    m_databasePath = databasePath.trimmed().isEmpty()
        ? (m_settings ? m_settings->GetActiveDatabasePath() : QString())
        : databasePath.trimmed();

    if (m_databasePath.trimmed().isEmpty())
    {
        m_lastOperationError = tr("Failed to resolve database path.");
        qCritical() << "Lymalink::DatabaseInit: failed to resolve database path";
        databaseResult = Error::FileSystemError;
        return databaseResult;
    }

    if (m_databaseManager.databaseFileExists(m_databasePath))
    {
        // Open existing database file
        qDebug() << "Lymalink::DatabaseInit: database already exists at" << m_databasePath;
        if (!m_databaseManager.openDatabase(m_databaseConnectionName, m_databasePath, createMissingDatabase, useWal))
        {
            qCritical() << "Lymalink::DatabaseInit: failed to open database:" << m_databaseManager.lastError();
            databaseResult = Error::DatabaseError;
            return databaseResult;
        }
    }
    else if (!createMissingDatabase)
    {
        m_lastOperationError = tr("Database file not found: %1").arg(m_databasePath);
        qCritical() << "Lymalink::DatabaseInit:" << m_lastOperationError;
        return missingDbIsFatalErr ? Error::DatabaseError : Error::NoError;
    }
    else if (!m_databaseManager.createDatabase(m_databaseConnectionName, m_databasePath, useWal))
    {
        qCritical() << "Lymalink::DatabaseInit: failed to create/open database:" << m_databaseManager.lastError();
        databaseResult = Error::DatabaseError;
        return databaseResult;
    }

    // Ensure emulator target table exists before app queries
    const bool gamesReady = m_databaseManager.createTable(
        m_databaseConnectionName,
        DATABASE_TABLE_EMU_GAMES,
        {
            "id INTEGER PRIMARY KEY NOT NULL",
            "game_name TEXT NOT NULL",
            "emulator_type TEXT",
            "executable_location TEXT",
            "prefix_location TEXT",
            "installation_dir TEXT",
            "data_opt TEXT",
            "custom_achievement_location INTEGER DEFAULT 0",
            "target_hidden INTEGER DEFAULT 0",
            "appid_dir_found INTEGER DEFAULT 0",
            "achievement_data_status INTEGER DEFAULT 0",
            "appid_dir_location TEXT",
            "total_amount_achievements INTEGER",
            "total_unlocked_amount_achievements INTEGER",
            "total_seconds_played INTEGER",
            "last_played_date INTEGER",
            "date_updated INTEGER",
            "date_added INTEGER"
        }
    );

    if (!gamesReady)
    {
        qCritical() << "Lymalink::DatabaseInit: failed to initialize" << DATABASE_TABLE_EMU_GAMES << "table:" << m_databaseManager.lastError();
        databaseResult = Error::DatabaseError;
        return databaseResult;
    }

    // Execute migrates for updated version of Lymalink
    bool achievementDataStatusColumnAdded = false;
    if (!EnsureColumn(DATABASE_TABLE_EMU_GAMES, "installation_dir", "installation_dir TEXT") ||
        !EnsureColumn(DATABASE_TABLE_EMU_GAMES, "data_opt", "data_opt TEXT") ||
        !EnsureColumn(DATABASE_TABLE_EMU_GAMES, "custom_achievement_location", "custom_achievement_location INTEGER DEFAULT 0") ||
        !EnsureColumn(DATABASE_TABLE_EMU_GAMES, "achievement_data_status", "achievement_data_status INTEGER DEFAULT 0", &achievementDataStatusColumnAdded))
    {
        qCritical() << "Lymalink::DatabaseInit: failed to migrate" << DATABASE_TABLE_EMU_GAMES << "table:" << m_databaseManager.lastError();
        databaseResult = Error::DatabaseError;
        return databaseResult;
    }
    if (achievementDataStatusColumnAdded)
    {
        if (!m_databaseManager.executeSql(m_databaseConnectionName, QString("UPDATE %1 SET achievement_data_status = 1 WHERE appid_dir_found = 1 AND achievement_data_status = 0").arg(DATABASE_TABLE_EMU_GAMES), true))
        {
            qCritical() << "Lymalink::DatabaseInit: failed to sync achievement data status:" << m_databaseManager.lastError();
            databaseResult = Error::DatabaseError;
            return databaseResult;
        }
    }

    // Ensure achievements table exists with cascade delete from game rows
    const bool achievementsReady = m_databaseManager.createTable(
        m_databaseConnectionName,
        DATABASE_TABLE_EMU_ACHIEVEMENTS,
        {
            "id INTEGER NOT NULL",
            "achievement_key TEXT NOT NULL",
            "achievement_name TEXT NOT NULL",
            "achievement_description TEXT",
            "achievement_hidden INTEGER DEFAULT 0",
            "global_unlock_percentage REAL",
            "cur_progress INTEGER DEFAULT 0",
            "max_progress INTEGER DEFAULT 0",
            "date_unlocked INTEGER",
            "date_updated INTEGER",
            "date_added INTEGER",
            "PRIMARY KEY (id, achievement_key)",
            QString("FOREIGN KEY (id) REFERENCES %1(id) ON DELETE CASCADE").arg(DATABASE_TABLE_EMU_GAMES)
        }
    );

    if (!achievementsReady)
    {
        qCritical() << "Lymalink::DatabaseInit: failed to initialize" << DATABASE_TABLE_EMU_ACHIEVEMENTS << "table:" << m_databaseManager.lastError();
        databaseResult = Error::DatabaseError;
        return databaseResult;
    }

    const bool steamGamesReady = m_databaseManager.createTable(
        m_databaseConnectionName,
        DATABASE_TABLE_GAMES,
        {
            "id INTEGER PRIMARY KEY NOT NULL",
            "game_name TEXT NOT NULL",
            "target_hidden INTEGER DEFAULT 0",
            "total_amount_achievements INTEGER",
            "total_unlocked_amount_achievements INTEGER",
            "total_seconds_played INTEGER",
            "last_played_date INTEGER",
            "date_updated INTEGER",
            "date_added INTEGER"
        }
    );

    if (!steamGamesReady)
    {
        qCritical() << "Lymalink::DatabaseInit: failed to initialize" << DATABASE_TABLE_GAMES << "table:" << m_databaseManager.lastError();
        databaseResult = Error::DatabaseError;
        return databaseResult;
    }

    const bool steamAchievementsReady = m_databaseManager.createTable(
        m_databaseConnectionName,
        DATABASE_TABLE_ACHIEVEMENTS,
        {
            "id INTEGER NOT NULL",
            "achievement_key TEXT NOT NULL",
            "achievement_name TEXT NOT NULL",
            "achievement_description TEXT",
            "achievement_hidden INTEGER DEFAULT 0",
            "global_unlock_percentage REAL",
            "date_unlocked INTEGER",
            "date_updated INTEGER",
            "date_added INTEGER",
            "PRIMARY KEY (id, achievement_key)",
            QString("FOREIGN KEY (id) REFERENCES %1(id) ON DELETE CASCADE").arg(DATABASE_TABLE_GAMES)
        }
    );

    if (!steamAchievementsReady)
    {
        qCritical() << "Lymalink::DatabaseInit: failed to initialize" << DATABASE_TABLE_ACHIEVEMENTS << "table:" << m_databaseManager.lastError();
        databaseResult = Error::DatabaseError;
        return databaseResult;
    }

    m_lastOperationError.clear();
    qDebug() << "Lymalink::DatabaseInit: database initialized at" << m_databasePath;
    return databaseResult;
}

/////////////////////////////////////////////////////////////////////

Error Lymalink::FileSystemInit()
{
    Error fileSystemResult = Error::NoError;

    // Resolve writable application data root for managed target folders
    const QString appDataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (appDataPath.isEmpty())
    {
        qCritical() << "Lymalink::FileSystemInit: failed to resolve app data location for filesystem init";
        fileSystemResult = Error::FileSystemError;
        return fileSystemResult;
    }

    QDir appDataDir(appDataPath);
    const QStringList requiredFolders = {"Emulator", "Steam", "Custom"};

    // Create all top-level target category folders
    for (const QString &folderName : requiredFolders)
    {
        if (!appDataDir.mkpath(folderName))
        {
            qCritical() << "Lymalink::FileSystemInit: failed to create folder:" << appDataDir.filePath(folderName);
            fileSystemResult = Error::FileSystemError;
            return fileSystemResult;
        }
    }

    qDebug() << "Lymalink::FileSystemInit: filesystem initialized at" << appDataPath;
    return fileSystemResult;
}

/////////////////////////////////////////////////////////////////////

bool Lymalink::EnsureColumn(const QString &tableName, const QString &columnName, const QString &columnDef, bool *columnAdded)
{
    if (columnAdded)
    {
        *columnAdded = false;
    }

    QString escapedTableName = tableName;
    escapedTableName.replace("'", "''");

    const QVariantMap row = m_databaseManager.selectFirst(
        m_databaseConnectionName,
        QString("pragma_table_info('%1')").arg(escapedTableName),
        "name = ?",
        {columnName}
    );
    if (row.contains("name"))
    {
        return true;
    }

    const QString sql = QString("ALTER TABLE %1 ADD COLUMN %2").arg(tableName, columnDef);
    if (!m_databaseManager.executeSql(m_databaseConnectionName, sql, true))
    {
        return false;
    }

    qInfo() << "Lymalink::EnsureColumn: altered table" << tableName << "added column" << columnName;
    if (columnAdded)
    {
        *columnAdded = true;
    }
    return true;
}

/////////////////////////////////////////////////////////////////////

bool Lymalink::ApplyNewAchievements(int appId, QString targetType, QVariantList achievements)
{
    if (IsSteamTargetType(targetType))
    {
        return false;
    }

    if (appId <= 0 || achievements.isEmpty())
    {
        qDebug() << "Lymalink::ApplyNewAchievements: no achievement payload to merge for appId:" << appId;
        return false;
    }

    int validPayloadRows = 0;
    for (const QVariant &val : achievements)
    {
        if (!Utils::MapStringValue(val.toMap(), "achievement_key").isEmpty())
        {
            ++validPayloadRows;
        }
    }
    if (validPayloadRows == 0)
    {
        qDebug() << "Lymalink::ApplyNewAchievements: no valid achievement keys to merge for appId:" << appId;
        return false;
    }

    if (!m_databaseManager.beginTransaction(m_databaseConnectionName))
    {
        qWarning() << "Lymalink::ApplyNewAchievements: failed to start transaction:" << m_databaseManager.lastError();
        return false;
    }

    const qint64 now = QDateTime::currentSecsSinceEpoch();
    int inserted = 0;
    int updated = 0;
    bool mergeSucceeded = true;

    // Merge fetched keys only. Missing remote keys must never remove local user data.
    for (const QVariant &val : achievements)
    {
        QVariantMap entry = val.toMap();
        const QString achievementKey = Utils::MapStringValue(entry, "achievement_key");
        if (achievementKey.isEmpty())
        {
            continue;
        }

        const QVariantMap existingAchievement = m_databaseManager.selectFirst(
            m_databaseConnectionName,
            DATABASE_TABLE_EMU_ACHIEVEMENTS,
            "id = ? AND achievement_key = ?",
            {appId, achievementKey}
        );

        if (existingAchievement.isEmpty())
        {
            entry["id"] = appId;
            entry["date_added"] = now;
            entry["date_updated"] = now;

            if (!m_databaseManager.insert(m_databaseConnectionName, DATABASE_TABLE_EMU_ACHIEVEMENTS, entry))
            {
                qWarning() << "Lymalink::ApplyNewAchievements: failed to insert achievement:" << achievementKey << m_databaseManager.lastError();
                mergeSucceeded = false;
                break;
            }
            ++inserted;
            continue;
        }

        QVariantMap achievementUpdate = {};

        const QString newName = Utils::MapStringValue(entry, "achievement_name");
        if (!newName.isEmpty() && newName != Utils::MapStringValue(existingAchievement, "achievement_name"))
        {
            achievementUpdate["achievement_name"] = newName;
        }

        const QString newDescription = Utils::MapStringValue(entry, "achievement_description");
        if (newDescription != Utils::MapStringValue(existingAchievement, "achievement_description"))
        {
            achievementUpdate["achievement_description"] = newDescription;
        }

        const int newHidden = Utils::MapIntValue(entry, "achievement_hidden");
        if (newHidden != Utils::MapIntValue(existingAchievement, "achievement_hidden"))
        {
            achievementUpdate["achievement_hidden"] = newHidden;
        }

        const double newGlobalUnlockPercentage = entry.value("global_unlock_percentage").toDouble();
        if (newGlobalUnlockPercentage != existingAchievement.value("global_unlock_percentage").toDouble())
        {
            achievementUpdate["global_unlock_percentage"] = newGlobalUnlockPercentage;
        }

        const int oldMaxProgress = Utils::MapIntValue(existingAchievement, "max_progress");
        const int newMaxProgress = Utils::MapIntValue(entry, "max_progress");
        // A zero metadata maximum means unavailable - keep known emulator progress
        if (newMaxProgress > 0 && newMaxProgress != oldMaxProgress)
        {
            const int oldCurProgress = Utils::MapIntValue(existingAchievement, "cur_progress");
            achievementUpdate["max_progress"] = newMaxProgress;

            if (oldMaxProgress > 0 && oldCurProgress >= oldMaxProgress)
            {
                achievementUpdate["cur_progress"] = newMaxProgress;
            }
            else if (oldCurProgress > newMaxProgress)
            {
                achievementUpdate["cur_progress"] = 0;
            }
        }

        if (achievementUpdate.isEmpty())
        {
            continue;
        }

        achievementUpdate["date_updated"] = now;
        if (!m_databaseManager.update(
            m_databaseConnectionName,
            DATABASE_TABLE_EMU_ACHIEVEMENTS,
            achievementUpdate,
            "id = ? AND achievement_key = ?",
            {appId, achievementKey}))
        {
            qWarning() << "Lymalink::ApplyNewAchievements: failed to update achievement:" << achievementKey << m_databaseManager.lastError();
            mergeSucceeded = false;
            break;
        }
        ++updated;
    }

    const int totalAchievements = m_databaseManager.count(m_databaseConnectionName, DATABASE_TABLE_EMU_ACHIEVEMENTS, "id = ?", {appId});
    if (mergeSucceeded && totalAchievements >= 0)
    {
        mergeSucceeded = m_databaseManager.update(
            m_databaseConnectionName,
            DATABASE_TABLE_EMU_GAMES,
            {{"total_amount_achievements", totalAchievements}, {"date_updated", now}},
            "id = ?",
            {appId});
    }
    else
    {
        mergeSucceeded = false;
    }

    if (mergeSucceeded && m_databaseManager.commitTransaction(m_databaseConnectionName))
    {
        qDebug() << "Lymalink::ApplyNewAchievements: inserted" << inserted << "updated" << updated << "achievements for appId:" << appId;
        return true;
    }

    m_databaseManager.rollbackTransaction(m_databaseConnectionName);
    qWarning() << "Lymalink::ApplyNewAchievements: failed to merge achievements for appId:" << appId << m_databaseManager.lastError();
    return false;
}

/////////////////////////////////////////////////////////////////////

QString Lymalink::PlaytimeText(int secondsPlayed) const
{
    QString playtimeText = "";

    // Display playtime as largest coarse unit used by details page
    if (secondsPlayed < 3600)   // Less than 1 hour (60 minutes)
    { 
        int minutes = secondsPlayed / 60;
        playtimeText = QString("%1 minute(s)").arg(minutes);
        return playtimeText;
    }
    else                        // 1 hour or more
    { 
        int hours = secondsPlayed / 3600;
        playtimeText = QString("%1 hour(s)").arg(hours);
        return playtimeText;
    }
}

/////////////////////////////////////////////////////////////////////

QVariantMap Lymalink::LatestUnlockedAchievement(const QVariantList &achievements) const
{
    QVariantMap latestAchievement = {};
    qint64 latestUnlockDate = 0;

    // Pick highest unlock timestamp for recent achievement preview
    for (const QVariant &achievementValue : achievements)
    {
        const QVariantMap achievement = achievementValue.toMap();
        const qint64 unlockDate = achievement.value("date_unlocked").toLongLong();
        if (unlockDate > latestUnlockDate)
        {
            latestUnlockDate = unlockDate;
            latestAchievement = achievement;
        }
    }

    return latestAchievement;
}

/////////////////////////////////////////////////////////////////////

QVariantList Lymalink::BuildAchievementDetails(int appId, const QString &iconsPath, const QString &targetType)
{
    QVariantList achievements;
    const QString normalizedTargetType = NormalizeTargetType(targetType);
    const QString achievementTable = AchievementTableForTargetType(normalizedTargetType);
    const bool steamTarget = IsSteamTargetType(normalizedTargetType);

    // Fetch achievement rows needed by target details sections
    QStringList columns = {
        "achievement_key",
        "achievement_name",
        "achievement_description",
        "achievement_hidden",
        "global_unlock_percentage",
        "date_unlocked"
    };
    if (!steamTarget)
    {
        columns << "cur_progress" << "max_progress";
    }

    const QVariantList rows = m_databaseManager.selectWhere(
        m_databaseConnectionName,
        achievementTable,
        "id = ?",
        {appId},
        columns
    );

    QVariantList unlockedAchievements;
    QVariantList lockedAchievements;
    QVariantList hiddenAchievements;

    for (const QVariant &rowValue : rows)
    {
        const QVariantMap row = rowValue.toMap();
        const qint64 unlockTimestamp = row.value("date_unlocked").toLongLong();
        const bool unlocked = unlockTimestamp > 0;
        const bool achievementHidden = row.value("achievement_hidden").toInt() == 1;
        const QString sectionKey = unlocked ? "unlocked" : (achievementHidden ? "achievementHidden" : "locked");
        const QDateTime unlockDateTime = QDateTime::fromSecsSinceEpoch(unlockTimestamp);

        QVariantMap achievement = {
            {"achievementKey", Utils::MapStringValue(row, "achievement_key")},
            {"iconSource", AchievementIconFilePath(iconsPath, row)},
            {"achievementName", Utils::MapStringValue(row, "achievement_name")},
            {"achievementDescription", Utils::MapStringValue(row, "achievement_description")},
            {"globalUnlockPercentage", row.value("global_unlock_percentage").isNull() ? 0.0 : row.value("global_unlock_percentage").toDouble()},
            {"curProgress", steamTarget ? 0 : Utils::MapIntValue(row, "cur_progress")},
            {"maxProgress", steamTarget ? 0 : Utils::MapIntValue(row, "max_progress")},
            {"unlockDate", unlocked ? Utils::LocalDate(unlockTimestamp) : QString()},
            {"unlockTime", unlocked ? QLocale::system().toString(unlockDateTime.time(), "HH:mm") : QString()},
            {"unlockTimestamp", unlockTimestamp},
            {"unlocked", unlocked},
            {"achievementHidden", achievementHidden},
            {"sectionKey", sectionKey}
        };

        if (sectionKey == "unlocked")
        {
            unlockedAchievements << achievement;
        }
        else if (sectionKey == "achievementHidden")
        {
            hiddenAchievements << achievement;
        }
        else
        {
            lockedAchievements << achievement;
        }
    }

    // Preserve UI section order: unlocked, locked, hidden
    achievements << unlockedAchievements << lockedAchievements << hiddenAchievements;
    return achievements;
}

/////////////////////////////////////////////////////////////////////

QString Lymalink::CoverImageFilePath(const QString &coversPath, const QString &fileName) const
{
    QString coverSource = "";

    // Return QML file URL only when expected cover exists
    const QString coverPath = QDir(coversPath).filePath(fileName);
    if (!QFileInfo::exists(coverPath))
    {
        // qDebug() << "Lymalink::CoverImageFilePath: no cover available for" << fileName;
        return coverSource;
    }

    coverSource = m_fileManager.LocalFileSource(coverPath);
    if (fileName.startsWith("custom_"))
    {
        coverSource += "?v=" + QString::number(QFileInfo(coverPath).lastModified().toMSecsSinceEpoch());
    }
    return coverSource;
}

/////////////////////////////////////////////////////////////////////

QString Lymalink::PreferredCoverImageFilePath(const QString &coversPath, const QString &fileName) const
{
    const QString customFileName = "custom_" + fileName;
    const QString customCoverSource = CoverImageFilePath(coversPath, customFileName);
    if (!customCoverSource.isEmpty())
    {
        return customCoverSource;
    }

    if (fileName != "cover_600x900.jpg")
    {
        const QString customScaledCoverSource = CoverImageFilePath(coversPath, "custom_cover_600x900.jpg");
        if (!customScaledCoverSource.isEmpty())
        {
            return customScaledCoverSource;
        }
    }

    const QString coverSource = CoverImageFilePath(coversPath, fileName);
    if (!coverSource.isEmpty())
    {
        return coverSource;
    }

    const QString defaultCoverFileName = "cover_600x900.jpg";
    if (fileName == defaultCoverFileName)
    {
        return QString();
    }

    const QString defaultCoverPath = CoverImageFilePath(coversPath, defaultCoverFileName);
    return defaultCoverPath;
}

/////////////////////////////////////////////////////////////////////

bool Lymalink::TargetHasMissingMetadata(int appId, const QString &targetType)
{
    const QString normalizedTargetType = NormalizeTargetType(targetType);
    const QString appDataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    const QString appIdText = QString::number(appId);
    const QDir targetAppDir(QDir(appDataPath).filePath(AssetFolderForTargetType(normalizedTargetType) + "/" + appIdText));
    const QString coversPath = targetAppDir.filePath("covers");
    const QString iconsPath = targetAppDir.filePath("icons");

    const QDir coversDir(coversPath);
    const QStringList coverFiles = {
        "cover_80x120.jpg",
        "cover_152x228.jpg",
        "cover_200x300.jpg",
        "cover_240x360.jpg",
        "cover_304x456.jpg",
        "cover_360x540.jpg",
        "cover_600x900.jpg"
    };

    const bool missingCover = std::any_of(coverFiles.cbegin(), coverFiles.cend(), [&coversDir](const QString &coverFile) {
        return !QFileInfo::exists(coversDir.filePath(coverFile));
    });
    
    const bool missingAchievementIcons = TargetHasMissingAchievementIcons(appId, iconsPath, normalizedTargetType);

    return missingCover || missingAchievementIcons;
}

/////////////////////////////////////////////////////////////////////

bool Lymalink::TargetHasMissingAchievementIcons(int appId, const QString &iconsPath, const QString &targetType)
{
    const QString achievementTable = AchievementTableForTargetType(targetType);
    const QVariantList rows = m_databaseManager.selectWhere(
        m_databaseConnectionName,
        achievementTable,
        "id = ?",
        {appId},
        {"achievement_key"}
    );
    if (!m_databaseManager.lastError().isEmpty())
    {
        qWarning() << "Lymalink::TargetHasMissingAchievementIcons: failed to fetch achievements:" << appId << targetType << m_databaseManager.lastError();
        return false;
    }

    const QDir iconsDir(iconsPath);
    for (const QVariant &rowValue : rows)
    {
        const QVariantMap row = rowValue.toMap();
        const QString achievementKey = Utils::MapStringValue(row, "achievement_key");
        if (achievementKey.isEmpty())
        {
            continue;
        }

        const bool hasIcon = QFileInfo::exists(iconsDir.filePath(achievementKey + "_icon.jpg"));
        const bool hasGrayIcon = QFileInfo::exists(iconsDir.filePath(achievementKey + "_gray_icon.jpg"));
        if (!hasIcon || !hasGrayIcon)
        {
            return true;
        }
    }

    return false;
}

/////////////////////////////////////////////////////////////////////

bool Lymalink::SaveCustomCoverVariant(const QImage &sourceImage, const QString &coversPath, const QString &fileName, const QSize &targetSize, bool enableBlackBars) const
{
    if (sourceImage.isNull() || coversPath.isEmpty() || fileName.isEmpty() || !targetSize.isValid())
    {
        qWarning() << "Lymalink::SaveCustomCoverVariant: invalid parameters:" << coversPath << fileName << targetSize;
        return false;
    }

    // Write the generated variant atomically so either render mode shares the same save path
    const auto saveResult = [&](const QImage &result) {
        const QString finalPath = QDir(coversPath).filePath(fileName);
        const QString tempPath = QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation)).filePath(fileName + ".tmp");
        if (!result.save(tempPath, "JPG", 90))
        {
            qWarning() << "Lymalink::SaveCustomCoverVariant: failed to save temp file:" << tempPath;
            return false;
        }

        QFile::remove(finalPath);
        if (!QFile::rename(tempPath, finalPath))
        {
            QFile::remove(tempPath);
            qWarning() << "Lymalink::SaveCustomCoverVariant: failed to move cover variant:" << tempPath << finalPath;
            return false;
        }

        return true;
    };

    if (enableBlackBars)
    {
        // Preserve the entire source image on an exact-ratio black canvas instead of cropping it
        QImage scaledImage = sourceImage;
        if (sourceImage.width() > targetSize.width() || sourceImage.height() > targetSize.height())
        {
            scaledImage = sourceImage.scaled(targetSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        }

        QImage result(targetSize, QImage::Format_RGB32);
        result.fill(Qt::black);
        QPainter painter(&result);
        painter.drawImage((targetSize.width() - scaledImage.width()) / 2, (targetSize.height() - scaledImage.height()) / 2, scaledImage);

        return saveResult(result);
    }

    // Reduce target dimensions to exact crop ratio
    const int targetGcd = std::gcd(targetSize.width(), targetSize.height());
    const int targetRatioWidth = targetSize.width() / targetGcd;
    const int targetRatioHeight = targetSize.height() / targetGcd;
    QRect cropRect = sourceImage.rect();

    if (static_cast<qint64>(sourceImage.width()) * targetRatioHeight > static_cast<qint64>(sourceImage.height()) * targetRatioWidth)
    {
        // Source too wide: remove equal left/right area
        const int cropHeight = sourceImage.height() - sourceImage.height() % targetRatioHeight;
        const int cropWidth = cropHeight / targetRatioHeight * targetRatioWidth;
        if (cropWidth > 0)
        {
            cropRect.setX((sourceImage.width() - cropWidth) / 2);
            cropRect.setWidth(cropWidth);
        }
    }
    else if (static_cast<qint64>(sourceImage.width()) * targetRatioHeight < static_cast<qint64>(sourceImage.height()) * targetRatioWidth)
    {
        // Source too tall: remove equal top/bottom area
        const int cropWidth = sourceImage.width() - sourceImage.width() % targetRatioWidth;
        const int cropHeight = cropWidth / targetRatioWidth * targetRatioHeight;
        if (cropHeight > 0)
        {
            cropRect.setY((sourceImage.height() - cropHeight) / 2);
            cropRect.setHeight(cropHeight);
        }
    }

    // Crop before resize so QML does not upscale a wrong-ratio variant
    const QImage croppedImage = sourceImage.copy(cropRect);
    QImage result;
    if (croppedImage.width() > targetSize.width() || croppedImage.height() > targetSize.height())
    {
        // Downscale only - preserve small source images
        result = croppedImage.scaled(targetSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    }
    else
    {
        result = croppedImage;
    }

    return saveResult(result);
}

/////////////////////////////////////////////////////////////////////

QString Lymalink::CommunityIconFilePath(const QString &iconsPath) const
{
    QString iconSource = "";

    // Locate downloaded community icon regardless of extension
    QDir iconsDir(iconsPath);
    if (!iconsDir.exists())
    {
        return iconSource;
    }

    const QFileInfoList iconFiles = iconsDir.entryInfoList({"community_icon.*"}, QDir::Files, QDir::Name | QDir::IgnoreCase);
    if (iconFiles.isEmpty())
    {
        return iconSource;
    }

    iconSource = m_fileManager.LocalFileSource(iconFiles.first().filePath());
    return iconSource;
}

/////////////////////////////////////////////////////////////////////

QString Lymalink::AchievementIconFilePath(const QString &iconsPath, const QVariantMap &achievement) const
{
    QString iconSource = "";

    // Resolve icon by achievement key and unlock state
    const QString achievementKey = Utils::MapStringValue(achievement, "achievement_key");
    if (achievementKey.isEmpty())
    {
        qDebug() << "Lymalink::AchievementIconFilePath: achievement key missing" << achievementKey;
        return iconSource;
    }

    const bool unlocked = achievement.value("date_unlocked").toLongLong() > 0;
    const QString iconFileName = achievementKey + (unlocked ? "_icon.jpg" : "_gray_icon.jpg");
    const QString iconPath = QDir(iconsPath).filePath(iconFileName);

    if (!QFileInfo::exists(iconPath))
    {
        qDebug() << "Lymalink::AchievementIconFilePath: no file exists for" << iconPath;
        return iconSource;
    }

    iconSource = m_fileManager.LocalFileSource(iconPath);
    return iconSource;
}

/////////////////////////////////////////////////////////////////////

QString Lymalink::ExecutableInstallationStatus(const QVariantMap &row) const
{
    QString installationStatus = "";

    // Missing executable path means target has not been installed/configured
    const QString executableLocation = Utils::MapStringValue(row, "executable_location");
    if (executableLocation.isEmpty())
    {
        installationStatus = tr("Not Installed");
        return installationStatus;
    }

    const QFileInfo executableInfo(executableLocation);
    installationStatus = executableInfo.exists() && executableInfo.isFile() ? tr("Installed") : tr("Not Installed");
    return installationStatus;
}

/////////////////////////////////////////////////////////////////////

bool Lymalink::IsTargetExecutableLocationInUse(const QString &executablePath, int excludedAppId, bool *querySucceeded)
{
    if (querySucceeded != nullptr)
    {
        *querySucceeded = false;
    }

    const int existingRows = m_databaseManager.count(
        m_databaseConnectionName,
        DATABASE_TABLE_EMU_GAMES,
        "TRIM(executable_location) = ? AND id != ?",
        {executablePath.trimmed(), excludedAppId}
    );
    if (existingRows < 0)
    {
        return false;
    }

    if (querySucceeded != nullptr)
    {
        *querySucceeded = true;
    }
    return existingRows > 0;
}

/////////////////////////////////////////////////////////////////////

QString Lymalink::NormalizeTargetType(const QString &targetType) const
{
    return targetType.trimmed().compare("Steam", Qt::CaseInsensitive) == 0 ? "Steam" : "Emulator";
}

/////////////////////////////////////////////////////////////////////

QString Lymalink::GameTableForTargetType(const QString &targetType) const
{
    return IsSteamTargetType(targetType) ? DATABASE_TABLE_GAMES : DATABASE_TABLE_EMU_GAMES;
}

/////////////////////////////////////////////////////////////////////

QString Lymalink::AchievementTableForTargetType(const QString &targetType) const
{
    return IsSteamTargetType(targetType) ? DATABASE_TABLE_ACHIEVEMENTS : DATABASE_TABLE_EMU_ACHIEVEMENTS;
}

/////////////////////////////////////////////////////////////////////

QString Lymalink::AssetFolderForTargetType(const QString &targetType) const
{
    return IsSteamTargetType(targetType) ? "Steam" : "Emulator";
}

/////////////////////////////////////////////////////////////////////

bool Lymalink::IsSteamTargetType(const QString &targetType) const
{
    return targetType.trimmed().compare("Steam", Qt::CaseInsensitive) == 0;
}
