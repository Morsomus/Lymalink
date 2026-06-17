/////////////////////////////////////////////////////////
// File: DataTransporter.cpp
// Date: 2026-06-17
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Implements achievement import/export helper
/////////////////////////////////////////////////////////

#include "DataTransporter.h"
#include "../Defines.h"
#include "../tools/Utils.h"

#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QStandardPaths>
#include <QSet>

#include <algorithm>

/////////////////////////////////////////////////////////////////////

DataTransporter::DataTransporter(QObject *parent) : QObject(parent)
{
    m_databaseConnectionName = QString("%1_data_transporter").arg(DATABASE_CONNECTION_NAME);
    m_databasePath = DefaultDatabasePath();
}

DataTransporter::~DataTransporter()
{
    // Destructor
}

/////////////////////////////////////////////////////////////////////
////////////////////////////// PUBLIC ///////////////////////////////
/////////////////////////////////////////////////////////////////////

QVariantMap DataTransporter::ExportAchievements(const QString &filePath)
{
    const QString trimmedFilePath = filePath.trimmed();
    if (trimmedFilePath.isEmpty())
    {
        return ErrorPayload(tr("Export path is empty."));
    }

    QVariantMap openPayload;
    if (!EnsureDatabaseOpen(openPayload))
    {
        return openPayload;
    }

    int exportedGameCount = 0;
    int exportedAchievementCount = 0;
    const QJsonObject exportJson = BuildExportJson(exportedGameCount, exportedAchievementCount);

    const QFileInfo fileInfo(trimmedFilePath);
    const QDir parentDir = fileInfo.absoluteDir();
    if (!parentDir.exists() && !parentDir.mkpath("."))
    {
        return ErrorPayload(tr("Couldn't create export directory: %1").arg(parentDir.absolutePath()), trimmedFilePath);
    }

    QFile file(trimmedFilePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        return ErrorPayload(tr("Couldn't write export file: %1").arg(file.errorString()), trimmedFilePath);
    }

    const QJsonDocument document(exportJson);
    if (file.write(document.toJson(QJsonDocument::Indented)) == -1)
    {
        return ErrorPayload(tr("Couldn't write export file: %1").arg(file.errorString()), trimmedFilePath);
    }

    return SuccessPayload(trimmedFilePath, exportedGameCount, exportedAchievementCount);
}

/////////////////////////////////////////////////////////////////////

QVariantMap DataTransporter::PreviewAchievementImport(const QString &filePath)
{
    const QString trimmedFilePath = filePath.trimmed();
    QVector<ImportedGame> games;
    QString error;
    if (!ReadImportFile(trimmedFilePath, games, error))
    {
        return ErrorPayload(error, trimmedFilePath);
    }

    QVariantMap openPayload;
    if (!EnsureDatabaseOpen(openPayload))
    {
        return openPayload;
    }

    QVariantList conflicts;
    for (const ImportedGame &game : games)
    {
        const QVariantMap existingGame = m_databaseManager.selectFirst(
            m_databaseConnectionName,
            DATABASE_TABLE_EMU_GAMES,
            "id = ?",
            {game.id}
        );
        if (existingGame.isEmpty())
        {
            continue;
        }

        const int currentAchievementCount = m_databaseManager.count(
            m_databaseConnectionName,
            DATABASE_TABLE_EMU_ACHIEVEMENTS,
            "id = ?",
            {game.id}
        );
        const int currentUnlockedCount = m_databaseManager.count(
            m_databaseConnectionName,
            DATABASE_TABLE_EMU_ACHIEVEMENTS,
            "id = ? AND date_unlocked > 0",
            {game.id}
        );

        conflicts.append(QVariantMap{
            {"id", game.id},
            {"name", game.name},
            {"currentName", Utils::MapStringValue(existingGame, "game_name")},
            {"importedAchievementCount", game.achievements.size()},
            {"currentAchievementCount", qMax(0, currentAchievementCount)},
            {"importedUnlockedCount", std::count_if(game.achievements.cbegin(), game.achievements.cend(), [](const ImportedAchievement &achievement) {
                return achievement.dateUnlocked > 0;
            })},
            {"currentUnlockedCount", qMax(0, currentUnlockedCount)},
            {"mode", QStringLiteral("merge")}
        });
    }

    return PreviewSuccessPayload(trimmedFilePath, games, conflicts);
}

/////////////////////////////////////////////////////////////////////

QVariantMap DataTransporter::ImportAchievements(const QString &filePath, const QVariantList &decisions)
{
    const QString trimmedFilePath = filePath.trimmed();
    QVector<ImportedGame> games;
    QString error;
    if (!ReadImportFile(trimmedFilePath, games, error))
    {
        return ErrorPayload(error, trimmedFilePath);
    }

    QVariantMap openPayload;
    if (!EnsureDatabaseOpen(openPayload))
    {
        return openPayload;
    }

    QHash<int, QString> decisionById;
    for (const QVariant &decisionValue : decisions)
    {
        const QVariantMap decision = decisionValue.toMap();
        const int appId = decision.value("id").toInt();
        const QString mode = decision.value("mode").toString().trimmed().toLower();
        if (appId > 0 && (mode == "merge" || mode == "replace"))
        {
            decisionById.insert(appId, mode);
        }
    }

    if (!m_databaseManager.beginTransaction(m_databaseConnectionName))
    {
        return ErrorPayload(tr("Couldn't start import transaction: %1").arg(m_databaseManager.lastError()), trimmedFilePath);
    }

    const qint64 now = QDateTime::currentSecsSinceEpoch();
    int addedGameCount = 0;
    int mergedGameCount = 0;
    int replacedGameCount = 0;
    int addedAchievementCount = 0;

    for (const ImportedGame &game : games)
    {
        const QVariantMap existingGame = m_databaseManager.selectFirst(
            m_databaseConnectionName,
            DATABASE_TABLE_EMU_GAMES,
            "id = ?",
            {game.id}
        );

        bool imported = false;
        if (existingGame.isEmpty())
        {
            imported = InsertImportedGame(game, now, addedAchievementCount, error);
            ++addedGameCount;
        }
        else if (decisionById.value(game.id, QStringLiteral("merge")) == "replace")
        {
            imported = ReplaceImportedGame(game, now, addedAchievementCount, error);
            ++replacedGameCount;
        }
        else
        {
            imported = MergeImportedGame(game, now, addedAchievementCount, error);
            ++mergedGameCount;
        }

        if (!imported)
        {
            m_databaseManager.rollbackTransaction(m_databaseConnectionName);
            return ErrorPayload(error, trimmedFilePath);
        }
    }

    if (!m_databaseManager.commitTransaction(m_databaseConnectionName))
    {
        return ErrorPayload(tr("Couldn't finish import transaction: %1").arg(m_databaseManager.lastError()), trimmedFilePath);
    }

    return ImportSuccessPayload(
        trimmedFilePath,
        games.size(),
        ImportedAchievementCount(games),
        addedGameCount,
        mergedGameCount,
        replacedGameCount
    );
}

/////////////////////////////////////////////////////////////////////
////////////////////////////// PRIVATE //////////////////////////////
/////////////////////////////////////////////////////////////////////

QString DataTransporter::DefaultDatabasePath() const
{
    const QString appDataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return appDataPath.isEmpty() ? QString() : QDir(appDataPath).filePath(DATABASE_FILE_NAME);
}

/////////////////////////////////////////////////////////////////////

bool DataTransporter::EnsureDatabaseOpen(QVariantMap &payload)
{
    if (m_databasePath.trimmed().isEmpty())
    {
        payload = ErrorPayload(tr("Couldn't resolve database path."));
        return false;
    }

    if (!m_databaseManager.databaseFileExists(m_databasePath))
    {
        payload = ErrorPayload(tr("Database file does not exist: %1").arg(m_databasePath));
        return false;
    }

    if (m_databaseManager.isDatabaseOpen(m_databaseConnectionName))
    {
        return true;
    }

    if (!m_databaseManager.openDatabase(m_databaseConnectionName, m_databasePath))
    {
        payload = ErrorPayload(tr("Couldn't open database: %1").arg(m_databaseManager.lastError()));
        return false;
    }

    return true;
}

/////////////////////////////////////////////////////////////////////

bool DataTransporter::ReadImportFile(const QString &filePath, QVector<ImportedGame> &games, QString &error) const
{
    if (filePath.trimmed().isEmpty())
    {
        error = tr("Import path is empty.");
        return false;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
    {
        error = tr("Couldn't read import file: %1").arg(file.errorString());
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError)
    {
        error = tr("Invalid JSON import file: %1").arg(parseError.errorString());
        return false;
    }

    return ParseImportDocument(document, games, error);
}

/////////////////////////////////////////////////////////////////////

bool DataTransporter::ParseImportDocument(const QJsonDocument &document, QVector<ImportedGame> &games, QString &error) const
{
    if (!document.isObject())
    {
        error = tr("Import file root must be a JSON object.");
        return false;
    }

    const QJsonObject root = document.object();
    if (root.value("format").toString() != "lymalink-achievements-export")
    {
        error = tr("Import file is not a Lymalink achievements export.");
        return false;
    }

    // In future, version will be used to determine possible legacy support
    const int version = JsonIntValue(root.value("version"));
    if (version != EXPORT_FILE_VERSION)
    {
        error = tr("Unsupported import file version: %1").arg(version);
        return false;
    }

    const QJsonArray gameArray = root.value("games").toArray();
    QSet<int> gameIds;
    games.clear();
    games.reserve(gameArray.size());

    for (const QJsonValue &gameValue : gameArray)
    {
        if (!gameValue.isObject())
        {
            error = tr("Import file contains an invalid game entry.");
            return false;
        }

        const QJsonObject gameObject = gameValue.toObject();
        ImportedGame game;
        game.id = JsonIntValue(gameObject.value("id"));
        game.name = gameObject.value("name").toString().trimmed();
        game.hidden = gameObject.value("hidden").toBool(false);

        const QJsonObject paths = gameObject.value("paths").toObject();
        game.executableLocation = paths.value("exe").toString();
        game.prefixLocation = paths.value("prefix").toString();
        game.installationDir = paths.value("install").toString();

        const QJsonObject stats = gameObject.value("stats").toObject();
        game.totalSecondsPlayed = JsonIntValue(stats.value("seconds"));

        const QJsonObject dates = gameObject.value("dates").toObject();
        game.lastPlayedDate = JsonDateValue(dates.value("last_played"));
        game.dateAdded = JsonDateValue(dates.value("added"));

        if (game.id <= 0 || game.name.isEmpty())
        {
            error = tr("Import file contains a game with invalid id or name.");
            return false;
        }
        if (gameIds.contains(game.id))
        {
            error = tr("Import file contains duplicate game id: %1").arg(game.id);
            return false;
        }
        gameIds.insert(game.id);

        const QJsonArray achievementArray = gameObject.value("achievements").toArray();
        QSet<QString> achievementKeys;
        game.achievements.reserve(achievementArray.size());

        for (const QJsonValue &achievementValue : achievementArray)
        {
            if (!achievementValue.isObject())
            {
                error = tr("Import file contains an invalid achievement entry for %1.").arg(game.name);
                return false;
            }

            const QJsonObject achievementObject = achievementValue.toObject();
            ImportedAchievement achievement;
            achievement.key = achievementObject.value("key").toString().trimmed();
            achievement.name = achievementObject.value("name").toString().trimmed();
            achievement.description = achievementObject.value("desc").toString();
            achievement.hidden = achievementObject.value("hidden").toBool(false);
            achievement.globalUnlockPercentage = achievementObject.value("percent").toDouble(0.0);

            const QJsonArray progress = achievementObject.value("progress").toArray();
            achievement.currentProgress = progress.size() > 0 ? JsonIntValue(progress.at(0)) : 0;
            achievement.maxProgress = progress.size() > 1 ? JsonIntValue(progress.at(1)) : 0;

            const QJsonObject achievementDates = achievementObject.value("dates").toObject();
            achievement.dateUnlocked = JsonDateValue(achievementDates.value("unlocked"));
            achievement.dateAdded = JsonDateValue(achievementDates.value("added"));

            if (achievement.key.isEmpty())
            {
                error = tr("Import file contains an achievement with empty key for %1.").arg(game.name);
                return false;
            }
            if (achievementKeys.contains(achievement.key))
            {
                error = tr("Import file contains duplicate achievement key %1 for %2.").arg(achievement.key, game.name);
                return false;
            }
            achievementKeys.insert(achievement.key);
            if (achievement.name.isEmpty())
            {
                achievement.name = achievement.key;
            }

            game.achievements.append(achievement);
        }

        games.append(game);
    }

    return true;
}

/////////////////////////////////////////////////////////////////////

bool DataTransporter::InsertImportedGame(const ImportedGame &game, qint64 now, int &addedAchievements, QString &error)
{
    if (!m_databaseManager.insert(m_databaseConnectionName, DATABASE_TABLE_EMU_GAMES, ImportedGameRow(game, now)))
    {
        error = tr("Couldn't insert imported game %1: %2").arg(game.name, m_databaseManager.lastError());
        return false;
    }

    for (const ImportedAchievement &achievement : game.achievements)
    {
        if (!InsertImportedAchievement(game.id, achievement, now, error))
        {
            return false;
        }
        ++addedAchievements;
    }

    return RefreshImportedGameCounts(game.id, now, error);
}

/////////////////////////////////////////////////////////////////////

bool DataTransporter::MergeImportedGame(const ImportedGame &game, qint64 now, int &addedAchievements, QString &error)
{
    for (const ImportedAchievement &achievement : game.achievements)
    {
        const QVariantMap existingAchievement = m_databaseManager.selectFirst(
            m_databaseConnectionName,
            DATABASE_TABLE_EMU_ACHIEVEMENTS,
            "id = ? AND achievement_key = ?",
            {game.id, achievement.key}
        );

        if (existingAchievement.isEmpty())
        {
            if (!InsertImportedAchievement(game.id, achievement, now, error))
            {
                return false;
            }
            ++addedAchievements;
            continue;
        }

        // Keep original unlocks intact
        const qint64 existingDateUnlocked = existingAchievement.value("date_unlocked").toLongLong();
        if (existingDateUnlocked > 0 && achievement.dateUnlocked > 0)
        {
            continue;
        }

        QVariantMap updateData;
        if (achievement.currentProgress > Utils::MapIntValue(existingAchievement, "cur_progress"))
        {
            updateData["cur_progress"] = achievement.currentProgress;
        }
        if (achievement.dateUnlocked > existingDateUnlocked)
        {
            updateData["date_unlocked"] = achievement.dateUnlocked;
        }

        if (!updateData.isEmpty())
        {
            updateData["date_updated"] = now;
            if (!m_databaseManager.update(
                m_databaseConnectionName,
                DATABASE_TABLE_EMU_ACHIEVEMENTS,
                updateData,
                "id = ? AND achievement_key = ?",
                {game.id, achievement.key}
            ))
            {
                error = tr("Couldn't merge achievement %1 for %2: %3").arg(achievement.key, game.name, m_databaseManager.lastError());
                return false;
            }
        }
    }

    return RefreshImportedGameCounts(game.id, now, error);
}

/////////////////////////////////////////////////////////////////////

bool DataTransporter::ReplaceImportedGame(const ImportedGame &game, qint64 now, int &addedAchievements, QString &error)
{
    if (!m_databaseManager.remove(m_databaseConnectionName, DATABASE_TABLE_EMU_GAMES, "id = ?", {game.id}))
    {
        error = tr("Couldn't replace existing game %1: %2").arg(game.name, m_databaseManager.lastError());
        return false;
    }

    return InsertImportedGame(game, now, addedAchievements, error);
}

/////////////////////////////////////////////////////////////////////

bool DataTransporter::InsertImportedAchievement(int gameId, const ImportedAchievement &achievement, qint64 now, QString &error)
{
    if (!m_databaseManager.insert(m_databaseConnectionName, DATABASE_TABLE_EMU_ACHIEVEMENTS, ImportedAchievementRow(gameId, achievement, now)))
    {
        error = tr("Couldn't insert imported achievement %1: %2").arg(achievement.key, m_databaseManager.lastError());
        return false;
    }

    return true;
}

/////////////////////////////////////////////////////////////////////

bool DataTransporter::RefreshImportedGameCounts(int gameId, qint64 now, QString &error)
{
    const int totalCount = m_databaseManager.count(
        m_databaseConnectionName,
        DATABASE_TABLE_EMU_ACHIEVEMENTS,
        "id = ?",
        {gameId}
    );
    const int unlockedCount = m_databaseManager.count(
        m_databaseConnectionName,
        DATABASE_TABLE_EMU_ACHIEVEMENTS,
        "id = ? AND date_unlocked > 0",
        {gameId}
    );
    if (totalCount < 0 || unlockedCount < 0)
    {
        error = tr("Couldn't refresh imported game counts: %1").arg(m_databaseManager.lastError());
        return false;
    }

    if (!m_databaseManager.update(
        m_databaseConnectionName,
        DATABASE_TABLE_EMU_GAMES,
        {
            {"emulator_type", ""},
            {"appid_dir_found", 0},
            {"appid_dir_location", ""},
            {"data_opt", ""},
            {"total_amount_achievements", totalCount},
            {"total_unlocked_amount_achievements", unlockedCount},
            {"date_updated", now}
        },
        "id = ?",
        {gameId}
    ))
    {
        error = tr("Couldn't update imported game metadata: %1").arg(m_databaseManager.lastError());
        return false;
    }

    return true;
}

/////////////////////////////////////////////////////////////////////

QVariantMap DataTransporter::ImportedGameRow(const ImportedGame &game, qint64 now) const
{
    return {
        {"id", game.id},
        {"game_name", game.name},
        {"emulator_type", ""},
        {"executable_location", game.executableLocation},
        {"prefix_location", game.prefixLocation},
        {"installation_dir", game.installationDir},
        {"data_opt", ""},
        {"target_hidden", game.hidden ? 1 : 0},
        {"appid_dir_found", 0},
        {"appid_dir_location", ""},
        {"total_amount_achievements", game.achievements.size()},
        {"total_unlocked_amount_achievements", 0},
        {"total_seconds_played", game.totalSecondsPlayed},
        {"last_played_date", game.lastPlayedDate},
        {"date_updated", now},
        {"date_added", game.dateAdded > 0 ? game.dateAdded : now}
    };
}

/////////////////////////////////////////////////////////////////////

QVariantMap DataTransporter::ImportedAchievementRow(int gameId, const ImportedAchievement &achievement, qint64 now) const
{
    return {
        {"id", gameId},
        {"achievement_key", achievement.key},
        {"achievement_name", achievement.name.isEmpty() ? achievement.key : achievement.name},
        {"achievement_description", achievement.description},
        {"achievement_hidden", achievement.hidden ? 1 : 0},
        {"global_unlock_percentage", achievement.globalUnlockPercentage},
        {"cur_progress", achievement.currentProgress},
        {"max_progress", achievement.maxProgress},
        {"date_unlocked", achievement.dateUnlocked},
        {"date_updated", now},
        {"date_added", achievement.dateAdded > 0 ? achievement.dateAdded : now}
    };
}

/////////////////////////////////////////////////////////////////////

QVariantMap DataTransporter::PreviewSuccessPayload(const QString &filePath, const QVector<ImportedGame> &games, const QVariantList &conflicts) const
{
    return {
        {"success", true},
        {"error", QString()},
        {"filePath", filePath},
        {"exportedGameCount", 0},
        {"exportedAchievementCount", 0},
        {"importedGameCount", games.size()},
        {"importedAchievementCount", ImportedAchievementCount(games)},
        {"addedGameCount", games.size() - conflicts.size()},
        {"mergedGameCount", 0},
        {"replacedGameCount", 0},
        {"conflicts", conflicts}
    };
}

/////////////////////////////////////////////////////////////////////

QVariantMap DataTransporter::ImportSuccessPayload(const QString &filePath, int importedGameCount, int importedAchievementCount, int addedGameCount, int mergedGameCount, int replacedGameCount) const
{
    return {
        {"success", true},
        {"error", QString()},
        {"filePath", filePath},
        {"exportedGameCount", 0},
        {"exportedAchievementCount", 0},
        {"importedGameCount", importedGameCount},
        {"importedAchievementCount", importedAchievementCount},
        {"addedGameCount", addedGameCount},
        {"mergedGameCount", mergedGameCount},
        {"replacedGameCount", replacedGameCount},
        {"conflicts", QVariantList()}
    };
}

/////////////////////////////////////////////////////////////////////

int DataTransporter::ImportedAchievementCount(const QVector<ImportedGame> &games) const
{
    int count = 0;
    for (const ImportedGame &game : games)
    {
        count += game.achievements.size();
    }
    return count;
}

/////////////////////////////////////////////////////////////////////

int DataTransporter::JsonIntValue(const QJsonValue &value) const
{
    return value.isNull() || value.isUndefined() ? 0 : value.toInt();
}

/////////////////////////////////////////////////////////////////////

qint64 DataTransporter::JsonDateValue(const QJsonValue &value) const
{
    if (value.isNull() || value.isUndefined())
    {
        return 0;
    }
    return static_cast<qint64>(value.toDouble(0));
}

/////////////////////////////////////////////////////////////////////

QJsonObject DataTransporter::BuildExportJson(int &exportedGameCount, int &exportedAchievementCount)
{
    exportedGameCount = 0;
    exportedAchievementCount = 0;

    QJsonArray games;
    const QVariantList gameRows = m_databaseManager.selectAll(
        m_databaseConnectionName,
        DATABASE_TABLE_EMU_GAMES,
        {
            "id",
            "game_name",
            "executable_location",
            "prefix_location",
            "installation_dir",
            "target_hidden",
            "total_amount_achievements",
            "total_unlocked_amount_achievements",
            "total_seconds_played",
            "last_played_date",
            "date_added"
        }
    );

    for (const QVariant &gameValue : gameRows)
    {
        const QVariantMap gameRow = gameValue.toMap();
        const int appId = Utils::MapIntValue(gameRow, "id");
        const QVariantList achievementRows = m_databaseManager.selectWhere(
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
                "cur_progress",
                "max_progress",
                "date_unlocked",
                "date_added"
            }
        );

        games.append(BuildGameJson(gameRow, achievementRows, exportedAchievementCount));
        ++exportedGameCount;
    }

    return {
        {"format", QStringLiteral("lymalink-achievements-export")},
        {"version", EXPORT_FILE_VERSION},
        {"game_count", exportedGameCount},
        {"exported_at", QDateTime::currentDateTimeUtc().toString(Qt::ISODate)},
        {"games", games}
    };
}

/////////////////////////////////////////////////////////////////////

QJsonObject DataTransporter::BuildGameJson(const QVariantMap &row, const QVariantList &achievementRows, int &exportedAchievementCount) const
{
    QJsonArray achievements;
    for (const QVariant &achievementValue : achievementRows)
    {
        achievements.append(BuildAchievementJson(achievementValue.toMap()));
        ++exportedAchievementCount;
    }

    return {
        {"id", NumberValue(row, "id")},
        {"name", Utils::MapStringValue(row, "game_name")},
        {"paths", QJsonObject{
            {"exe", Utils::MapStringValue(row, "executable_location")},
            {"prefix", Utils::MapStringValue(row, "prefix_location")},
            {"install", Utils::MapStringValue(row, "installation_dir")}
        }},
        {"hidden", row.value("target_hidden").toInt() == 1},
        {"stats", QJsonObject{
            {"total", NumberValue(row, "total_amount_achievements")},
            {"unlocked", NumberValue(row, "total_unlocked_amount_achievements")},
            {"seconds", NumberValue(row, "total_seconds_played")}
        }},
        {"dates", QJsonObject{
            {"last_played", DateValue(row, "last_played_date")},
            {"added", DateValue(row, "date_added")}
        }},
        {"achievements", achievements}
    };
}

/////////////////////////////////////////////////////////////////////

QJsonObject DataTransporter::BuildAchievementJson(const QVariantMap &row) const
{
    return {
        {"key", Utils::MapStringValue(row, "achievement_key")},
        {"name", Utils::MapStringValue(row, "achievement_name")},
        {"desc", Utils::MapStringValue(row, "achievement_description")},
        {"hidden", row.value("achievement_hidden").toInt() == 1},
        {"percent", NumberValue(row, "global_unlock_percentage")},
        {"progress", QJsonArray{
            NumberValue(row, "cur_progress"),
            NumberValue(row, "max_progress")
        }},
        {"dates", QJsonObject{
            {"unlocked", DateValue(row, "date_unlocked")},
            {"added", DateValue(row, "date_added")}
        }}
    };
}

/////////////////////////////////////////////////////////////////////

QJsonValue DataTransporter::DateValue(const QVariantMap &row, const QString &key) const
{
    const QVariant value = row.value(key);
    if (value.isNull() || !value.isValid())
    {
        return QJsonValue::Null;
    }
    return QJsonValue(value.toLongLong());
}

/////////////////////////////////////////////////////////////////////

QJsonValue DataTransporter::NumberValue(const QVariantMap &row, const QString &key) const
{
    const QVariant value = row.value(key);
    if (value.isNull() || !value.isValid())
    {
        return QJsonValue::Null;
    }

    if (value.metaType().id() == QMetaType::Double || value.metaType().id() == QMetaType::Float)
    {
        return QJsonValue(value.toDouble());
    }

    return QJsonValue(value.toLongLong());
}

/////////////////////////////////////////////////////////////////////

QVariantMap DataTransporter::SuccessPayload(const QString &filePath, int exportedGameCount, int exportedAchievementCount) const
{
    return {
        {"success", true},
        {"error", QString()},
        {"filePath", filePath},
        {"exportedGameCount", exportedGameCount},
        {"exportedAchievementCount", exportedAchievementCount},
        {"importedGameCount", 0},
        {"importedAchievementCount", 0},
        {"addedGameCount", 0},
        {"mergedGameCount", 0},
        {"replacedGameCount", 0},
        {"conflicts", QVariantList()}
    };
}

/////////////////////////////////////////////////////////////////////

QVariantMap DataTransporter::ErrorPayload(const QString &error, const QString &filePath) const
{
    if (filePath.isEmpty())
    {
        qWarning() << "DataTransporter:" << error;
    }
    else
    {
        qWarning() << "DataTransporter:" << error << "file:" << filePath;
    }

    return {
        {"success", false},
        {"error", error},
        {"filePath", filePath},
        {"exportedGameCount", 0},
        {"exportedAchievementCount", 0},
        {"importedGameCount", 0},
        {"importedAchievementCount", 0},
        {"addedGameCount", 0},
        {"mergedGameCount", 0},
        {"replacedGameCount", 0},
        {"conflicts", QVariantList()}
    };
}
