/////////////////////////////////////////////////////////
// File: SteamImportAutoSyncWorker.cpp
// Date: 2026-07-31
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Implements Steam automatic import sync worker
/////////////////////////////////////////////////////////

#include "SteamImportAutoSyncWorker.h"
#include "../Defines.h"

#include <QDateTime>
#include <QDebug>
#include <QMap>
#include <QSet>
#include <QThread>

/////////////////////////////////////////////////////////////////////

SteamImportAutoSyncWorker::SteamImportAutoSyncWorker(QObject *parent) : QObject(parent)
{
    // Constructor
}

SteamImportAutoSyncWorker::~SteamImportAutoSyncWorker()
{
    // Destructor
}

/////////////////////////////////////////////////////////////////////
////////////////////////////// PUBLIC ///////////////////////////////
/////////////////////////////////////////////////////////////////////

void SteamImportAutoSyncWorker::Run(const QString &databasePath, const QString &steamId, const QString &apiKey)
{
    // Payload is consumed by QML to update scheduling and surface summary state
    QVariantMap payload = {
        {"success", false},
        {"checkedCount", 0},
        {"changedCount", 0},
        {"updatedCount", 0},
        {"skippedCount", 0},
        {"assetRefreshAppIds", QVariantList()},
        {"errors", QVariantList()}
    };
    qDebug() << "SteamImportAutoSyncWorker::Run: starting Steam progress sync";

    if (databasePath.trimmed().isEmpty())
    {
        payload["errors"] = QVariantList{tr("Steam progress sync failed: target database is unavailable.")};
        qWarning() << "SteamImportAutoSyncWorker::Run: database path is empty";
        emit signalError(tr("Steam progress sync failed"), payload.value("errors").toList().first().toString());
        emit signalFinished(payload);
        return;
    }

    // Fetch current Steam-owned game timestamps before opening the local target database
    SteamApi steamApi;
    QList<SteamOwnedGameData> ownedGames;
    const Error fetchError = steamApi.FetchOwnedGames(steamId, ownedGames, apiKey);
    if (fetchError != Error::NoError && fetchError != Error::NoData)
    {
        const QString errorText = SteamFetchErrorText(fetchError);
        payload["errors"] = QVariantList{errorText};
        qWarning() << "SteamImportAutoSyncWorker::Run: FetchOwnedGames failed:" << static_cast<int>(fetchError);
        emit signalError(tr("Steam progress sync failed"), errorText);
        emit signalFinished(payload);
        return;
    }

    qDebug() << "SteamImportAutoSyncWorker::Run: fetched owned games:" << ownedGames.size();

    // Use a thread-specific connection name because the worker runs outside the UI thread
    SQLiteManager databaseManager;
    const QString connectionName = QStringLiteral("steam_auto_sync_%1").arg(reinterpret_cast<quintptr>(QThread::currentThreadId()));
    if (!databaseManager.openDatabase(connectionName, databasePath))
    {
        const QString errorText = tr("Steam progress sync failed: couldn't open target database.");
        payload["errors"] = QVariantList{errorText};
        qWarning() << "SteamImportAutoSyncWorker::Run: openDatabase failed:" << databaseManager.lastError();
        emit signalError(tr("Steam progress sync failed"), errorText);
        emit signalFinished(payload);
        return;
    }

    // Build a lookup of imported Steam targets by app id and last played timestamp
    const QVariantList importedRows = databaseManager.selectAll(
        connectionName,
        DATABASE_TABLE_GAMES,
        QStringList{"id", "last_played_date"}
    );

    QMap<int, qint64> importedLastPlayed;
    for (const QVariant &rowValue : importedRows)
    {
        const QVariantMap row = rowValue.toMap();
        const int appId = row.value("id").toInt();
        if (appId > 0)
        {
            importedLastPlayed[appId] = row.value("last_played_date").toLongLong();
        }
    }
    qDebug() << "SteamImportAutoSyncWorker::Run: imported Steam targets:" << importedLastPlayed.size();

    // Only imported targets with a changed Steam last-played timestamp need hydration
    QList<SteamOwnedGameData> changedGames;
    changedGames.reserve(ownedGames.size());
    for (const SteamOwnedGameData &ownedGame : ownedGames)
    {
        if (!importedLastPlayed.contains(ownedGame.appId))
        {
            continue;
        }

        if (importedLastPlayed.value(ownedGame.appId) == ownedGame.lastPlayedDate)
        {
            continue;
        }

        qDebug() << "SteamImportAutoSyncWorker::Run: changed Steam target detected appId:" << ownedGame.appId << "localLastPlayed:" << importedLastPlayed.value(ownedGame.appId) << "steamLastPlayed:" << ownedGame.lastPlayedDate;
        changedGames << ownedGame;
    }

    payload["checkedCount"] = importedLastPlayed.size();
    payload["changedCount"] = changedGames.size();

    if (!changedGames.isEmpty())
    {
        qDebug() << "SteamImportAutoSyncWorker::Run: syncing changed Steam targets:" << changedGames.size();
        QVariantMap updatePayload = UpdateChangedGames(databaseManager, connectionName, changedGames, steamId, apiKey);
        payload["success"] = updatePayload.value("success").toBool();
        payload["updatedCount"] = updatePayload.value("updatedCount").toInt();
        payload["skippedCount"] = updatePayload.value("skippedCount").toInt();
        payload["assetRefreshAppIds"] = updatePayload.value("assetRefreshAppIds").toList();
        payload["errors"] = updatePayload.value("errors").toList();
    }
    else
    {
        payload["success"] = true;
        qDebug() << "SteamImportAutoSyncWorker::Run: no changed imported Steam targets";
    }

    const QVariantList errors = payload.value("errors").toList();
    if (!errors.isEmpty())
    {
        qWarning() << "SteamImportAutoSyncWorker::Run: finished with errors:" << errors.size() << "first:" << errors.first().toString();
        emit signalError(tr("Steam progress sync failed"), errors.first().toString());
    }

    qDebug() << "SteamImportAutoSyncWorker::Run: finished checked:" << payload.value("checkedCount").toInt() << "changed:" << payload.value("changedCount").toInt() << "updated:" << payload.value("updatedCount").toInt() << "skipped:" << payload.value("skippedCount").toInt();
    emit signalFinished(payload);
}

/////////////////////////////////////////////////////////////////////
///////////////////////////// PRIVATE ///////////////////////////////
/////////////////////////////////////////////////////////////////////

QVariantMap SteamImportAutoSyncWorker::UpdateChangedGames(SQLiteManager &databaseManager, const QString &connectionName, const QList<SteamOwnedGameData> &games, const QString &steamId, const QString &apiKey)
{
    QVariantMap payload = {
        {"success", false},
        {"updatedCount", 0},
        {"skippedCount", 0},
        {"assetRefreshAppIds", QVariantList()},
        {"errors", QVariantList()}
    };

    SteamApi steamApi;
    QVariantList errors;
    QVariantList assetRefreshAppIds;
    int updatedCount = 0;
    int skippedCount = 0;
    bool cancelRemainingFetches = false;

    for (const SteamOwnedGameData &game : games)
    {
        if (cancelRemainingFetches)
        {
            break;
        }

        const int appId = game.appId;
        const QString gameName = game.gameName.trimmed();
        const qint64 totalSecondsPlayed = game.totalSecondsPlayed;
        const qint64 lastPlayedDate = game.lastPlayedDate;
        const QString errorPrefix = gameName.isEmpty() ? tr("App ID %1").arg(appId) : QString("%1 (%2)").arg(gameName).arg(appId);

        // Skip malformed Steam rows without stopping the whole sync run
        if (appId <= 0 || gameName.isEmpty())
        {
            ++skippedCount;
            errors << tr("%1: invalid game data.").arg(errorPrefix);
            qWarning() << "SteamImportAutoSyncWorker::UpdateChangedGames: invalid game data appId:" << appId << "name:" << gameName;
            continue;
        }

        // Guard against targets removed locally between initial scan and update
        const QVariantMap existingGame = databaseManager.selectFirst(connectionName, DATABASE_TABLE_GAMES, "id = ?", {appId});
        if (existingGame.isEmpty())
        {
            ++skippedCount;
            qWarning() << "SteamImportAutoSyncWorker::UpdateChangedGames: imported game missing before update appId:" << appId;
            continue;
        }

        // Read existing achievement keys so stale/new achievement rows can be reconciled
        const QVariantList existingAchievementRows = databaseManager.selectWhere(
            connectionName,
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

        // Prefer public app achievement metadata, then fall back to the Web API endpoint
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
            qWarning() << "SteamImportAutoSyncWorker::UpdateChangedGames: FetchAchievementData failed appId:" << appId << "error:" << static_cast<int>(publicError);
            continue;
        }

        QList<SteamPlayerAchievementData> playerAchievements;
        const bool hasPublicAchievements = publicError == Error::NoError && !publicAchievements.isEmpty();
        const bool hasExistingAchievements = !existingKeys.isEmpty();
        const bool shouldFetchPlayerAchievements = hasPublicAchievements || hasExistingAchievements;
        const Error playerError = shouldFetchPlayerAchievements
            ? steamApi.FetchPlayerAchievements(appId, steamId, playerAchievements, apiKey)
            : Error::NoData;
        const bool hasPlayerAchievements = playerError == Error::NoError;

        // A private profile affects all remaining games, so stop after reporting it once
        if (shouldFetchPlayerAchievements && playerError != Error::NoError && playerError != Error::NoData)
        {
            ++skippedCount;
            if (playerError == Error::ProfileNotPublic)
            {
                errors << tr("%1: Steam profile is not public. Make your Steam profile and game details public, then retry.").arg(errorPrefix);
                qWarning() << "SteamImportAutoSyncWorker::UpdateChangedGames: profile not public appId:" << appId;
                cancelRemainingFetches = true;
            }
            else
            {
                errors << tr("%1: couldn't fetch player achievements.").arg(errorPrefix);
                qWarning() << "SteamImportAutoSyncWorker::UpdateChangedGames: FetchPlayerAchievements failed appId:" << appId << "error:" << static_cast<int>(playerError);
            }
            continue;
        }

        // Index player progress by achievement key for quick row merge/update
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

        QSet<QString> fetchedKeys;
        int unlockedCount = 0;
        for (const SteamAchievementData &achievement : publicAchievements)
        {
            if (achievement.achievementKey.isEmpty())
            {
                continue;
            }

            fetchedKeys.insert(achievement.achievementKey);
            if (playerByKey.value(achievement.achievementKey).dateUnlocked > 0)
            {
                ++unlockedCount;
            }
        }

        const bool achievementStructureChanged = hasPublicAchievements && existingKeys != fetchedKeys;
        const qint64 now = QDateTime::currentSecsSinceEpoch();

        // Update a single game atomically so partial achievement writes are rolled back
        if (!databaseManager.beginTransaction(connectionName))
        {
            ++skippedCount;
            errors << tr("%1: couldn't start database transaction.").arg(errorPrefix);
            qWarning() << "SteamImportAutoSyncWorker::UpdateChangedGames: beginTransaction failed appId:" << appId << databaseManager.lastError();
            continue;
        }

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
            gameRow["total_unlocked_amount_achievements"] = playerUnlockedCount;
        }

        bool updated = databaseManager.update(connectionName, DATABASE_TABLE_GAMES, gameRow, "id = ?", {appId});

        // Remove achievements no longer reported by Steam metadata
        if (updated && hasPublicAchievements)
        {
            const QSet<QString> removedKeys = existingKeys - fetchedKeys;
            for (const QString &achievementKey : removedKeys)
            {
                if (!databaseManager.remove(connectionName, DATABASE_TABLE_ACHIEVEMENTS, "id = ? AND achievement_key = ?", {appId, achievementKey}))
                {
                    qWarning() << "SteamImportAutoSyncWorker::UpdateChangedGames: remove stale achievement failed appId:" << appId << "achievement:" << achievementKey << databaseManager.lastError();
                    updated = false;
                    break;
                }
            }
        }

        // Upsert achievement metadata and unlock progress when metadata is available
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

                if (existingKeys.contains(achievement.achievementKey))
                {
                    if (!databaseManager.update(connectionName, DATABASE_TABLE_ACHIEVEMENTS, achievementRow, "id = ? AND achievement_key = ?", {appId, achievement.achievementKey}))
                    {
                        qWarning() << "SteamImportAutoSyncWorker::UpdateChangedGames: update achievement failed appId:" << appId << "achievement:" << achievement.achievementKey << databaseManager.lastError();
                        updated = false;
                        break;
                    }
                }
                else
                {
                    achievementRow["id"] = appId;
                    achievementRow["achievement_key"] = achievement.achievementKey;
                    achievementRow["date_added"] = now;
                    if (!databaseManager.insert(connectionName, DATABASE_TABLE_ACHIEVEMENTS, achievementRow))
                    {
                        qWarning() << "SteamImportAutoSyncWorker::UpdateChangedGames: insert achievement failed appId:" << appId << "achievement:" << achievement.achievementKey << databaseManager.lastError();
                        updated = false;
                        break;
                    }
                }
            }
        }

        // If metadata is unavailable, only update unlock state for achievements already imported
        if (updated && !hasPublicAchievements && playerError == Error::NoError)
        {
            for (const QString &achievementKey : existingKeys)
            {
                if (!playerByKey.contains(achievementKey))
                {
                    continue;
                }

                const SteamPlayerAchievementData playerAchievement = playerByKey.value(achievementKey);
                if (!databaseManager.update(
                    connectionName,
                    DATABASE_TABLE_ACHIEVEMENTS,
                    {{"date_unlocked", playerAchievement.dateUnlocked}, {"date_updated", now}},
                    "id = ? AND achievement_key = ?",
                    {appId, achievementKey}))
                {
                    qWarning() << "SteamImportAutoSyncWorker::UpdateChangedGames: update fallback player achievement failed appId:" << appId << "achievement:" << achievementKey << databaseManager.lastError();
                    updated = false;
                    break;
                }
            }
        }

        if (updated && databaseManager.commitTransaction(connectionName))
        {
            ++updatedCount;
            qDebug() << "SteamImportAutoSyncWorker::UpdateChangedGames: updated appId:" << appId << "achievementStructureChanged:" << achievementStructureChanged;
            if (achievementStructureChanged)
            {
                assetRefreshAppIds << appId;
            }
        }
        else
        {
            databaseManager.rollbackTransaction(connectionName);
            ++skippedCount;
            errors << tr("%1: couldn't write Steam update to database.").arg(errorPrefix);
            qWarning() << "SteamImportAutoSyncWorker::UpdateChangedGames: write or commit failed appId:" << appId << databaseManager.lastError();
        }
    }

    payload["success"] = updatedCount > 0;
    payload["updatedCount"] = updatedCount;
    payload["skippedCount"] = skippedCount;
    payload["assetRefreshAppIds"] = assetRefreshAppIds;
    payload["errors"] = errors;
    return payload;
}

/////////////////////////////////////////////////////////////////////

QString SteamImportAutoSyncWorker::SteamFetchErrorText(Error error) const
{
    switch (error)
    {
        case Error::InvalidParameter:
            return tr("Steam progress sync failed: Steam ID, API key, or automatic sync activation is invalid.");
        case Error::AccessDenied:
            return tr("Steam progress sync failed: Steam profile is private or Steam Web API key was rejected.");
        case Error::NotFound:
            return tr("Steam progress sync failed: Steam progress could not be loaded. Check API credentials.");
        case Error::NoData:
            return tr("Steam progress sync failed: no Steam progress data available.");
        case Error::ParseError:
            return tr("Steam progress sync failed: Steam progress response could not be parsed.");
        default:
            return tr("Steam progress sync failed: Check Steam API credentials and internet connection.");
    }
}
