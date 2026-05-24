/////////////////////////////////////////////////////////
// File: DBusService.h
// Date: 2026-05-23
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Qt D-Bus client for lymalinkd service.
/////////////////////////////////////////////////////////

#pragma once

#include "../Defines.h"

#include <QObject>
#include <QString>

class QDBusPendingCallWatcher;
class QTimer;

class DBusService : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool serviceAvailable READ GetServiceAvailable NOTIFY signalServiceAvailabilityChanged)
    Q_PROPERTY(bool serviceActive READ GetServiceActive NOTIFY signalServiceStatusChanged)
    Q_PROPERTY(bool serviceStarting READ GetServiceStarting NOTIFY signalServiceStatusChanged)
    Q_PROPERTY(bool serviceEnabled READ GetServiceEnabled NOTIFY signalServiceStatusChanged)
    Q_PROPERTY(QString lastError READ GetLastError NOTIFY signalLastErrorChanged)

public:
    explicit DBusService(QObject *parent = nullptr);
    ~DBusService();

    bool StopServiceIfNotEnabled();

    Q_INVOKABLE void PingBackend();
    Q_INVOKABLE bool StartService();
    Q_INVOKABLE bool StopService();
    Q_INVOKABLE bool RestartService();
    Q_INVOKABLE bool SetServiceEnabled(bool enabled);
    Q_INVOKABLE bool RefreshServiceStatus();

    inline bool GetServiceAvailable() const { return m_serviceAvailable; }
    inline bool GetServiceActive() const { return m_serviceActive; }
    inline bool GetServiceStarting() const { return m_serviceStarting; }
    inline bool GetServiceEnabled() const { return m_serviceEnabled; }
    inline QString GetLastError() const { return m_lastError; }

signals:
    void signalServiceAvailabilityChanged();
    void signalServiceStatusChanged();
    void signalLastErrorChanged();

private slots:
    void OnPingFinished(QDBusPendingCallWatcher *watcher);

private:
    QTimer *m_pingTimer;
    bool m_pingInFlight;
    uint16_t m_pingIntervalMs;
    uint16_t m_pingTimeoutMs;
    int m_systemdTimeoutMs;
    bool m_serviceAvailable;
    bool m_serviceActive;
    bool m_serviceStarting;
    bool m_serviceEnabled;
    QString m_lastError;

    void ResetPingTimer();
    bool EnableService();
    bool DisableService();
    bool CallSystemdUnitMethod(const QString &method);
    bool FetchServiceActiveStatus();
    bool FetchServiceEnabledStatus();
    void SetServiceAvailable(bool available);
    void SetServiceActive(bool active);
    void SetServiceEnabledState(bool enabled);
    void SetServiceStarting(bool starting);
    void SetLastError(const QString &error);
};
