/////////////////////////////////////////////////////////
// File: FreedesktopNotificationService.cpp
// Date: 2026-05-25
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Implements native Linux desktop notifications
//              over D-Bus
/////////////////////////////////////////////////////////

#include "FreedesktopNotificationService.h"
#include "Defines.h"
#include "tools/Logger.h"

#include <map>
#include <string>
#include <vector>

#define COMPONENT "FreedesktopNotificationService"

/////////////////////////////////////////////////////////////////////

FreedesktopNotificationService::FreedesktopNotificationService()
{
    m_connection = nullptr;
    m_notificationsProxy = nullptr;
}

FreedesktopNotificationService::~FreedesktopNotificationService()
{
    Stop();
}

/////////////////////////////////////////////////////////////////////
////////////////////////////// PUBLIC ///////////////////////////////
/////////////////////////////////////////////////////////////////////

Error FreedesktopNotificationService::Init()
{
    Error err = Error::NoError;

    try
    {
        // Connect to session bus used by desktop notification service
        m_connection = sdbus::createSessionBusConnection();

        // Create proxy for org.freedesktop.Notifications service
        m_notificationsProxy = sdbus::createProxy(
            *m_connection,
            sdbus::ServiceName{"org.freedesktop.Notifications"},
            sdbus::ObjectPath{"/org/freedesktop/Notifications"}
        );
    }
    catch (const sdbus::Error& e)
    {
        LOG_BE(Urgency::Critical, "Init failed: %s", e.what());
        m_notificationsProxy.reset();
        m_connection.reset();
        err = Error::UnknownError;
        return err;
    }

    LOG_BE(Urgency::Debug, "Ready.");
    return err;
}

/////////////////////////////////////////////////////////////////////

void FreedesktopNotificationService::Stop()
{
    m_notificationsProxy.reset();
    m_connection.reset();
    LOG_BE(Urgency::Debug, "Stopped and D-Bus proxy released.");
}

/////////////////////////////////////////////////////////////////////

bool FreedesktopNotificationService::ShowErrorToast(const std::string& summary, const std::string& body, const std::string& iconPath)
{
    bool notificationSent = false;

    if (!m_notificationsProxy)
    {
        LOG_BE(Urgency::Warning, "Cannot show error toast: service proxy not initialized.");
        return notificationSent;
    }

    std::map<std::string, sdbus::Variant> hints;
    hints.emplace("desktop-entry", sdbus::Variant{std::string{"lymalink"}});
    hints.emplace("urgency", sdbus::Variant{uint8_t{2}});

    const std::vector<std::string> actions;
    const int32_t expireTimeoutMs = 0;
    uint32_t notificationId = 0;

    try
    {
        m_notificationsProxy->callMethod("Notify")
            .onInterface("org.freedesktop.Notifications")
            .withArguments(
                std::string{"Lymalink"},
                uint32_t{0},
                iconPath,
                summary,
                body,
                actions,
                hints,
                expireTimeoutMs
            )
            .storeResultsTo(notificationId);

        LOG_BE(Urgency::Debug, "Error notification sent successfully (ID: %u).", notificationId);
        notificationSent = true;
        return notificationSent;
    }
    catch (const sdbus::Error& e)
    {
        LOG_BE(Urgency::Critical, "Notify error method call failed: %s", e.what());
        return notificationSent;
    }
}
