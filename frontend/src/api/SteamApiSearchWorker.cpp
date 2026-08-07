/////////////////////////////////////////////////////////
// File: SteamApiSearchWorker.cpp
// Date: 2026-05-19
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Implements Steam API search worker
/////////////////////////////////////////////////////////

#include "SteamApiSearchWorker.h"

#include <QDebug>

/////////////////////////////////////////////////////////////////////

SteamApiSearchWorker::SteamApiSearchWorker(QObject *parent) : QObject(parent)
{
    m_cancelled.storeRelease(0);
    m_steamApi = nullptr;
}

SteamApiSearchWorker::~SteamApiSearchWorker()
{
    // Destructor
}

/////////////////////////////////////////////////////////////////////
////////////////////////////// PUBLIC ///////////////////////////////
/////////////////////////////////////////////////////////////////////

void SteamApiSearchWorker::Init()
{
    m_steamApi = new SteamApi(this);
    qDebug() << "SteamApiSearchWorker::Init: SteamApi initialized";
}

/////////////////////////////////////////////////////////////////////

void SteamApiSearchWorker::SearchAppIds(const QString &term)
{
    if (!m_steamApi)
    {
        qWarning() << "SteamApiSearchWorker::SearchAppIds: SteamApi is not initialized";
        emit signalSearchAppIdsFinished(false, false, {});
        return;
    }

    if (term.trimmed().isEmpty())
    {
        qWarning() << "SteamApiSearchWorker::SearchAppIds: empty search term";
        emit signalSearchAppIdsFinished(false, false, {});
        return;
    }

    // Reset cancellation state before starting a new search
    m_cancelled.storeRelease(0);

    // Fetch candidate app IDs from Steam store search
    QList<SteamSearchResult> searchResults = {};
    const Error searchError = m_steamApi->SearchAppId(term, searchResults);
    if (searchError != Error::NoError)
    {
        qWarning() << "SteamApiSearchWorker::SearchAppIds: SearchAppId failed:" << static_cast<int>(searchError);
        emit signalSearchAppIdsFinished(false, false, {});
        return;
    }

    QVariantList qmlResults = {};
    for (const SteamSearchResult &result : searchResults)
    {
        // Stop promptly if UI requested cancellation
        if (m_cancelled.loadAcquire())
        {
            qDebug() << "SteamApiSearchWorker::SearchAppIds: cancelled";
            emit signalSearchAppIdsFinished(false, true, {});
            return;
        }

        // Hydrate each candidate enough to filter non-game app types
        SteamGameInfo gameInfo = {};
        const Error gameInfoError = m_steamApi->SearchGameInfo(result.appId, gameInfo, SteamApi::English, true);
        if (gameInfoError != Error::NoError)
        {
            if (gameInfoError != Error::ParseError)
            {
                qWarning() << "SteamApiSearchWorker::SearchAppIds: SearchGameInfo failed for app id" << result.appId << ":" << static_cast<int>(gameInfoError);
            }
            continue;
        }

        if (gameInfo.type != SteamAppType::Game)
        {
            continue;
        }

        // Package minimal search result for QML
        QVariantMap item = {};
        item["id"] = gameInfo.appId;
        item["name"] = gameInfo.gameName;
        qmlResults.append(item);
    }

    emit signalSearchAppIdsFinished(true, false, qmlResults);
    qDebug() << "SteamApiSearchWorker::SearchAppIds: finished with results:" << qmlResults.size();
}

/////////////////////////////////////////////////////////////////////

void SteamApiSearchWorker::CancelSearchAppIds()
{
    if (!m_steamApi)
    {
        qWarning() << "SteamApiSearchWorker::CancelSearchAppIds: SteamApi is not initialized";
        return;
    }

    // Signal current SearchAppIds loop to stop after current network call
    m_cancelled.storeRelease(1);
    qDebug() << "SteamApiSearchWorker::CancelSearchAppIds: cancellation requested";
}

/////////////////////////////////////////////////////////////////////
///////////////////////////// PRIVATE ///////////////////////////////
/////////////////////////////////////////////////////////////////////
