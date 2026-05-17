/////////////////////////////////////////////////////////
// File: SteamApi.h
// Date: 2026-05-15
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Declares Steam API for assets
/////////////////////////////////////////////////////////

#pragma once

#include "../Error.h"

#include <QList>
#include <QMap>
#include <QNetworkAccessManager>
#include <QObject>
#include <QPair>
#include <QString>

struct SteamSearchResult
{
    QString gameName;
    int appId = 0;
};

struct SteamGameInfo
{
    int appId = 0;
    QString gameName;
    QString lcSuffix;
    QString ciSuffix;
    QString assetUrlFormat;
};

struct SteamAchievementData
{
    int appId = 0;
    QString achievementKey;
    QString name;
    QString description;
    bool hidden = false;
    double globalPercentage = 0.0;
    QString iconSuffix;         // Suffix or Full URL - depends on method FetchAchievementDataPrimary/Secondary
    QString iconGraySuffix;     // Suffix or Full URL - depends on method FetchAchievementDataPrimary/Secondary
};

struct SteamAchievementIconUrls
{
    QString achievementKey;
    QList<QString> iconUrls;
    QList<QString> iconGrayUrls;
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
    Error GetAchievementIconUrls(int appId, const QList<SteamAchievementData> &achievements, QList<SteamAchievementIconUrls> &urls);
    Error FetchAchievementDataPrimary(int appId, QList<SteamAchievementData> &achievements, Locale locale = English);
    Error FetchAchievementDataSecondary(int appId, QList<SteamAchievementData> &achievements, Locale locale, const QString &apiKey);

private:
    QNetworkAccessManager *m_networkManager;
    QMap<Locale, QPair<QString, QString>> m_localeMap;

    void InitializeLocaleMap();
    bool IncludesExpectedExtension(const QString &value) const;
    QString NormalizeSteamImageFileName(const QString &value) const;
    QList<SteamSearchResult> ParseSearchResponse(const QByteArray &jsonResponse);
    SteamGameInfo ParseGameInfoResponse(const QByteArray &jsonResponse, int appId, QString *errorMessage);
    QMap<QString, double> ParseGlobalAchievementPercentagesResponse(const QByteArray &jsonResponse, QString *errorMessage);
    QMap<QString, QString> ParseSteamHuntersAchievementDescriptionsResponse(const QByteArray &jsonResponse, QString *errorMessage);
    QByteArray BuildPrimaryAchievementDataResponseFromSecondary(const QByteArray &schemaResponse, const QByteArray &percentagesResponse, const QByteArray &descriptionsResponse, QString *errorMessage);
    QList<SteamAchievementData> ParseAchievementDataResponse(const QByteArray &jsonResponse, int appId, QString *errorMessage);
};
