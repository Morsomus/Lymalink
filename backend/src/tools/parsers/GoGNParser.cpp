/////////////////////////////////////////////////////////
// File: GoGNParser.cpp
// Date: 2026-06-13
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Implements parser for Nemirtingas GOG
//              achievement JSON files
/////////////////////////////////////////////////////////

#include "GoGNParser.h"
#include "../Utils.h"

#include <nlohmann/json.hpp>
#include <fstream>

/////////////////////////////////////////////////////////////////////

GoGNParser::GoGNParser()
{
    // Constructor
}

GoGNParser::~GoGNParser()
{
    // Destructor
}

/////////////////////////////////////////////////////////////////////
////////////////////////////// PUBLIC ///////////////////////////////
/////////////////////////////////////////////////////////////////////

std::string GoGNParser::GetFileName() const
{
    return "achievements.json";
}

/////////////////////////////////////////////////////////////////////

std::vector<AchievementData> GoGNParser::Parse(const std::string& filePath)
{
    std::ifstream file(filePath);
    if (!file.is_open())
    {
        return {};
    }

    nlohmann::json root;
    try
    {
        root = nlohmann::json::parse(file);
    }
    catch (const nlohmann::json::parse_error&)
    {
        return {};
    }

    std::vector<AchievementData> results;

    if (root.is_array())
    {
        for (const auto& entry : root)
        {
            if (!entry.is_object())
            {
                continue;
            }

            const auto nameIt = entry.find("name");
            const auto timeIt = entry.find("earned_time");
            if (nameIt == entry.end() || timeIt == entry.end() || !nameIt->is_string())
            {
                continue;
            }

            int64_t unlockTime = 0;
            if (timeIt->is_number_integer())
            {
                unlockTime = timeIt->get<int64_t>();
            }
            else if (timeIt->is_string())
            {
                unlockTime = Utils::ToInt64(timeIt->get<std::string>());
            }

            if (unlockTime <= 0)
            {
                continue;
            }

            AchievementData data{};
            data.key = nameIt->get<std::string>();
            data.achieved = true;
            data.unlockTime = unlockTime;
            results.push_back(data);
        }

        return results;
    }

    if (!root.is_object())
    {
        return results;
    }

    for (const auto& [key, entry] : root.items())
    {
        if (!entry.is_object())
        {
            continue;
        }

        int64_t unlockTime = 0;
        for (const char* timeKey : {"unlock_time", "unlockTime", "unlock_date"})
        {
            const auto timeIt = entry.find(timeKey);
            if (timeIt == entry.end())
            {
                continue;
            }

            if (timeIt->is_number_integer())
            {
                unlockTime = timeIt->get<int64_t>();
            }
            else if (timeIt->is_string())
            {
                unlockTime = Utils::ToInt64(timeIt->get<std::string>());
            }

            if (unlockTime > 0)
            {
                break;
            }
        }

        if (unlockTime <= 0)
        {
            continue;
        }

        AchievementData data{};
        data.key = key;
        data.achieved = true;
        data.unlockTime = unlockTime;
        results.push_back(data);
    }

    return results;
}
