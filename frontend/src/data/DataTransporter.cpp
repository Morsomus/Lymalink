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
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QStandardPaths>

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
        {"exportedAchievementCount", exportedAchievementCount}
    };
}

/////////////////////////////////////////////////////////////////////

QVariantMap DataTransporter::ErrorPayload(const QString &error, const QString &filePath) const
{
    return {
        {"success", false},
        {"error", error},
        {"filePath", filePath},
        {"exportedGameCount", 0},
        {"exportedAchievementCount", 0}
    };
}
