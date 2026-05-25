/////////////////////////////////////////////////////////
// File: AchievementNotificationService.h
// Date: 2026-05-25
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Declares building and sending achievement
//              unlock notifications
/////////////////////////////////////////////////////////

#pragma once

#include "database/SQLiteManager.h"
#include "notification/ISoundService.h"

#include <cstdint>
#include <string>

struct AchievementNotification
{
    int32_t targetId = 0;
    std::string achievementKey;
    std::string achievementName;
    std::string achievementDescription;
    std::string gameName;
    std::string iconPath;
    std::string appIconPath;
};

class IDesktopNotificationService
{
public:
    virtual ~IDesktopNotificationService() = default;
    virtual bool ShowAchievementToast(const AchievementNotification& notification) = 0;
};

class AchievementNotificationService
{
public:
    AchievementNotificationService(SQLiteManager& database, IDesktopNotificationService& notifier, ISoundService& soundService);
    ~AchievementNotificationService();

    void Configure(std::string connectionName, std::string appDataPath);
    bool NotifyUnlocked(int32_t targetId, const std::string& achievementKey);

private:
    SQLiteManager& m_database;
    IDesktopNotificationService& m_notifier;
    ISoundService& m_soundService;
    std::string m_connectionName;
    std::string m_appDataPath;

    bool BuildNotification(int32_t targetId, const std::string& achievementKey, AchievementNotification& notification);
};
