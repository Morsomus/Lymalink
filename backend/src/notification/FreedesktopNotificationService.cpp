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
    try
    {
        m_connection = sdbus::createSessionBusConnection();
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
        return Error::UnknownError;
    }

    Logger::Log("[FreedesktopNotificationService][Init] Ready.");
    return Error::NoError;
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
    if (!m_notificationsProxy)
    {
        return false;
    }

    const std::string summary = notification.achievementName.empty() ? "Achievement unlocked" : notification.achievementName;
    const std::string body = notification.achievementDescription.empty() ? notification.gameName : notification.achievementDescription;
    const std::string appIcon = notification.appIconPath.empty() ? "lymalink" : notification.appIconPath;

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
        return true;
    }
    catch (const sdbus::Error& e)
    {
        Logger::Log("[FreedesktopNotificationService][ShowAchievementToast] Notify failed: " + std::string(e.what()));
        return false;
    }
}
