/////////////////////////////////////////////////////////
// File: DBusService.cpp
// Date: 2026-05-23
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Qt D-Bus client for lymalinkd service.
/////////////////////////////////////////////////////////

#include "DBusService.h"

#include <QDBusConnection>
#include <QDBusError>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusPendingCall>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusReply>
#include <QTimer>
#include <QVariant>

/////////////////////////////////////////////////////////////////////

DBusService::DBusService(QObject *parent) : QObject(parent)
{
    m_pingTimer = nullptr;
    m_activeTargetsRequestTimer = nullptr;
    m_pingInFlight = false;
    m_pingIntervalMs = 5000;
    m_pingTimeoutMs = 1000;
    m_systemdTimeoutMs = 5000;
    m_serviceAvailable = false;
    m_serviceActive = false;
    m_serviceStarting = false;
    m_serviceEnabled = false;
    m_lastError = "";
    m_activeTargetIds = {};

    // Poll backend availability so UI recovers when daemon starts later
    m_pingTimer = new QTimer(this);
    m_pingTimer->setInterval(m_pingIntervalMs);
    connect(m_pingTimer, &QTimer::timeout, this, &DBusService::PingBackend);
    m_pingTimer->start();

    // Delay active target refresh until daemon has finished state transitions
    m_activeTargetsRequestTimer = new QTimer(this);
    m_activeTargetsRequestTimer->setInterval(m_systemdTimeoutMs + 1000);
    m_activeTargetsRequestTimer->setSingleShot(true);
    connect(m_activeTargetsRequestTimer, &QTimer::timeout, this, &DBusService::RequestActiveTargets);

    // Subscribe and bootstrap daemon state immediately for first UI paint
    ConnectDaemonSignals();
    PingBackend();
    if (!m_serviceActive)
    {
        StartService();
    }
}

DBusService::~DBusService()
{
    // Destructor
}

/////////////////////////////////////////////////////////////////////
////////////////////////////// PUBLIC ///////////////////////////////
/////////////////////////////////////////////////////////////////////

bool DBusService::StopServiceIfNotEnabled()
{
    bool serviceStopped = false;

    // Keep enabled service running across frontend shutdown
    if (!FetchServiceEnabledStatus())
    {
        return serviceStopped;
    }

    if (m_serviceEnabled)
    {
        serviceStopped = true;
        return serviceStopped;
    }

    // Stop transient daemon only when autostart is disabled
    SetServiceStarting(false);
    serviceStopped = CallSystemdUnitMethod(QStringLiteral("StopUnit"));
    return serviceStopped;
}

/////////////////////////////////////////////////////////////////////

void DBusService::PingBackend()
{
    if (m_pingInFlight)
    {
        return;
    }

    // Ping session bus to detect daemon availability without blocking UI
    QDBusConnection sessionBus = QDBusConnection::sessionBus();
    if (!sessionBus.isConnected())
    {
        SetLastError(QStringLiteral("D-Bus session bus unavailable"));
        SetServiceAvailable(false);
        return;
    }

    m_pingInFlight = true;

    QDBusMessage message = QDBusMessage::createMethodCall(
        DBUS_BUS_NAME,
        DBUS_OBJECT_PATH,
        DBUS_INTERFACE,
        QStringLiteral("Ping")
    );
    QDBusPendingCall call = sessionBus.asyncCall(message, m_pingTimeoutMs);
    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(call, this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, &DBusService::OnPingFinished);
}

/////////////////////////////////////////////////////////////////////

bool DBusService::StartService()
{
    bool serviceStarted = false;

    // Show starting state while systemd processes StartUnit
    ResetPingTimer();
    SetServiceActive(false);
    SetServiceStarting(true);
    serviceStarted = CallSystemdUnitMethod(QStringLiteral("StartUnit"));
    if (!serviceStarted)
    {
        SetServiceStarting(false);
    }
    return serviceStarted;
}

/////////////////////////////////////////////////////////////////////

bool DBusService::StopService()
{
    bool serviceStopped = false;

    // Clear active state immediately so UI does not wait for next ping
    ResetPingTimer();
    SetServiceActive(false);
    SetServiceStarting(false);
    serviceStopped = CallSystemdUnitMethod(QStringLiteral("StopUnit"));
    return serviceStopped;
}

/////////////////////////////////////////////////////////////////////

bool DBusService::RestartService()
{
    bool serviceRestarted = false;

    // Restart is a stop/start transition from UI perspective
    ResetPingTimer();
    SetServiceActive(false);
    SetServiceStarting(true);
    serviceRestarted = CallSystemdUnitMethod(QStringLiteral("RestartUnit"));
    if (!serviceRestarted)
    {
        SetServiceStarting(false);
    }
    return serviceRestarted;
}

/////////////////////////////////////////////////////////////////////

bool DBusService::SetServiceEnabled(bool enabled)
{
    bool serviceEnabled = false;

    // Route to systemd enable/disable calls because signatures differ
    serviceEnabled = enabled ? EnableService() : DisableService();
    return serviceEnabled;
}

/////////////////////////////////////////////////////////////////////

bool DBusService::RefreshServiceStatus()
{
    bool statusRefreshed = false;

    // Read both runtime and autostart state for settings page consistency
    const bool activeRead = FetchServiceActiveStatus();
    const bool enabledRead = FetchServiceEnabledStatus();
    statusRefreshed = activeRead && enabledRead;
    return statusRefreshed;
}

/////////////////////////////////////////////////////////////////////

void DBusService::ReloadAllTargets()
{
    // Fire-and-forget reload; daemon publishes changed state through signals
    QDBusConnection sessionBus = QDBusConnection::sessionBus();
    if (!sessionBus.isConnected() || !m_serviceAvailable)
    {
        return;
    }

    QDBusMessage message = QDBusMessage::createMethodCall(
        DBUS_BUS_NAME,
        DBUS_OBJECT_PATH,
        DBUS_INTERFACE,
        QStringLiteral("ReloadAllTargets")
    );
    sessionBus.asyncCall(message, m_pingTimeoutMs);
}

/////////////////////////////////////////////////////////////////////

void DBusService::ReloadConfig()
{
    // Fire-and-forget config reload; ping loop handles daemon errors
    QDBusConnection sessionBus = QDBusConnection::sessionBus();
    if (!sessionBus.isConnected() || !m_serviceAvailable)
    {
        return;
    }

    QDBusMessage message = QDBusMessage::createMethodCall(
        DBUS_BUS_NAME,
        DBUS_OBJECT_PATH,
        DBUS_INTERFACE,
        QStringLiteral("ReloadConfig")
    );
    sessionBus.asyncCall(message, m_pingTimeoutMs);
}

/////////////////////////////////////////////////////////////////////

void DBusService::TestToast()
{
    // Fire-and-forget notification test from settings UI
    QDBusConnection sessionBus = QDBusConnection::sessionBus();
    if (!sessionBus.isConnected() || !m_serviceAvailable)
    {
        return;
    }

    QDBusMessage message = QDBusMessage::createMethodCall(
        DBUS_BUS_NAME,
        DBUS_OBJECT_PATH,
        DBUS_INTERFACE,
        QStringLiteral("TestToast")
    );
    sessionBus.asyncCall(message, m_pingTimeoutMs);
}

/////////////////////////////////////////////////////////////////////
//////////////////////////// PRIVATE SLOTS //////////////////////////
/////////////////////////////////////////////////////////////////////

void DBusService::OnPingFinished(QDBusPendingCallWatcher *watcher)
{
    // Watcher owns async ping reply until this slot consumes it
    QDBusPendingReply<QString> reply = *watcher;
    watcher->deleteLater();
    m_pingInFlight = false;

    if (reply.isError())
    {
        SetLastError(reply.error().message());
        SetServiceAvailable(false);
        return;
    }

    const bool available = (reply.value() == QStringLiteral("pong"));
    SetLastError(available ? QString() : QStringLiteral("Unexpected PingBackend response"));
    SetServiceAvailable(available);

    // Refresh systemd state once daemon responds again
    if (GetServiceAvailable() == true && GetServiceActive() == false)
    {
        RefreshServiceStatus();
    }
}

/////////////////////////////////////////////////////////////////////

void DBusService::OnGameStateChanged(const QList<int> &targetIds, const QString &state)
{
    bool changed = false;

    // Maintain cached active target list from daemon signal deltas
    for (const int targetId : targetIds)
    {
        if (targetId <= 0)
        {
            continue;
        }

        const QVariant targetValue = targetId;
        const int existingIndex = m_activeTargetIds.indexOf(targetValue);

        if (state == QStringLiteral("Active"))
        {
            if (existingIndex == -1)
            {
                m_activeTargetIds.append(targetValue);
                changed = true;
            }
        }
        else if (state == QStringLiteral("Inactive"))
        {
            if (existingIndex != -1)
            {
                m_activeTargetIds.removeAt(existingIndex);
                changed = true;
            }
        }
    }

    if (changed)
    {
        emit signalActiveTargetIdsChanged();
    }
}

/////////////////////////////////////////////////////////////////////
///////////////////////////// PRIVATE ///////////////////////////////
/////////////////////////////////////////////////////////////////////

void DBusService::ResetPingTimer()
{
    // Restart interval after user-triggered service operation
    if (m_pingTimer)
    {
        m_pingTimer->start();
    }
}

/////////////////////////////////////////////////////////////////////

void DBusService::ConnectDaemonSignals()
{
    // Subscribe to daemon target-state notifications over session bus
    QDBusConnection sessionBus = QDBusConnection::sessionBus();
    if (!sessionBus.isConnected())
    {
        return;
    }

    const bool connected = sessionBus.connect(
        QString::fromLatin1(DBUS_BUS_NAME),
        QString::fromLatin1(DBUS_OBJECT_PATH),
        QString::fromLatin1(DBUS_INTERFACE),
        QStringLiteral("GameStateChanged"),
        this,
        SLOT(OnGameStateChanged(QList<int>,QString))
    );

    if (!connected)
    {
        SetLastError(QStringLiteral("Failed to subscribe to GameStateChanged signal"));
    }
}

/////////////////////////////////////////////////////////////////////

void DBusService::ScheduleActiveTargetsRequest()
{
    // Debounce target refresh while service startup settles
    if (!m_activeTargetsRequestTimer || !m_serviceAvailable)
    {
        return;
    }

    m_activeTargetsRequestTimer->start();
}

/////////////////////////////////////////////////////////////////////

void DBusService::RequestActiveTargets()
{
    // Ask daemon to emit current active targets after reconnect/startup
    QDBusConnection sessionBus = QDBusConnection::sessionBus();
    if (!sessionBus.isConnected() || !m_serviceAvailable)
    {
        return;
    }

    QDBusMessage message = QDBusMessage::createMethodCall(
        DBUS_BUS_NAME,
        DBUS_OBJECT_PATH,
        DBUS_INTERFACE,
        QStringLiteral("RequestActiveTargets")
    );
    sessionBus.asyncCall(message, m_pingTimeoutMs);
}

/////////////////////////////////////////////////////////////////////

bool DBusService::EnableService()
{
    bool serviceEnabled = false;

    // systemd EnableUnitFiles persists daemon autostart
    QDBusConnection sessionBus = QDBusConnection::sessionBus();
    if (!sessionBus.isConnected())
    {
        SetLastError(QStringLiteral("D-Bus session bus unavailable"));
        return serviceEnabled;
    }

    QDBusMessage message = QDBusMessage::createMethodCall(
        DBUS_SYSTEMD_BUS_NAME,
        DBUS_SYSTEMD_OBJECT_PATH,
        DBUS_SYSTEMD_MANAGER_INTERFACE,
        QStringLiteral("EnableUnitFiles")
    );
    message << QStringList{QString::fromLatin1(DBUS_SYSTEMD_UNIT_NAME)} << false << true;

    QDBusMessage reply = sessionBus.call(message, QDBus::BlockWithGui, m_systemdTimeoutMs);
    if (reply.type() == QDBusMessage::ErrorMessage)
    {
        SetLastError(reply.errorMessage());
        RefreshServiceStatus();
        return serviceEnabled;
    }

    SetLastError(QString());
    RefreshServiceStatus();
    serviceEnabled = true;
    return serviceEnabled;
}

/////////////////////////////////////////////////////////////////////

bool DBusService::DisableService()
{
    bool serviceDisabled = false;

    // systemd DisableUnitFiles removes daemon autostart
    QDBusConnection sessionBus = QDBusConnection::sessionBus();
    if (!sessionBus.isConnected())
    {
        SetLastError(QStringLiteral("D-Bus session bus unavailable"));
        return serviceDisabled;
    }

    QDBusMessage message = QDBusMessage::createMethodCall(
        DBUS_SYSTEMD_BUS_NAME,
        DBUS_SYSTEMD_OBJECT_PATH,
        DBUS_SYSTEMD_MANAGER_INTERFACE,
        QStringLiteral("DisableUnitFiles")
    );
    message << QStringList{QString::fromLatin1(DBUS_SYSTEMD_UNIT_NAME)} << false;

    QDBusMessage reply = sessionBus.call(message, QDBus::BlockWithGui, m_systemdTimeoutMs);
    if (reply.type() == QDBusMessage::ErrorMessage)
    {
        SetLastError(reply.errorMessage());
        RefreshServiceStatus();
        return serviceDisabled;
    }

    SetLastError(QString());
    RefreshServiceStatus();
    serviceDisabled = true;
    return serviceDisabled;
}

/////////////////////////////////////////////////////////////////////

bool DBusService::CallSystemdUnitMethod(const QString &method)
{
    bool methodCalled = false;

    // Shared helper for StartUnit/StopUnit/RestartUnit
    QDBusConnection sessionBus = QDBusConnection::sessionBus();
    if (!sessionBus.isConnected())
    {
        SetLastError(QStringLiteral("D-Bus session bus unavailable"));
        return methodCalled;
    }

    QDBusMessage message = QDBusMessage::createMethodCall(
        DBUS_SYSTEMD_BUS_NAME,
        DBUS_SYSTEMD_OBJECT_PATH,
        DBUS_SYSTEMD_MANAGER_INTERFACE,
        method
    );
    message << QString::fromLatin1(DBUS_SYSTEMD_UNIT_NAME) << QStringLiteral("replace");

    QDBusMessage reply = sessionBus.call(message, QDBus::BlockWithGui, m_systemdTimeoutMs);
    if (reply.type() == QDBusMessage::ErrorMessage)
    {
        SetLastError(reply.errorMessage());
        return methodCalled;
    }

    SetLastError(QString());
    methodCalled = true;
    return methodCalled;
}

/////////////////////////////////////////////////////////////////////

bool DBusService::FetchServiceActiveStatus()
{
    bool statusFetched = false;

    // Query systemd unit object before reading ActiveState property
    QDBusConnection sessionBus = QDBusConnection::sessionBus();
    if (!sessionBus.isConnected())
    {
        SetLastError(QStringLiteral("D-Bus session bus unavailable"));
        SetServiceActive(false);
        SetServiceStarting(false);
        return statusFetched;
    }

    QDBusMessage getUnitMessage = QDBusMessage::createMethodCall(
        DBUS_SYSTEMD_BUS_NAME,
        DBUS_SYSTEMD_OBJECT_PATH,
        DBUS_SYSTEMD_MANAGER_INTERFACE,
        QStringLiteral("GetUnit")
    );
    getUnitMessage << QString::fromLatin1(DBUS_SYSTEMD_UNIT_NAME);

    QDBusReply<QDBusObjectPath> getUnitReply = sessionBus.call(getUnitMessage, QDBus::BlockWithGui, m_systemdTimeoutMs);
    if (!getUnitReply.isValid())
    {
        SetServiceActive(false);
        SetServiceStarting(false);
        return statusFetched;
    }

    QDBusMessage activeStateMessage = QDBusMessage::createMethodCall(
        DBUS_SYSTEMD_BUS_NAME,
        getUnitReply.value().path(),
        DBUS_PROPERTIES_INTERFACE,
        QStringLiteral("Get")
    );
    activeStateMessage << QString::fromLatin1(DBUS_SYSTEMD_UNIT_INTERFACE) << QStringLiteral("ActiveState");

    QDBusReply<QVariant> activeStateReply = sessionBus.call(activeStateMessage, QDBus::BlockWithGui, m_systemdTimeoutMs);
    if (!activeStateReply.isValid())
    {
        SetServiceActive(false);
        SetServiceStarting(false);
        return statusFetched;
    }

    const QString activeState = activeStateReply.value().toString();
    SetServiceStarting(activeState == QStringLiteral("activating"));
    SetServiceActive(activeState == QStringLiteral("active"));
    statusFetched = true;
    return statusFetched;
}

/////////////////////////////////////////////////////////////////////

bool DBusService::FetchServiceEnabledStatus()
{
    bool statusFetched = false;

    // Query systemd unit file state for autostart toggle
    QDBusConnection sessionBus = QDBusConnection::sessionBus();
    if (!sessionBus.isConnected())
    {
        SetLastError(QStringLiteral("D-Bus session bus unavailable"));
        SetServiceEnabledState(false);
        return statusFetched;
    }

    QDBusMessage message = QDBusMessage::createMethodCall(
        DBUS_SYSTEMD_BUS_NAME,
        DBUS_SYSTEMD_OBJECT_PATH,
        DBUS_SYSTEMD_MANAGER_INTERFACE,
        QStringLiteral("GetUnitFileState")
    );
    message << QString::fromLatin1(DBUS_SYSTEMD_UNIT_NAME);

    QDBusReply<QString> reply = sessionBus.call(message, QDBus::BlockWithGui, m_systemdTimeoutMs);
    if (!reply.isValid())
    {
        SetServiceEnabledState(false);
        return statusFetched;
    }

    SetServiceEnabledState(reply.value() == QStringLiteral("enabled"));
    statusFetched = true;
    return statusFetched;
}

/////////////////////////////////////////////////////////////////////

void DBusService::SetServiceAvailable(bool available)
{
    if (m_serviceAvailable == available)
    {
        return;
    }

    qDebug() << "SetServiceAvailable: lymalinkd dbus status:" << (available ? "available" : "not available");

    // Clear stale active targets when daemon disappears
    m_serviceAvailable = available;
    if (!available && !m_activeTargetIds.empty())
    {
        m_activeTargetIds.clear();
        emit signalActiveTargetIdsChanged();
    }
    if (available)
    {
        ScheduleActiveTargetsRequest();
    }
    emit signalServiceAvailabilityChanged();
}

/////////////////////////////////////////////////////////////////////

void DBusService::SetServiceActive(bool active)
{
    if (m_serviceActive == active)
    {
        return;
    }

    // Notify all service status bindings together
    m_serviceActive = active;
    emit signalServiceStatusChanged();
}

/////////////////////////////////////////////////////////////////////

void DBusService::SetServiceEnabledState(bool enabled)
{
    if (m_serviceEnabled == enabled)
    {
        return;
    }

    // Notify settings toggle after systemd state changes
    m_serviceEnabled = enabled;
    emit signalServiceStatusChanged();
}

/////////////////////////////////////////////////////////////////////

void DBusService::SetServiceStarting(bool starting)
{
    if (m_serviceStarting == starting)
    {
        return;
    }

    // Expose transitional state while systemd job is running
    m_serviceStarting = starting;
    emit signalServiceStatusChanged();
}


/////////////////////////////////////////////////////////////////////

void DBusService::SetLastError(const QString &error)
{
    if (m_lastError == error)
    {
        return;
    }

    // Avoid duplicate error notifications for same message
    m_lastError = error;
    emit signalLastErrorChanged();
}
