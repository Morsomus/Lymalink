/////////////////////////////////////////////////////////
// File: SteamApi.h
// Date: 2026-05-15
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Declares Steam API for assets
/////////////////////////////////////////////////////////

#pragma once

#include "../Error.h"

#include <QByteArray>
#include <QList>
#include <QMap>
#include <QNetworkAccessManager>
#include <QObject>
#include <QPair>
#include <QString>
#include <QStringList>

enum class SteamAppType {
    Unknown = -1,
    Game = 0,
    Demo = 1,
    DLC = 4,
    Music = 11
};

struct SteamSearchResult
{
    QString gameName;
    int appId = 0;
};

struct SteamGameInfo
{
    int appId = 0;
    SteamAppType type = SteamAppType::Unknown;
    QString gameName;
    QString lcSuffix;
    QString ciSuffix;
    QString assetUrlFormat;
};

struct SteamAchievementData
{
    int appId = 0;
    QString achievementKey;
    QString achievementName;
    QString achievementDescription;
    bool achievementHidden = false;
    double globalUnlockPercentage = 0.0;
    int minProgress = 0;
    int maxProgress = 0;
    QString iconSuffix;         // Suffix or Full URL - depends on method FetchAchievementDataPrimary/Secondary
    QString iconGraySuffix;     // Suffix or Full URL - depends on method FetchAchievementDataPrimary/Secondary
};

struct SteamAchievementIconUrls
{
    QString achievementKey;
    QList<QString> iconUrls;
    QList<QString> iconGrayUrls;
};

struct SteamOwnedGameData
{
    int appId = 0;
    QString gameName;
    qint64 totalSecondsPlayed = 0;
    qint64 lastPlayedDate = 0;
};

struct SteamPlayerAchievementData
{
    int appId = 0;
    QString achievementKey;
    QString achievementName;
    QString achievementDescription;
    qint64 dateUnlocked = 0;
};

class SteamApi : public QObject
{
    Q_OBJECT

public:
    enum Locale
    {
        English,
        Finnish,
        German,
        Russian,
        French,
        Spanish,
        SimplifiedChinese,
        Japanese
    };
    Q_ENUM(Locale)

    explicit SteamApi(QObject *parent = nullptr);
    ~SteamApi();

    Error SearchAppId(const QString &term, QList<SteamSearchResult> &results, Locale locale = English);
    Error SearchGameInfo(int appId, SteamGameInfo &gameInfo, Locale locale = English);
    Error GetLibraryCapsuleUrls(int appId, const QString &lcSuffix, const QString &assetUrlFormat, QList<QString> &urls);
    Error GetCommunityIconUrls(int appId, const QString &ciSuffix, QList<QString> &urls);
    Error GetAchievementIconUrls(int appId, const QList<SteamAchievementData> &achievements, QList<SteamAchievementIconUrls> &urls, const QStringList &benchmarkedUrlFormats = QStringList());
    Error BenchmarkAchievementIconCdn(QStringList &benchmarkedUrlFormats);
    Error FetchAchievementDataPrimary(int appId, QList<SteamAchievementData> &achievements, Locale locale = English);
    Error FetchAchievementDataSecondary(int appId, QList<SteamAchievementData> &achievements, Locale locale, const QString &apiKey);
    Error FetchOwnedGames(const QString &steamId, QList<SteamOwnedGameData> &games, const QString &apiKey);
    Error FetchPlayerAchievements(int appId, const QString &steamId, QList<SteamPlayerAchievementData> &achievements, const QString &apiKey, Locale locale = English);

private:
    QNetworkAccessManager *m_networkManager;
    QMap<Locale, QPair<QString, QString>> m_localeMap;

    void InitializeLocaleMap();
    bool IsValidSteamId(const QString &steamId) const;
    bool IsPrivateProfileResponse(const QByteArray &jsonResponse) const;
    bool IncludesExpectedExtension(const QString &value) const;
    QString NormalizeSteamImageFileName(const QString &value) const;
    QStringList AchievementIconUrlFormats() const;
    Error DownloadRawImageUrl(const QString &url, int transferTimeoutMs, QByteArray &data);
    QList<SteamSearchResult> ParseSearchResponse(const QByteArray &jsonResponse);
    SteamGameInfo ParseGameInfoResponse(const QByteArray &jsonResponse, int appId, QString *errorMessage);
    Error ParseOwnedGamesResponse(const QByteArray &jsonResponse, QList<SteamOwnedGameData> &games, QString *errorMessage) const;
    Error ParsePlayerAchievementsResponse(const QByteArray &jsonResponse, int appId, QList<SteamPlayerAchievementData> &achievements, QString *errorMessage) const;
    QMap<QString, double> ParseGlobalAchievementPercentagesResponse(const QByteArray &jsonResponse, QString *errorMessage);
    QMap<QString, QString> ParseSteamHuntersAchievementDescriptionsResponse(const QByteArray &jsonResponse, QString *errorMessage);
    QByteArray BuildPrimaryAchievementDataResponseFromSecondary(const QByteArray &schemaResponse, const QByteArray &percentagesResponse, const QByteArray &descriptionsResponse, QString *errorMessage);
    QList<SteamAchievementData> ParseAchievementDataResponse(const QByteArray &jsonResponse, int appId, QString *errorMessage);
};
