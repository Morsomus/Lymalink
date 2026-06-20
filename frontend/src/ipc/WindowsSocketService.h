/////////////////////////////////////////////////////////
// File: WindowsSocketService.h
// Date: 2026-06-19
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Declares Windows backend-control placeholder
//              for future local sockets.
/////////////////////////////////////////////////////////

#pragma once

#include "BackendControl.h"

#include <QString>
#include <QVariantList>

class WindowsSocketService : public BackendControl
{
    Q_OBJECT
    Q_PROPERTY(bool serviceAvailable READ GetServiceAvailable NOTIFY signalServiceAvailabilityChanged)
    Q_PROPERTY(bool serviceActive READ GetServiceActive NOTIFY signalServiceStatusChanged)
    Q_PROPERTY(bool serviceStarting READ GetServiceStarting NOTIFY signalServiceStatusChanged)
    Q_PROPERTY(bool serviceEnabled READ GetServiceEnabled NOTIFY signalServiceStatusChanged)
    Q_PROPERTY(QString lastError READ GetLastError NOTIFY signalLastErrorChanged)
    Q_PROPERTY(QVariantList activeTargetIds READ GetActiveTargetIds NOTIFY signalActiveTargetIdsChanged)
    Q_PROPERTY(bool supportsServiceAutostart READ GetSupportsServiceAutostart CONSTANT)
    Q_PROPERTY(bool supportsOverlayLaunchHints READ GetSupportsOverlayLaunchHints CONSTANT)

public:
    explicit WindowsSocketService(QObject *parent = nullptr);
    ~WindowsSocketService() override;

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

    inline bool GetServiceAvailable() const { return m_serviceAvailable; }
    inline bool GetServiceActive() const { return m_serviceActive; }
    inline bool GetServiceStarting() const { return m_serviceStarting; }
    inline bool GetServiceEnabled() const { return m_serviceEnabled; }
    inline QString GetLastError() const { return m_lastError; }
    inline QVariantList GetActiveTargetIds() const { return m_activeTargetIds; }
    inline bool GetSupportsServiceAutostart() const { return false; }
    inline bool GetSupportsOverlayLaunchHints() const { return false; }

signals:
    void signalServiceAvailabilityChanged();
    void signalServiceStatusChanged();
    void signalLastErrorChanged();
    void signalActiveTargetIdsChanged();
    void signalAchievementUnlocked(int appId, const QString &achievementKey);

private:
    bool m_serviceAvailable = false;
    bool m_serviceActive = false;
    bool m_serviceStarting = false;
    bool m_serviceEnabled = false;
    QString m_lastError;
    QVariantList m_activeTargetIds;

    void ReportUnsupported();
    void SetServiceAvailable(bool available);
    void SetServiceActive(bool active);
    void SetServiceEnabledState(bool enabled);
    void SetServiceStarting(bool starting);
    void SetLastError(const QString &error);
};
