/////////////////////////////////////////////////////////
// File: StatisticsHandler.cpp
// Date: 2026-11-23
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Builds statistics from a local database snapshot
/////////////////////////////////////////////////////////

#include "StatisticsHandler.h"
#include "../Defines.h"
#include "../database/SQLiteManager.h"

#include <QMap>
#include <QStringList>
#include <QUuid>
#include <QVariantList>

#include <algorithm>
#include <cmath>

/////////////////////////////////////////////////////////////////////

StatisticsHandler::StatisticsHandler(QObject *parent) : QObject(parent)
{
    // Constructor
}

StatisticsHandler::~StatisticsHandler()
{
    // Destructor
}

/////////////////////////////////////////////////////////////////////
////////////////////////////// PUBLIC ///////////////////////////////
/////////////////////////////////////////////////////////////////////

QVariantMap StatisticsHandler::Read(const QString &databasePath) const
{
    QVariantMap payload = {{"success", false}};
    const QString connectionName = "statistics_" + QUuid::createUuid().toString(QUuid::WithoutBraces);
    SQLiteManager databaseManager;
    if (databasePath.trimmed().isEmpty() || !databaseManager.openDatabase(connectionName, databasePath, false, false, true))
    {
        payload["error"] = tr("Couldn't open the local statistics database.");
        return payload;
    }

    if (!databaseManager.beginTransaction(connectionName))
    {
        payload["error"] = tr("Couldn't start the statistics snapshot.");
        return payload;
    }

    const QStringList gameColumns = {"id", "game_name", "target_hidden", "total_seconds_played", "last_played_date"};
    const QStringList achievementColumns = {"id", "achievement_key", "achievement_name", "date_unlocked", "global_unlock_percentage"};
    const QVariantList emulatorGames = databaseManager.selectAll(connectionName, DATABASE_TABLE_EMU_GAMES, gameColumns);
    QString error = databaseManager.lastError();
    QVariantList emulatorAchievements;
    QVariantList steamGames;
    QVariantList steamAchievements;
    if (error.isEmpty())
    {
        emulatorAchievements = databaseManager.selectAll(connectionName, DATABASE_TABLE_EMU_ACHIEVEMENTS, achievementColumns);
        error = databaseManager.lastError();
    }
    if (error.isEmpty())
    {
        steamGames = databaseManager.selectAll(connectionName, DATABASE_TABLE_GAMES, gameColumns);
        error = databaseManager.lastError();
    }
    if (error.isEmpty())
    {
        steamAchievements = databaseManager.selectAll(connectionName, DATABASE_TABLE_ACHIEVEMENTS, achievementColumns);
        error = databaseManager.lastError();
    }

    if (error.isEmpty() && databaseManager.commitTransaction(connectionName))
    {
        payload["emulators"] = BuildSource(emulatorGames, emulatorAchievements);
        payload["steamImports"] = BuildSource(steamGames, steamAchievements);
        payload["success"] = true;
    }
    else
    {
        if (error.isEmpty())
        {
            error = databaseManager.lastError();
        }
        databaseManager.rollbackTransaction(connectionName);
        payload["error"] = tr("Couldn't read local statistics: %1").arg(error);
    }
    return payload;
}

/////////////////////////////////////////////////////////////////////
///////////////////////////// PRIVATE ///////////////////////////////
/////////////////////////////////////////////////////////////////////

bool StatisticsHandler::TitleLess(const QVariant &left, const QVariant &right) const
{
    const QVariantMap a = left.toMap();
    const QVariantMap b = right.toMap();
    const int comparison = QString::compare(a.value("title").toString(), b.value("title").toString(), Qt::CaseInsensitive);
    const bool titleLess = comparison != 0 ? comparison < 0 : a.value("appId").toInt() < b.value("appId").toInt();

    return titleLess;
}

/////////////////////////////////////////////////////////////////////

bool StatisticsHandler::HighlightTieLess(const QVariant &left, const QVariant &right) const
{
    bool highlightTieLess = false;
    const bool leftTitleLess = TitleLess(left, right);
    const bool rightTitleLess = TitleLess(right, left);
    if (leftTitleLess)
    {
        highlightTieLess = true;
    }
    else if (!rightTitleLess)
    {
        highlightTieLess = left.toMap().value("achievementKey").toString() < right.toMap().value("achievementKey").toString();
    }

    return highlightTieLess;
}

/////////////////////////////////////////////////////////////////////

QVariantMap StatisticsHandler::BuildSource(const QVariantList &gameRows, const QVariantList &achievementRows) const
{
    QMap<int, QVariantList> achievementsByGame;
    for (const QVariant &value : achievementRows)
    {
        achievementsByGame[value.toMap().value("id").toInt()] << value;
    }

    QVariantList games;
    QVariantList recentUnlocks;
    QVariantList rarestUnlocks;
    qint64 totalSeconds = 0;
    qint64 totalAchievements = 0;
    qint64 unlockedAchievements = 0;
    int completedGames = 0;
    int gamesWithAchievements = 0;

    for (const QVariant &value : gameRows)
    {
        const QVariantMap row = value.toMap();
        const int appId = row.value("id").toInt();
        const QString title = row.value("game_name").toString();
        const QVariantList achievements = achievementsByGame.value(appId);
        int unlocked = 0;
        qint64 latestUnlock = 0;
        for (const QVariant &achievementValue : achievements)
        {
            const QVariantMap achievement = achievementValue.toMap();
            const qint64 timestamp = achievement.value("date_unlocked").toLongLong();
            if (timestamp <= 0)
            {
                continue;
            }

            ++unlocked;
            latestUnlock = std::max(latestUnlock, timestamp);
            const QString key = achievement.value("achievement_key").toString();
            const QString name = achievement.value("achievement_name").toString();
            QVariantMap highlight = {
                {"appId", appId},
                {"title", title},
                {"achievementKey", key},
                {"achievementName", name.isEmpty() ? key : name},
                {"unlockTimestamp", timestamp}
            };
            recentUnlocks << highlight;

            bool validPercentage = false;
            const QVariant percentageValue = achievement.value("global_unlock_percentage");
            const double percentage = percentageValue.toDouble(&validPercentage);
            if (!percentageValue.isNull() && validPercentage && std::isfinite(percentage) && percentage >= 0.0 && percentage <= 100.0)
            {
                highlight["globalUnlockPercentage"] = percentage;
                rarestUnlocks << highlight;
            }
        }

        // Achievement rows are authoritative, cached dashboard counts may be stale
        const qint64 total = achievements.size();
        const qint64 seconds = std::max(qint64(0), row.value("total_seconds_played").toLongLong());
        QVariantMap game = {
            {"appId", appId},
            {"title", title},
            {"hidden", row.value("target_hidden").toBool()},
            {"playtimeSeconds", seconds},
            {"lastPlayedTimestamp", std::max(qint64(0), row.value("last_played_date").toLongLong())},
            {"latestUnlockTimestamp", latestUnlock},
            {"totalAchievements", total},
            {"unlockedAchievements", unlocked},
            {"hasAchievementData", total > 0},
            {"completion", total > 0 ? 100.0 * unlocked / total : -1.0}
        };

        games << game;
        totalSeconds += seconds;
        totalAchievements += total;
        unlockedAchievements += unlocked;
        gamesWithAchievements += total > 0 ? 1 : 0;
        completedGames += total > 0 && unlocked == total ? 1 : 0;
    }

    std::sort(games.begin(), games.end(), [this](const QVariant &left, const QVariant &right) {
        const bool titleLess = TitleLess(left, right);

        return titleLess;
    });
    std::sort(recentUnlocks.begin(), recentUnlocks.end(), [this](const QVariant &left, const QVariant &right) {
        const qint64 a = left.toMap().value("unlockTimestamp").toLongLong();
        const qint64 b = right.toMap().value("unlockTimestamp").toLongLong();
        const bool recentUnlockLess = a != b ? a > b : HighlightTieLess(left, right);

        return recentUnlockLess;
    });
    std::sort(rarestUnlocks.begin(), rarestUnlocks.end(), [this](const QVariant &left, const QVariant &right) {
        const double a = left.toMap().value("globalUnlockPercentage").toDouble();
        const double b = right.toMap().value("globalUnlockPercentage").toDouble();
        const bool rarestUnlockLess = a != b ? a < b : HighlightTieLess(left, right);

        return rarestUnlockLess;
    });

    QVariantMap mostPlayed;
    QVariantMap recentlyPlayed;
    for (const QVariant &value : games)
    {
        const QVariantMap game = value.toMap();
        if (game.value("playtimeSeconds").toLongLong() > mostPlayed.value("playtimeSeconds").toLongLong())
        {
            mostPlayed = game;
        }

        if (game.value("lastPlayedTimestamp").toLongLong() > recentlyPlayed.value("lastPlayedTimestamp").toLongLong())
        {
            recentlyPlayed = game;
        }
    }

    QVariantMap summary = {
        {"gameCount", games.size()},
        {"playtimeSeconds", totalSeconds},
        {"totalAchievements", totalAchievements},
        {"unlockedAchievements", unlockedAchievements},
        {"completion", totalAchievements > 0 ? 100.0 * unlockedAchievements / totalAchievements : -1.0},
        {"completedGames", completedGames},
        {"gamesWithAchievements", gamesWithAchievements},
        {"mostPlayed", mostPlayed},
        {"recentlyPlayed", recentlyPlayed},
        {"latestUnlock", recentUnlocks.isEmpty() ? QVariantMap() : recentUnlocks.first().toMap()}
    };
    const QVariantMap source = {
        {"summary", summary},
        {"games", games},
        {"recentUnlocks", recentUnlocks.mid(0, 5)},
        {"rarestUnlocks", rarestUnlocks.mid(0, 5)}
    };
    
    return source;
}
