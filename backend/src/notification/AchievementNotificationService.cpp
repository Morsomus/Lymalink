/////////////////////////////////////////////////////////
// File: AchievementNotificationService.cpp
// Date: 2026-05-25
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Implements building and sending achievement
//              unlock notifications.
/////////////////////////////////////////////////////////

#include "AchievementNotificationService.h"
#include "Defines.h"

#include <filesystem>
#include <utility>

namespace fs = std::filesystem;

/////////////////////////////////////////////////////////////////////

AchievementNotificationService::AchievementNotificationService(SQLiteManager& database, IDesktopNotificationService& notifier, ISoundService& soundService) :
    m_database(database),
    m_notifier(notifier),
    m_soundService(soundService)
{
    m_connectionName = DATABASE_CONNECTION_NAME;
    m_appDataPath = "";
}

AchievementNotificationService::~AchievementNotificationService()
{
    // Destructor
}

/////////////////////////////////////////////////////////////////////
////////////////////////////// PUBLIC ///////////////////////////////
/////////////////////////////////////////////////////////////////////

void AchievementNotificationService::Configure(std::string connectionName, std::string appDataPath)
{
    m_connectionName = std::move(connectionName);
    m_appDataPath = std::move(appDataPath);
}

/////////////////////////////////////////////////////////////////////

bool AchievementNotificationService::NotifyUnlocked(int32_t targetId, const std::string& achievementKey)
{
    AchievementNotification notification;
    if (!BuildNotification(targetId, achievementKey, notification))
    {
        return false;
    }

    const bool notificationSent = m_notifier.ShowAchievementToast(notification);
    m_soundService.PlayNotificationSound();
    return notificationSent;
}

/////////////////////////////////////////////////////////////////////
///////////////////////////// PRIVATE ///////////////////////////////
/////////////////////////////////////////////////////////////////////

bool AchievementNotificationService::BuildNotification(int32_t targetId, const std::string& achievementKey, AchievementNotification& notification)
{
    notification = AchievementNotification{};

    if (targetId <= 0 || achievementKey.empty() || m_appDataPath.empty())
    {
        return false;
    }

    const fs::path iconsPath = fs::path(m_appDataPath)
        / "Emulator"
        / std::to_string(targetId)
        / "icons";

    const DbRecord achievement = m_database.SelectFirst(
        m_connectionName,
        DATABASE_TABLE_EMU_ACHIEVEMENTS,
        "id = ? AND achievement_key = ?",
        {static_cast<int64_t>(targetId), achievementKey}
    );

    if (achievement.empty())
    {
        return false;
    }

    const DbRecord target = m_database.SelectFirst(
        m_connectionName,
        DATABASE_TABLE_EMU_GAMES,
        "id = ?",
        {static_cast<int64_t>(targetId)}
    );

    notification.targetId = targetId;
    notification.achievementKey = achievementKey;
    notification.achievementName = SQLiteManager::RowString(achievement, "achievement_name");
    notification.achievementDescription = SQLiteManager::RowString(achievement, "achievement_description");
    notification.gameName = SQLiteManager::RowString(target, "game_name");

    const fs::path iconPath = iconsPath / (achievementKey + "_icon.jpg");

    if (fs::exists(iconPath))
    {
        notification.iconPath = iconPath.string();
    }

    const fs::path appIconPath = iconsPath / "community_icon.jpg";

    if (fs::exists(appIconPath))
    {
        notification.appIconPath = appIconPath.string();
    }

    return true;
}
