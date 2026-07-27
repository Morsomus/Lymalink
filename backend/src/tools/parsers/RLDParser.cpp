/////////////////////////////////////////////////////////
// File: RLDParser.cpp
// Date: 2026-07-27
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Implements parser for Reloaded INI
//              achievement files
/////////////////////////////////////////////////////////

#include "RLDParser.h"
#include "../Utils.h"

#include <cctype>
#include <cstdint>
#include <fstream>
#include <string>

/////////////////////////////////////////////////////////////////////

RLDParser::RLDParser()
{
    // Constructor
}

RLDParser::~RLDParser()
{
    // Destructor
}

/////////////////////////////////////////////////////////////////////
////////////////////////////// PUBLIC ///////////////////////////////
/////////////////////////////////////////////////////////////////////

std::string RLDParser::GetFileName() const
{
    return "achievements.ini";
}

/////////////////////////////////////////////////////////////////////

std::vector<AchievementData> RLDParser::Parse(const std::string& filePath)
{
    std::vector<AchievementData> results;

    std::ifstream file(filePath);
    if (!file.is_open())
    {
        return results;
    }

    AchievementData current{};
    bool inSection = false;
    std::string line;

    // RLD stores values as hex bytes without separators
    auto IsHexString = [](const std::string& value)
    {
        if (value.empty())
        {
            return false;
        }

        for (const char c : value)
        {
            if (!std::isxdigit(static_cast<unsigned char>(c)))
            {
                return false;
            }
        }

        return true;
    };

    // RLD values can be 5 bytes - first 4 bytes hold little-endian uint32, final byte is ignored
    auto ParseRldNumber = [&IsHexString](const std::string& rawValue)
    {
        const std::string value = Utils::TrimWhitespace(rawValue);
        if ((value.size() == 10 || value.size() == 8) && IsHexString(value))
        {
            return static_cast<int64_t>(Utils::ParseHexLeUint32(value.substr(0, 8)));
        }

        return Utils::ToInt64(value);
    };

    // Steam section contains file metadata, not achievement records
    auto IsSkippedSection = [](const std::string& name)
    {
        return Utils::ToLower(name) == "steam";
    };

    // RLD marks completed achievements through timestamp + progress, State is ignored
    auto ApplyAchievementRule = [](AchievementData& achievement)
    {
        if (achievement.unlockTime <= 0)
        {
            achievement.achieved = false;
            return;
        }
        const bool zeroOfZeroProgress = achievement.hasCurProgress && achievement.hasMaxProgress && achievement.curProgress == 0 && achievement.maxProgress == 0;
        const bool completedProgress = achievement.hasCurProgress && achievement.hasMaxProgress && achievement.maxProgress > 0 && achievement.curProgress >= achievement.maxProgress;
        achievement.achieved = zeroOfZeroProgress || completedProgress;
    };

    // Store completed section before moving to next INI section
    auto FlushCurrent = [&]()
    {
        if (inSection && !current.key.empty())
        {
            ApplyAchievementRule(current);
            results.push_back(current);
        }
    };

    while (std::getline(file, line))
    {
        // Strip trailing CR and spaces
        line = Utils::TrimWhitespace(line);
        if (line.empty() || line.front() == ';' || line.front() == '#')
        {
            continue;
        }

        // Section header: [ACH_KEY]
        if (line.size() >= 3 && line.front() == '[' && line.back() == ']')
        {
            FlushCurrent();
            current = {};
            current.key = Utils::TrimWhitespace(line.substr(1, line.size() - 2));
            inSection = !current.key.empty() && !IsSkippedSection(current.key);
            continue;
        }

        if (!inSection)
        {
            continue;
        }

        // Key=Value pair, split on first '=' only
        const size_t separator = line.find('=');
        if (separator == std::string::npos)
        {
            continue;
        }

        const std::string key = Utils::TrimWhitespace(line.substr(0, separator));
        const std::string value = Utils::TrimWhitespace(line.substr(separator + 1));

        if (key == "Time")
        {
            current.unlockTime = ParseRldNumber(value);
        }
        else if (key == "CurProgress")
        {
            current.curProgress = static_cast<int>(ParseRldNumber(value));
            current.hasCurProgress = true;
        }
        else if (key == "MaxProgress")
        {
            current.maxProgress = static_cast<int>(ParseRldNumber(value));
            current.hasMaxProgress = true;
        }

        // TODO: Add State parsing
    }

    FlushCurrent();
    return results;
}
