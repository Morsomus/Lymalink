/////////////////////////////////////////////////////////
// File: WinSocketService.h
// Date: 2026-06-19
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Declares Windows local socket backend-control service
/////////////////////////////////////////////////////////

#pragma once

#include "BackendControl.h"
#include "../Defines.h"

#include <QLocalSocket>
#include <QJsonObject>
#include <QTimer>
#include <QVariantList>

class WinSocketService : public BackendControl
{
    Q_OBJECT
    Q_PROPERTY(bool serviceAvailable READ GetServiceAvailable NOTIFY signalServiceAvailabilityChanged)
    Q_PROPERTY(bool serviceActive READ GetServiceActive NOTIFY signalServiceStatusChanged)
    Q_PROPERTY(bool serviceStarting READ GetServiceStarting NOTIFY signalServiceStatusChanged)
    Q_PROPERTY(bool serviceEnabled READ GetServiceEnabled NOTIFY signalServiceStatusChanged)
    Q_PROPERTY(QString lastError READ GetLastError NOTIFY signalLastErrorChanged)
    Q_PROPERTY(QVariantList activeTargetIds READ GetActiveTargetIds NOTIFY signalActiveTargetIdsChanged)

public:
    explicit WinSocketService(QObject *parent = nullptr);
    ~WinSocketService() override;

    bool StopServiceIfNotEnabled() override;

    Q_INVOKABLE void PingBackend() override;
    Q_INVOKABLE bool StartService() override;
    Q_INVOKABLE bool StopService() override;
    Q_INVOKABLE bool RestartService() override;
    Q_INVOKABLE bool SetServiceEnabled(bool enabled) override;
    Q_INVOKABLE bool RefreshServiceStatus() override;
    Q_INVOKABLE void ReloadAllTargets() override;
    Q_INVOKABLE void ReloadConfig() override;
    Q_INVOKABLE void TestToast() override;
    Q_INVOKABLE void TestSound() override;

    bool GetServiceAvailable() const { return m_serviceAvailable; }
    bool GetServiceActive() const { return m_serviceActive; }
    bool GetServiceStarting() const { return m_serviceStarting; }
    bool GetServiceEnabled() const { return m_serviceEnabled; }
    QString GetLastError() const { return m_lastError; }
    QVariantList GetActiveTargetIds() const { return m_activeTargetIds; }

signals:
    void signalServiceAvailabilityChanged();
    void signalServiceStatusChanged();
    void signalLastErrorChanged();
    void signalActiveTargetIdsChanged();
    void signalAchievementUnlocked(int appId, const QString &achievementKey);

private:
    QLocalSocket m_socket;
    QTimer m_pingTimer;
    QTimer m_startTimeoutTimer;
    uint16_t m_pingIntervalMs;
    quint64 m_nextRequestId;
    bool m_serviceAvailable;
    bool m_serviceActive;
    bool m_serviceStarting;
    bool m_serviceEnabled;
    QString m_lastError;
    QVariantList m_activeTargetIds;

    void ConnectSocket();
    bool ConnectExistingBackend(int timeoutMs);
    void SendRequest(const QString &method, const QJsonObject &params = {});
    void HandleMessage(const QJsonObject &message);
    bool FetchServiceEnabledStatus();
    QString GetDaemonExecutablePath() const;
    void SetServiceAvailable(bool available);
    void SetServiceActive(bool active);
    void SetServiceEnabledState(bool enabled);
    void SetServiceStarting(bool starting);
    void SetLastError(const QString &error);
};
