/////////////////////////////////////////////////////////
// File: WinNotificationService.h
// Date: 2026-06-20
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Declares reserved Windows notification adapter
//              NOTE: Not used in project currently, left for future use
/////////////////////////////////////////////////////////

#pragma once

#include "notification/AchievementNotificationService.h"

class WinNotificationService : public IDesktopNotificationService
{
public:
    WinNotificationService();
    ~WinNotificationService() override;

    bool ShowAchievementToast(const AchievementNotification& notification) override;
};
