/////////////////////////////////////////////////////////
// File: TenokeParser.cpp
// Date: 2026-08-02
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Implements parser for Tenoke achievement files
//
// Format:
// [STATS]
// ...
// [ACHIEVEMENTS]
// "achievement_key" = {unlocked = true, time = 1785652344}
/////////////////////////////////////////////////////////

#include "TenokeParser.h"
#include "../Utils.h"

#include <fstream>
#include <regex>

/////////////////////////////////////////////////////////////////////

TenokeParser::TenokeParser()
{
    // Constructor
}

TenokeParser::~TenokeParser()
{
    // Destructor
}

/////////////////////////////////////////////////////////////////////
////////////////////////////// PUBLIC ///////////////////////////////
/////////////////////////////////////////////////////////////////////

std::string TenokeParser::GetFileName() const
{
    return "user_stats.ini";
}

/////////////////////////////////////////////////////////////////////

std::vector<AchievementData> TenokeParser::Parse(const std::string& filePath)
{
    std::vector<AchievementData> results;

    std::ifstream file(filePath);
    if (!file.is_open())
    {
        return results;
    }

    static const std::regex achievementPattern(
        R"TENOKE(^\s*"([^"]+)"\s*=\s*\{\s*unlocked\s*=\s*(true|false|1|0)\s*,\s*time\s*=\s*([0-9]+)\s*\}\s*$)TENOKE",
        std::regex::icase
    );

    bool inAchievements = false;
    std::string line;
    while (std::getline(file, line))
    {
        line = Utils::TrimWhitespace(line);
        if (line.empty() || line[0] == ';' || line[0] == '#')
        {
            continue;
        }

        if (line.front() == '[' && line.back() == ']')
        {
            inAchievements = Utils::TrimWhitespace(line.substr(1, line.size() - 2)) == "ACHIEVEMENTS";
            continue;
        }

        if (!inAchievements)
        {
            continue;
        }

        std::smatch match;
        if (!std::regex_match(line, match, achievementPattern))
        {
            continue;
        }

        const std::string key = match[1].str();
        if (key.empty())
        {
            continue;
        }

        AchievementData data{};
        data.key = key;
        data.achieved = Utils::ToLower(match[2].str()) == "true" || match[2].str() == "1";
        data.unlockTime = Utils::ToInt64(match[3].str());
        results.push_back(data);
    }

    return results;
}
