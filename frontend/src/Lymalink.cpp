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

#include <QDebug>
#include <QDir>
#include <QStandardPaths>

/////////////////////////////////////////////////////////////////////

Lymalink::Lymalink(QObject *parent) : QObject(parent)
{
    m_databaseConnectionName = DATABASE_CONNECTION_NAME;
    m_databasePath = "";
}

Lymalink::~Lymalink()
{
    m_workerThread.quit();
    m_workerThread.wait();
}

/////////////////////////////////////////////////////////////////////
////////////////////////////// PUBLIC ///////////////////////////////
/////////////////////////////////////////////////////////////////////

Error Lymalink::Initialize()
{
    m_steamApiWorker = new SteamApiWorker();
    m_steamApiWorker->moveToThread(&m_workerThread);

    connect(&m_workerThread, &QThread::started, m_steamApiWorker, &SteamApiWorker::Init);
    connect(&m_workerThread, &QThread::finished, m_steamApiWorker, &QObject::deleteLater);
    
    connect(this, &Lymalink::signalRequestSearchSteamAppIds, m_steamApiWorker, &SteamApiWorker::SearchSteamAppIds);
    connect(this, &Lymalink::signalRequestCancel, m_steamApiWorker, &SteamApiWorker::Cancel);
    connect(m_steamApiWorker, &SteamApiWorker::signalSearchAppIdsFinished, this, &Lymalink::signalSteamAppIdsReady);

    m_workerThread.start();

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
    emit signalRequestCancel();
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
        return Error::NoError;
    }

    if (!m_databaseManager.createDatabase(m_databaseConnectionName, m_databasePath))
    {
        qCritical() << "Lymalink: failed to create/open database:" << m_databaseManager.lastError();
        return Error::DatabaseError;
    }

    const bool gamesReady = m_databaseManager.createTable(
        m_databaseConnectionName,
        "steam_emu_games",
        {
            "id INTEGER PRIMARY KEY NOT NULL",
            "name TEXT NOT NULL",
            "lc2_url TEXT",
            "ci_url TEXT",
            "emulator_type TEXT",
            "installation_location TEXT",
            "prefix_location TEXT",
            "target_dir_found INTEGER DEFAULT 0",
            "target_dir_location TEXT",
            "total_amount_achievements INTEGER",
            "unlocked_amount_achievements INTEGER",
            "hours_played_total INTEGER",
            "last_unlock_date INTEGER",
            "last_played_date INTEGER",
            "updated_date INTEGER",
            "added_date INTEGER"
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
            "name TEXT NOT NULL",
            "description TEXT",
            "hidden INTEGER DEFAULT 0",
            "global_percentage REAL",
            "icon_url TEXT",
            "icon_gray_url TEXT",
            "achieved_date INTEGER",
            "updated_date INTEGER",
            "added_date INTEGER",
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
