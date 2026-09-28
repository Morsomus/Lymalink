/////////////////////////////////////////////////////////
// File: UpdateChecker.h
// Date: 2026-09-28
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Declares checks for Lymalink release updates
/////////////////////////////////////////////////////////

#pragma once

#include "Defines.h"

#include <QNetworkAccessManager>
#include <QObject>
#include <QString>

class Settings;

class UpdateChecker : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool updateAvailable READ GetUpdateAvailable NOTIFY signalUpdateAvailabilityChanged)

public:
    explicit UpdateChecker(Settings *settings, QObject *parent = nullptr);
    ~UpdateChecker();

    Q_INVOKABLE void CheckForUpdate();
    bool GetUpdateAvailable() const;

signals:
    void signalUpdateAvailable(QString tag, QString releaseNotes, QString releaseUrl);
    void signalUpdateAvailabilityChanged();

private:
    Settings *m_settings;
    QNetworkAccessManager m_networkManager;
    bool m_checkStarted;

    static bool IsNewerVersion(const QString &releaseTag, const QString &currentVersion);
};
