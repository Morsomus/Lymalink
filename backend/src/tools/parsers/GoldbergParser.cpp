/////////////////////////////////////////////////////////
// File: GoldbergParser.cpp
// Date: 2026-06-02
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Implements parser, parses JSON achievement files
//
// Primary achievement JSON format:
// {
//   "ACHIEVEMENT_API_NAME": {
//     "earned": true,
//     "earned_time": 1712345678
//   }
// }
//
// Extended JSON fields:
// {
//   "ACHIEVEMENT_API_NAME": {
//     "earned": true,
//     "earned_time": 1712345678,
//     "progress": 5,
//     "max_progress": 10
//   }
// }
//
// Legacy/Fallback INI format:
// [ACHIEVEMENT_API_NAME]
// HaveAchieved=1
// HaveAchievedTime=1712345678
//
// Legacy/Fallback INI format secondary:
// [ACHIEVEMENT_API_NAME]
// HaveAchieved=1
// HaveHaveAchievedTime=1712345678
/////////////////////////////////////////////////////////

#include "GoldbergParser.h"
#include "../Utils.h"

#include <nlohmann/json.hpp>
#include <fstream>
#include <istream>

/////////////////////////////////////////////////////////////////////

GoldbergParser::GoldbergParser()
{
    // Constructor
}

GoldbergParser::~GoldbergParser()
{
    // Destructor
}

/////////////////////////////////////////////////////////////////////
////////////////////////////// PUBLIC ///////////////////////////////
/////////////////////////////////////////////////////////////////////

std::string GoldbergParser::GetFileName() const
{
    return "achievements.json";
}

/////////////////////////////////////////////////////////////////////

std::vector<AchievementData> GoldbergParser::Parse(const std::string& filePath)
{
    std::ifstream file(filePath);
    if (!file.is_open())
    {
        return {};
    }

    nlohmann::json root;
    try
    {
        // Attempt to parse as JSON
        root = nlohmann::json::parse(file);
    }
    catch (const nlohmann::json::parse_error&)
    {
        file.clear();
        file.seekg(0, std::ios::beg);
        return ParseIni(file); // Fallback to INI parsing
    }

    if (!root.is_object())
    {
        return {};
    }

    std::vector<AchievementData> results;

    for (const auto& [key, entry] : root.items())
    {
        if (!entry.is_object())
        {
            continue;
        }

        AchievementData data{};
        data.key = key;

        // Extract earned status
        if (const auto it = entry.find("earned"); it != entry.end())
        {
            if (it->is_boolean())
            {
                data.achieved = it->get<bool>();
            }
            else if (it->is_number_integer())
            {
                data.achieved = it->get<int>() != 0;
            }
            else if (it->is_string())
            {
                const std::string value = Utils::ToLower(it->get<std::string>());
                data.achieved = value == "1" || value == "true" || value == "yes";
            }
        }

        // Extract unlock timestamp
        if (const auto it = entry.find("earned_time"); it != entry.end())
        {
            if (it->is_number_integer())
            {
                data.unlockTime = it->get<int64_t>();
            }
            else if (it->is_string())
            {
                data.unlockTime = Utils::ToInt64(it->get<std::string>());
            }
        }

        // Extract current progress
        if (const auto it = entry.find("progress"); it != entry.end())
        {
            if (it->is_number_integer())
            {
                data.curProgress = it->get<int>();
                data.hasCurProgress = true;
            }
            else if (it->is_string())
            {
                data.curProgress = static_cast<int>(Utils::ToInt64(it->get<std::string>()));
                data.hasCurProgress = true;
            }
        }

        // Extract maximum progress
        if (const auto it = entry.find("max_progress"); it != entry.end())
        {
            if (it->is_number_integer())
            {
                data.maxProgress = it->get<int>();
                data.hasMaxProgress = true;
            }
            else if (it->is_string())
            {
                data.maxProgress = static_cast<int>(Utils::ToInt64(it->get<std::string>()));
                data.hasMaxProgress = true;
            }
        }

        results.push_back(data);    // Add parsed achievement to results
    }

    return results;
}

/////////////////////////////////////////////////////////////////////
///////////////////////////// PRIVATE ///////////////////////////////
/////////////////////////////////////////////////////////////////////

std::vector<AchievementData> GoldbergParser::ParseIni(std::istream& file)
{
    std::vector<AchievementData> results;

    AchievementData current{};
    bool hasSection = false;

    // Make sure if there is no non-wanted sections and skip them
    auto IsSkippedSection = [](const std::string& name)
    {
        const std::string lowerName = Utils::ToLower(name);
        return lowerName == "steamachievements" || lowerName == "steam64" || lowerName == "steam";
    };

    // Read file line by line
    std::string line;
    while (std::getline(file, line))
    {
        line = Utils::TrimWhitespace(line);

        if (line.empty() || line[0] == ';' || line[0] == '#')
        {
            continue;
        }

        // Detect INI section header with bounds check
        if (line.size() >= 3 && line.front() == '[' && line.back() == ']')
        {
            if (hasSection)
            {
                results.push_back(current);
            }

            current = AchievementData{};
            current.key = Utils::TrimWhitespace(line.substr(1, line.size() - 2));

            // Check for skippable sections
            if (IsSkippedSection(current.key))
            {
                hasSection = false;
                continue;
            }

            hasSection = !current.key.empty();
            continue;
        }

        if (!hasSection)
        {
            continue;
        }

        const std::size_t separator = line.find('=');
        if (separator == std::string::npos)
        {
            continue;
        }

        const std::string field = Utils::TrimWhitespace(line.substr(0, separator));
        const std::string value = Utils::TrimWhitespace(line.substr(separator + 1));
        if (field == "HaveAchieved")    // Parse achievement status
        {
            current.achieved = value == "1" || Utils::ToLower(value) == "true" || Utils::ToLower(value) == "yes";
        }
        else if (field == "HaveAchievedTime" || field == "HaveHaveAchievedTime")    // Parse unlock timestamp
        {
            current.unlockTime = Utils::ToInt64(value);
        }
    }

    if (hasSection)
    {
        results.push_back(current);
    }

    return results;
}
