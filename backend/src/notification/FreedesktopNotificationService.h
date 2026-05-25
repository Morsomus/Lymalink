/////////////////////////////////////////////////////////
// File: FreedesktopNotificationService.h
// Date: 2026-05-25
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Declares sending native desktop
//              notifications over D-Bus
/////////////////////////////////////////////////////////

#pragma once

#include "Error.h"
#include "notification/AchievementNotificationService.h"

#include <cstdint>
#include <memory>
#include <sdbus-c++/sdbus-c++.h>

class FreedesktopNotificationService : public IDesktopNotificationService
{
public:
    FreedesktopNotificationService();
    ~FreedesktopNotificationService();

    Error Init();
    void Stop();

    bool ShowAchievementToast(const AchievementNotification& notification) override;

private:
    std::unique_ptr<sdbus::IConnection> m_connection;
    std::unique_ptr<sdbus::IProxy> m_notificationsProxy;
};
