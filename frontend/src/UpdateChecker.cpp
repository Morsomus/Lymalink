/////////////////////////////////////////////////////////
// File: UpdateChecker.cpp
// Date: 2026-09-28
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Implements checks for Lymalink release updates
/////////////////////////////////////////////////////////

#include "UpdateChecker.h"
#include "Settings.h"

#include <QDateTime>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QVersionNumber>

/////////////////////////////////////////////////////////////////////

UpdateChecker::UpdateChecker(Settings *settings, QObject *parent) : QObject(parent),
    m_settings(settings)
{
    m_checkStarted = false;

    if (m_settings != nullptr)
    {
        connect(m_settings, &Settings::signalConfigChanged, this, &UpdateChecker::signalUpdateAvailabilityChanged);
    }
}

UpdateChecker::~UpdateChecker()
{
    // Destructor
}

/////////////////////////////////////////////////////////////////////
////////////////////////////// PUBLIC ///////////////////////////////
/////////////////////////////////////////////////////////////////////

void UpdateChecker::CheckForUpdate()
{
    if (m_checkStarted || m_settings == nullptr)
    {
        return;
    }
    m_checkStarted = true;

    qDebug() << "UpdateChecker::CheckForUpdate - Check for an update";

    // Prevent repeated launches from bursting requests
    const qint64 currentTime = QDateTime::currentSecsSinceEpoch();
    if (m_settings->GetLatestReleaseCheckBlockedUntil() > currentTime || (m_settings->GetLatestReleaseCheckAt() > 0 && currentTime < m_settings->GetLatestReleaseCheckAt() + 60))
    {
        qDebug() << "UpdateChecker::CheckForUpdate - Cancelling update check - Rate limited";
        return;
    }
    m_settings->SaveValue(Settings::LatestReleaseCheckAt, currentTime, false);

    // Fetch only once per application start without surfacing failures in UI
    QNetworkRequest request(QUrl(QStringLiteral(GH_RELEASES_URL)));
    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setRawHeader("X-GitHub-Api-Version", GH_API_VERSION);
    request.setRawHeader("User-Agent", QStringLiteral(ORGANIZATION "/%1 (+%2)").arg(m_settings->GetCurrentVersion(), QStringLiteral(GH_REPOSITORY_URL)).toUtf8());
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setTransferTimeout(10000);

    QNetworkReply *reply = m_networkManager.get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

        // Persist GitHub-requested delays so restarting cannot bypass them
        const qint64 responseTime = QDateTime::currentSecsSinceEpoch();
        qint64 blockedUntil = 0;
        bool validHeader = false;
        const qint64 retryAfter = reply->rawHeader("Retry-After").trimmed().toLongLong(&validHeader);
        if (validHeader && retryAfter > 0)
        {
            blockedUntil = responseTime + retryAfter;
        }
        else if (reply->rawHeader("X-RateLimit-Remaining").trimmed() == "0")
        {
            const qint64 rateLimitReset = reply->rawHeader("X-RateLimit-Reset").trimmed().toLongLong(&validHeader);
            if (validHeader && rateLimitReset > responseTime)
            {
                blockedUntil = rateLimitReset;
            }
        }

        const qint64 pollInterval = reply->rawHeader("X-Poll-Interval").trimmed().toLongLong(&validHeader);
        if (validHeader && pollInterval > 0)
        {
            blockedUntil = qMax(blockedUntil, responseTime + pollInterval);
        }
        if (statusCode >= 400 && statusCode < 600 && blockedUntil <= responseTime)
        {
            blockedUntil = responseTime + 900;
        }

        if (blockedUntil > responseTime)
        {
            m_settings->SaveValue(Settings::LatestReleaseCheckBlockedUntil, blockedUntil, false);
        }
        else if (statusCode == 200 && m_settings->GetLatestReleaseCheckBlockedUntil() != 0)
        {
            m_settings->SaveValue(Settings::LatestReleaseCheckBlockedUntil, 0, false);
        }

        if (reply->error() != QNetworkReply::NoError || statusCode != 200)
        {
            if (statusCode > 0)
            {
                qWarning() << "UpdateChecker::CheckForUpdate - Update check returned HTTP" << statusCode;
            }
            else
            {
                qDebug() << "UpdateChecker::CheckForUpdate - GitHub request failed:" << reply->errorString();
            }
            reply->deleteLater();
            return;
        }

        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(reply->readAll(), &parseError);
        reply->deleteLater();
        if (parseError.error != QJsonParseError::NoError || !document.isObject())
        {
            return;
        }

        // Keep tag unchanged for display and persisted shown state
        const QJsonObject release = document.object();
        const QString tag = release.value(QStringLiteral("tag_name")).toString().trimmed();
        const QString releaseUrl = release.value(QStringLiteral("html_url")).toString().trimmed();
        const QUrl parsedReleaseUrl(releaseUrl);
        const QUrl repositoryUrl(QStringLiteral(GH_REPOSITORY_URL));
        if (tag.isEmpty() || !parsedReleaseUrl.isValid() || parsedReleaseUrl.scheme() != repositoryUrl.scheme() || parsedReleaseUrl.host() != repositoryUrl.host())
        {
            return;
        }

        if (tag == m_settings->GetLatestShownReleaseTag() || !IsNewerVersion(tag, m_settings->GetCurrentVersion()))
        {
            return;
        }

        qInfo() << "UpdateChecker::CheckForUpdate - Update available" << tag;

        const QString releaseNotes = release.value(QStringLiteral("body")).toString();
        emit signalUpdateAvailable(tag, releaseNotes, releaseUrl);
    });
}

/////////////////////////////////////////////////////////////////////

bool UpdateChecker::GetUpdateAvailable() const
{
    bool updateAvailable = m_settings != nullptr && IsNewerVersion(m_settings->GetLatestShownReleaseTag(), m_settings->GetCurrentVersion());
    return updateAvailable;
}

/////////////////////////////////////////////////////////////////////
///////////////////////////// PRIVATE ///////////////////////////////
/////////////////////////////////////////////////////////////////////

bool UpdateChecker::IsNewerVersion(const QString &releaseTag, const QString &currentVersion)
{
    // Compare numeric version components
    QString normalizedRelease = releaseTag.trimmed();
    if (normalizedRelease.startsWith('v', Qt::CaseInsensitive))
    {
        normalizedRelease.remove(0, 1);
    }

    qsizetype releaseSuffixIndex = 0;
    qsizetype currentSuffixIndex = 0;
    const QVersionNumber releaseVersion = QVersionNumber::fromString(normalizedRelease, &releaseSuffixIndex);
    const QVersionNumber installedVersion = QVersionNumber::fromString(currentVersion.trimmed(), &currentSuffixIndex);
    if (releaseVersion.isNull() || installedVersion.isNull())
    {
        return false;
    }

    return QVersionNumber::compare(releaseVersion, installedVersion) > 0;
}
