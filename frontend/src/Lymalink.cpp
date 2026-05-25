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
#include "tools/Utils.h"

#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QVariantMap>

/////////////////////////////////////////////////////////////////////

Lymalink::Lymalink(QObject *parent) : QObject(parent)
{
    m_databaseConnectionName = DATABASE_CONNECTION_NAME;
    m_databasePath = "";
    m_steamApiSearchWorker = nullptr;
    m_steamApiHydrationWorker = nullptr;
}

Lymalink::~Lymalink()
{
    m_searchWorkerThread.quit();
    m_searchWorkerThread.wait();

    m_hydrationWorkerThread.quit();
    m_hydrationWorkerThread.wait();
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
    connect(this, &Lymalink::signalRequestCancelSteamHydration,      m_steamApiHydrationWorker, &SteamApiHydrationWorker::CancelAllEnqueueTasks);
    connect(m_steamApiHydrationWorker, &SteamApiHydrationWorker::signalHydrationTaskStarted,   this, &Lymalink::signalSteamHydrationTaskStarted);
    connect(m_steamApiHydrationWorker, &SteamApiHydrationWorker::signalHydrationTaskProgress,  this, &Lymalink::signalSteamHydrationTaskProgress);
    connect(m_steamApiHydrationWorker, &SteamApiHydrationWorker::signalHydrationTaskFinished,  this, &Lymalink::signalSteamHydrationTaskFinished);
    connect(m_steamApiHydrationWorker, &SteamApiHydrationWorker::signalHydrationQueueFinished, this, &Lymalink::signalSteamHydrationQueueFinished);
    connect(m_steamApiHydrationWorker, &SteamApiHydrationWorker::signalHydrationTaskError, this,
        [this](int appId, const QString &title, const QString &message) {
            emit signalErrorOccurred(title, QString("%1\n\nApp ID: %2").arg(message).arg(appId));
        });
    connect(m_steamApiHydrationWorker, &SteamApiHydrationWorker::signalAchievementsReady, this, &Lymalink::ApplyNewAchievements);
    m_hydrationWorkerThread.start();

    initResult = DatabaseInit();
    return initResult;
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

void Lymalink::EnqueueSteamHydrationTask(int appId, bool reloadAssets)
{
    emit signalRequestEnqueueSteamHydrationTask(appId, reloadAssets);
}

/////////////////////////////////////////////////////////////////////

void Lymalink::CancelSteamHydration()
{
    emit signalRequestCancelSteamHydration();
}

/////////////////////////////////////////////////////////////////////

bool Lymalink::CreateNewSteamEmuTarget(int appId, QString gameName, QString exePath, QString prefixPath)
{
    bool targetCreated = false;

    // Normalize user-provided paths before validation/database write
    gameName = gameName.trimmed();
    exePath = exePath.trimmed();
    prefixPath = prefixPath.trimmed();

    if (appId <= 0 || gameName.isEmpty() || exePath.isEmpty() || prefixPath.isEmpty())
    {
        qWarning() << "Lymalink: invalid emulator target data";
        return targetCreated;
    }

    const QString appDataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (appDataPath.isEmpty())
    {
        qCritical() << "Lymalink: failed to resolve app data location for emulator target";
        return targetCreated;
    }

    // Open database lazily for QML calls made after startup
    if (!m_databaseManager.isDatabaseOpen(m_databaseConnectionName) && !m_databaseManager.openDatabase(m_databaseConnectionName, m_databasePath))
    {
        qCritical() << "Lymalink: failed to open database for emulator target:" << m_databaseManager.lastError();
        return targetCreated;
    }

    const QString appIdText = QString::number(appId);
    QDir emulatorDir(QDir(appDataPath).filePath("Emulator"));
    const QString targetPath = emulatorDir.filePath(appIdText);

    // Check filesystem first so target folder conflict blocks creation
    if (QFileInfo::exists(targetPath))
    {
        qWarning() << "Lymalink: emulator target already exists on disk:" << targetPath;
        return targetCreated;
    }

    // Check database entry to avoid duplicate target rows
    const int existingGameRows = m_databaseManager.count(m_databaseConnectionName, DATABASE_TABLE_EMU_GAMES, "id = ?", {appId});
    if (existingGameRows < 0)
    {
        qCritical() << "Lymalink: failed to check emulator target database row:" << m_databaseManager.lastError();
        return targetCreated;
    }

    if (existingGameRows > 0)
    {
        qWarning() << "Lymalink: emulator target already exists in database, skipping creation:" << appId;
        return targetCreated;
    }

    // Create per-target asset folders before database insert
    if (!emulatorDir.mkpath(appIdText) || !emulatorDir.mkpath(appIdText + "/icons") || !emulatorDir.mkpath(appIdText + "/covers"))
    {
        qCritical() << "Lymalink: failed to create emulator target folders:" << targetPath;
        return targetCreated;
    }

    // Insert target metadata after filesystem is ready
    const QVariantMap data = {
        {"id", appId},
        {"game_name", gameName},
        {"executable_location", exePath},
        {"prefix_location", prefixPath},
        {"date_added", QDateTime::currentSecsSinceEpoch()}
    };

    if (!m_databaseManager.insert(m_databaseConnectionName, DATABASE_TABLE_EMU_GAMES, data))
    {
        // Roll back folder creation when database insert fails
        QDir cleanupDir(targetPath);
        cleanupDir.removeRecursively();
        qCritical() << "Lymalink: failed to insert emulator target:" << m_databaseManager.lastError();
        return targetCreated;
    }

    qDebug() << "Lymalink: emulator target created:" << appId << targetPath;
    targetCreated = true;
    return targetCreated;
}

/////////////////////////////////////////////////////////////////////

bool Lymalink::SetTargetHidden(int appId, bool hidden)
{
    bool targetUpdated = false;

    if (appId <= 0)
    {
        qWarning() << "Lymalink: invalid appId for target hidden update:" << appId;
        return targetUpdated;
    }

    // Ensure database is open before toggling visibility
    if (!m_databaseManager.isDatabaseOpen(m_databaseConnectionName) && !m_databaseManager.openDatabase(m_databaseConnectionName, m_databasePath))
    {
        qCritical() << "Lymalink: failed to open database for target hidden update:" << m_databaseManager.lastError();
        return targetUpdated;
    }

    targetUpdated = m_databaseManager.update(
        m_databaseConnectionName,
        DATABASE_TABLE_EMU_GAMES,
        {{"target_hidden", hidden ? 1 : 0}},
        "id = ?",
        {appId}
    );

    if (!targetUpdated)
    {
        qCritical() << "Lymalink: failed to update target hidden state:" << m_databaseManager.lastError();
    }

    return targetUpdated;
}

/////////////////////////////////////////////////////////////////////

bool Lymalink::SetTargetPrefixLocation(int appId, const QString &prefixPath)
{
    bool targetUpdated = false;

    // Prefix changes invalidate discovered Steam appid directory cache
    const QString trimmedPrefixPath = prefixPath.trimmed();
    if (appId <= 0 || trimmedPrefixPath.isEmpty())
    {
        qWarning() << "Lymalink: invalid target prefix location update:" << appId << trimmedPrefixPath;
        return targetUpdated;
    }

    if (!m_databaseManager.isDatabaseOpen(m_databaseConnectionName) && !m_databaseManager.openDatabase(m_databaseConnectionName, m_databasePath))
    {
        qCritical() << "Lymalink: failed to open database for target prefix location update:" << m_databaseManager.lastError();
        return targetUpdated;
    }

    targetUpdated = m_databaseManager.update(
        m_databaseConnectionName,
        DATABASE_TABLE_EMU_GAMES,
        {
            {"prefix_location", trimmedPrefixPath},
            {"appid_dir_found", 0},
            {"appid_dir_location", ""},
            {"date_updated", QDateTime::currentSecsSinceEpoch()}
        },
        "id = ?",
        {appId}
    );

    if (!targetUpdated)
    {
        qCritical() << "Lymalink: failed to update target prefix location:" << m_databaseManager.lastError();
    }

    return targetUpdated;
}

/////////////////////////////////////////////////////////////////////

bool Lymalink::SetTargetExecutableLocation(int appId, const QString &executablePath)
{
    bool targetUpdated = false;

    // Store trimmed executable path and update modification timestamp
    const QString trimmedExecutablePath = executablePath.trimmed();
    if (appId <= 0 || trimmedExecutablePath.isEmpty())
    {
        qWarning() << "Lymalink: invalid target executable location update:" << appId << trimmedExecutablePath;
        return targetUpdated;
    }

    if (!m_databaseManager.isDatabaseOpen(m_databaseConnectionName) && !m_databaseManager.openDatabase(m_databaseConnectionName, m_databasePath))
    {
        qCritical() << "Lymalink: failed to open database for target executable location update:" << m_databaseManager.lastError();
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
        qCritical() << "Lymalink: failed to update target executable location:" << m_databaseManager.lastError();
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
        qWarning() << "Lymalink: invalid achievement unlock update:" << appId << trimmedKey;
        return achievementStateUpdated;
    }

    if (!m_databaseManager.isDatabaseOpen(m_databaseConnectionName) && !m_databaseManager.openDatabase(m_databaseConnectionName, m_databasePath))
    {
        qCritical() << "Lymalink: failed to open database for achievement unlock update:" << m_databaseManager.lastError();
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
        qWarning() << "Lymalink: achievement not found for unlock update:" << appId << trimmedKey;
        return achievementStateUpdated;
    }

    // Use provided unlock time when present; otherwise current time
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    const qint64 normalizedUnlockTimestamp = unlocked
        ? (unlockTimestamp > 0 ? unlockTimestamp : now)
        : 0;

    const bool achievementUpdated = m_databaseManager.update(
        m_databaseConnectionName,
        DATABASE_TABLE_EMU_ACHIEVEMENTS,
        {
            {"date_unlocked", normalizedUnlockTimestamp},
            {"date_updated", now}
        },
        "id = ? AND achievement_key = ?",
        {appId, trimmedKey}
    );
    if (!achievementUpdated)
    {
        qCritical() << "Lymalink: failed to update achievement unlock state:" << m_databaseManager.lastError();
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
        qCritical() << "Lymalink: failed to count unlocked achievements:" << m_databaseManager.lastError();
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
        qCritical() << "Lymalink: failed to update target achievement count:" << m_databaseManager.lastError();
    }

    return achievementStateUpdated;
}

/////////////////////////////////////////////////////////////////////

bool Lymalink::DeleteTarget(int appId)
{
    bool targetDeleted = false;

    if (appId <= 0)
    {
        qWarning() << "Lymalink: invalid appId for target delete:" << appId;
        return targetDeleted;
    }

    if (!m_databaseManager.isDatabaseOpen(m_databaseConnectionName) && !m_databaseManager.openDatabase(m_databaseConnectionName, m_databasePath))
    {
        qCritical() << "Lymalink: failed to open database for target delete:" << m_databaseManager.lastError();
        return targetDeleted;
    }

    // Require target row before deleting assets/database data
    const QVariantMap row = m_databaseManager.selectFirst(m_databaseConnectionName, DATABASE_TABLE_EMU_GAMES, "id = ?", {appId});
    if (row.isEmpty())
    {
        qWarning() << "Lymalink: target delete row not found:" << appId;
        return targetDeleted;
    }

    const QString appDataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (appDataPath.isEmpty())
    {
        qCritical() << "Lymalink: failed to resolve app data location for target delete";
        return targetDeleted;
    }

    const QString targetPath = QDir(appDataPath).filePath("Emulator/" + QString::number(appId));

    if (!m_databaseManager.beginTransaction(m_databaseConnectionName))
    {
        qCritical() << "Lymalink: failed to begin target delete transaction:" << m_databaseManager.lastError();
        return targetDeleted;
    }

    // Delete game row first; achievement rows cascade through foreign key
    const bool targetRemoved = m_databaseManager.remove(
        m_databaseConnectionName,
        DATABASE_TABLE_EMU_GAMES,
        "id = ?",
        {appId}
    );

    if (!targetRemoved)
    {
        m_databaseManager.rollbackTransaction(m_databaseConnectionName);
        qCritical() << "Lymalink: failed to delete target:" << appId << m_databaseManager.lastError();
        return targetDeleted;
    }

    if (!m_databaseManager.commitTransaction(m_databaseConnectionName))
    {
        m_databaseManager.rollbackTransaction(m_databaseConnectionName);
        qCritical() << "Lymalink: failed to commit target delete:" << m_databaseManager.lastError();
        return targetDeleted;
    }

    // Remove assets after database commit so UI no longer references target
    const bool assetsRemoved = !QFileInfo::exists(targetPath) || m_fileManager.DeleteFolder(targetPath);
    if (!assetsRemoved)
    {
        qWarning() << "Lymalink: target deleted from database but asset removal failed:" << targetPath;
        return targetDeleted;
    }

    qDebug() << "Lymalink: target deleted:" << appId;
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

QVariantList Lymalink::FetchDashboardTargets()
{
    QVariantList targets;

    if (!m_databaseManager.isDatabaseOpen(m_databaseConnectionName) && !m_databaseManager.openDatabase(m_databaseConnectionName, m_databasePath))
    {
        qCritical() << "Lymalink: failed to open database for dashboard targets:" << m_databaseManager.lastError();
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
        const QString coverSourceCard = CoverImageFilePath(coversPath, "cover_200x300.jpg");
        const QString coverSourceCardSmall = CoverImageFilePath(coversPath, "cover_150x225.jpg");
        const QString coverSourceRowDetailed = CoverImageFilePath(coversPath, "cover_80x120.jpg");
        const QString coverSourceTargetDetails = CoverImageFilePath(coversPath, "cover_240x360.jpg");
        const QString coverSource = !coverSourceCard.isEmpty() ? coverSourceCard : m_fileManager.FirstImageInDirectory(coversPath);

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
            {"coverSource", coverSource},
            {"coverSourceCard", coverSourceCard.isEmpty() ? coverSource : coverSourceCard},
            {"coverSourceCardSmall", coverSourceCardSmall.isEmpty() ? coverSource : coverSourceCardSmall},
            {"coverSourceRowDetailed", coverSourceRowDetailed.isEmpty() ? coverSource : coverSourceRowDetailed},
            {"coverSourceTargetDetails", coverSourceTargetDetails.isEmpty() ? coverSource : coverSourceTargetDetails},
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

    return targets;
}

/////////////////////////////////////////////////////////////////////

QVariantMap Lymalink::FetchTargetDetails(int appId)
{
    QVariantMap targetDetails = {};

    if (appId <= 0)
    {
        qWarning() << "Lymalink: invalid appId for target details:" << appId;
        return targetDetails;
    }

    if (!m_databaseManager.isDatabaseOpen(m_databaseConnectionName) && !m_databaseManager.openDatabase(m_databaseConnectionName, m_databasePath))
    {
        qCritical() << "Lymalink: failed to open database for target details:" << m_databaseManager.lastError();
        return targetDetails;
    }

    // Fetch target row before building heavy details payload
    const QVariantMap row = m_databaseManager.selectFirst(m_databaseConnectionName, DATABASE_TABLE_EMU_GAMES, "id = ?", {appId});
    if (row.isEmpty())
    {
        qWarning() << "Lymalink: target details not found for appId:" << appId;
        return targetDetails;
    }

    const QString appDataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    const QString appIdText = QString::number(appId);
    const QDir emulatorAppDir(QDir(appDataPath).filePath("Emulator/" + appIdText));
    const QString coversPath = emulatorAppDir.filePath("covers");
    const QString iconsPath = emulatorAppDir.filePath("icons");

    // Prefer detail-sized cover, then fallback to first image in covers folder
    const QString coverSourceTargetDetails = CoverImageFilePath(coversPath, "cover_240x360.jpg");
    const QString coverSource = !coverSourceTargetDetails.isEmpty() ? coverSourceTargetDetails : m_fileManager.FirstImageInDirectory(coversPath);
    const QVariantList achievements = BuildAchievementDetails(appId, iconsPath);
    const QVariantMap latestAchievement = LatestUnlockedAchievement(m_databaseManager.selectWhere(
        m_databaseConnectionName,
        DATABASE_TABLE_EMU_ACHIEVEMENTS,
        "id = ?",
        {appId},
        {"achievement_key", "achievement_name", "achievement_description", "date_unlocked"}
    ));

    targetDetails = {
        {"id", appId},
        {"title", Utils::MapStringValue(row, "game_name")},
        {"coverSource", coverSource},
        {"coverSourceTargetDetails", coverSource},
        {"achievementCount", Utils::MapIntValue(row, "total_unlocked_amount_achievements")},
        {"achievementTotal", Utils::MapIntValue(row, "total_amount_achievements")},
        {"targetType", "Emulator"},
        {"installationStatus", ExecutableInstallationStatus(row)},
        {"lastPlayed", Utils::LocalDate(row.value("last_played_date").toLongLong())},
        {"recentUnlock", Utils::LocalDate(latestAchievement.value("date_unlocked").toLongLong())},
        {"playtime", PlaytimeText(Utils::MapIntValue(row, "total_seconds_played"))},
        {"targetHidden", row.value("target_hidden").toInt() == 1},
        {"achievements", achievements}
    };
    return targetDetails;
}

/////////////////////////////////////////////////////////////////////
///////////////////////////// PRIVATE ///////////////////////////////
/////////////////////////////////////////////////////////////////////

Error Lymalink::DatabaseInit()
{
    Error databaseResult = Error::NoError;

    // Resolve app data path for SQLite file location
    const QString appDataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (appDataPath.isEmpty())
    {
        qCritical() << "Lymalink: failed to resolve app data location";
        databaseResult = Error::FileSystemError;
        return databaseResult;
    }

    m_databasePath = QDir(appDataPath).filePath(DATABASE_FILE_NAME);
    if (m_databaseManager.databaseFileExists(m_databasePath))
    {
        // Open existing database file
        qDebug() << "Lymalink: database already exists at" << m_databasePath;
        if (!m_databaseManager.openDatabase(m_databaseConnectionName, m_databasePath))
        {
            qCritical() << "Lymalink: failed to open database:" << m_databaseManager.lastError();
            databaseResult = Error::DatabaseError;
            return databaseResult;
        }
    }
    else if (!m_databaseManager.createDatabase(m_databaseConnectionName, m_databasePath))
    {
        qCritical() << "Lymalink: failed to create/open database:" << m_databaseManager.lastError();
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
            "target_hidden INTEGER DEFAULT 0",
            "appid_dir_found INTEGER DEFAULT 0",
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
        qCritical() << "Lymalink: failed to initialize" << DATABASE_TABLE_EMU_GAMES << "table:" << m_databaseManager.lastError();
        databaseResult = Error::DatabaseError;
        return databaseResult;
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
            "icon_gray_url TEXT",
            "date_unlocked INTEGER",
            "date_updated INTEGER",
            "date_added INTEGER",
            "PRIMARY KEY (id, achievement_key)",
            QString("FOREIGN KEY (id) REFERENCES %1(id) ON DELETE CASCADE").arg(DATABASE_TABLE_EMU_GAMES)
        }
    );

    if (!achievementsReady)
    {
        qCritical() << "Lymalink: failed to initialize" << DATABASE_TABLE_EMU_ACHIEVEMENTS << "table:" << m_databaseManager.lastError();
        databaseResult = Error::DatabaseError;
        return databaseResult;
    }

    qDebug() << "Lymalink: database initialized at" << m_databasePath;
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
        qCritical() << "Lymalink: failed to resolve app data location for filesystem init";
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
            qCritical() << "Lymalink: failed to create folder:" << appDataDir.filePath(folderName);
            fileSystemResult = Error::FileSystemError;
            return fileSystemResult;
        }
    }

    qDebug() << "Lymalink: filesystem initialized at" << appDataPath;
    return fileSystemResult;
}

/////////////////////////////////////////////////////////////////////

void Lymalink::ApplyNewAchievements(int appId, QVariantList achievements)
{
    int inserted = 0;

    // Insert only new achievement keys from hydration payload
    for (const QVariant &val : achievements)
    {
        QVariantMap entry = val.toMap();
        entry["id"] = appId;

        const QString achievementKey = Utils::MapStringValue(entry, "achievement_key");
        if (achievementKey.isEmpty())
        {
            continue;
        }

        const int existingRows = m_databaseManager.count(
            m_databaseConnectionName,
            DATABASE_TABLE_EMU_ACHIEVEMENTS,
            "id = ? AND achievement_key = ?",
            {appId, achievementKey}
        );
        if (existingRows > 0)
        {
            continue;
        }

        if (!m_databaseManager.insert(m_databaseConnectionName, DATABASE_TABLE_EMU_ACHIEVEMENTS, entry))
        {
            qWarning() << "Lymalink: failed to insert achievement:" << achievementKey << m_databaseManager.lastError();
            continue;
        }
        ++inserted;
    }

    if (inserted > 0)
    {
        // Refresh target achievement total after inserting new rows
        const qint64 now = QDateTime::currentSecsSinceEpoch();
        const int totalAchievements = m_databaseManager.count(m_databaseConnectionName, DATABASE_TABLE_EMU_ACHIEVEMENTS, "id = ?", {appId});
        if (!m_databaseManager.update(
            m_databaseConnectionName,
            DATABASE_TABLE_EMU_GAMES,
            {{"total_amount_achievements", totalAchievements}, {"date_updated", now}},
            "id = ?",
            {appId}))
        {
            qWarning() << "Lymalink: failed to update achievement count for appId:" << appId;
        }
    }

    qDebug() << "Lymalink: inserted" << inserted << "new achievements for appId:" << appId;
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

QVariantList Lymalink::BuildAchievementDetails(int appId, const QString &iconsPath)
{
    QVariantList achievements;

    // Fetch achievement rows needed by target details sections
    const QVariantList rows = m_databaseManager.selectWhere(
        m_databaseConnectionName,
        DATABASE_TABLE_EMU_ACHIEVEMENTS,
        "id = ?",
        {appId},
        {
            "achievement_key",
            "achievement_name",
            "achievement_description",
            "achievement_hidden",
            "global_unlock_percentage",
            "date_unlocked"
        }
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

        QVariantMap achievement = {
            {"achievementKey", Utils::MapStringValue(row, "achievement_key")},
            {"iconSource", AchievementIconFilePath(iconsPath, row)},
            {"achievementName", Utils::MapStringValue(row, "achievement_name")},
            {"achievementDescription", Utils::MapStringValue(row, "achievement_description")},
            {"globalUnlockPercentage", row.value("global_unlock_percentage").isNull() ? 0.0 : row.value("global_unlock_percentage").toDouble()},
            {"unlockDate", unlocked ? Utils::LocalDate(unlockTimestamp) : QString()},
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
        return coverSource;
    }

    coverSource = m_fileManager.LocalFileSource(coverPath);
    return coverSource;
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
        return iconSource;
    }

    const bool unlocked = achievement.value("date_unlocked").toLongLong() > 0;
    const QString iconFileName = achievementKey + (unlocked ? "_icon.jpg" : "_gray_icon.jpg");
    const QString iconPath = QDir(iconsPath).filePath(iconFileName);

    if (!QFileInfo::exists(iconPath))
    {
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
