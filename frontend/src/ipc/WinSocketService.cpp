/////////////////////////////////////////////////////////
// File: WinSocketService.cpp
// Date: 2026-06-19
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Implements Windows local socket backend-control service
/////////////////////////////////////////////////////////

#include "WinSocketService.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QSettings>
#include <QTimer>

#define STARTUP_TIMEOUT_MS 10000

/////////////////////////////////////////////////////////////////////

WinSocketService::WinSocketService(QObject *parent) : BackendControl(parent)
{
    m_pingIntervalMs = 5000;
    m_nextRequestId = 1;
    m_serviceAvailable = false;
    m_serviceActive = false;
    m_serviceStarting = false;
    m_serviceEnabled = false;
    m_lastError = "";
    m_activeTargetIds = {};

    // Poll backend availability and configure startup timeout handling
    m_pingTimer.setInterval(m_pingIntervalMs);
    connect(&m_pingTimer, &QTimer::timeout, this, &WinSocketService::PingBackend);

    m_startTimeoutTimer.setSingleShot(true);
    connect(&m_startTimeoutTimer, &QTimer::timeout, this, [this] {
        if (!m_serviceStarting)
        {
            return;
        }

        SetServiceStarting(false);
        SetServiceActive(false);
        SetServiceAvailable(false);
        SetLastError(QStringLiteral("lymalinkd.exe did not become available."));
    });
    connect(&m_socket, &QLocalSocket::connected, this, [this] {
        qDebug() << "WinSocketService::WinSocketService: connected to lymalinkd socket.";
        SetLastError({});
        PingBackend();
    });
    connect(&m_socket, &QLocalSocket::readyRead, this, [this] {
        while (m_socket.canReadLine())
        {
            const QJsonDocument message = QJsonDocument::fromJson(m_socket.readLine().trimmed());
            if (message.isObject())
            {
                HandleMessage(message.object());
            }
        }
    });
    connect(&m_socket, &QLocalSocket::errorOccurred, this, [this](QLocalSocket::LocalSocketError) {
        // Clear stale availability when the daemon socket fails
        if (m_socket.state() == QLocalSocket::UnconnectedState)
        {
            SetServiceAvailable(false);
        }
    });

    FetchServiceEnabledStatus();
    StartService();
    m_pingTimer.start();
}

WinSocketService::~WinSocketService()
{
    // Destructor
}

/////////////////////////////////////////////////////////////////////
////////////////////////////// PUBLIC ///////////////////////////////
/////////////////////////////////////////////////////////////////////

bool WinSocketService::StopServiceIfNotEnabled()
{
    bool serviceStopped = false;
    
    // Keep enabled daemon running across frontend shutdown
    if (!FetchServiceEnabledStatus())
    {
        return serviceStopped;
    }

    if (m_serviceEnabled)
    {
        serviceStopped = true;
        return serviceStopped;
    }

    // Stop transient daemon when autostart is disabled
    return StopService();
}

/////////////////////////////////////////////////////////////////////

void WinSocketService::PingBackend()
{
    // Connect lazily, then request backend availability state
    ConnectSocket();
    if (m_socket.state() == QLocalSocket::ConnectedState)
    {
        SendRequest(QStringLiteral("Ping"));
    }
}

/////////////////////////////////////////////////////////////////////

bool WinSocketService::StartService()
{
    // One frontend must not spawn another daemon while its first launch starts
    if (m_serviceStarting)
    {
        return true;
    }

    if (ConnectExistingBackend(250))
    {
        qDebug() << "WinSocketService::StartService: using existing lymalinkd instance.";
        m_startTimeoutTimer.stop();
        SetServiceStarting(false);
        PingBackend();
        return true;
    }

    const QString executable = GetDaemonExecutablePath();
    if (!QFileInfo::exists(executable))
    {
        SetLastError(QStringLiteral("lymalinkd.exe is missing beside Lymalink.exe."));
        return false;
    }
    SetServiceStarting(true);
    SetServiceActive(false);
    SetLastError({});

    qDebug() << "WinSocketService::StartService: launching lymalinkd:" << executable;
    if (!QProcess::startDetached(executable, {}, QCoreApplication::applicationDirPath()))
    {
        SetServiceStarting(false);
        SetLastError(QStringLiteral("Could not start lymalinkd.exe."));
        return false;
    }

    m_startTimeoutTimer.start(STARTUP_TIMEOUT_MS);
    QTimer::singleShot(250, this, &WinSocketService::PingBackend);
    return true;
}

/////////////////////////////////////////////////////////////////////

bool WinSocketService::StopService()
{
    // Connect to daemon before requesting orderly shutdown
    m_startTimeoutTimer.stop();
    if (!ConnectExistingBackend(1000))
    {
        SetLastError(QStringLiteral("lymalinkd.exe is not responding."));
        return false;
    }

    SendRequest(QStringLiteral("Shutdown"));
    if (!m_socket.waitForDisconnected(3000))
    {
        SetLastError(QStringLiteral("lymalinkd.exe did not stop within timeout."));
        return false;
    }

    SetServiceStarting(false);
    SetServiceActive(false);
    SetServiceAvailable(false);

    return true;
}

/////////////////////////////////////////////////////////////////////

bool WinSocketService::RestartService()
{
    // Restart daemon through existing shutdown and startup flows
    StopService();
    return StartService();
}

/////////////////////////////////////////////////////////////////////

bool WinSocketService::SetServiceEnabled(bool enabled)
{
    // Persist daemon autostart in the current user's Windows Run key
    QSettings autostartSettings(QStringLiteral(WIN_AUTOSTART_REGISTRY_KEY), QSettings::NativeFormat);
    if (enabled)
    {
        const QString executable = GetDaemonExecutablePath();
        if (!QFileInfo::exists(executable))
        {
            SetLastError(QStringLiteral("lymalinkd.exe is missing beside Lymalink.exe."));
            return false;
        }

        const QString command = QStringLiteral("\"") + QDir::toNativeSeparators(executable) + QStringLiteral("\"");
        autostartSettings.setValue(QStringLiteral(WIN_AUTOSTART_VALUE_NAME), command);
    }
    else
    {
        autostartSettings.remove(QStringLiteral(WIN_AUTOSTART_VALUE_NAME));
    }

    autostartSettings.sync();
    if (autostartSettings.status() != QSettings::NoError)
    {
        SetLastError(QStringLiteral("Could not update Windows startup registration."));
        return false;
    }

    SetLastError({});
    return FetchServiceEnabledStatus();
}

/////////////////////////////////////////////////////////////////////

bool WinSocketService::RefreshServiceStatus()
{
    // Refresh runtime and persistent autostart state
    PingBackend();
    return FetchServiceEnabledStatus();
}

/////////////////////////////////////////////////////////////////////

void WinSocketService::ReloadAllTargets()
{
    // Ask daemon to reload all configured targets
    SendRequest(QStringLiteral("ReloadAllTargets"));
}

/////////////////////////////////////////////////////////////////////

void WinSocketService::StartManualAchievementDataScan(int appId)
{
    SendRequest(QStringLiteral("StartManualAchievementDataScan"), {{"targetId", appId}});
}

/////////////////////////////////////////////////////////////////////

void WinSocketService::CancelManualAchievementDataScan(int appId)
{
    SendRequest(QStringLiteral("CancelManualAchievementDataScan"), {{"targetId", appId}});
}

/////////////////////////////////////////////////////////////////////

void WinSocketService::ReloadConfig()
{
    // Ask daemon to reload its configuration
    SendRequest(QStringLiteral("ReloadConfig"));
}

/////////////////////////////////////////////////////////////////////

void WinSocketService::TestToast()
{
    // Ask daemon to display test toast notification
    SendRequest(QStringLiteral("TestToast"));
}

/////////////////////////////////////////////////////////////////////

void WinSocketService::TestSound()
{
    // Ask daemon to play test notification sound
    SendRequest(QStringLiteral("TestSound"));
}

/////////////////////////////////////////////////////////////////////
///////////////////////////// PRIVATE ///////////////////////////////
/////////////////////////////////////////////////////////////////////

void WinSocketService::ConnectSocket()
{
    // Start local socket connection only when currently disconnected
    if (m_socket.state() == QLocalSocket::UnconnectedState)
    {
        m_socket.connectToServer(QString::fromLatin1(WIN_SOCKET_SERVER_NAME));
    }
}

/////////////////////////////////////////////////////////////////////

bool WinSocketService::ConnectExistingBackend(int timeoutMs)
{
    // Reuse connected socket or wait briefly for existing daemon
    if (m_socket.state() == QLocalSocket::ConnectedState)
    {
        return true;
    }

    if (m_socket.state() != QLocalSocket::ConnectingState)
    {
        m_socket.connectToServer(QString::fromLatin1(WIN_SOCKET_SERVER_NAME));
    }

    if (m_socket.waitForConnected(timeoutMs))
    {
        return true;
    }

    m_socket.abort();
    return false;
}

/////////////////////////////////////////////////////////////////////

void WinSocketService::SendRequest(const QString &method, const QJsonObject &params)
{
    // Send newline-delimited JSON request when socket is connected
    if (m_socket.state() != QLocalSocket::ConnectedState)
    {
        return;
    }

    QJsonObject request{{"type", "request"}, {"id", static_cast<qint64>(m_nextRequestId++)}, {"method", method}};
    for (auto it = params.begin(); it != params.end(); ++it)
    {
        request.insert(it.key(), it.value());
    }

    m_socket.write(QJsonDocument(request).toJson(QJsonDocument::Compact) + '\n');
    m_socket.flush();
}

/////////////////////////////////////////////////////////////////////

void WinSocketService::HandleMessage(const QJsonObject &message)
{
    // Update service state from responses and forward backend events
    if (message.value("type") == "response")
    {
        if (!message.value("ok").toBool())
        {
            SetLastError(message.value("error").toString());
        }
        else if (message.value("result").toString() == "pong")
        {
            m_startTimeoutTimer.stop();
            SetServiceStarting(false);
            SetServiceActive(true);
            SetServiceAvailable(true);
            FetchServiceEnabledStatus();
        }
        return;
    }

    if (message.value("event") == "AchievementUnlocked")
    {
        qDebug() << "WinSocketService::HandleMessage: received AchievementUnlocked event";
        emit signalAchievementUnlocked(message.value("targetId").toInt(), message.value("key").toString());
    }

    if (message.value("event") == "GameStateChanged")
    {
        qDebug() << "WinSocketService::HandleMessage: received GameStateChanged event";
        const bool active = message.value("state").toString() == "Active";
        for (const QJsonValue& value : message.value("targetIds").toArray())
        {
            const QVariant targetId = value.toInt(); const int index = m_activeTargetIds.indexOf(targetId);
            if (active && index < 0)
            {
                m_activeTargetIds.append(targetId);
            }
            if (!active && index >= 0)
            {
                m_activeTargetIds.removeAt(index);
            }
        }
        emit signalActiveTargetIdsChanged();
    }

    if (message.value("event") == "TargetDataChanged")
    {
        qDebug() << "WinSocketService::HandleMessage: received TargetDataChanged event";
        const int targetId = message.value("targetId").toInt();
        // Emit after delay, so possible unlocked achievements are also updated
        QTimer::singleShot(3000, this, [this, targetId]() {
            emit signalTargetDataChanged(targetId);
        });
    }

    if (message.value("event") == "ManualAchievementDataScanFinished")
    {
        qDebug() << "WinSocketService::HandleMessage: received ManualAchievementDataScanFinished event";
        emit signalManualAchievementDataScanFinished(message.value("targetId").toInt(), message.value("found").toBool(), message.value("reason").toString());
    }
}

/////////////////////////////////////////////////////////////////////

bool WinSocketService::FetchServiceEnabledStatus()
{
    // Read persistent autostart state from the current user's Run key
    QSettings autostartSettings(QStringLiteral(WIN_AUTOSTART_REGISTRY_KEY), QSettings::NativeFormat);
    const bool enabled = autostartSettings.contains(QStringLiteral(WIN_AUTOSTART_VALUE_NAME));
    if (autostartSettings.status() != QSettings::NoError)
    {
        SetLastError(QStringLiteral("Could not read Windows startup registration."));
        SetServiceEnabledState(false);
        return false;
    }

    SetServiceEnabledState(enabled);
    return true;
}

/////////////////////////////////////////////////////////////////////

QString WinSocketService::GetDaemonExecutablePath() const
{
    // Resolve daemon from the installed frontend directory
    return QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("lymalinkd.exe"));
}

/////////////////////////////////////////////////////////////////////

void WinSocketService::SetServiceAvailable(bool available)
{
    if (m_serviceAvailable == available)
    {
        return;
    }

    // Clear stale active targets when daemon disappears
    m_serviceAvailable = available;
    qDebug() << "WinSocketService::SetServiceAvailable: lymalinkd socket status:" << (available ? "available" : "not available");
    if (!available && !m_activeTargetIds.empty())
    {
        m_activeTargetIds.clear();
        emit signalActiveTargetIdsChanged();
    }
    if (available)
    {
        SendRequest(QStringLiteral("RequestActiveTargets"));
    }
    emit signalServiceAvailabilityChanged();
}

/////////////////////////////////////////////////////////////////////

void WinSocketService::SetServiceActive(bool active)
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

void WinSocketService::SetServiceEnabledState(bool enabled)
{
    if (m_serviceEnabled == enabled)
    {
        return;
    }

    // Notify bindings when persistent autostart state changes
    m_serviceEnabled = enabled;
    emit signalServiceStatusChanged();
}

/////////////////////////////////////////////////////////////////////

void WinSocketService::SetServiceStarting(bool starting)
{
    if (m_serviceStarting == starting)
    {
        return;
    }

    // Notify bindings when startup transition changes
    m_serviceStarting = starting;
    emit signalServiceStatusChanged();
}

/////////////////////////////////////////////////////////////////////

void WinSocketService::SetLastError(const QString &error)
{
    if (m_lastError == error)
    {
        return;
    }

    // Avoid duplicate error notifications for same message
    qDebug() << "WinSocketService::SetLastError:" << error;
    m_lastError = error;
    emit signalLastErrorChanged();
}
