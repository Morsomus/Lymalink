/////////////////////////////////////////////////////////
// File: WindowsSocketService.cpp
// Date: 2026-06-19
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Implements Windows backend-control placeholder
//              for future local sockets.
/////////////////////////////////////////////////////////

#include "WindowsSocketService.h"

#include <QDebug>


#define WINDOWS_IPC_UNAVAILABLE "Windows backend IPC is not implemented yet. Install a build with a compatible lymalinkd.exe."

/////////////////////////////////////////////////////////////////////

WindowsSocketService::WindowsSocketService(QObject *parent) : BackendControl(parent)
{
    ReportUnsupported();
}

WindowsSocketService::~WindowsSocketService()
{
    // Destructor
}

/////////////////////////////////////////////////////////////////////
////////////////////////////// PUBLIC ///////////////////////////////
/////////////////////////////////////////////////////////////////////

bool WindowsSocketService::StopServiceIfNotEnabled()
{
    ReportUnsupported();
    return false;
}

/////////////////////////////////////////////////////////////////////

void WindowsSocketService::PingBackend()
{
    ReportUnsupported();
}

/////////////////////////////////////////////////////////////////////

bool WindowsSocketService::StartService()
{
    ReportUnsupported();
    return false;
}

/////////////////////////////////////////////////////////////////////

bool WindowsSocketService::StopService()
{
    ReportUnsupported();
    return false;
}

/////////////////////////////////////////////////////////////////////

bool WindowsSocketService::RestartService()
{
    ReportUnsupported();
    return false;
}

/////////////////////////////////////////////////////////////////////

bool WindowsSocketService::SetServiceEnabled(bool enabled)
{
    Q_UNUSED(enabled);
    ReportUnsupported();
    return false;
}

/////////////////////////////////////////////////////////////////////

bool WindowsSocketService::RefreshServiceStatus()
{
    ReportUnsupported();
    return false;
}

/////////////////////////////////////////////////////////////////////

void WindowsSocketService::ReloadAllTargets()
{
    ReportUnsupported();
}

/////////////////////////////////////////////////////////////////////

void WindowsSocketService::ReloadConfig()
{
    ReportUnsupported();
}

/////////////////////////////////////////////////////////////////////

void WindowsSocketService::TestToast()
{
    ReportUnsupported();
}

/////////////////////////////////////////////////////////////////////

void WindowsSocketService::TestSound()
{
    ReportUnsupported();
}

/////////////////////////////////////////////////////////////////////
//////////////////////////// PRIVATE ///////////////////////////////
/////////////////////////////////////////////////////////////////////

void WindowsSocketService::ReportUnsupported()
{
    SetServiceStarting(false);
    SetServiceActive(false);
    SetServiceAvailable(false);
    SetServiceEnabledState(false);
    SetLastError(WINDOWS_IPC_UNAVAILABLE);
    qWarning() << "WindowsSocketService:" << WINDOWS_IPC_UNAVAILABLE;
}

/////////////////////////////////////////////////////////////////////

void WindowsSocketService::SetServiceAvailable(bool available)
{
    if (m_serviceAvailable == available)
    {
        return;
    }

    m_serviceAvailable = available;
    if (!available && !m_activeTargetIds.empty())
    {
        m_activeTargetIds.clear();
        emit signalActiveTargetIdsChanged();
    }
    emit signalServiceAvailabilityChanged();
}

/////////////////////////////////////////////////////////////////////

void WindowsSocketService::SetServiceActive(bool active)
{
    if (m_serviceActive == active)
    {
        return;
    }

    m_serviceActive = active;
    emit signalServiceStatusChanged();
}

/////////////////////////////////////////////////////////////////////

void WindowsSocketService::SetServiceEnabledState(bool enabled)
{
    if (m_serviceEnabled == enabled)
    {
        return;
    }

    m_serviceEnabled = enabled;
    emit signalServiceStatusChanged();
}

/////////////////////////////////////////////////////////////////////

void WindowsSocketService::SetServiceStarting(bool starting)
{
    if (m_serviceStarting == starting)
    {
        return;
    }

    m_serviceStarting = starting;
    emit signalServiceStatusChanged();
}

/////////////////////////////////////////////////////////////////////

void WindowsSocketService::SetLastError(const QString &error)
{
    if (m_lastError == error)
    {
        return;
    }

    m_lastError = error;
    emit signalLastErrorChanged();
}
