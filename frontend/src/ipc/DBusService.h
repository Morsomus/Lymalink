/////////////////////////////////////////////////////////
// File: DBusService.h
// Date: 2026-05-23
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Qt D-Bus client for lymalinkd service.
/////////////////////////////////////////////////////////

#pragma once

#include "../Defines.h"

#include <QList>
#include <QObject>
#include <QString>
#include <QVariantList>

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
    Q_PROPERTY(QVariantList activeTargetIds READ GetActiveTargetIds NOTIFY signalActiveTargetIdsChanged)

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
    Q_INVOKABLE void ReloadAllTargets();
    Q_INVOKABLE void ReloadConfig();
    Q_INVOKABLE void TestToast();
    Q_INVOKABLE void TestSound();

    inline bool GetServiceAvailable() const { return m_serviceAvailable; }
    inline bool GetServiceActive() const { return m_serviceActive; }
    inline bool GetServiceStarting() const { return m_serviceStarting; }
    inline bool GetServiceEnabled() const { return m_serviceEnabled; }
    inline QString GetLastError() const { return m_lastError; }
    inline QVariantList GetActiveTargetIds() const { return m_activeTargetIds; }

signals:
    void signalServiceAvailabilityChanged();
    void signalServiceStatusChanged();
    void signalLastErrorChanged();
    void signalActiveTargetIdsChanged();
    void signalAchievementUnlocked(int appId, const QString &achievementKey);

private slots:
    void OnPingFinished(QDBusPendingCallWatcher *watcher);
    void OnGameStateChanged(const QList<int> &targetIds, const QString &state);
    void OnAchievementUnlocked(int appId, const QString &achievementKey);

private:
    QTimer *m_pingTimer;
    QTimer *m_activeTargetsRequestTimer;
    bool m_pingInFlight;
    uint16_t m_pingIntervalMs;
    uint16_t m_pingTimeoutMs;
    int m_systemdTimeoutMs;
    bool m_serviceAvailable;
    bool m_serviceActive;
    bool m_serviceStarting;
    bool m_serviceEnabled;
    QString m_lastError;
    QVariantList m_activeTargetIds;

    void ResetPingTimer();
    void ConnectDaemonSignals();
    void ScheduleActiveTargetsRequest();
    void RequestActiveTargets();
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
