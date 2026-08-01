/////////////////////////////////////////////////////////
// File: AchievementKeyResolver.cpp
// Date: 2026-08-01
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Implements achievement key resolver helpers
/////////////////////////////////////////////////////////

#include "AchievementKeyResolver.h"
#include "Utils.h"
#include "Defines.h"

#include <cstdint>
#include <utility>

/////////////////////////////////////////////////////////////////////

AchievementKeyResolver::AchievementKeyResolver(SQLiteManager& database, std::string connectionName) :
    m_database(database),
    m_connectionName(std::move(connectionName))
{
    // Constructor
}

AchievementKeyResolver::~AchievementKeyResolver()
{
    // Destructor
}

/////////////////////////////////////////////////////////////////////
////////////////////////////// PUBLIC ///////////////////////////////
/////////////////////////////////////////////////////////////////////

void AchievementKeyResolver::PrepareTargetKeys(int targetId)
{
    if (targetId <= 0)
    {
        return;
    }

    std::unordered_map<std::string, std::string> keyMap;
    // Keep CRC lookup isolated per target so multiple games can run at once
    const DbRows achievements = m_database.SelectWhere(
        m_connectionName,
        DATABASE_TABLE_EMU_ACHIEVEMENTS,
        "id = ?",
        {static_cast<int64_t>(targetId)},
        {"achievement_key"}
    );

    for (const DbRow& row : achievements)
    {
        const std::string storedKey = SQLiteManager::RowString(row, "achievement_key");
        if (storedKey.empty())
        {
            continue;
        }

        const std::string crcKey = "crc32:" + Utils::ToUpperHexUint32(Utils::Crc32(storedKey));
        keyMap[Utils::ToLower(crcKey)] = storedKey;
    }

    m_keyMap[targetId] = std::move(keyMap);
}

/////////////////////////////////////////////////////////////////////

std::string AchievementKeyResolver::ResolveKey(int targetId, const std::string& achievementKey)
{
    const std::string lowerKey = Utils::ToLower(achievementKey);
    if (targetId <= 0 || lowerKey.rfind("crc32:", 0) != 0)
    {
        return achievementKey;
    }

    const auto targetIt = m_keyMap.find(targetId);
    if (targetIt != m_keyMap.end())
    {
        const auto keyIt = targetIt->second.find(lowerKey);
        if (keyIt != targetIt->second.end())
        {
            return keyIt->second;
        }
    }

    return achievementKey;
}

/////////////////////////////////////////////////////////////////////

bool AchievementKeyResolver::ShouldIgnoreUnresolvedKey(int targetId, const std::string& achievementKey)
{
    // Once a target has a prepared CRC map, unresolved CRC keys are unknown entries
    return Utils::ToLower(achievementKey).rfind("crc32:", 0) == 0 && m_keyMap.find(targetId) != m_keyMap.end();
}
