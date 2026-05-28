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
        qDebug() << "SearchAppId - app id search failed:" << reply->errorString();
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
        qDebug() << "SearchGameInfo - primary game info search failed:" << reply->errorString();
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
        qDebug() << "SearchGameInfo - primary game info parse failed:" << errorMessage;
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
        qDebug() << "GetLibraryCapsuleUrls - invalid app id:" << appId;
        err = Error::InvalidParameter;
        return err;
    }

    // Normalize Steam asset filename from API suffix
    QString normalizedFileName = lcSuffix.trimmed();
    if (normalizedFileName.isEmpty())
    {
        qDebug() << "GetLibraryCapsuleUrls - empty lcSuffix";
        err = Error::InvalidParameter;
        return err;
    }

    if (normalizedFileName.startsWith("http", Qt::CaseInsensitive))
    {
        qDebug() << "GetLibraryCapsuleUrls - lcSuffix already contains URL:" << normalizedFileName;
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
        qDebug() << "GetLibraryCapsuleUrls - invalid lcSuffix filename:" << lcSuffix;
        err = Error::InvalidParameter;
        return err;
    }

    // Normalize Steam asset URL format and verify placeholder
    QString normalizedAssetUrlFormat = assetUrlFormat.trimmed();
    if (normalizedAssetUrlFormat.isEmpty())
    {
        qDebug() << "GetLibraryCapsuleUrls - empty assetUrlFormat";
        err = Error::InvalidParameter;
        return err;
    }

    if (normalizedAssetUrlFormat.startsWith("http", Qt::CaseInsensitive))
    {
        qDebug() << "GetLibraryCapsuleUrls - assetUrlFormat contains URL:" << normalizedAssetUrlFormat;
        err = Error::InvalidParameter;
        return err;
    }

    while (normalizedAssetUrlFormat.startsWith('/'))
    {
        normalizedAssetUrlFormat.remove(0, 1);
    }
    if (!normalizedAssetUrlFormat.contains("${FILENAME}"))
    {
        qDebug() << "GetLibraryCapsuleUrls - assetUrlFormat missing filename placeholder:" << assetUrlFormat;
        err = Error::InvalidParameter;
        return err;
    }

    // Reject asset formats that point at another app id
    const QString expectedAssetPrefix = QString("steam/apps/%1/").arg(appId);
    if (!normalizedAssetUrlFormat.startsWith(expectedAssetPrefix))
    {
        qDebug() << "GetLibraryCapsuleUrls - assetUrlFormat app id mismatch:" << assetUrlFormat;
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
        qDebug() << "GetCommunityIconUrls - invalid app id:" << appId;
        err = Error::InvalidParameter;
        return err;
    }

    const QString trimmedCiUrl = ciSuffix.trimmed();
    if (trimmedCiUrl.isEmpty())
    {
        qDebug() << "GetCommunityIconUrls - empty ciSuffix";
        err = Error::InvalidParameter;
        return err;
    }

    if (trimmedCiUrl.startsWith("http", Qt::CaseInsensitive))
    {
        qDebug() << "GetCommunityIconUrls - ciSuffix already contains URL:" << trimmedCiUrl;
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
        qDebug() << "GetCommunityIconUrls - invalid ciSuffix filename:" << ciSuffix;
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
        qDebug() << "GetAchievementIconUrls - invalid app id:" << appId;
        err = Error::InvalidParameter;
        return err;
    }

    if (achievements.isEmpty())
    {
        qDebug() << "GetAchievementIconUrls - empty achievements";
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
            qDebug() << "GetAchievementIconUrls - achievement app id mismatch:" << achievement.appId;
            urls.clear();
            err = Error::InvalidParameter;
            return err;
        }
        if (achievement.achievementKey.isEmpty())
        {
            qDebug() << "GetAchievementIconUrls - empty achievement key";
            urls.clear();
            err = Error::InvalidParameter;
            return err;
        }

        // Normalize active and locked achievement icon filenames
        const QString normalizedIconFileName = NormalizeSteamImageFileName(achievement.iconSuffix);
        const QString normalizedGrayIconFileName = NormalizeSteamImageFileName(achievement.iconGraySuffix);
        if (normalizedIconFileName.isEmpty() || normalizedGrayIconFileName.isEmpty())
        {
            qDebug() << "GetAchievementIconUrls - invalid achievement icon suffix:" << achievement.achievementKey;
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
        qDebug() << "FetchAchievementDataPrimary - primary achievement data fetch failed:" << reply->errorString();
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
                err = Error::NoData;
                return err;
            }
        }
        else if (noDataDoc.isArray() && noDataDoc.array().isEmpty())
        {
            err = Error::NoData;
            return err;
        }
    }

    // Parse normalized achievement data from primary response
    achievements = ParseAchievementDataResponse(data, appId, &errorMessage);
    if (!errorMessage.isEmpty())
    {
        achievements.clear();
        err = Error::ParseError;
        return err;
    }
    if (achievements.isEmpty())
    {
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
        err = Error::InvalidParameter;
        return err;
    }

    if (apiKey.isEmpty())
    {
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
        qDebug() << "FetchAchievementDataSecondary - secondary achievement schema fetch failed:" << schemaReply->errorString();
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
        qDebug() << "FetchAchievementDataSecondary - global achievement percentage fetch failed:" << percentageReply->errorString();
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
        qDebug() << "FetchAchievementDataSecondary - Steam Hunters achievement descriptions fetch failed:" << descriptionsReply->errorString();
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
        qDebug() << "FetchAchievementDataSecondary - Error while building primary data response from secondary:" << errorMessage;
        err = Error::ParseError;
        return err;
    }

    // Parse merged response using primary parser
    achievements = ParseAchievementDataResponse(primaryCompatibleData, appId, &errorMessage);
    if (!errorMessage.isEmpty())
    {
        achievements.clear();
        err = Error::ParseError;
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
        *errorMessage = "ParseGameInfoResponse - Failed to parse primary game info response";
        return gameInfo;
    }

    if (!doc.isObject())
    {
        *errorMessage = "ParseGameInfoResponse - primary game info response is not an object";
        return gameInfo;
    }

    // Validate Steam returned one successful store item for requested app
    const QJsonArray storeItems = doc.object()["response"].toObject()["store_items"].toArray();
    if (storeItems.isEmpty() || !storeItems.first().isObject())
    {
        *errorMessage = "ParseGameInfoResponse - primary game info response has no store item";
        return gameInfo;
    }

    const QJsonObject storeItem = storeItems.first().toObject();
    if (storeItem["success"].toInt() != 1)
    {
        *errorMessage = "ParseGameInfoResponse - primary game info store item failed";
        return gameInfo;
    }

    const int responseAppId = storeItem["appid"].toInt(storeItem["id"].toInt());
    if (responseAppId <= 0)
    {
        *errorMessage = "ParseGameInfoResponse - primary game info missing app id";
        return gameInfo;
    }
    if (responseAppId != appId)
    {
        *errorMessage = "ParseGameInfoResponse - primary game info app id mismatch";
        return gameInfo;
    }
    gameInfo.appId = responseAppId;
    gameInfo.type = static_cast<SteamAppType>(storeItem["type"].toInt(-1));

    // Extract required display name
    gameInfo.gameName = storeItem["name"].toString();
    if (gameInfo.gameName.isEmpty())
    {
        *errorMessage = "ParseGameInfoResponse - primary game info missing game name";
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
        *errorMessage = "ParseGameInfoResponse - primary game info missing library capsule";
        return gameInfo;
    }

    gameInfo.ciSuffix = assets["community_icon"].toString();
    if (gameInfo.ciSuffix.isEmpty())
    {
        *errorMessage = "ParseGameInfoResponse - primary game info missing community icon";
        return gameInfo;
    }

    gameInfo.assetUrlFormat = assets["asset_url_format"].toString();
    if (gameInfo.assetUrlFormat.isEmpty())
    {
        gameInfo.assetUrlFormat = assets["asset_url_format"].toString();
    }
    if (gameInfo.assetUrlFormat.isEmpty())
    {
        *errorMessage = "ParseGameInfoResponse - primary game info missing asset url format";
        return gameInfo;
    }

    return gameInfo;
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
        *errorMessage = "ParseGlobalAchievementPercentagesResponse - Failed to parse global achievement percentages response";
        return percentages;
    }

    if (!doc.isObject())
    {
        *errorMessage = "ParseGlobalAchievementPercentagesResponse - global achievement percentages response is not an object";
        return percentages;
    }

    // Extract achievements array from Steam percentage response
    const QJsonValue achievementsValue = doc.object()["achievementpercentages"].toObject()["achievements"];
    if (!achievementsValue.isArray())
    {
        *errorMessage = "ParseGlobalAchievementPercentagesResponse - global achievement percentages response missing achievements array";
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
        *errorMessage = "ParseSteamHuntersAchievementDescriptionsResponse - Failed to parse Steam Hunters achievement descriptions response";
        return descriptions;
    }

    if (!doc.isArray())
    {
        *errorMessage = "ParseSteamHuntersAchievementDescriptionsResponse - Steam Hunters achievement descriptions response is not an array";
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
        *errorMessage = "BuildPrimaryAchievementDataResponseFromSecondary - Failed to parse secondary achievement schema response";
        return primaryCompatibleResponse;
    }

    if (!doc.isObject())
    {
        *errorMessage = "BuildPrimaryAchievementDataResponseFromSecondary - secondary achievement schema response is not an object";
        return primaryCompatibleResponse;
    }

    // Locate schema achievement array
    const QJsonValue achievementsValue = doc.object()["game"].toObject()["availableGameStats"].toObject()["achievements"];
    if (!achievementsValue.isArray())
    {
        *errorMessage = "BuildPrimaryAchievementDataResponseFromSecondary - secondary achievement schema response missing achievements array";
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
        *errorMessage = "ParseAchievementDataResponse - Failed to parse primary achievement data response";
        return achievements;
    }

    if (!doc.isObject())
    {
        *errorMessage = "ParseAchievementDataResponse - primary achievement data response is not an object";
        return achievements;
    }

    // Locate response.achievements array
    const QJsonValue responseValue = doc.object()["response"];
    if (!responseValue.isObject())
    {
        *errorMessage = "ParseAchievementDataResponse - primary achievement data response missing response object";
        return achievements;
    }

    const QJsonValue achievementsValue = responseValue.toObject()["achievements"];
    if (!achievementsValue.isArray())
    {
        *errorMessage = "ParseAchievementDataResponse - primary achievement data response missing achievements array";
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
        const int minProgress = achievementObject["min_progress"].toInt(0);
        const int maxProgress = achievementObject["max_progress"].toInt(0);

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
