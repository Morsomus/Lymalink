/////////////////////////////////////////////////////////
// File: DatabaseUtils.cpp
// Date: 2026-09-03
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Implements Lymalink specific DatabaseUtils
/////////////////////////////////////////////////////////

#include "DatabaseUtils.h"
#include "../Defines.h"
#include "../tools/Utils.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryFile>

DatabaseUtils::DatabaseUtils(QObject *parent) : QObject(parent)
{
    // Constructor
}

DatabaseUtils::~DatabaseUtils()
{
    // Destructor
}

/////////////////////////////////////////////////////////////////////
////////////////////////////// PUBLIC ///////////////////////////////
/////////////////////////////////////////////////////////////////////

QVariantMap DatabaseUtils::VerifyDatabaseLocation(const QString &folderPath, const QString &databaseFileName) const
{
    const QString path = folderPath.trimmed();
    if (path.isEmpty())
    {
        return {{"success", false}, {"error", tr("Database path is empty.")}};
    }

    const QFileInfo inputInfo(path);
    if (!inputInfo.exists() || !inputInfo.isDir())
    {
        return {{"success", false}, {"error", tr("Select an existing folder.")}};
    }

    const QDir targetDir(inputInfo.absoluteFilePath());
    QTemporaryFile writeTest(targetDir.filePath(QStringLiteral(".sqlite_write_test_XXXXXX")));
    if (!writeTest.open())
    {
        return {{"success", false}, {"error", tr("Target path is not writable.")}};
    }
    writeTest.close();

    const QString resolvedPath = targetDir.filePath(databaseFileName);
    const QFileInfo fileInfo(resolvedPath);
    const bool databaseAlreadyExists = fileInfo.exists();
    if (databaseAlreadyExists && (!fileInfo.isFile() || !fileInfo.isReadable() || !fileInfo.isWritable()))
    {
        return {{"success", false}, {"error", tr("Database file '%1' is not readable and writable.").arg(databaseFileName)}};
    }

    const QString connectionName = QStringLiteral("sqlite_database_verify");
    if (QSqlDatabase::contains(connectionName))
    {
        QSqlDatabase::removeDatabase(connectionName);
    }

    bool success = true;
    QString failure;
    if (databaseAlreadyExists)
    {
        QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", connectionName);
        db.setDatabaseName(resolvedPath);
        if (!db.open())
        {
            success = false;
            failure = tr("Couldn't open database: %1").arg(db.lastError().text());
        }

        if (success)
        {
            QSqlQuery writableQuery(db);
            bool transactionStarted = false;
            if (!writableQuery.exec(QStringLiteral("BEGIN IMMEDIATE")))
            {
                success = false;
                failure = tr("Database write check failed: %1").arg(writableQuery.lastError().text());
            }
            else
            {
                transactionStarted = true;
                const QStringList writeStatements = {
                    QStringLiteral("CREATE TABLE IF NOT EXISTS lymalink_write_probe(id INTEGER PRIMARY KEY)"),
                    QStringLiteral("INSERT OR REPLACE INTO lymalink_write_probe(id) VALUES (1)"),
                    QStringLiteral("DELETE FROM lymalink_write_probe WHERE id = 1")
                };
                for (const QString &statement : writeStatements)
                {
                    if (!writableQuery.exec(statement))
                    {
                        success = false;
                        failure = tr("Database write check failed: %1").arg(writableQuery.lastError().text());
                        break;
                    }
                }
            }

            if (transactionStarted)
            {
                writableQuery.exec(QStringLiteral("ROLLBACK"));
            }
        }

        if (success)
        {
            QSqlQuery integrityQuery(db);
            if (!integrityQuery.exec(QStringLiteral("PRAGMA integrity_check")) || !integrityQuery.next() || integrityQuery.value(0).toString() != QStringLiteral("ok"))
            {
                const QString error = integrityQuery.lastError().isValid() ? integrityQuery.lastError().text() : integrityQuery.value(0).toString();
                success = false;
                failure = tr("Database integrity check failed: %1").arg(error);
            }
        }

        db.close();
        db = QSqlDatabase();
    }
    QSqlDatabase::removeDatabase(connectionName);

    return {{"success", success}, {"error", failure}, {"databaseExists", databaseAlreadyExists}};
}

/////////////////////////////////////////////////////////////////////

bool DatabaseUtils::IsDatabaseWriteAllowed(const QString &databasePath, QString *error, bool allowUnlockedInitWrite) const
{
    const QString id = Utils::MachineId();
    if (databasePath.isEmpty() || id.isEmpty())
    {
        if (error)
        {
            *error = DatabaseLockError();
        }
        return false;
    }

    QFile lockFile(QFileInfo(databasePath).absoluteDir().filePath(DATABASE_LOCK_FILE_NAME));
    if (!lockFile.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        if (allowUnlockedInitWrite)
        {
            return true;
        }
        if (error)
        {
            *error = DatabaseLockError();
        }
        return false;
    }

    const QByteArray lockData = lockFile.readAll();
    if (lockFile.error() != QFileDevice::NoError)
    {
        if (error)
        {
            *error = DatabaseLockError();
        }
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(lockData, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
    {
        if (error)
        {
            *error = DatabaseLockError();
        }
        return false;
    }

    const QJsonObject root = document.object();
    const QString lockMachineId = root.value(QStringLiteral("machineId")).toString();
    const qint64 lastHeartbeatAt = root.value(QStringLiteral("lastHeartbeatAt")).toVariant().toLongLong();
    if (lockMachineId.isEmpty() || lastHeartbeatAt <= 0)
    {
        if (error)
        {
            *error = DatabaseLockError();
        }
        return false;
    }

    const bool stale = QDateTime::currentSecsSinceEpoch() - lastHeartbeatAt > (DATABASE_DB_LOCK_LIFESPAN_SEC + 3);
    if (lockMachineId != id || stale)
    {
        if (allowUnlockedInitWrite && (lockMachineId.isEmpty() || stale))
        {
            return true;
        }
        if (error)
        {
            *error = DatabaseLockError();
        }
        return false;
    }

    return true;
}

/////////////////////////////////////////////////////////////////////

bool DatabaseUtils::ProbeRuntimeWriteAccess(const QString &connectionName, QString *error, bool *transientBusy) const
{
    if (error)
    {
        error->clear();
    }
    if (transientBusy)
    {
        *transientBusy = false;
    }

    if (!QSqlDatabase::contains(connectionName))
    {
        if (error)
        {
            *error = tr("Database connection is not available.");
        }
        return false;
    }

    QSqlDatabase db = QSqlDatabase::database(connectionName, false);
    if (!db.isOpen())
    {
        if (error)
        {
            *error = tr("Database connection is not open.");
        }
        return false;
    }

    auto handleLockError = [&](const QSqlError &sqlError) -> bool {
        bool ok = false;
        const int errorCode = sqlError.nativeErrorCode().toInt(&ok);
        if (ok)
        {
            constexpr int sqliteBusy = 5;
            constexpr int sqliteLocked = 6;
            const int primaryCode = errorCode & 0xFF;

            if (primaryCode == sqliteBusy || primaryCode == sqliteLocked)
            {
                if (transientBusy)
                {
                    *transientBusy = true;
                }
                return true;
            }
        }

        const QString errorText = sqlError.text().toLower();
        if (errorText.contains(QStringLiteral("database is locked")) ||
            errorText.contains(QStringLiteral("database is busy")) ||
            errorText.contains(QStringLiteral("database table is locked")))
        {
            if (transientBusy)
            {
                *transientBusy = true;
            }
            return true;
        }

        return false;
    };

    QSqlQuery query(db);
    const QStringList statements = {
        QStringLiteral("BEGIN IMMEDIATE"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS lymalink_write_probe(id INTEGER PRIMARY KEY)"),
        QStringLiteral("INSERT OR REPLACE INTO lymalink_write_probe(id) VALUES (1)"),
        QStringLiteral("DELETE FROM lymalink_write_probe WHERE id = 1")
    };

    bool transactionStarted = false;
    for (int i = 0; i < statements.size(); ++i)
    {
        const QString &statement = statements[i];
        if (!query.exec(statement))
        {
            const QSqlError probeSqlError = query.lastError();
            if (transactionStarted)
            {
                query.exec(QStringLiteral("ROLLBACK"));
            }

            if (handleLockError(probeSqlError))
            {
                return false;
            }

            if (error)
            {
                *error = probeSqlError.text();
            }
            return false;
        }

        if (i == 0)
        {
            transactionStarted = true;
        }
    }

    if (!query.exec(QStringLiteral("ROLLBACK")))
    {
        const QSqlError rollbackSqlError = query.lastError();
        if (handleLockError(rollbackSqlError))
        {
            return false;
        }

        if (error)
        {
            *error = rollbackSqlError.text();
        }
        return false;
    }

    return true;
}

/////////////////////////////////////////////////////////////////////
///////////////////////////// PRIVATE ///////////////////////////////
/////////////////////////////////////////////////////////////////////

QString DatabaseUtils::DatabaseLockError() const
{
    return tr("Database connection is not fully established. Please make sure the background service is running, and only one application instance is accessing to the database.");
}
