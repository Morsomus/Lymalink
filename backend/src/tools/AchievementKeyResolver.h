/////////////////////////////////////////////////////////
// File: AchievementKeyResolver.h
// Date: 2026-08-01
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Declares achievement key resolver helpers
/////////////////////////////////////////////////////////

#pragma once

#include "../database/SQLiteManager.h"

#include <string>
#include <unordered_map>

/////////////////////////////////////////////////////////////////////

class AchievementKeyResolver
{
public:
    AchievementKeyResolver(SQLiteManager& database, std::string connectionName);
    ~AchievementKeyResolver();

    void PrepareTargetKeys(int targetId);
    std::string ResolveKey(int targetId, const std::string& achievementKey);
    bool ShouldIgnoreUnresolvedKey(int targetId, const std::string& achievementKey);

private:
    SQLiteManager& m_database;
    std::string m_connectionName;
    std::unordered_map<int, std::unordered_map<std::string, std::string>> m_keyMap;
};
