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
    const Error fileSystemError = FileSystemInit();
    if (fileSystemError != Error::NoError)
    {
        return fileSystemError;
    }

    // SteamApiSearchWorker
    m_steamApiSearchWorker = new SteamApiSearchWorker();
    m_steamApiSearchWorker->moveToThread(&m_searchWorkerThread);
    connect(&m_searchWorkerThread, &QThread::started,  m_steamApiSearchWorker, &SteamApiSearchWorker::Init);
    connect(&m_searchWorkerThread, &QThread::finished, m_steamApiSearchWorker, &QObject::deleteLater);
    connect(this, &Lymalink::signalRequestSearchSteamAppIds,        m_steamApiSearchWorker, &SteamApiSearchWorker::SearchAppIds);
    connect(this, &Lymalink::signalRequestCancelSearchSteamAppIds,  m_steamApiSearchWorker, &SteamApiSearchWorker::CancelSearchAppIds);
    connect(m_steamApiSearchWorker, &SteamApiSearchWorker::signalSearchAppIdsFinished, this, &Lymalink::signalSteamAppIdsSearchReady);
    m_searchWorkerThread.start();

    // SteamApiHydrationWorker
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

    return DatabaseInit();
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
    gameName = gameName.trimmed();
    exePath = exePath.trimmed();
    prefixPath = prefixPath.trimmed();

    if (appId <= 0 || gameName.isEmpty() || exePath.isEmpty() || prefixPath.isEmpty())
    {
        qWarning() << "Lymalink: invalid emulator target data";
        return false;
    }

    const QString appDataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (appDataPath.isEmpty())
    {
        qCritical() << "Lymalink: failed to resolve app data location for emulator target";
        return false;
    }

    if (!m_databaseManager.isDatabaseOpen(m_databaseConnectionName) && !m_databaseManager.openDatabase(m_databaseConnectionName, m_databasePath))
    {
        qCritical() << "Lymalink: failed to open database for emulator target:" << m_databaseManager.lastError();
        return false;
    }

    const QString appIdText = QString::number(appId);
    QDir emulatorDir(QDir(appDataPath).filePath("Emulator"));
    const QString targetPath = emulatorDir.filePath(appIdText);

    // Check filesystem first
    if (QFileInfo::exists(targetPath))
    {
        qWarning() << "Lymalink: emulator target already exists on disk:" << targetPath;
        return false;
    }

    // Check database entry
    const int existingGameRows = m_databaseManager.count(m_databaseConnectionName, "steam_emu_games", "id = ?", {appId});
    if (existingGameRows < 0)
    {
        qCritical() << "Lymalink: failed to check emulator target database row:" << m_databaseManager.lastError();
        return false;
    }

    // If database entry exists, block creation and return early
    if (existingGameRows > 0)
    {
        qWarning() << "Lymalink: emulator target already exists in database, skipping creation:" << appId;
        return false;
    }

    // Create directory structure
    if (!emulatorDir.mkpath(appIdText) || !emulatorDir.mkpath(appIdText + "/icons") || !emulatorDir.mkpath(appIdText + "/covers"))
    {
        qCritical() << "Lymalink: failed to create emulator target folders:" << targetPath;
        return false;
    }

    // Insert into database
    const QVariantMap data = {
        {"id", appId},
        {"game_name", gameName},
        {"executable_location", exePath},
        {"prefix_location", prefixPath},
        {"date_added", QDateTime::currentSecsSinceEpoch()}
    };

    if (!m_databaseManager.insert(m_databaseConnectionName, "steam_emu_games", data))
    {
        QDir cleanupDir(targetPath);
        cleanupDir.removeRecursively();
        qCritical() << "Lymalink: failed to insert emulator target:" << m_databaseManager.lastError();
        return false;
    }

    qDebug() << "Lymalink: emulator target created:" << appId << targetPath;
    return true;
}

/////////////////////////////////////////////////////////////////////

QVariantList Lymalink::FetchDashboardTargets()
{
    if (!m_databaseManager.isDatabaseOpen(m_databaseConnectionName) && !m_databaseManager.openDatabase(m_databaseConnectionName, m_databasePath))
    {
        qCritical() << "Lymalink: failed to open database for dashboard targets:" << m_databaseManager.lastError();
        return {};
    }

    const QVariantList rows = m_databaseManager.selectAll(
        m_databaseConnectionName,
        "steam_emu_games",
        {
            "id",
            "game_name",
            "executable_location",
            "total_amount_achievements",
            "total_unlocked_amount_achievements",
            "last_played_date",
            "date_added"
        }
    );

    const QString appDataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QVariantList targets;
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

        const QVariantList achievements = m_databaseManager.selectWhere(
            m_databaseConnectionName,
            "steam_emu_achievements",
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
        
        QVariantMap target = {
            {"id", appId},
            {"title", Utils::MapStringValue(row, "game_name")},
            {"coverSource", coverSource},
            {"coverSourceCard", coverSourceCard.isEmpty() ? coverSource : coverSourceCard},
            {"coverSourceCardSmall", coverSourceCardSmall.isEmpty() ? coverSource : coverSourceCardSmall},
            {"coverSourceRowDetailed", coverSourceRowDetailed.isEmpty() ? coverSource : coverSourceRowDetailed},
            {"coverSourceTargetDetails", coverSourceTargetDetails.isEmpty() ? coverSource : coverSourceTargetDetails},
            {"logoSource", CommunityIconFilePath(iconsPath)},
            {"achievementCount", Utils::MapIntValue(row, "total_unlocked_amount_achievements")},
            {"achievementTotal", Utils::MapIntValue(row, "total_amount_achievements")},
            {"targetType", "Emulator"},
            {"status", ExecutableInstallationStatus(row)},
            {"lastPlayed", Utils::RelativeTime(row.value("last_played_date").toLongLong())},
            {"recentUnlock", Utils::LocalDate(latestAchievement.value("date_unlocked").toLongLong())},
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
    if (appId <= 0)
    {
        qWarning() << "Lymalink: invalid appId for target details:" << appId;
        return {};
    }

    if (!m_databaseManager.isDatabaseOpen(m_databaseConnectionName) && !m_databaseManager.openDatabase(m_databaseConnectionName, m_databasePath))
    {
        qCritical() << "Lymalink: failed to open database for target details:" << m_databaseManager.lastError();
        return {};
    }

    const QVariantMap row = m_databaseManager.selectFirst(m_databaseConnectionName, "steam_emu_games", "id = ?", {appId});
    if (row.isEmpty())
    {
        qWarning() << "Lymalink: target details not found for appId:" << appId;
        return {};
    }

    const QString appDataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    const QString appIdText = QString::number(appId);
    const QDir emulatorAppDir(QDir(appDataPath).filePath("Emulator/" + appIdText));
    const QString coversPath = emulatorAppDir.filePath("covers");
    const QString iconsPath = emulatorAppDir.filePath("icons");

    const QString coverSourceTargetDetails = CoverImageFilePath(coversPath, "cover_240x360.jpg");
    const QString coverSource = !coverSourceTargetDetails.isEmpty() ? coverSourceTargetDetails : m_fileManager.FirstImageInDirectory(coversPath);
    const QVariantList achievements = BuildAchievementDetails(appId, iconsPath);
    const QVariantMap latestAchievement = LatestUnlockedAchievement(m_databaseManager.selectWhere(
        m_databaseConnectionName,
        "steam_emu_achievements",
        "id = ?",
        {appId},
        {"achievement_key", "achievement_name", "achievement_description", "date_unlocked"}
    ));

    return {
        {"id", appId},
        {"title", Utils::MapStringValue(row, "game_name")},
        {"coverSource", coverSource},
        {"coverSourceTargetDetails", coverSource},
        {"achievementCount", Utils::MapIntValue(row, "total_unlocked_amount_achievements")},
        {"achievementTotal", Utils::MapIntValue(row, "total_amount_achievements")},
        {"targetType", "Emulator"},
        {"installationStatus", ExecutableInstallationStatus(row)},
        {"lastPlayed", Utils::RelativeTime(row.value("last_played_date").toLongLong())},
        {"recentUnlock", Utils::LocalDate(latestAchievement.value("date_unlocked").toLongLong())},
        {"playtime", PlaytimeText(Utils::MapIntValue(row, "total_seconds_played"))},
        {"achievements", achievements}
    };
}

/////////////////////////////////////////////////////////////////////
///////////////////////////// PRIVATE ///////////////////////////////
/////////////////////////////////////////////////////////////////////

Error Lymalink::DatabaseInit()
{
    const QString appDataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (appDataPath.isEmpty())
    {
        qCritical() << "Lymalink: failed to resolve app data location";
        return Error::FileSystemError;
    }

    m_databasePath = QDir(appDataPath).filePath(DATABASE_FILE_NAME);
    if (m_databaseManager.databaseFileExists(m_databasePath))
    {
        qDebug() << "Lymalink: database already exists at" << m_databasePath;
        if (!m_databaseManager.openDatabase(m_databaseConnectionName, m_databasePath))
        {
            qCritical() << "Lymalink: failed to open database:" << m_databaseManager.lastError();
            return Error::DatabaseError;
        }
    }
    else if (!m_databaseManager.createDatabase(m_databaseConnectionName, m_databasePath))
    {
        qCritical() << "Lymalink: failed to create/open database:" << m_databaseManager.lastError();
        return Error::DatabaseError;
    }

    const bool gamesReady = m_databaseManager.createTable(
        m_databaseConnectionName,
        "steam_emu_games",
        {
            "id INTEGER PRIMARY KEY NOT NULL",
            "game_name TEXT NOT NULL",
            "emulator_type TEXT",
            "executable_location TEXT",
            "prefix_location TEXT",
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
        qCritical() << "Lymalink: failed to initialize steam_emu_games table:" << m_databaseManager.lastError();
        return Error::DatabaseError;
    }

    const bool achievementsReady = m_databaseManager.createTable(
        m_databaseConnectionName,
        "steam_emu_achievements",
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
            "FOREIGN KEY (id) REFERENCES steam_emu_games(id) ON DELETE CASCADE"
        }
    );

    if (!achievementsReady)
    {
        qCritical() << "Lymalink: failed to initialize steam_emu_achievements table:" << m_databaseManager.lastError();
        return Error::DatabaseError;
    }

    qDebug() << "Lymalink: database initialized at" << m_databasePath;
    return Error::NoError;
}

/////////////////////////////////////////////////////////////////////

Error Lymalink::FileSystemInit()
{
    const QString appDataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (appDataPath.isEmpty())
    {
        qCritical() << "Lymalink: failed to resolve app data location for filesystem init";
        return Error::FileSystemError;
    }

    QDir appDataDir(appDataPath);
    const QStringList requiredFolders = {"Emulator", "Steam", "Custom"};

    for (const QString &folderName : requiredFolders)
    {
        if (!appDataDir.mkpath(folderName))
        {
            qCritical() << "Lymalink: failed to create folder:" << appDataDir.filePath(folderName);
            return Error::FileSystemError;
        }
    }

    qDebug() << "Lymalink: filesystem initialized at" << appDataPath;
    return Error::NoError;
}

/////////////////////////////////////////////////////////////////////

void Lymalink::ApplyNewAchievements(int appId, QVariantList achievements)
{
    int inserted = 0;

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
            "steam_emu_achievements",
            "id = ? AND achievement_key = ?",
            {appId, achievementKey}
        );
        if (existingRows > 0)
        {
            continue;
        }

        if (!m_databaseManager.insert(m_databaseConnectionName, "steam_emu_achievements", entry))
        {
            qWarning() << "Lymalink: failed to insert achievement:" << achievementKey << m_databaseManager.lastError();
            continue;
        }
        ++inserted;
    }

    if (inserted > 0)
    {
        const qint64 now = QDateTime::currentSecsSinceEpoch();
        const int totalAchievements = m_databaseManager.count(m_databaseConnectionName, "steam_emu_achievements", "id = ?", {appId});
        if (!m_databaseManager.update(
            m_databaseConnectionName,
            "steam_emu_games",
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
    if (secondsPlayed < 3600)   // Less than 1 hour (60 minutes)
    { 
        int minutes = secondsPlayed / 60;
        return QString("%1 minute(s)").arg(minutes);
    }
    else                        // 1 hour or more
    { 
        int hours = secondsPlayed / 3600;
        return QString("%1 hour(s)").arg(hours);
    }
}

/////////////////////////////////////////////////////////////////////

QVariantMap Lymalink::LatestUnlockedAchievement(const QVariantList &achievements) const
{
    QVariantMap latestAchievement;
    qint64 latestUnlockDate = 0;

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
    const QVariantList rows = m_databaseManager.selectWhere(
        m_databaseConnectionName,
        "steam_emu_achievements",
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
        const bool unlocked = row.value("date_unlocked").toLongLong() > 0;
        const bool achievementHidden = row.value("achievement_hidden").toInt() == 1;
        const QString sectionKey = unlocked ? "unlocked" : (achievementHidden ? "achievementHidden" : "locked");

        QVariantMap achievement = {
            {"iconSource", AchievementIconFilePath(iconsPath, row)},
            {"achievementName", Utils::MapStringValue(row, "achievement_name")},
            {"achievementDescription", Utils::MapStringValue(row, "achievement_description")},
            {"globalUnlockPercentage", row.value("global_unlock_percentage").isNull() ? 0.0 : row.value("global_unlock_percentage").toDouble()},
            {"unlockDate", unlocked ? Utils::LocalDate(row.value("date_unlocked").toLongLong()) : QString()},
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

    QVariantList achievements;
    achievements << unlockedAchievements << lockedAchievements << hiddenAchievements;
    return achievements;
}

/////////////////////////////////////////////////////////////////////

QString Lymalink::CoverImageFilePath(const QString &coversPath, const QString &fileName) const
{
    const QString coverPath = QDir(coversPath).filePath(fileName);
    if (!QFileInfo::exists(coverPath))
    {
        return QString();
    }

    return m_fileManager.LocalFileSource(coverPath);
}

/////////////////////////////////////////////////////////////////////

QString Lymalink::CommunityIconFilePath(const QString &iconsPath) const
{
    QDir iconsDir(iconsPath);
    if (!iconsDir.exists())
    {
        return QString();
    }

    const QFileInfoList iconFiles = iconsDir.entryInfoList({"community_icon.*"}, QDir::Files, QDir::Name | QDir::IgnoreCase);
    if (iconFiles.isEmpty())
    {
        return QString();
    }

    return m_fileManager.LocalFileSource(iconFiles.first().filePath());
}

/////////////////////////////////////////////////////////////////////

QString Lymalink::AchievementIconFilePath(const QString &iconsPath, const QVariantMap &achievement) const
{
    const QString achievementKey = Utils::MapStringValue(achievement, "achievement_key");
    if (achievementKey.isEmpty())
    {
        return QString();
    }

    const bool unlocked = achievement.value("date_unlocked").toLongLong() > 0;
    const QString iconFileName = achievementKey + (unlocked ? "_icon.jpg" : "_gray_icon.jpg");
    const QString iconPath = QDir(iconsPath).filePath(iconFileName);

    if (!QFileInfo::exists(iconPath))
    {
        return QString();
    }

    return m_fileManager.LocalFileSource(iconPath);
}

/////////////////////////////////////////////////////////////////////

QString Lymalink::ExecutableInstallationStatus(const QVariantMap &row) const
{
    const QString executableLocation = Utils::MapStringValue(row, "executable_location");
    if (executableLocation.isEmpty())
    {
        return tr("Not Installed");
    }

    const QFileInfo executableInfo(executableLocation);
    return executableInfo.exists() && executableInfo.isFile() ? tr("Installed") : tr("Not Installed");
}
