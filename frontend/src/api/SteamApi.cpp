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
    results.clear();

    if (term.isEmpty())
    {
        return Error::InvalidParameter;
    }

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

    QNetworkReply *reply = m_networkManager->get(request);

    QEventLoop loop;
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec(QEventLoop::ExcludeUserInputEvents);
    if (reply->error() != QNetworkReply::NoError)
    {
        qDebug() << "SearchAppId - app id search failed:" << reply->errorString();
        reply->deleteLater();
        return Error::NotFound;
    }

    const QByteArray data = reply->readAll();
    reply->deleteLater();

    results = ParseSearchResponse(data);
    return Error::NoError;
}

/////////////////////////////////////////////////////////////////////


Error SteamApi::SearchGameInfo(int appId, SteamGameInfo &gameInfo, Locale locale)
{
    gameInfo = SteamGameInfo();

    if (appId <= 0)
    {
        return Error::InvalidParameter;
    }

    const QPair<QString, QString> localeSettings = m_localeMap.value(locale, m_localeMap.value(English));

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

    QNetworkReply *reply = m_networkManager->get(request);

    QEventLoop loop;
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec(QEventLoop::ExcludeUserInputEvents);
    if (reply->error() != QNetworkReply::NoError)
    {
        qDebug() << "SearchGameInfo - primary game info search failed:" << reply->errorString();
        reply->deleteLater();
        return Error::NotFound;
    }

    QString errorMessage;
    const QByteArray data = reply->readAll();
    reply->deleteLater();

    gameInfo = ParseGameInfoResponse(data, appId, &errorMessage);
    if (!errorMessage.isEmpty())
    {
        qDebug() << "SearchGameInfo - primary game info parse failed:" << errorMessage;
        gameInfo = SteamGameInfo();
        return Error::ParseError;
    }

    return Error::NoError;
}

/////////////////////////////////////////////////////////////////////

Error SteamApi::GetLibraryCapsuleUrls(int appId, const QString &lcSuffix, const QString &assetUrlFormat, QList<QString> &urls)
{
    urls.clear();

    if (appId <= 0)
    {
        qDebug() << "GetLibraryCapsuleUrls - invalid app id:" << appId;
        return Error::InvalidParameter;
    }

    QString normalizedFileName = lcSuffix.trimmed();
    if (normalizedFileName.isEmpty())
    {
        qDebug() << "GetLibraryCapsuleUrls - empty lcSuffix";
        return Error::InvalidParameter;
    }

    if (normalizedFileName.startsWith("http", Qt::CaseInsensitive))
    {
        qDebug() << "GetLibraryCapsuleUrls - lcSuffix already contains URL:" << normalizedFileName;
        return Error::InvalidParameter;
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
        return Error::InvalidParameter;
    }

    QString normalizedAssetUrlFormat = assetUrlFormat.trimmed();
    if (normalizedAssetUrlFormat.isEmpty())
    {
        qDebug() << "GetLibraryCapsuleUrls - empty assetUrlFormat";
        return Error::InvalidParameter;
    }

    if (normalizedAssetUrlFormat.startsWith("http", Qt::CaseInsensitive))
    {
        qDebug() << "GetLibraryCapsuleUrls - assetUrlFormat contains URL:" << normalizedAssetUrlFormat;
        return Error::InvalidParameter;
    }

    while (normalizedAssetUrlFormat.startsWith('/'))
    {
        normalizedAssetUrlFormat.remove(0, 1);
    }
    if (!normalizedAssetUrlFormat.contains("${FILENAME}"))
    {
        qDebug() << "GetLibraryCapsuleUrls - assetUrlFormat missing filename placeholder:" << assetUrlFormat;
        return Error::InvalidParameter;
    }

    const QString expectedAssetPrefix = QString("steam/apps/%1/").arg(appId);
    if (!normalizedAssetUrlFormat.startsWith(expectedAssetPrefix))
    {
        qDebug() << "GetLibraryCapsuleUrls - assetUrlFormat app id mismatch:" << assetUrlFormat;
        return Error::InvalidParameter;
    }

    const QString assetPath = normalizedAssetUrlFormat.replace("${FILENAME}", normalizedFileName);
    urls.append(QString("https://shared.steamstatic.com/store_item_assets/%1").arg(assetPath));
    urls.append(QString("https://shared.cloudflare.steamstatic.com/store_item_assets/%1").arg(assetPath));
    urls.append(QString("https://shared.fastly.steamstatic.com/store_item_assets/%1").arg(assetPath));
    urls.append(QString("https://shared.akamai.steamstatic.com/store_item_assets/%1").arg(assetPath));

    return Error::NoError;
}

/////////////////////////////////////////////////////////////////////

Error SteamApi::GetCommunityIconUrls(int appId, const QString &ciSuffix, QList<QString> &urls)
{
    urls.clear();

    if (appId <= 0)
    {
        qDebug() << "GetCommunityIconUrls - invalid app id:" << appId;
        return Error::InvalidParameter;
    }

    const QString trimmedCiUrl = ciSuffix.trimmed();
    if (trimmedCiUrl.isEmpty())
    {
        qDebug() << "GetCommunityIconUrls - empty ciSuffix";
        return Error::InvalidParameter;
    }

    if (trimmedCiUrl.startsWith("http", Qt::CaseInsensitive))
    {
        qDebug() << "GetCommunityIconUrls - ciSuffix already contains URL:" << trimmedCiUrl;
        return Error::InvalidParameter;
    }

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
        return Error::InvalidParameter;
    }

    if (!IncludesExpectedExtension(normalizedFileName))
    {
        normalizedFileName.append(".jpg");
    }

    const QString appIdString = QString::number(appId);
    urls.append(QString("https://cdn.cloudflare.steamstatic.com/steamcommunity/public/images/apps/%1/%2").arg(appIdString, normalizedFileName));
    urls.append(QString("https://cdn.fastly.steamstatic.com/steamcommunity/public/images/apps/%1/%2").arg(appIdString, normalizedFileName));
    urls.append(QString("https://cdn.akamai.steamstatic.com/steamcommunity/public/images/apps/%1/%2").arg(appIdString, normalizedFileName));
    urls.append(QString("https://shared.cloudflare.steamstatic.com/community_assets/images/apps/%1/%2").arg(appIdString, normalizedFileName));
    urls.append(QString("https://shared.fastly.steamstatic.com/community_assets/images/apps/%1/%2").arg(appIdString, normalizedFileName));
    urls.append(QString("https://shared.akamai.steamstatic.com/community_assets/images/apps/%1/%2").arg(appIdString, normalizedFileName));
    urls.append(QString("https://steamcdn-a.akamaihd.net/steamcommunity/public/images/apps/%1/%2").arg(appIdString, normalizedFileName));

    return Error::NoError;
}

/////////////////////////////////////////////////////////////////////

Error SteamApi::GetAchievementIconUrls(int appId, const QList<SteamAchievementData> &achievements, QList<SteamAchievementIconUrls> &urls)
{
    urls.clear();

    if (appId <= 0)
    {
        qDebug() << "GetAchievementIconUrls - invalid app id:" << appId;
        return Error::InvalidParameter;
    }

    if (achievements.isEmpty())
    {
        qDebug() << "GetAchievementIconUrls - empty achievements";
        return Error::InvalidParameter;
    }

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
        if (achievement.appId != appId)
        {
            qDebug() << "GetAchievementIconUrls - achievement app id mismatch:" << achievement.appId;
            urls.clear();
            return Error::InvalidParameter;
        }
        if (achievement.achievementKey.isEmpty())
        {
            qDebug() << "GetAchievementIconUrls - empty achievement key";
            urls.clear();
            return Error::InvalidParameter;
        }

        const QString normalizedIconFileName = NormalizeSteamImageFileName(achievement.iconSuffix);
        const QString normalizedGrayIconFileName = NormalizeSteamImageFileName(achievement.iconGraySuffix);
        if (normalizedIconFileName.isEmpty() || normalizedGrayIconFileName.isEmpty())
        {
            qDebug() << "GetAchievementIconUrls - invalid achievement icon suffix:" << achievement.achievementKey;
            urls.clear();
            return Error::InvalidParameter;
        }

        SteamAchievementIconUrls achievementUrls;
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

    return Error::NoError;
}

/////////////////////////////////////////////////////////////////////

Error SteamApi::FetchAchievementDataPrimary(int appId, QList<SteamAchievementData> &achievements, Locale locale)
{
    achievements.clear();

    if (appId <= 0)
    {
        return Error::InvalidParameter;
    }

    const QPair<QString, QString> localeSettings = m_localeMap.value(locale, m_localeMap.value(English));

    QUrl url("https://api.steampowered.com/IPlayerService/GetGameAchievements/v1/");
    QUrlQuery query;
    query.addQueryItem("appid", QString::number(appId));
    query.addQueryItem("language", localeSettings.second);
    url.setQuery(query);

    QNetworkRequest request(url);
    request.setRawHeader("User-Agent", "Mozilla/5.0");
    request.setRawHeader("Accept", "application/json");

    QNetworkReply *reply = m_networkManager->get(request);

    QEventLoop loop;
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec(QEventLoop::ExcludeUserInputEvents);
    if (reply->error() != QNetworkReply::NoError)
    {
        qDebug() << "FetchAchievementDataPrimary - primary achievement data fetch failed:" << reply->errorString();
        reply->deleteLater();
        return Error::NotFound;
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
            return Error::NoData;
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
                return Error::NoData;
            }
        }
        else if (noDataDoc.isArray() && noDataDoc.array().isEmpty())
        {
            return Error::NoData;
        }
    }

    achievements = ParseAchievementDataResponse(data, appId, &errorMessage);
    if (!errorMessage.isEmpty())
    {
        achievements.clear();
        return Error::ParseError;
    }
    if (achievements.isEmpty())
    {
        return Error::NoData;
    }

    return Error::NoError;
}

/////////////////////////////////////////////////////////////////////

Error SteamApi::FetchAchievementDataSecondary(int appId, QList<SteamAchievementData> &achievements, Locale locale, const QString &apiKey)
{
    achievements.clear();

    if (appId <= 0)
    {
        return Error::InvalidParameter;
    }

    if (apiKey.isEmpty())
    {
        return Error::InvalidParameter;
    }

    const QPair<QString, QString> localeSettings = m_localeMap.value(locale, m_localeMap.value(English));

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
        return Error::NotFound;
    }

    const QByteArray schemaData = schemaReply->readAll();
    schemaReply->deleteLater();

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
        return Error::NotFound;
    }

    const QByteArray percentageData = percentageReply->readAll();
    percentageReply->deleteLater();

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
        return Error::NotFound;
    }

    const QByteArray descriptionsData = descriptionsReply->readAll();
    descriptionsReply->deleteLater();

    QString errorMessage;
    const QByteArray primaryCompatibleData = BuildPrimaryAchievementDataResponseFromSecondary(schemaData, percentageData, descriptionsData, &errorMessage);
    if (!errorMessage.isEmpty())
    {
        qDebug() << "FetchAchievementDataSecondary - Error while building primary data response from secondary:" << errorMessage;
        return Error::ParseError;
    }

    achievements = ParseAchievementDataResponse(primaryCompatibleData, appId, &errorMessage);
    if (!errorMessage.isEmpty())
    {
        achievements.clear();
        return Error::ParseError;
    }

    return Error::NoError;
}

/////////////////////////////////////////////////////////////////////
///////////////////////////// PRIVATE ///////////////////////////////
/////////////////////////////////////////////////////////////////////

void SteamApi::InitializeLocaleMap()
{
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
    const QString lowerValue = value.toLower();
    const QStringList extensions = {".jpg", ".jpeg", ".png", ".webp", ".gif"};
    for (const QString &extension : extensions)
    {
        if (lowerValue.endsWith(extension))
        {
            return true;
        }
    }

    return false;
}

/////////////////////////////////////////////////////////////////////

QString SteamApi::NormalizeSteamImageFileName(const QString &value) const
{
    QString fileName = value.trimmed();
    if (fileName.isEmpty() || fileName.startsWith("http", Qt::CaseInsensitive))
    {
        return QString();
    }

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
        return QString();
    }

    if (!IncludesExpectedExtension(fileName))
    {
        fileName.append(".jpg");
    }

    return fileName;
}

/////////////////////////////////////////////////////////////////////

QList<SteamSearchResult> SteamApi::ParseSearchResponse(const QByteArray &jsonResponse)
{
    QList<SteamSearchResult> results = {};

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

        SteamSearchResult result;
        result.gameName = entry["name"].toString();
        result.appId = entry["id"].toInt();
        results.append(result);
    }

    return results;
}

/////////////////////////////////////////////////////////////////////

SteamGameInfo SteamApi::ParseGameInfoResponse(const QByteArray &jsonResponse, int appId, QString *errorMessage)
{
    SteamGameInfo gameInfo;
    gameInfo.appId = appId;

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

    gameInfo.gameName = storeItem["name"].toString();
    if (gameInfo.gameName.isEmpty())
    {
        *errorMessage = "ParseGameInfoResponse - primary game info missing game name";
        return gameInfo;
    }

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
    QMap<QString, double> percentages;

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

    const QJsonValue achievementsValue = doc.object()["achievementpercentages"].toObject()["achievements"];
    if (!achievementsValue.isArray())
    {
        *errorMessage = "ParseGlobalAchievementPercentagesResponse - global achievement percentages response missing achievements array";
        return percentages;
    }

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
    QMap<QString, QString> descriptions;

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
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(schemaResponse, &parseError);
    if (parseError.error != QJsonParseError::NoError)
    {
        *errorMessage = "BuildPrimaryAchievementDataResponseFromSecondary - Failed to parse secondary achievement schema response";
        return QByteArray();
    }

    if (!doc.isObject())
    {
        *errorMessage = "BuildPrimaryAchievementDataResponseFromSecondary - secondary achievement schema response is not an object";
        return QByteArray();
    }

    const QJsonValue achievementsValue = doc.object()["game"].toObject()["availableGameStats"].toObject()["achievements"];
    if (!achievementsValue.isArray())
    {
        *errorMessage = "BuildPrimaryAchievementDataResponseFromSecondary - secondary achievement schema response missing achievements array";
        return QByteArray();
    }

    const QMap<QString, double> percentages = ParseGlobalAchievementPercentagesResponse(percentagesResponse, errorMessage);
    if (!errorMessage->isEmpty())
    {
        return QByteArray();
    }

    const QMap<QString, QString> descriptions = ParseSteamHuntersAchievementDescriptionsResponse(descriptionsResponse, errorMessage);
    if (!errorMessage->isEmpty())
    {
        return QByteArray();
    }

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

        QJsonObject achievement;
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

        if (percentages.contains(achievementKey))
        {
            achievement["player_percent_unlocked"] = percentages.value(achievementKey);
        }

        achievements.append(achievement);
    }

    QJsonObject response;
    response["achievements"] = achievements;

    QJsonObject root;
    root["response"] = response;

    return QJsonDocument(root).toJson(QJsonDocument::Compact);
}

/////////////////////////////////////////////////////////////////////

QList<SteamAchievementData> SteamApi::ParseAchievementDataResponse(const QByteArray &jsonResponse, int appId, QString *errorMessage)
{
    QList<SteamAchievementData> achievements = {};

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

        SteamAchievementData achievement;
        achievement.appId = appId;
        achievement.achievementKey = achievementKey;
        achievement.achievementName = name;
        achievement.achievementDescription = achievementDescription;
        achievement.achievementHidden = achievementObject["hidden"].toBool();
        achievement.globalUnlockPercentage = globalUnlockPercentage;
        achievement.iconSuffix = icon;
        achievement.iconGraySuffix = iconGray;

        achievements.append(achievement);
    }

    return achievements;
}
