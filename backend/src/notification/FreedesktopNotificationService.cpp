/////////////////////////////////////////////////////////
// File: FreedesktopNotificationService.cpp
// Date: 2026-05-25
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Implements native desktop notifications
//              over D-Bus
/////////////////////////////////////////////////////////

#include "FreedesktopNotificationService.h"
#include "tools/Logger.h"

#include <map>
#include <string>
#include <vector>

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
        Logger::Log("[FreedesktopNotificationService][Init] Init failed: " + std::string(e.what()));
        m_notificationsProxy.reset();
        m_connection.reset();
        err = Error::UnknownError;
        return err;
    }

    Logger::Log("[FreedesktopNotificationService][Init] Ready.");
    return err;
}

/////////////////////////////////////////////////////////////////////

void FreedesktopNotificationService::Stop()
{
    m_notificationsProxy.reset();
    m_connection.reset();
}

/////////////////////////////////////////////////////////////////////

bool FreedesktopNotificationService::ShowAchievementToast(const AchievementNotification& notification)
{
    bool notificationSent = false;

    if (!m_notificationsProxy)
    {
        return notificationSent;
    }

    // Build user-visible notification fields with fallbacks
    const std::string summary = notification.achievementName.empty() ? "Achievement unlocked" : notification.achievementName;
    const std::string body = notification.achievementDescription.empty() ? notification.gameName : notification.achievementDescription;
    const std::string appIcon = notification.appIconPath.empty() ? "lymalink" : notification.appIconPath;

    // Pass desktop-entry and optional image as freedesktop notification hints
    std::map<std::string, sdbus::Variant> hints;
    hints.emplace("desktop-entry", sdbus::Variant{std::string{"lymalink"}});
    if (!notification.iconPath.empty())
    {
        hints.emplace("image-path", sdbus::Variant{notification.iconPath});
    }

    const std::vector<std::string> actions;
    const int32_t expireTimeoutMs = 4000;
    uint32_t notificationId = 0;

    try
    {
        // Send Notify method call to desktop notification daemon
        m_notificationsProxy->callMethod(sdbus::MethodName{"Notify"})
            .onInterface(sdbus::InterfaceName{"org.freedesktop.Notifications"})
            .withArguments(
                std::string{"Lymalink"},
                uint32_t{0},
                appIcon,
                summary,
                body,
                actions,
                hints,
                expireTimeoutMs
            )
            .storeResultsTo(notificationId);

        Logger::Log("[FreedesktopNotificationService][ShowAchievementToast] Notification sent: targetId=" + std::to_string(notification.targetId) + " key=" + notification.achievementKey);
        notificationSent = true;
        return notificationSent;
    }
    catch (const sdbus::Error& e)
    {
        Logger::Log("[FreedesktopNotificationService][ShowAchievementToast] Notify failed: " + std::string(e.what()));
        return notificationSent;
    }
}
