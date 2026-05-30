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
#include "../tools/Logger.h"

#include <filesystem>
#include <utility>

#define COMPONENT "AchievementNotificationService"

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
    
    LOG_BE(Urgency::Debug, "Configured service: connectionName=%s, appDataPath=%s", m_connectionName.c_str(), m_appDataPath.c_str());
}

/////////////////////////////////////////////////////////////////////

bool AchievementNotificationService::NotifyUnlocked(int32_t targetId, const std::string& achievementKey)
{
    bool notificationSent = false;

    // Build complete notification payload
    AchievementNotification notification;
    if (!BuildNotification(targetId, achievementKey, notification))
    {
        LOG_BE(Urgency::Warning, "Failed to build notification payload: targetId=%d, key=%s", targetId, achievementKey.c_str());
        return notificationSent;
    }

    // Show desktop toast and play configured sound for successful unlocks
    notificationSent = m_notifier.ShowAchievementToast(notification);
    m_soundService.PlayNotificationSound();
    
    LOG_BE(Urgency::Debug, "Notification process completed. Sent successfully: %s", notificationSent ? "true" : "false");
    return notificationSent;
}

/////////////////////////////////////////////////////////////////////
///////////////////////////// PRIVATE ///////////////////////////////
/////////////////////////////////////////////////////////////////////

bool AchievementNotificationService::BuildNotification(int32_t targetId, const std::string& achievementKey, AchievementNotification& notification)
{
    bool notificationBuilt = false;

    // Clear caller-provided output before validation or database reads
    notification = AchievementNotification{};

    if (targetId <= 0 || achievementKey.empty() || m_appDataPath.empty())
    {
        LOG_BE(Urgency::Warning, "Validation failed: targetId=%d, keyEmpty=%d, appDataPathEmpty=%d", targetId, achievementKey.empty(), m_appDataPath.empty());
        return notificationBuilt;
    }

    // Resolve per-target icon directory from configured app data path
    const fs::path iconsPath = fs::path(m_appDataPath)
        / "Emulator"
        / std::to_string(targetId)
        / "icons";

    // Load achievement details for notification title/body
    const DbRecord achievement = m_database.SelectFirst(
        m_connectionName,
        DATABASE_TABLE_EMU_ACHIEVEMENTS,
        "id = ? AND achievement_key = ?",
        {static_cast<int64_t>(targetId), achievementKey}
    );

    if (achievement.empty())
    {
        LOG_BE(Urgency::Warning, "Achievement record not found in DB: targetId=%d, key=%s", targetId, achievementKey.c_str());
        return notificationBuilt;
    }

    // Load target details for fallback notification body
    const DbRecord target = m_database.SelectFirst(
        m_connectionName,
        DATABASE_TABLE_EMU_GAMES,
        "id = ?",
        {static_cast<int64_t>(targetId)}
    );

    // Copy database fields into transport-neutral notification model
    notification.targetId = targetId;
    notification.achievementKey = achievementKey;
    notification.achievementName = SQLiteManager::RowString(achievement, "achievement_name");
    notification.achievementDescription = SQLiteManager::RowString(achievement, "achievement_description");
    notification.gameName = SQLiteManager::RowString(target, "game_name");

    const fs::path iconPath = iconsPath / (achievementKey + "_icon.jpg");

    // Attach achievement image if scraper stored it locally
    if (fs::exists(iconPath))
    {
        notification.iconPath = iconPath.string();
    }
    else
    {
        LOG_BE(Urgency::Debug, "Primary achievement icon missing from path: %s", iconPath.c_str());
    }

    const fs::path appIconPath = iconsPath / "community_icon.jpg";

    // Attach game/community image if scraper stored it locally
    if (fs::exists(appIconPath))
    {
        notification.appIconPath = appIconPath.string();
    }
    else
    {
        LOG_BE(Urgency::Debug, "Community fallback icon missing from path: %s", appIconPath.c_str());
    }

    notificationBuilt = true;
    
    LOG_BE(Urgency::Debug, "Successfully built notification for '%s' (Game: '%s')", notification.achievementName.c_str(), notification.gameName.c_str());
           
    return notificationBuilt;
}
