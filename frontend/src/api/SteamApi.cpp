/////////////////////////////////////////////////////////
// File: SteamApi.cpp
// Date: 2026-05-15
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Implements Steam API for assets
/////////////////////////////////////////////////////////

#include "SteamApi.h"

#include <QDebug>
#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QStringList>
#include <QUrl>
#include <QUrlQuery>
#include <cmath>

/////////////////////////////////////////////////////////////////////

SteamApi::SteamApi(QObject *parent) : QObject(parent)
{
    m_networkManager = nullptr;
    m_localeMap = {};

    m_networkManager = new QNetworkAccessManager();
    m_networkManager->setTransferTimeout(15000);
    InitializeLocaleMap();
}

SteamApi::~SteamApi()
{
    delete m_networkManager;
}

/////////////////////////////////////////////////////////////////////
////////////////////////////// PUBLIC ///////////////////////////////
/////////////////////////////////////////////////////////////////////

Error SteamApi::SearchAppId(const QString &term, QList<SteamSearchResult> &results, Locale locale)
{
    Error err = Error::NoError;

    // Clear caller output before validation and network request
    results.clear();

    if (term.isEmpty())
    {
        qWarning() << "SteamApi::SearchAppId: Search term is empty";
        err = Error::InvalidParameter;
        return err;
    }

    // Build localized store search URL
    QUrl url("https://store.steampowered.com/api/storesearch");
    const QPair<QString, QString> localeSettings = m_localeMap.value(locale, m_localeMap.value(English));

    QUrlQuery query;
    query.addQueryItem("term", term);
    query.addQueryItem("cc", localeSettings.first);
    query.addQueryItem("l", localeSettings.second);
    url.setQuery(query);

    QNetworkRequest request(url);
    request.setRawHeader("User-Agent", "Mozilla/5.0");
    request.setRawHeader("Accept", "application/json");

    // Execute request synchronously inside worker thread
    QNetworkReply *reply = m_networkManager->get(request);

    QEventLoop loop;
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec(QEventLoop::ExcludeUserInputEvents);
    if (reply->error() != QNetworkReply::NoError)
    {
        // Muutettu qDebug -> qWarning, koska kyseessä on verkkoyhteysvirhe
        qWarning() << "SteamApi::SearchAppId: App id search failed:" << reply->errorString();
        reply->deleteLater();
        err = Error::NotFound;
        return err;
    }

    // Parse Steam store search result list
    const QByteArray data = reply->readAll();
    reply->deleteLater();

    results = ParseSearchResponse(data);
    return err;
}

/////////////////////////////////////////////////////////////////////


Error SteamApi::SearchGameInfo(int appId, SteamGameInfo &gameInfo, Locale locale)
{
    Error err = Error::NoError;

    // Reset output before validation and API request
    gameInfo = SteamGameInfo();

    if (appId <= 0)
    {
        qWarning() << "SteamApi::SearchGameInfo: Invalid appId:" << appId;
        err = Error::InvalidParameter;
        return err;
    }

    const QPair<QString, QString> localeSettings = m_localeMap.value(locale, m_localeMap.value(English));

    // Build GetItems input_json payload for one app
    QJsonObject idObject;
    idObject["appid"] = appId;

    QJsonArray ids;
    ids.append(idObject);

    QJsonObject context;
    context["country_code"] = localeSettings.first.toUpper();

    QJsonObject dataRequest;
    dataRequest["include_assets"] = true;

    QJsonObject input;
    input["ids"] = ids;
    input["context"] = context;
    input["data_request"] = dataRequest;

    QUrl url("https://api.steampowered.com/IStoreBrowseService/GetItems/v1/");
    QUrlQuery query;
    query.addQueryItem("input_json", QString::fromUtf8(QJsonDocument(input).toJson(QJsonDocument::Compact)));
    url.setQuery(query);

    QNetworkRequest request(url);
    request.setRawHeader("User-Agent", "Mozilla/5.0");
    request.setRawHeader("Accept", "application/json");

    // Execute request synchronously inside worker thread
    QNetworkReply *reply = m_networkManager->get(request);

    QEventLoop loop;
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec(QEventLoop::ExcludeUserInputEvents);
    if (reply->error() != QNetworkReply::NoError)
    {
        qWarning() << "SteamApi::SearchGameInfo: Primary game info search failed:" << reply->errorString();
        reply->deleteLater();
        err = Error::NotFound;
        return err;
    }

    QString errorMessage;
    const QByteArray data = reply->readAll();
    reply->deleteLater();

    // Convert primary response to normalized game info model
    gameInfo = ParseGameInfoResponse(data, appId, &errorMessage);
    if (!errorMessage.isEmpty())
    {
        qCritical() << "SteamApi::SearchGameInfo: Primary game info parse failed:" << errorMessage;
        gameInfo = SteamGameInfo();
        err = Error::ParseError;
        return err;
    }

    return err;
}

/////////////////////////////////////////////////////////////////////

Error SteamApi::GetLibraryCapsuleUrls(int appId, const QString &lcSuffix, const QString &assetUrlFormat, QList<QString> &urls)
{
    Error err = Error::NoError;

    // Clear caller output before validating URL parts
    urls.clear();

    if (appId <= 0)
    {
        qWarning() << "SteamApi::GetLibraryCapsuleUrls: Invalid app id:" << appId;
        err = Error::InvalidParameter;
        return err;
    }

    // Normalize Steam asset filename from API suffix
    QString normalizedFileName = lcSuffix.trimmed();
    if (normalizedFileName.isEmpty())
    {
        qWarning() << "SteamApi::GetLibraryCapsuleUrls: Empty lcSuffix";
        err = Error::InvalidParameter;
        return err;
    }

    if (normalizedFileName.startsWith("http", Qt::CaseInsensitive))
    {
        qWarning() << "SteamApi::GetLibraryCapsuleUrls: lcSuffix already contains URL:" << normalizedFileName;
        err = Error::InvalidParameter;
        return err;
    }

    while (normalizedFileName.startsWith('/'))
    {
        normalizedFileName.remove(0, 1);
    }
    if (normalizedFileName.contains('?'))
    {
        normalizedFileName = normalizedFileName.section('?', 0, 0);
    }
    if (!IncludesExpectedExtension(normalizedFileName))
    {
        qWarning() << "SteamApi::GetLibraryCapsuleUrls: Invalid lcSuffix filename:" << lcSuffix;
        err = Error::InvalidParameter;
        return err;
    }

    // Normalize Steam asset URL format and verify placeholder
    QString normalizedAssetUrlFormat = assetUrlFormat.trimmed();
    if (normalizedAssetUrlFormat.isEmpty())
    {
        qWarning() << "SteamApi::GetLibraryCapsuleUrls: Empty assetUrlFormat";
        err = Error::InvalidParameter;
        return err;
    }

    if (normalizedAssetUrlFormat.startsWith("http", Qt::CaseInsensitive))
    {
        qWarning() << "SteamApi::GetLibraryCapsuleUrls: assetUrlFormat contains URL:" << normalizedAssetUrlFormat;
        err = Error::InvalidParameter;
        return err;
    }

    while (normalizedAssetUrlFormat.startsWith('/'))
    {
        normalizedAssetUrlFormat.remove(0, 1);
    }
    if (!normalizedAssetUrlFormat.contains("${FILENAME}"))
    {
        qWarning() << "SteamApi::GetLibraryCapsuleUrls: assetUrlFormat missing filename placeholder:" << assetUrlFormat;
        err = Error::InvalidParameter;
        return err;
    }

    // Reject asset formats that point at another app id
    const QString expectedAssetPrefix = QString("steam/apps/%1/").arg(appId);
    if (!normalizedAssetUrlFormat.startsWith(expectedAssetPrefix))
    {
        qWarning() << "SteamApi::GetLibraryCapsuleUrls: assetUrlFormat app id mismatch:" << assetUrlFormat;
        err = Error::InvalidParameter;
        return err;
    }

    // Build CDN fallback list for same asset path
    const QString assetPath = normalizedAssetUrlFormat.replace("${FILENAME}", normalizedFileName);
    urls.append(QString("https://shared.steamstatic.com/store_item_assets/%1").arg(assetPath));
    urls.append(QString("https://shared.cloudflare.steamstatic.com/store_item_assets/%1").arg(assetPath));
    urls.append(QString("https://shared.fastly.steamstatic.com/store_item_assets/%1").arg(assetPath));
    urls.append(QString("https://shared.akamai.steamstatic.com/store_item_assets/%1").arg(assetPath));

    return err;
}

/////////////////////////////////////////////////////////////////////

Error SteamApi::GetCommunityIconUrls(int appId, const QString &ciSuffix, QList<QString> &urls)
{
    Error err = Error::NoError;

    // Clear caller output before validating community icon suffix
    urls.clear();

    if (appId <= 0)
    {
        qWarning() << "SteamApi::GetCommunityIconUrls: Invalid app id:" << appId;
        err = Error::InvalidParameter;
        return err;
    }

    const QString trimmedCiUrl = ciSuffix.trimmed();
    if (trimmedCiUrl.isEmpty())
    {
        qWarning() << "SteamApi::GetCommunityIconUrls: Empty ciSuffix";
        err = Error::InvalidParameter;
        return err;
    }

    if (trimmedCiUrl.startsWith("http", Qt::CaseInsensitive))
    {
        qWarning() << "SteamApi::GetCommunityIconUrls: ciSuffix already contains URL:" << trimmedCiUrl;
        err = Error::InvalidParameter;
        return err;
    }

    // Normalize icon filename from suffix/path/query value
    QString normalizedFileName = trimmedCiUrl;
    if (normalizedFileName.contains('/'))
    {
        normalizedFileName = normalizedFileName.section('/', -1);
    }
    if (normalizedFileName.contains('?'))
    {
        normalizedFileName = normalizedFileName.section('?', 0, 0);
    }

    if (normalizedFileName.isEmpty())
    {
        qWarning() << "SteamApi::GetCommunityIconUrls: Invalid ciSuffix filename:" << ciSuffix;
        err = Error::InvalidParameter;
        return err;
    }

    if (!IncludesExpectedExtension(normalizedFileName))
    {
        normalizedFileName.append(".jpg");
    }

    // Build CDN fallback list for community icon
    const QString appIdString = QString::number(appId);
    urls.append(QString("https://cdn.cloudflare.steamstatic.com/steamcommunity/public/images/apps/%1/%2").arg(appIdString, normalizedFileName));
    urls.append(QString("https://cdn.fastly.steamstatic.com/steamcommunity/public/images/apps/%1/%2").arg(appIdString, normalizedFileName));
    urls.append(QString("https://cdn.akamai.steamstatic.com/steamcommunity/public/images/apps/%1/%2").arg(appIdString, normalizedFileName));
    urls.append(QString("https://shared.cloudflare.steamstatic.com/community_assets/images/apps/%1/%2").arg(appIdString, normalizedFileName));
    urls.append(QString("https://shared.fastly.steamstatic.com/community_assets/images/apps/%1/%2").arg(appIdString, normalizedFileName));
    urls.append(QString("https://shared.akamai.steamstatic.com/community_assets/images/apps/%1/%2").arg(appIdString, normalizedFileName));
    urls.append(QString("https://steamcdn-a.akamaihd.net/steamcommunity/public/images/apps/%1/%2").arg(appIdString, normalizedFileName));

    return err;
}

/////////////////////////////////////////////////////////////////////

Error SteamApi::GetAchievementIconUrls(int appId, const QList<SteamAchievementData> &achievements, QList<SteamAchievementIconUrls> &urls)
{
    Error err = Error::NoError;

    // Clear caller output before validating achievement list
    urls.clear();

    if (appId <= 0)
    {
        qWarning() << "SteamApi::GetAchievementIconUrls: Invalid app id:" << appId;
        err = Error::InvalidParameter;
        return err;
    }

    if (achievements.isEmpty())
    {
        qWarning() << "SteamApi::GetAchievementIconUrls: Empty achievements";
        err = Error::InvalidParameter;
        return err;
    }

    // Use all known Steam community CDN hostnames as fallback sources
    const QString appIdString = QString::number(appId);
    const QStringList urlFormats = {
        "https://cdn.cloudflare.steamstatic.com/steamcommunity/public/images/apps/%1/%2",
        "https://cdn.fastly.steamstatic.com/steamcommunity/public/images/apps/%1/%2",
        "https://cdn.akamai.steamstatic.com/steamcommunity/public/images/apps/%1/%2",
        "https://shared.cloudflare.steamstatic.com/community_assets/images/apps/%1/%2",
        "https://shared.fastly.steamstatic.com/community_assets/images/apps/%1/%2",
        "https://shared.akamai.steamstatic.com/community_assets/images/apps/%1/%2",
        "https://steamcdn-a.akamaihd.net/steamcommunity/public/images/apps/%1/%2"
    };

    for (const SteamAchievementData &achievement : achievements)
    {
        // Validate every achievement belongs to requested app
        if (achievement.appId != appId)
        {
            qWarning() << "SteamApi::GetAchievementIconUrls: Achievement app id mismatch:" << achievement.appId;
            urls.clear();
            err = Error::InvalidParameter;
            return err;
        }
        if (achievement.achievementKey.isEmpty())
        {
            qWarning() << "SteamApi::GetAchievementIconUrls: Empty achievement key";
            urls.clear();
            err = Error::InvalidParameter;
            return err;
        }

        // Normalize active and locked achievement icon filenames
        const QString normalizedIconFileName = NormalizeSteamImageFileName(achievement.iconSuffix);
        const QString normalizedGrayIconFileName = NormalizeSteamImageFileName(achievement.iconGraySuffix);
        if (normalizedIconFileName.isEmpty() || normalizedGrayIconFileName.isEmpty())
        {
            qWarning() << "SteamApi::GetAchievementIconUrls: Invalid achievement icon suffix:" << achievement.achievementKey;
            urls.clear();
            err = Error::InvalidParameter;
            return err;
        }

        // Build active and grayscale icon CDN URL lists
        SteamAchievementIconUrls achievementUrls = {};
        achievementUrls.achievementKey = achievement.achievementKey;
        for (const QString &urlFormat : urlFormats)
        {
            achievementUrls.iconUrls.append(QString(urlFormat).arg(appIdString, normalizedIconFileName));
        }
        for (const QString &urlFormat : urlFormats)
        {
            achievementUrls.iconGrayUrls.append(QString(urlFormat).arg(appIdString, normalizedGrayIconFileName));
        }
        urls.append(achievementUrls);
    }

    return err;
}

/////////////////////////////////////////////////////////////////////

Error SteamApi::FetchAchievementDataPrimary(int appId, QList<SteamAchievementData> &achievements, Locale locale)
{
    Error err = Error::NoError;

    // Clear caller output before validation and network request
    achievements.clear();

    if (appId <= 0)
    {
        qWarning() << "SteamApi::FetchAchievementDataPrimary: Invalid app id:" << appId;
        err = Error::InvalidParameter;
        return err;
    }

    const QPair<QString, QString> localeSettings = m_localeMap.value(locale, m_localeMap.value(English));

    // Build primary public Steam achievement endpoint request
    QUrl url("https://api.steampowered.com/IPlayerService/GetGameAchievements/v1/");
    QUrlQuery query;
    query.addQueryItem("appid", QString::number(appId));
    query.addQueryItem("language", localeSettings.second);
    url.setQuery(query);

    QNetworkRequest request(url);
    request.setRawHeader("User-Agent", "Mozilla/5.0");
    request.setRawHeader("Accept", "application/json");

    // Execute request synchronously inside worker thread
    QNetworkReply *reply = m_networkManager->get(request);

    QEventLoop loop;
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec(QEventLoop::ExcludeUserInputEvents);
    if (reply->error() != QNetworkReply::NoError)
    {
        qWarning() << "SteamApi::FetchAchievementDataPrimary: Primary achievement data fetch failed:" << reply->errorString();
        reply->deleteLater();
        err = Error::NotFound;
        return err;
    }

    QString errorMessage;
    const QByteArray data = reply->readAll();
    reply->deleteLater();

    // Check if response contains actual achievements data
    QJsonParseError noDataParseError;
    const QJsonDocument noDataDoc = QJsonDocument::fromJson(data, &noDataParseError);
    if (noDataParseError.error == QJsonParseError::NoError)
    {
        if (noDataDoc.isNull())
        {
            qDebug() << "SteamApi::FetchAchievementDataPrimary: JSON document is null";
            err = Error::NoData;
            return err;
        }

        if (noDataDoc.isObject())
        {
            const QJsonObject root = noDataDoc.object();
            const QJsonValue responseValue = root["response"];
            const QJsonObject responseObject = responseValue.isObject() ? responseValue.toObject() : QJsonObject();
            const QJsonValue achievementsValue = responseObject["achievements"];

            if (root.isEmpty() ||
                (responseValue.isObject() && responseObject.isEmpty()) ||
                (achievementsValue.isArray() && achievementsValue.toArray().isEmpty()))
            {
                qDebug() << "SteamApi::FetchAchievementDataPrimary: Response object or achievement list is empty";
                err = Error::NoData;
                return err;
            }
        }
        else if (noDataDoc.isArray() && noDataDoc.array().isEmpty())
        {
            qDebug() << "SteamApi::FetchAchievementDataPrimary: JSON array is empty";
            err = Error::NoData;
            return err;
        }
    }

    // Parse normalized achievement data from primary response
    achievements = ParseAchievementDataResponse(data, appId, &errorMessage);
    if (!errorMessage.isEmpty())
    {
        qCritical() << "SteamApi::FetchAchievementDataPrimary: Primary achievement data parse failed:" << errorMessage;
        achievements.clear();
        err = Error::ParseError;
        return err;
    }
    if (achievements.isEmpty())
    {
        qDebug() << "SteamApi::FetchAchievementDataPrimary: Parsed achievements list is empty";
        err = Error::NoData;
    }

    return err;
}

/////////////////////////////////////////////////////////////////////

Error SteamApi::FetchAchievementDataSecondary(int appId, QList<SteamAchievementData> &achievements, Locale locale, const QString &apiKey)
{
    Error err = Error::NoError;

    // Clear caller output before validation and fallback requests
    achievements.clear();

    if (appId <= 0)
    {
        qWarning() << "SteamApi::FetchAchievementDataSecondary: Invalid app id:" << appId;
        err = Error::InvalidParameter;
        return err;
    }

    if (apiKey.isEmpty())
    {
        qWarning() << "SteamApi::FetchAchievementDataSecondary: Api key is empty";
        err = Error::InvalidParameter;
        return err;
    }

    const QPair<QString, QString> localeSettings = m_localeMap.value(locale, m_localeMap.value(English));

    // Request private schema data using API key
    QUrl schemaUrl("https://api.steampowered.com/ISteamUserStats/GetSchemaForGame/v2/");
    QUrlQuery schemaQuery;
    schemaQuery.addQueryItem("key", apiKey);
    schemaQuery.addQueryItem("appid", QString::number(appId));
    schemaQuery.addQueryItem("l", localeSettings.second);
    schemaUrl.setQuery(schemaQuery);

    QNetworkRequest schemaRequest(schemaUrl);
    schemaRequest.setRawHeader("User-Agent", "Mozilla/5.0");
    schemaRequest.setRawHeader("Accept", "application/json");

    QNetworkReply *schemaReply = m_networkManager->get(schemaRequest);

    QEventLoop schemaLoop;
    connect(schemaReply, &QNetworkReply::finished, &schemaLoop, &QEventLoop::quit);
    schemaLoop.exec(QEventLoop::ExcludeUserInputEvents);
    if (schemaReply->error() != QNetworkReply::NoError)
    {
        qWarning() << "SteamApi::FetchAchievementDataSecondary: Secondary achievement schema fetch failed:" << schemaReply->errorString();
        schemaReply->deleteLater();
        err = Error::NotFound;
        return err;
    }

    const QByteArray schemaData = schemaReply->readAll();
    schemaReply->deleteLater();

    // Request global unlock percentages from public endpoint
    QUrl percentageUrl("https://api.steampowered.com/ISteamUserStats/GetGlobalAchievementPercentagesForApp/v0002/");
    QUrlQuery percentageQuery;
    percentageQuery.addQueryItem("gameid", QString::number(appId));
    percentageQuery.addQueryItem("format", "json");
    percentageUrl.setQuery(percentageQuery);

    QNetworkRequest percentageRequest(percentageUrl);
    percentageRequest.setRawHeader("User-Agent", "Mozilla/5.0");
    percentageRequest.setRawHeader("Accept", "application/json");

    QNetworkReply *percentageReply = m_networkManager->get(percentageRequest);

    QEventLoop percentageLoop;
    connect(percentageReply, &QNetworkReply::finished, &percentageLoop, &QEventLoop::quit);
    percentageLoop.exec(QEventLoop::ExcludeUserInputEvents);
    if (percentageReply->error() != QNetworkReply::NoError)
    {
        qWarning() << "SteamApi::FetchAchievementDataSecondary: Global achievement percentage fetch failed:" << percentageReply->errorString();
        percentageReply->deleteLater();
        err = Error::NotFound;
        return err;
    }

    const QByteArray percentageData = percentageReply->readAll();
    percentageReply->deleteLater();

    // Request community descriptions from Steam Hunters fallback API
    QUrl descriptionsUrl(QString("https://steamhunters.com/api/apps/%1/achievements").arg(appId));

    QNetworkRequest descriptionsRequest(descriptionsUrl);
    descriptionsRequest.setRawHeader("User-Agent", "Mozilla/5.0");
    descriptionsRequest.setRawHeader("Accept", "application/json");

    QNetworkReply *descriptionsReply = m_networkManager->get(descriptionsRequest);

    QEventLoop descriptionsLoop;
    connect(descriptionsReply, &QNetworkReply::finished, &descriptionsLoop, &QEventLoop::quit);
    descriptionsLoop.exec(QEventLoop::ExcludeUserInputEvents);
    if (descriptionsReply->error() != QNetworkReply::NoError)
    {
        qWarning() << "SteamApi::FetchAchievementDataSecondary: Steam Hunters achievement descriptions fetch failed:" << descriptionsReply->errorString();
        descriptionsReply->deleteLater();
        err = Error::NotFound;
        return err;
    }

    const QByteArray descriptionsData = descriptionsReply->readAll();
    descriptionsReply->deleteLater();

    // Merge secondary sources into primary-compatible JSON shape
    QString errorMessage;
    const QByteArray primaryCompatibleData = BuildPrimaryAchievementDataResponseFromSecondary(schemaData, percentageData, descriptionsData, &errorMessage);
    if (!errorMessage.isEmpty())
    {
        qCritical() << "SteamApi::FetchAchievementDataSecondary: Error while building primary data response from secondary:" << errorMessage;
        err = Error::ParseError;
        return err;
    }

    // Parse merged response using primary parser
    achievements = ParseAchievementDataResponse(primaryCompatibleData, appId, &errorMessage);
    if (!errorMessage.isEmpty())
    {
        qCritical() << "SteamApi::FetchAchievementDataSecondary: Secondary achievement data parse failed:" << errorMessage;
        achievements.clear();
        err = Error::ParseError;
    }

    return err;
}

/////////////////////////////////////////////////////////////////////

Error SteamApi::FetchOwnedGames(const QString &steamId, QList<SteamOwnedGameData> &games, const QString &apiKey)
{
    Error err = Error::NoError;

    // Clear caller output before validation and network request
    games.clear();

    const QString trimmedSteamId = steamId.trimmed();
    const QString trimmedApiKey = apiKey.trimmed();
    if (trimmedApiKey.isEmpty() || !IsValidSteamId(trimmedSteamId))
    {
        qWarning() << "SteamApi::FetchOwnedGames: invalid credentials or Steam ID";
        err = Error::InvalidParameter;
        return err;
    }

    QUrl url("https://api.steampowered.com/IPlayerService/GetOwnedGames/v1/");
    QUrlQuery query;
    query.addQueryItem("key", trimmedApiKey);
    query.addQueryItem("steamid", trimmedSteamId);
    query.addQueryItem("include_appinfo", "1");
    query.addQueryItem("include_played_free_games", "1");
    query.addQueryItem("format", "json");
    url.setQuery(query);

    QNetworkRequest request(url);
    request.setRawHeader("User-Agent", "Mozilla/5.0");
    request.setRawHeader("Accept", "application/json");

    // Execute request synchronously inside worker thread
    QNetworkReply *reply = m_networkManager->get(request);

    QEventLoop loop;
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec(QEventLoop::ExcludeUserInputEvents);

    const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QByteArray data = reply->readAll();
    if (reply->error() != QNetworkReply::NoError)
    {
        qWarning() << "SteamApi::FetchOwnedGames: owned games fetch failed with status:" << statusCode << "network error:" << static_cast<int>(reply->error());
        reply->deleteLater();
        err = statusCode == 403 ? Error::AccessDenied : Error::NotFound;
        return err;
    }
    reply->deleteLater();

    QString errorMessage;
    err = ParseOwnedGamesResponse(data, games, &errorMessage);
    if (!errorMessage.isEmpty())
    {
        qCritical() << "SteamApi::FetchOwnedGames:" << errorMessage;
    }

    return err;
}

/////////////////////////////////////////////////////////////////////

Error SteamApi::FetchPlayerAchievements(int appId, const QString &steamId, QList<SteamPlayerAchievementData> &achievements, const QString &apiKey, Locale locale)
{
    Error err = Error::NoError;

    // Clear caller output before validation and network request
    achievements.clear();

    const QString trimmedSteamId = steamId.trimmed();
    const QString trimmedApiKey = apiKey.trimmed();
    if (appId <= 0 || trimmedApiKey.isEmpty() || !IsValidSteamId(trimmedSteamId))
    {
        qWarning() << "SteamApi::FetchPlayerAchievements: invalid app ID, credentials or Steam ID";
        err = Error::InvalidParameter;
        return err;
    }

    const QPair<QString, QString> localeSettings = m_localeMap.value(locale, m_localeMap.value(English));

    QUrl url("https://api.steampowered.com/ISteamUserStats/GetPlayerAchievements/v1/");
    QUrlQuery query;
    query.addQueryItem("key", trimmedApiKey);
    query.addQueryItem("steamid", trimmedSteamId);
    query.addQueryItem("appid", QString::number(appId));
    query.addQueryItem("l", localeSettings.second);
    query.addQueryItem("format", "json");
    url.setQuery(query);

    QNetworkRequest request(url);
    request.setRawHeader("User-Agent", "Mozilla/5.0");
    request.setRawHeader("Accept", "application/json");

    // Execute request synchronously inside worker thread
    QNetworkReply *reply = m_networkManager->get(request);

    QEventLoop loop;
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec(QEventLoop::ExcludeUserInputEvents);

    // Handle network errors and special-case private profile detection
    const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QByteArray data = reply->readAll();
    if (reply->error() != QNetworkReply::NoError)
    {
        if (IsPrivateProfileResponse(data))
        {
            err = Error::ProfileNotPublic;
        }
        else if (statusCode == 403)
        {
            err = Error::NoData;
        }
        else
        {
            err = Error::NotFound;
        }
        
        if (err == Error::ProfileNotPublic)
        {
            qWarning() << "SteamApi::FetchPlayerAchievements: Steam profile is not public for appId:" << appId;
        }
        else if (err == Error::NoData)
        {
            qDebug() << "SteamApi::FetchPlayerAchievements: player achievements unavailable for appId:" << appId << "status:" << statusCode << "network error:" << static_cast<int>(reply->error());
        }
        else
        {
            qWarning() << "SteamApi::FetchPlayerAchievements: player achievements fetch failed for appId:" << appId << "status:" << statusCode << "network error:" << static_cast<int>(reply->error());
        }
        reply->deleteLater();
        return err;
    }
    reply->deleteLater();

    QString errorMessage;
    err = ParsePlayerAchievementsResponse(data, appId, achievements, &errorMessage);
    if (!errorMessage.isEmpty())
    {
        qCritical() << "SteamApi::FetchPlayerAchievements:" << errorMessage;
    }

    return err;
}

/////////////////////////////////////////////////////////////////////
///////////////////////////// PRIVATE ///////////////////////////////
/////////////////////////////////////////////////////////////////////

void SteamApi::InitializeLocaleMap()
{
    // Map UI locale enum to Steam country code and language parameter
    m_localeMap[SteamApi::English]           = qMakePair(QString("us"), QString("english"));
    m_localeMap[SteamApi::Finnish]           = qMakePair(QString("fi"), QString("finnish"));
    m_localeMap[SteamApi::German]            = qMakePair(QString("de"), QString("german"));
    m_localeMap[SteamApi::Russian]           = qMakePair(QString("ru"), QString("russian"));
    m_localeMap[SteamApi::French]            = qMakePair(QString("fr"), QString("french"));
    m_localeMap[SteamApi::Spanish]           = qMakePair(QString("es"), QString("spanish"));
    m_localeMap[SteamApi::SimplifiedChinese] = qMakePair(QString("cn"), QString("schinese"));
    m_localeMap[SteamApi::Japanese]          = qMakePair(QString("jp"), QString("japanese"));
}

/////////////////////////////////////////////////////////////////////

bool SteamApi::IsValidSteamId(const QString &steamId) const
{
    if (steamId.isEmpty())
    {
        return false;
    }

    // Ensure the string contains only numeric characters
    for (const QChar &character : steamId)
    {
        if (!character.isDigit())
        {
            return false;
        }
    }

    // Check for explicit success=false and exact error message indicating restricted access
    bool ok = false;
    const qulonglong steamIdValue = steamId.toULongLong(&ok);
    return ok && steamIdValue > 0;
}

/////////////////////////////////////////////////////////////////////

bool SteamApi::IsPrivateProfileResponse(const QByteArray &jsonResponse) const
{
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(jsonResponse, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject())
    {
        return false;
    }

    const QJsonObject playerStats = doc.object()["playerstats"].toObject();
    bool responseOk = playerStats.contains("success") && !playerStats["success"].toBool(true) && playerStats["error"].toString().compare("Profile is not public", Qt::CaseInsensitive) == 0;

    return responseOk;
}

/////////////////////////////////////////////////////////////////////

bool SteamApi::IncludesExpectedExtension(const QString &value) const
{
    bool includesExpectedExtension = false;

    // Accept known image file extensions returned by Steam APIs
    const QString lowerValue = value.toLower();
    const QStringList extensions = {".jpg", ".jpeg", ".png", ".webp", ".gif"};
    for (const QString &extension : extensions)
    {
        if (lowerValue.endsWith(extension))
        {
            includesExpectedExtension = true;
        }
    }

    return includesExpectedExtension;
}

/////////////////////////////////////////////////////////////////////

QString SteamApi::NormalizeSteamImageFileName(const QString &value) const
{
    QString normalizedFileName = "";

    // Trim suffix and reject already-expanded URLs
    QString fileName = value.trimmed();
    if (fileName.isEmpty() || fileName.startsWith("http", Qt::CaseInsensitive))
    {
        return normalizedFileName;
    }

    // Keep only filename, dropping path and query parts
    if (fileName.contains('/'))
    {
        fileName = fileName.section('/', -1);
    }
    if (fileName.contains('?'))
    {
        fileName = fileName.section('?', 0, 0);
    }
    if (fileName.isEmpty())
    {
        return normalizedFileName;
    }

    // Default Steam community icon filenames to jpg when extension is omitted
    if (!IncludesExpectedExtension(fileName))
    {
        fileName.append(".jpg");
    }

    normalizedFileName = fileName;
    return normalizedFileName;
}

/////////////////////////////////////////////////////////////////////

QList<SteamSearchResult> SteamApi::ParseSearchResponse(const QByteArray &jsonResponse)
{
    QList<SteamSearchResult> results = {};

    // Parse store search JSON root
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(jsonResponse, &parseError);
    if (parseError.error != QJsonParseError::NoError)
    {
        return results;
    }

    if (!doc.isObject())
    {
        return results;
    }

    // Extract app-only search results
    const QJsonArray items = doc.object()["items"].toArray();
    for (const QJsonValue &item : items)
    {
        if (!item.isObject())
        {
            continue;
        }

        const QJsonObject entry = item.toObject();
        const QString type = entry["type"].toString();
        if (type != "app")
        {
            continue;
        }

        SteamSearchResult result = {};
        result.gameName = entry["name"].toString();
        result.appId = entry["id"].toInt();
        results.append(result);
    }

    return results;
}

/////////////////////////////////////////////////////////////////////

SteamGameInfo SteamApi::ParseGameInfoResponse(const QByteArray &jsonResponse, int appId, QString *errorMessage)
{
    SteamGameInfo gameInfo = {};
    gameInfo.appId = appId;

    // Parse GetItems JSON root
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(jsonResponse, &parseError);
    if (parseError.error != QJsonParseError::NoError)
    {
        *errorMessage = "SteamApi::ParseGameInfoResponse: Failed to parse primary game info response";
        qCritical() << *errorMessage;
        return gameInfo;
    }

    if (!doc.isObject())
    {
        *errorMessage = "SteamApi::ParseGameInfoResponse: Primary game info response is not an object";
        qCritical() << *errorMessage;
        return gameInfo;
    }

    // Validate Steam returned one successful store item for requested app
    const QJsonArray storeItems = doc.object()["response"].toObject()["store_items"].toArray();
    if (storeItems.isEmpty() || !storeItems.first().isObject())
    {
        *errorMessage = "SteamApi::ParseGameInfoResponse: Primary game info response has no store item";
        qCritical() << *errorMessage;
        return gameInfo;
    }

    const QJsonObject storeItem = storeItems.first().toObject();
    if (storeItem["success"].toInt() != 1)
    {
        *errorMessage = "SteamApi::ParseGameInfoResponse: Primary game info store item failed";
        qCritical() << *errorMessage;
        return gameInfo;
    }

    const int responseAppId = storeItem["appid"].toInt(storeItem["id"].toInt());
    if (responseAppId <= 0)
    {
        *errorMessage = "SteamApi::ParseGameInfoResponse: Primary game info missing app id";
        qCritical() << *errorMessage;
        return gameInfo;
    }
    if (responseAppId != appId)
    {
        *errorMessage = "SteamApi::ParseGameInfoResponse: Primary game info app id mismatch";
        qCritical() << *errorMessage;
        return gameInfo;
    }
    gameInfo.appId = responseAppId;
    gameInfo.type = static_cast<SteamAppType>(storeItem["type"].toInt(-1));

    // Extract required display name
    gameInfo.gameName = storeItem["name"].toString();
    if (gameInfo.gameName.isEmpty())
    {
        *errorMessage = "SteamApi::ParseGameInfoResponse: Primary game info missing game name";
        qCritical() << *errorMessage;
        return gameInfo;
    }

    // Extract required asset suffixes used by later CDN URL builders
    const QJsonObject assets = storeItem["assets"].toObject();
    gameInfo.lcSuffix = assets["library_capsule"].toString();
    if (gameInfo.lcSuffix.isEmpty())
    {
        gameInfo.lcSuffix = assets["library_capsule_2x"].toString();
    }
    if (gameInfo.lcSuffix.isEmpty())
    {
        gameInfo.lcSuffix = assets["hero_capsule"].toString();
    }
    if (gameInfo.lcSuffix.isEmpty())
    {
        *errorMessage = "SteamApi::ParseGameInfoResponse: Primary game info missing library capsule";
        qCritical() << *errorMessage;
        return gameInfo;
    }

    gameInfo.ciSuffix = assets["community_icon"].toString();
    if (gameInfo.ciSuffix.isEmpty())
    {
        *errorMessage = "SteamApi::ParseGameInfoResponse: Primary game info missing community icon";
        qCritical() << *errorMessage;
        return gameInfo;
    }

    gameInfo.assetUrlFormat = assets["asset_url_format"].toString();
    if (gameInfo.assetUrlFormat.isEmpty())
    {
        gameInfo.assetUrlFormat = assets["asset_url_format"].toString();
    }
    if (gameInfo.assetUrlFormat.isEmpty())
    {
        *errorMessage = "SteamApi::ParseGameInfoResponse: Primary game info missing asset url format";
        qCritical() << *errorMessage;
        return gameInfo;
    }

    return gameInfo;
}

/////////////////////////////////////////////////////////////////////

Error SteamApi::ParseOwnedGamesResponse(const QByteArray &jsonResponse, QList<SteamOwnedGameData> &games, QString *errorMessage) const
{
    games.clear();

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(jsonResponse, &parseError);
    if (parseError.error != QJsonParseError::NoError)
    {
        *errorMessage = "ParseOwnedGamesResponse: failed to parse owned games response";
        return Error::ParseError;
    }

    if (!doc.isObject())
    {
        *errorMessage = "ParseOwnedGamesResponse: owned games response is not an object";
        return Error::ParseError;
    }

    const QJsonValue responseValue = doc.object()["response"];
    if (!responseValue.isObject())
    {
        *errorMessage = "ParseOwnedGamesResponse: missing response object";
        return Error::ParseError;
    }

    const QJsonValue gamesValue = responseValue.toObject()["games"];
    if (!gamesValue.isArray())
    {
        *errorMessage = "ParseOwnedGamesResponse: missing games array";
        return Error::ParseError;
    }

    const QJsonArray gameItems = gamesValue.toArray();
    if (gameItems.isEmpty())
    {
        return Error::NoData;
    }

    for (const QJsonValue &gameItem : gameItems)
    {
        if (!gameItem.isObject())
        {
            continue;
        }

        const QJsonObject gameObject = gameItem.toObject();
        const int appId = gameObject["appid"].toInt(0);
        const QString gameName = gameObject["name"].toString();
        if (appId <= 0 || gameName.isEmpty())
        {
            continue;
        }

        SteamOwnedGameData game = {};
        game.appId = appId;
        game.gameName = gameName;
        game.totalSecondsPlayed = static_cast<qint64>(gameObject["playtime_forever"].toInt(0)) * 60; // Convert minute-based playtime to seconds
        game.lastPlayedDate = static_cast<qint64>(gameObject["rtime_last_played"].toDouble(0));

        games.append(game);
    }

    Error success = games.isEmpty() ? Error::NoData : Error::NoError;

    return success;
}

/////////////////////////////////////////////////////////////////////

Error SteamApi::ParsePlayerAchievementsResponse(const QByteArray &jsonResponse, int appId, QList<SteamPlayerAchievementData> &achievements, QString *errorMessage) const
{
    achievements.clear();

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(jsonResponse, &parseError);
    if (parseError.error != QJsonParseError::NoError)
    {
        *errorMessage = "ParsePlayerAchievementsResponse: failed to parse player achievements response";
        return Error::ParseError;
    }

    if (!doc.isObject())
    {
        *errorMessage = "ParsePlayerAchievementsResponse: player achievements response is not an object";
        return Error::ParseError;
    }

    const QJsonValue playerStatsValue = doc.object()["playerstats"];
    if (!playerStatsValue.isObject())
    {
        *errorMessage = "ParsePlayerAchievementsResponse: missing playerstats object";
        return Error::ParseError;
    }

    const QJsonObject playerStats = playerStatsValue.toObject();
    if (playerStats.contains("success") && !playerStats["success"].toBool(true))
    {
        if (playerStats["error"].toString().compare("Profile is not public", Qt::CaseInsensitive) == 0)
        {
            return Error::ProfileNotPublic;
        }

        return Error::NotFound;
    }

    const QJsonValue achievementsValue = playerStats["achievements"];
    if (!achievementsValue.isArray())
    {
        return Error::NoData;
    }

    const QJsonArray achievementItems = achievementsValue.toArray();
    if (achievementItems.isEmpty())
    {
        return Error::NoData;
    }

    for (const QJsonValue &achievementItem : achievementItems)
    {
        if (!achievementItem.isObject())
        {
            continue;
        }

        const QJsonObject achievementObject = achievementItem.toObject();
        const QString achievementKey = achievementObject["apiname"].toString();
        if (achievementKey.isEmpty())
        {
            continue;
        }

        SteamPlayerAchievementData achievement = {};
        achievement.appId = appId;
        achievement.achievementKey = achievementKey;
        achievement.achievementName = achievementObject["name"].toString();
        achievement.achievementDescription = achievementObject["description"].toString();
        achievement.dateUnlocked = achievementObject["achieved"].toInt(0) != 0 ? static_cast<qint64>(achievementObject["unlocktime"].toDouble(0)) : 0;

        achievements.append(achievement);
    }

    return achievements.isEmpty() ? Error::NoData : Error::NoError;
}

/////////////////////////////////////////////////////////////////////

QMap<QString, double> SteamApi::ParseGlobalAchievementPercentagesResponse(const QByteArray &jsonResponse, QString *errorMessage)
{
    QMap<QString, double> percentages = {};

    // Parse global achievement percentage JSON root
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(jsonResponse, &parseError);
    if (parseError.error != QJsonParseError::NoError)
    {
        *errorMessage = "SteamApi::ParseGlobalAchievementPercentagesResponse: Failed to parse global achievement percentages response";
        qCritical() << *errorMessage;
        return percentages;
    }

    if (!doc.isObject())
    {
        *errorMessage = "SteamApi::ParseGlobalAchievementPercentagesResponse: Global achievement percentages response is not an object";
        qCritical() << *errorMessage;
        return percentages;
    }

    // Extract achievements array from Steam percentage response
    const QJsonValue achievementsValue = doc.object()["achievementpercentages"].toObject()["achievements"];
    if (!achievementsValue.isArray())
    {
        *errorMessage = "SteamApi::ParseGlobalAchievementPercentagesResponse: Global achievement percentages response missing achievements array";
        qCritical() << *errorMessage;
        return percentages;
    }

    // Store percentage by achievement API name
    const QJsonArray achievementItems = achievementsValue.toArray();
    for (const QJsonValue &achievementItem : achievementItems)
    {
        if (!achievementItem.isObject())
        {
            continue;
        }

        const QJsonObject achievementObject = achievementItem.toObject();
        const QString achievementKey = achievementObject["name"].toString();
        const QJsonValue percentageValue = achievementObject["percent"];

        bool percentageOk = false;
        double percentage = 0.0;
        if (percentageValue.isString())
        {
            percentage = percentageValue.toString().toDouble(&percentageOk);
        }
        else if (percentageValue.isDouble())
        {
            percentage = percentageValue.toDouble();
            percentageOk = true;
        }

        if (!achievementKey.isEmpty() && percentageOk)
        {
            percentages[achievementKey] = percentage;
        }
    }

    return percentages;
}

/////////////////////////////////////////////////////////////////////

QMap<QString, QString> SteamApi::ParseSteamHuntersAchievementDescriptionsResponse(const QByteArray &jsonResponse, QString *errorMessage)
{
    QMap<QString, QString> descriptions = {};

    // Parse Steam Hunters array response
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(jsonResponse, &parseError);
    if (parseError.error != QJsonParseError::NoError)
    {
        *errorMessage = "SteamApi::ParseSteamHuntersAchievementDescriptionsResponse: Failed to parse Steam Hunters achievement descriptions response";
        qCritical() << *errorMessage;
        return descriptions;
    }

    if (!doc.isArray())
    {
        *errorMessage = "SteamApi::ParseSteamHuntersAchievementDescriptionsResponse: Steam Hunters achievement descriptions response is not an array";
        qCritical() << *errorMessage;
        return descriptions;
    }

    // Store non-empty descriptions by achievement API name
    const QJsonArray achievementItems = doc.array();
    for (const QJsonValue &achievementItem : achievementItems)
    {
        if (!achievementItem.isObject())
        {
            continue;
        }

        const QJsonObject achievementObject = achievementItem.toObject();
        const QString achievementKey = achievementObject["apiName"].toString();
        const QString description = achievementObject["description"].toString();
        if (!achievementKey.isEmpty() && !description.isEmpty())
        {
            descriptions[achievementKey] = description;
        }
    }

    return descriptions;
}

/////////////////////////////////////////////////////////////////////

QByteArray SteamApi::BuildPrimaryAchievementDataResponseFromSecondary(const QByteArray &schemaResponse, const QByteArray &percentagesResponse, const QByteArray &descriptionsResponse, QString *errorMessage)
{
    QByteArray primaryCompatibleResponse = {};

    // Parse secondary schema JSON root
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(schemaResponse, &parseError);
    if (parseError.error != QJsonParseError::NoError)
    {
        *errorMessage = "SteamApi::BuildPrimaryAchievementDataResponseFromSecondary: Failed to parse secondary achievement schema response";
        qCritical() << *errorMessage;
        return primaryCompatibleResponse;
    }

    if (!doc.isObject())
    {
        *errorMessage = "SteamApi::BuildPrimaryAchievementDataResponseFromSecondary: Secondary achievement schema response is not an object";
        qCritical() << *errorMessage;
        return primaryCompatibleResponse;
    }

    // Locate schema achievement array
    const QJsonValue achievementsValue = doc.object()["game"].toObject()["availableGameStats"].toObject()["achievements"];
    if (!achievementsValue.isArray())
    {
        *errorMessage = "SteamApi::BuildPrimaryAchievementDataResponseFromSecondary: Secondary achievement schema response missing achievements array";
        qCritical() << *errorMessage;
        return primaryCompatibleResponse;
    }

    // Parse secondary companion sources before merging
    const QMap<QString, double> percentages = ParseGlobalAchievementPercentagesResponse(percentagesResponse, errorMessage);
    if (!errorMessage->isEmpty())
    {
        return primaryCompatibleResponse;
    }

    const QMap<QString, QString> descriptions = ParseSteamHuntersAchievementDescriptionsResponse(descriptionsResponse, errorMessage);
    if (!errorMessage->isEmpty())
    {
        return primaryCompatibleResponse;
    }

    // Convert secondary schema items into primary response achievement objects
    QJsonArray achievements;
    const QJsonArray secondaryAchievements = achievementsValue.toArray();
    for (const QJsonValue &achievementItem : secondaryAchievements)
    {
        if (!achievementItem.isObject())
        {
            continue;
        }

        const QJsonObject secondaryAchievement = achievementItem.toObject();
        const QString achievementKey = secondaryAchievement["name"].toString();

        QJsonObject achievement = {};
        achievement["internal_name"] = achievementKey;
        achievement["localized_name"] = secondaryAchievement["displayName"].toString();
        QString description = secondaryAchievement["description"].toString();
        if (description.isEmpty() && descriptions.contains(achievementKey))
        {
            description = descriptions.value(achievementKey);
        }
        achievement["localized_desc"] = description;
        achievement["icon"] = secondaryAchievement["icon"].toString();
        achievement["icon_gray"] = secondaryAchievement["icongray"].toString();

        // Normalize hidden flag when schema returns int instead of bool
        if (secondaryAchievement.contains("hidden"))
        {
            const QJsonValue hiddenValue = secondaryAchievement["hidden"];
            if (hiddenValue.isBool())
            {
                achievement["hidden"] = hiddenValue.toBool();
            }
            else
            {
                achievement["hidden"] = hiddenValue.toInt() != 0;
            }
        }

        // Attach global unlock percentage when available
        if (percentages.contains(achievementKey))
        {
            achievement["player_percent_unlocked"] = percentages.value(achievementKey);
        }

        achievements.append(achievement);
    }

    // Wrap achievements in same root shape as primary endpoint
    QJsonObject response;
    response["achievements"] = achievements;

    QJsonObject root;
    root["response"] = response;

    primaryCompatibleResponse = QJsonDocument(root).toJson(QJsonDocument::Compact);
    return primaryCompatibleResponse;
}

/////////////////////////////////////////////////////////////////////

QList<SteamAchievementData> SteamApi::ParseAchievementDataResponse(const QByteArray &jsonResponse, int appId, QString *errorMessage)
{
    QList<SteamAchievementData> achievements = {};

    // Parse primary-compatible achievement JSON root
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(jsonResponse, &parseError);
    if (parseError.error != QJsonParseError::NoError)
    {
        *errorMessage = "SteamApi::ParseAchievementDataResponse: Failed to parse primary achievement data response";
        qCritical() << *errorMessage;
        return achievements;
    }

    if (!doc.isObject())
    {
        *errorMessage = "SteamApi::ParseAchievementDataResponse: Primary achievement data response is not an object";
        qCritical() << *errorMessage;
        return achievements;
    }

    // Locate response.achievements array
    const QJsonValue responseValue = doc.object()["response"];
    if (!responseValue.isObject())
    {
        *errorMessage = "SteamApi::ParseAchievementDataResponse: Primary achievement data response missing response object";
        qCritical() << *errorMessage;
        return achievements;
    }

    const QJsonValue achievementsValue = responseValue.toObject()["achievements"];
    if (!achievementsValue.isArray())
    {
        *errorMessage = "SteamApi::ParseAchievementDataResponse: Primary achievement data response missing achievements array";
        qCritical() << *errorMessage;
        return achievements;
    }

    // Convert complete achievement entries into internal model
    const QJsonArray achievementItems = achievementsValue.toArray();
    for (const QJsonValue &achievementItem : achievementItems)
    {
        if (!achievementItem.isObject())
        {
            continue;
        }

        const QJsonObject achievementObject = achievementItem.toObject();
        const QString achievementKey = achievementObject["internal_name"].toString();
        const QString name = achievementObject["localized_name"].toString();
        const QString achievementDescription = achievementObject["localized_desc"].toString();
        const QString icon = achievementObject["icon"].toString();
        const QString iconGray = achievementObject["icon_gray"].toString();
        // Steam uses *_int for newer progress entries. Keep legacy names for older games.
        const int minProgress = achievementObject.contains("min_progress_int")
            ? achievementObject["min_progress_int"].toInt(0)
            : achievementObject["min_progress"].toInt(0);
        const int maxProgress = achievementObject.contains("max_progress_int")
            ? achievementObject["max_progress_int"].toInt(0)
            : achievementObject["max_progress"].toInt(0);

        // Normalize percentage from string or numeric JSON value
        bool percentageOk = false;
        const QJsonValue percentageValue = achievementObject["player_percent_unlocked"];
        double globalUnlockPercentage = 0.0;
        if (percentageValue.isString())
        {
            globalUnlockPercentage = percentageValue.toString().toDouble(&percentageOk);
        }
        else if (percentageValue.isDouble())
        {
            globalUnlockPercentage = percentageValue.toDouble();
            percentageOk = true;
        }

        if (percentageOk)
        {
            globalUnlockPercentage = std::trunc(globalUnlockPercentage * 10.0) / 10.0;
        }
        
        // Do not accept incomplete achievement data
        if (achievementKey.isEmpty() ||
            name.isEmpty() ||
            // achievementDescription.isEmpty() ||
            icon.isEmpty() ||
            iconGray.isEmpty() ||
            !achievementObject.contains("hidden") ||
            !achievementObject["hidden"].isBool() ||
            !IncludesExpectedExtension(icon) ||
            !IncludesExpectedExtension(iconGray) ||
            !percentageOk)
        {
            continue;
        }

        SteamAchievementData achievement = {};
        achievement.appId = appId;
        achievement.achievementKey = achievementKey;
        achievement.achievementName = name;
        achievement.achievementDescription = achievementDescription;
        achievement.achievementHidden = achievementObject["hidden"].toBool();
        achievement.globalUnlockPercentage = globalUnlockPercentage;
        achievement.minProgress = minProgress;
        achievement.maxProgress = maxProgress;
        achievement.iconSuffix = icon;
        achievement.iconGraySuffix = iconGray;

        achievements.append(achievement);
    }

    return achievements;
}
