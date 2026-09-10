/////////////////////////////////////////////////////////
// File: FreedesktopNotificationService.h
// Date: 2026-05-25
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Declares native Linux desktop notifications
//              over D-Bus
/////////////////////////////////////////////////////////

#pragma once

#include "Error.h"
#include <cstdint>
#include <memory>
#include <sdbus-c++/sdbus-c++.h>
#include <string>

class FreedesktopNotificationService
{
public:
    FreedesktopNotificationService();
    ~FreedesktopNotificationService();

    Error Init();
    void Stop();

    bool ShowErrorToast(const std::string& summary, const std::string& body, const std::string& iconPath);

private:
    std::unique_ptr<sdbus::IConnection> m_connection;
    std::unique_ptr<sdbus::IProxy> m_notificationsProxy;
};
