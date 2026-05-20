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
}

/////////////////////////////////////////////////////////////////////

void SteamApiSearchWorker::SearchSteamAppIds(const QString &term)
{
    m_cancelled.storeRelease(0);

    QList<SteamSearchResult> searchResults;
    const Error searchError = m_steamApi->SearchAppId(term, searchResults);
    if (searchError != Error::NoError)
    {
        qDebug() << "SteamApiSearchWorker: SearchAppId failed:" << static_cast<int>(searchError);
        emit signalSearchAppIdsFinished(false, false, {});
        return;
    }

    QVariantList qmlResults;
    for (const SteamSearchResult &result : searchResults)
    {
        if (m_cancelled.loadAcquire())
        {
            qDebug() << "SteamApiSearchWorker: SearchSteamAppIds cancelled";
            emit signalSearchAppIdsFinished(false, true, {});
            return;
        }

        SteamGameInfo gameInfo;
        const Error gameInfoError = m_steamApi->SearchGameInfo(result.appId, gameInfo);
        if (gameInfoError != Error::NoError)
        {
            if (gameInfoError != Error::ParseError)
            {
                qDebug() << "SteamApiSearchWorker: SearchGameInfo failed for app id" << result.appId << ":" << static_cast<int>(gameInfoError);
            }
            continue;
        }

        if (gameInfo.type != SteamAppType::Game)
        {
            continue;
        }

        QVariantMap item;
        item["id"] = gameInfo.appId;
        item["name"] = gameInfo.gameName;
        qmlResults.append(item);
    }

    emit signalSearchAppIdsFinished(true, false, qmlResults);
}

/////////////////////////////////////////////////////////////////////

void SteamApiSearchWorker::Cancel()
{
    m_cancelled.storeRelease(1);
}

/////////////////////////////////////////////////////////////////////
///////////////////////////// PRIVATE ///////////////////////////////
/////////////////////////////////////////////////////////////////////