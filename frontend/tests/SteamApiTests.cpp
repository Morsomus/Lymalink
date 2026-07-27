/////////////////////////////////////////////////////////
// File: SteamApiTests.cpp
// Date: 2026-05-15
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Tests SteamApi
/////////////////////////////////////////////////////////

#include "../src/api/SteamApi.h"

#include <QDebug>
#include <QStringList>
#include <QtTest/QtTest>

class SteamApiTests : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    void searchAppId_witcher_returnsExpectedGames();
    void searchAppId_simplifiedChinese_returnsExpectedGames();
    void searchGameInfo_arcRaiders_returnsExpectedInfo();
    void searchGameInfo_dishonored_countryRestrictedFallback_returnsExpectedInfo();
    void getLibraryCapsuleUrls_validInputs_returnsExpectedUrls();
    void getCommunityIconUrls_validCiUrl_returnsExpectedUrls();
    void getAchievementIconUrls_validSuffixes_returnsExpectedUrls();
    void fetchAchievementDataPrimary_witcher3_returnsExpectedAchievements();
    void fetchAchievementDataSecondary_witcher3_returnsExpectedAchievements();
    void fetchOwnedGames_invalidInputs_returnsInvalidParameter();
    void fetchPlayerAchievements_invalidInputs_returnsInvalidParameter();
    void parsePlayerAchievements_privateProfile_returnsProfileNotPublic();
    void fetchOwnedGames_liveCredentials_returnsExpectedGames();
    void fetchPlayerAchievements_liveCredentials_returnsExpectedAchievementsOrNoData();

private:
    bool hasImageExtension(const QString &value);
    bool containsGame(const QList<SteamSearchResult> &results, const QString &gameName, int appId) const;
    bool containsAchievement(const QList<SteamAchievementData> &results, const QString &achievementKey) const;
    void debugPrintResults(const QString &term, const QList<SteamSearchResult> &results) const;
    void debugPrintGameInfo(const SteamGameInfo &gameInfo) const;
    void debugPrintAchievementDataPrimary(const QList<SteamAchievementData> &achievements) const;
};

/////////////////////////////////////////////////////////////////////

void SteamApiTests::initTestCase()
{
    qputenv("QT_LOGGING_RULES", "*.debug=true");
}

/////////////////////////////////////////////////////////////////////

void SteamApiTests::searchAppId_witcher_returnsExpectedGames()
{
    SteamApi steamApi;

    QList<SteamSearchResult> witcherResults;
    Error error = steamApi.SearchAppId("witcher", witcherResults);
    QVERIFY(error == Error::NoError);

    const int foundCount = witcherResults.size();

    QVERIFY(foundCount >= 3);
    QVERIFY(containsGame(witcherResults, "The Witcher 3: Wild Hunt", 292030));
    QVERIFY(containsGame(witcherResults, "The Witcher 2: Assassins of Kings Enhanced Edition", 20920));
    QVERIFY(containsGame(witcherResults, "The Witcher: Enhanced Edition Director's Cut", 20900));

    QList<SteamSearchResult> theWitcherResults;
    error = steamApi.SearchAppId("the witcher", theWitcherResults);
    QVERIFY(error == Error::NoError);

    QCOMPARE(theWitcherResults.size(), foundCount);

    debugPrintResults("witcher", witcherResults);
    debugPrintResults("the witcher", theWitcherResults);
    QThread::msleep(1250);
}

/////////////////////////////////////////////////////////////////////

void SteamApiTests::searchAppId_simplifiedChinese_returnsExpectedGames()
{
    SteamApi steamApi;

    QList<SteamSearchResult> results;
    const Error error = steamApi.SearchAppId("the witcher 3", results, SteamApi::SimplifiedChinese);
    QVERIFY(error == Error::NoError);

    QVERIFY(containsGame(results, QString::fromUtf8("巫师 3：狂猎"), 292030));
    QVERIFY(containsGame(results, QString::fromUtf8("巫师 3：狂猎 - 血与酒"), 378648));
    QVERIFY(containsGame(results, QString::fromUtf8("巫师 3：狂猎 - 石之心"), 378649));

    debugPrintResults("the witcher 3", results);
    QThread::msleep(1250);
}

/////////////////////////////////////////////////////////////////////

void SteamApiTests::searchGameInfo_arcRaiders_returnsExpectedInfo()
{
    SteamApi steamApi;

    SteamGameInfo gameInfo;
    const Error error = steamApi.SearchGameInfo(1808500, gameInfo);
    QVERIFY(error == Error::NoError);

    QCOMPARE(gameInfo.appId, 1808500);
    QCOMPARE(gameInfo.type, SteamAppType::Game);
    QCOMPARE(gameInfo.gameName, QString("ARC Raiders"));
    QVERIFY(!gameInfo.lcSuffix.isEmpty());
    QVERIFY(!gameInfo.ciSuffix.isEmpty());
    QVERIFY(!gameInfo.assetUrlFormat.isEmpty());

    debugPrintGameInfo(gameInfo);
    QThread::msleep(1250);
}

/////////////////////////////////////////////////////////////////////

void SteamApiTests::searchGameInfo_dishonored_countryRestrictedFallback_returnsExpectedInfo()
{
    SteamApi steamApi;

    SteamGameInfo gameInfo;
    const Error error = steamApi.SearchGameInfo(217980, gameInfo, SteamApi::Finnish);
    QVERIFY(error == Error::NoError);

    QCOMPARE(gameInfo.appId, 217980);
    QCOMPARE(gameInfo.type, SteamAppType::Game);
    QCOMPARE(gameInfo.gameName, QString("Dishonored"));
    QVERIFY(!gameInfo.lcSuffix.isEmpty());
    QVERIFY(!gameInfo.ciSuffix.isEmpty());
    QVERIFY(!gameInfo.assetUrlFormat.isEmpty());

    debugPrintGameInfo(gameInfo);
    QThread::msleep(1250);
}

/////////////////////////////////////////////////////////////////////

void SteamApiTests::getLibraryCapsuleUrls_validInputs_returnsExpectedUrls()
{
    SteamApi steamApi;

    QList<QString> urls;
    const Error error = steamApi.GetLibraryCapsuleUrls(
        292030,
        "fe26986a2bd1601004ef0e4e1dfadd02948e3897/library_600x900.jpg",
        "steam/apps/292030/${FILENAME}?t=1768303991",
        urls);
    QVERIFY(error == Error::NoError);

    QCOMPARE(urls.size(), 4);
    for (const QString &url : urls)
    {
        QVERIFY(url.contains("/store_item_assets/steam/apps/292030/fe26986a2bd1601004ef0e4e1dfadd02948e3897/library_600x900.jpg?t=1768303991"));
        QVERIFY(hasImageExtension(url.section('?', 0, 0)));
    }
}

/////////////////////////////////////////////////////////////////////

void SteamApiTests::getCommunityIconUrls_validCiUrl_returnsExpectedUrls()
{
    SteamApi steamApi;

    QList<QString> urls;
    const Error error = steamApi.GetCommunityIconUrls(292030, "78d0ff98b67851f24539cdf2402cf147679134f4", urls);
    QVERIFY(error == Error::NoError);

    QCOMPARE(urls.size(), 7);
    for (const QString &url : urls)
    {
        QVERIFY(url.contains("/292030/78d0ff98b67851f24539cdf2402cf147679134f4.jpg"));
        QVERIFY(hasImageExtension(url));
    }
}

/////////////////////////////////////////////////////////////////////

void SteamApiTests::getAchievementIconUrls_validSuffixes_returnsExpectedUrls()
{
    SteamApi steamApi;

    QList<SteamAchievementData> achievements;
    SteamAchievementData achievement;
    achievement.appId = 292030;
    achievement.achievementKey = "LILAC";
    achievement.iconSuffix = "6078587189483353f06f48d0eefdaaa0791e9e13.jpg";
    achievement.iconGraySuffix = "8246dc3a496e13c058572dab37099e76a6cd0b77";
    achievements.append(achievement);

    SteamAchievementData secondAchievement;
    secondAchievement.appId = 292030;
    secondAchievement.achievementKey = "FRIEND_IN_NEED";
    secondAchievement.iconSuffix = "07bae88f1ee9b856ddfc1d8e28ae7eedd4bcde95";
    secondAchievement.iconGraySuffix = "cea97617b65ab7d42f37cbb9a77c7290775c789a.jpg";
    achievements.append(secondAchievement);

    QList<SteamAchievementIconUrls> urls;
    const Error error = steamApi.GetAchievementIconUrls(292030, achievements, urls);
    QVERIFY(error == Error::NoError);
    QCOMPARE(urls.size(), 2);

    QCOMPARE(urls[0].achievementKey, QString("LILAC"));
    QCOMPARE(urls[0].iconUrls.size(), 7);
    QCOMPARE(urls[0].iconGrayUrls.size(), 7);
    for (int i = 0; i < 7; ++i)
    {
        QVERIFY(urls[0].iconUrls[i].contains("/292030/6078587189483353f06f48d0eefdaaa0791e9e13.jpg"));
        QVERIFY(urls[0].iconGrayUrls[i].contains("/292030/8246dc3a496e13c058572dab37099e76a6cd0b77.jpg"));
        QVERIFY(hasImageExtension(urls[0].iconUrls[i]));
        QVERIFY(hasImageExtension(urls[0].iconGrayUrls[i]));
    }

    QCOMPARE(urls[1].achievementKey, QString("FRIEND_IN_NEED"));
    QCOMPARE(urls[1].iconUrls.size(), 7);
    QCOMPARE(urls[1].iconGrayUrls.size(), 7);
    for (int i = 0; i < 7; ++i)
    {
        QVERIFY(urls[1].iconUrls[i].contains("/292030/07bae88f1ee9b856ddfc1d8e28ae7eedd4bcde95.jpg"));
        QVERIFY(urls[1].iconGrayUrls[i].contains("/292030/cea97617b65ab7d42f37cbb9a77c7290775c789a.jpg"));
        QVERIFY(hasImageExtension(urls[1].iconUrls[i]));
        QVERIFY(hasImageExtension(urls[1].iconGrayUrls[i]));
    }
}

/////////////////////////////////////////////////////////////////////

void SteamApiTests::fetchAchievementDataPrimary_witcher3_returnsExpectedAchievements()
{
    SteamApi steamApi;

    QList<SteamAchievementData> achievements;
    const Error error = steamApi.FetchAchievementDataPrimary(292030, achievements);
    QVERIFY(error == Error::NoError);

    QVERIFY(achievements.size() >= 78);
    QVERIFY(containsAchievement(achievements, "LILAC"));
    QVERIFY(containsAchievement(achievements, "FRIEND_IN_NEED"));
    QVERIFY(containsAchievement(achievements, "PASSED_THE_TRIAL"));

    const SteamAchievementData firstAchievement = achievements.first();
    QCOMPARE(firstAchievement.appId, 292030);
    QVERIFY(!firstAchievement.achievementKey.isEmpty());
    QVERIFY(!firstAchievement.achievementName.isEmpty());
    QVERIFY(!firstAchievement.achievementDescription.isEmpty());
    QVERIFY(firstAchievement.globalUnlockPercentage >= 0.0);
    QVERIFY(hasImageExtension(firstAchievement.iconSuffix));
    QVERIFY(hasImageExtension(firstAchievement.iconGraySuffix));

    debugPrintAchievementDataPrimary(achievements);
    QThread::msleep(1250);
}

/////////////////////////////////////////////////////////////////////

void SteamApiTests::fetchAchievementDataSecondary_witcher3_returnsExpectedAchievements()
{
    SteamApi steamApi;

    QList<SteamAchievementData> achievements;
    const Error error = steamApi.FetchAchievementDataSecondary(292030, achievements, SteamApi::English, "SET_YOUR_API_KEY_HERE");
    QVERIFY(error == Error::NoError);

    QVERIFY(achievements.size() >= 78);
    QVERIFY(containsAchievement(achievements, "LILAC"));
    QVERIFY(containsAchievement(achievements, "FRIEND_IN_NEED"));
    QVERIFY(containsAchievement(achievements, "PASSED_THE_TRIAL"));

    const SteamAchievementData firstAchievement = achievements.first();
    QCOMPARE(firstAchievement.appId, 292030);
    QVERIFY(!firstAchievement.achievementKey.isEmpty());
    QVERIFY(!firstAchievement.achievementName.isEmpty());
    QVERIFY(!firstAchievement.achievementDescription.isEmpty());
    QVERIFY(firstAchievement.globalUnlockPercentage >= 0.0);
    QVERIFY(hasImageExtension(firstAchievement.iconSuffix));
    QVERIFY(hasImageExtension(firstAchievement.iconGraySuffix));

    debugPrintAchievementDataPrimary(achievements);
    QThread::msleep(1250);
}

/////////////////////////////////////////////////////////////////////

void SteamApiTests::fetchOwnedGames_invalidInputs_returnsInvalidParameter()
{
    SteamApi steamApi;

    QList<SteamOwnedGameData> games;

    QCOMPARE(steamApi.FetchOwnedGames("", games, "test-key"), Error::InvalidParameter);
    QVERIFY(games.isEmpty());

    QCOMPARE(steamApi.FetchOwnedGames("not-numeric", games, "test-key"), Error::InvalidParameter);
    QVERIFY(games.isEmpty());

    QCOMPARE(steamApi.FetchOwnedGames("76561198122619890", games, ""), Error::InvalidParameter);
    QVERIFY(games.isEmpty());
}

/////////////////////////////////////////////////////////////////////

void SteamApiTests::fetchPlayerAchievements_invalidInputs_returnsInvalidParameter()
{
    SteamApi steamApi;

    QList<SteamPlayerAchievementData> achievements;

    QCOMPARE(steamApi.FetchPlayerAchievements(0, "76561198122619890", achievements, "test-key"), Error::InvalidParameter);
    QVERIFY(achievements.isEmpty());

    QCOMPARE(steamApi.FetchPlayerAchievements(292030, "", achievements, "test-key"), Error::InvalidParameter);
    QVERIFY(achievements.isEmpty());

    QCOMPARE(steamApi.FetchPlayerAchievements(292030, "not-numeric", achievements, "test-key"), Error::InvalidParameter);
    QVERIFY(achievements.isEmpty());

    QCOMPARE(steamApi.FetchPlayerAchievements(292030, "76561198122619890", achievements, ""), Error::InvalidParameter);
    QVERIFY(achievements.isEmpty());
}

/////////////////////////////////////////////////////////////////////

void SteamApiTests::parsePlayerAchievements_privateProfile_returnsProfileNotPublic()
{
    SteamApi steamApi;

    const QByteArray response = R"json(
{
    "playerstats": {
        "error": "Profile is not public",
        "success": false
    }
}
)json";

    QString errorMessage;
    QList<SteamPlayerAchievementData> achievements;
    const Error error = steamApi.ParsePlayerAchievementsResponse(response, 292030, achievements, &errorMessage);

    QCOMPARE(error, Error::ProfileNotPublic);
    QVERIFY(errorMessage.isEmpty());
    QVERIFY(achievements.isEmpty());
}

/////////////////////////////////////////////////////////////////////

void SteamApiTests::fetchOwnedGames_liveCredentials_returnsExpectedGames()
{
    const QString steamId = qEnvironmentVariable("LYMALINK_STEAM_ID");
    const QString apiKey = qEnvironmentVariable("LYMALINK_STEAM_WEB_API_KEY");
    if (steamId.isEmpty() || apiKey.isEmpty())
    {
        QSKIP("Set LYMALINK_STEAM_ID and LYMALINK_STEAM_WEB_API_KEY to run live Steam owned-games test.");
    }

    SteamApi steamApi;

    QList<SteamOwnedGameData> games;
    const Error error = steamApi.FetchOwnedGames(steamId, games, apiKey);
    QCOMPARE(error, Error::NoError);
    QVERIFY(!games.isEmpty());

    for (const SteamOwnedGameData &game : games)
    {
        QVERIFY(game.appId > 0);
        QVERIFY(!game.gameName.isEmpty());
        QVERIFY(game.totalSecondsPlayed >= 0);
        QVERIFY(game.lastPlayedDate >= 0);
    }
}

/////////////////////////////////////////////////////////////////////

void SteamApiTests::fetchPlayerAchievements_liveCredentials_returnsExpectedAchievementsOrNoData()
{
    const QString steamId = qEnvironmentVariable("LYMALINK_STEAM_ID");
    const QString apiKey = qEnvironmentVariable("LYMALINK_STEAM_WEB_API_KEY");
    const QString appIdText = qEnvironmentVariable("LYMALINK_STEAM_TEST_APP_ID");
    const int appId = appIdText.isEmpty() ? 292030 : appIdText.toInt();
    if (steamId.isEmpty() || apiKey.isEmpty())
    {
        QSKIP("Set LYMALINK_STEAM_ID and LYMALINK_STEAM_WEB_API_KEY to run live Steam player-achievements test.");
    }

    SteamApi steamApi;

    QList<SteamPlayerAchievementData> achievements;
    const Error error = steamApi.FetchPlayerAchievements(appId, steamId, achievements, apiKey);
    if (error == Error::NoData)
    {
        QSKIP("Configured Steam account has no player achievement data for selected app.");
    }
    if (error == Error::ProfileNotPublic)
    {
        QSKIP("Configured Steam profile is not public.");
    }

    QCOMPARE(error, Error::NoError);
    QVERIFY(!achievements.isEmpty());

    for (const SteamPlayerAchievementData &achievement : achievements)
    {
        QCOMPARE(achievement.appId, appId);
        QVERIFY(!achievement.achievementKey.isEmpty());
        QVERIFY(achievement.dateUnlocked >= 0);
    }
}

/////////////////////////////////////////////////////////////////////
///////////////////////////// PRIVATE ///////////////////////////////
/////////////////////////////////////////////////////////////////////

bool SteamApiTests::hasImageExtension(const QString &value)
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

bool SteamApiTests::containsGame(const QList<SteamSearchResult> &results, const QString &gameName, int appId) const
{
    for (const SteamSearchResult &result : results)
    {
        if (result.gameName == gameName && result.appId == appId)
        {
            return true;
        }
    }

    return false;
}

/////////////////////////////////////////////////////////////////////

bool SteamApiTests::containsAchievement(const QList<SteamAchievementData> &results, const QString &achievementKey) const
{
    for (const SteamAchievementData &result : results)
    {
        if (result.achievementKey == achievementKey)
        {
            return true;
        }
    }

    return false;
}

/////////////////////////////////////////////////////////////////////

void SteamApiTests::debugPrintResults(const QString &term, const QList<SteamSearchResult> &results) const
{
    qDebug() << "debugPrintResults - SearchAppId results for" << term << "count:" << results.size();
    for (const SteamSearchResult &result : results)
    {
        qDebug() << result.gameName << result.appId;
    }
}

/////////////////////////////////////////////////////////////////////

void SteamApiTests::debugPrintGameInfo(const SteamGameInfo &gameInfo) const
{
    qDebug() << "debugPrintGameInfo - SearchGameInfo result:";
    qDebug() << "appId:" << gameInfo.appId;
    qDebug() << "type:" << static_cast<int>(gameInfo.type);
    qDebug() << "gameName:" << gameInfo.gameName;
    qDebug() << "lcSuffix:" << gameInfo.lcSuffix;
    qDebug() << "ciSuffix:" << gameInfo.ciSuffix;
}

/////////////////////////////////////////////////////////////////////

void SteamApiTests::debugPrintAchievementDataPrimary(const QList<SteamAchievementData> &achievements) const
{
    qDebug() << "debugPrintAchievementDataPrimary - count:" << achievements.size();
    for (const SteamAchievementData &achievement : achievements)
    {
        qDebug() << achievement.appId
            << achievement.achievementKey
            << achievement.achievementName
            << achievement.achievementDescription
            << achievement.achievementHidden
            << achievement.globalUnlockPercentage
            << achievement.iconSuffix
            << achievement.iconGraySuffix;
    }
}

/////////////////////////////////////////////////////////////////////

QTEST_MAIN(SteamApiTests)
#include "SteamApiTests.moc"
