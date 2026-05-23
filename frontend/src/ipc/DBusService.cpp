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
    m_serviceAvailable = false;
    m_pingInFlight = false;
    m_pingIntervalMs = 5000;
    m_pingTimeoutMs = 1000;
    m_systemdTimeoutMs = 5000;
    m_serviceActive = false;
    m_serviceStarting = false;
    m_serviceEnabled = false;
    m_lastError = "";

    m_pingTimer = new QTimer(this);
    m_pingTimer->setInterval(m_pingIntervalMs);
    connect(m_pingTimer, &QTimer::timeout, this, &DBusService::PingBackend);
    m_pingTimer->start();

    PingBackend();
    RefreshServiceStatus();
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
    if (!FetchServiceEnabledStatus())
    {
        return false;
    }

    if (m_serviceEnabled)
    {
        return true;
    }

    SetServiceStarting(false);
    return CallSystemdUnitMethod(QStringLiteral("StopUnit"));
}

/////////////////////////////////////////////////////////////////////

void DBusService::PingBackend()
{
    if (m_pingInFlight) return;

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
    SetServiceStarting(true);
    const bool success = CallSystemdUnitMethod(QStringLiteral("StartUnit"));
    if (!success)
    {
        SetServiceStarting(false);
    }
    QTimer::singleShot(1000, this, [this]() {
        RefreshServiceStatus();
        PingBackend();
    });
    return success;
}

/////////////////////////////////////////////////////////////////////

bool DBusService::StopService()
{
    SetServiceStarting(false);
    const bool success = CallSystemdUnitMethod(QStringLiteral("StopUnit"));
    QTimer::singleShot(1000, this, [this]() {
        RefreshServiceStatus();
        PingBackend();
    });
    return success;
}

/////////////////////////////////////////////////////////////////////

bool DBusService::RestartService()
{
    SetServiceStarting(true);
    const bool success = CallSystemdUnitMethod(QStringLiteral("RestartUnit"));
    if (!success)
    {
        SetServiceStarting(false);
    }
    QTimer::singleShot(1000, this, [this]() {
        RefreshServiceStatus();
        PingBackend();
    });
    return success;
}

/////////////////////////////////////////////////////////////////////

bool DBusService::SetServiceEnabled(bool enabled)
{
    return enabled ? EnableService() : DisableService();
}

/////////////////////////////////////////////////////////////////////

bool DBusService::RefreshServiceStatus()
{
    const bool activeRead = FetchServiceActiveStatus();
    const bool enabledRead = FetchServiceEnabledStatus();
    return activeRead && enabledRead;
}

/////////////////////////////////////////////////////////////////////
//////////////////////////// PRIVATE SLOTS //////////////////////////
/////////////////////////////////////////////////////////////////////

void DBusService::OnPingFinished(QDBusPendingCallWatcher *watcher)
{
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
}

/////////////////////////////////////////////////////////////////////
///////////////////////////// PRIVATE ///////////////////////////////
/////////////////////////////////////////////////////////////////////

bool DBusService::EnableService()
{
    QDBusConnection sessionBus = QDBusConnection::sessionBus();
    if (!sessionBus.isConnected())
    {
        SetLastError(QStringLiteral("D-Bus session bus unavailable"));
        return false;
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
        return false;
    }

    SetLastError(QString());
    RefreshServiceStatus();
    return true;
}

/////////////////////////////////////////////////////////////////////

bool DBusService::DisableService()
{
    QDBusConnection sessionBus = QDBusConnection::sessionBus();
    if (!sessionBus.isConnected())
    {
        SetLastError(QStringLiteral("D-Bus session bus unavailable"));
        return false;
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
        return false;
    }

    SetLastError(QString());
    RefreshServiceStatus();
    return true;
}

/////////////////////////////////////////////////////////////////////

bool DBusService::CallSystemdUnitMethod(const QString &method)
{
    QDBusConnection sessionBus = QDBusConnection::sessionBus();
    if (!sessionBus.isConnected())
    {
        SetLastError(QStringLiteral("D-Bus session bus unavailable"));
        return false;
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
        return false;
    }

    SetLastError(QString());
    return true;
}

/////////////////////////////////////////////////////////////////////

bool DBusService::FetchServiceActiveStatus()
{
    QDBusConnection sessionBus = QDBusConnection::sessionBus();
    if (!sessionBus.isConnected())
    {
        SetLastError(QStringLiteral("D-Bus session bus unavailable"));
        SetServiceActive(false);
        SetServiceStarting(false);
        return false;
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
        return false;
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
        return false;
    }

    const QString activeState = activeStateReply.value().toString();
    SetServiceStarting(activeState == QStringLiteral("activating"));
    SetServiceActive(activeState == QStringLiteral("active"));
    return true;
}

/////////////////////////////////////////////////////////////////////

bool DBusService::FetchServiceEnabledStatus()
{
    QDBusConnection sessionBus = QDBusConnection::sessionBus();
    if (!sessionBus.isConnected())
    {
        SetLastError(QStringLiteral("D-Bus session bus unavailable"));
        SetServiceEnabledState(false);
        return false;
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
        return false;
    }

    SetServiceEnabledState(reply.value() == QStringLiteral("enabled"));
    return true;
}

/////////////////////////////////////////////////////////////////////

void DBusService::SetServiceAvailable(bool available)
{
    if (m_serviceAvailable == available) return;

    // Refresh also systemd active & enabled statuses when availability changes
    qDebug() << "SetServiceAvailable - Checking service systemd status because service has been set:" << (available ? "available" : "not available");
    RefreshServiceStatus();

    m_serviceAvailable = available;
    emit signalServiceAvailabilityChanged();
}

/////////////////////////////////////////////////////////////////////

void DBusService::SetServiceActive(bool active)
{
    if (m_serviceActive == active) return;

    m_serviceActive = active;
    emit signalServiceStatusChanged();
}

/////////////////////////////////////////////////////////////////////

void DBusService::SetServiceEnabledState(bool enabled)
{
    if (m_serviceEnabled == enabled) return;

    m_serviceEnabled = enabled;
    emit signalServiceStatusChanged();
}

/////////////////////////////////////////////////////////////////////

void DBusService::SetServiceStarting(bool starting)
{
    if (m_serviceStarting == starting) return;

    m_serviceStarting = starting;
    emit signalServiceStatusChanged();
}


/////////////////////////////////////////////////////////////////////

void DBusService::SetLastError(const QString &error)
{
    if (m_lastError == error) return;

    m_lastError = error;
    emit signalLastErrorChanged();
}
