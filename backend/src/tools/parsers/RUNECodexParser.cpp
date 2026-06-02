/////////////////////////////////////////////////////////
// File: RUNECodexParser.cpp
// Date: 2026-05-26
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Implements parser, INI achievement files
//
// Primary format (RLD! style) - hex uint32 LE values:
//  [ACHIEVEMENT_KEY]
//  State=01000000
//  CurProgress=00000000
//  MaxProgress=00000000
//  Time=12AB34CD
//
// Simpler variant - decimal values:
//  [ACHIEVEMENT_KEY]
//  Achieved=1
//  CurProgress=0
//  MaxProgress=0
//  UnlockTime=0000000000
/////////////////////////////////////////////////////////

#include "RUNECodexParser.h"
#include "../Utils.h"

#include <fstream>
#include <string>

/////////////////////////////////////////////////////////////////////

RUNECodexParser::RUNECodexParser()
{
    // Constructor
}

RUNECodexParser::~RUNECodexParser()
{
    // Destructor
}

/////////////////////////////////////////////////////////////////////
////////////////////////////// PUBLIC ///////////////////////////////
/////////////////////////////////////////////////////////////////////

std::string RUNECodexParser::GetFileName() const
{
    return "achievements.ini";
}

/////////////////////////////////////////////////////////////////////

std::vector<AchievementData> RUNECodexParser::Parse(const std::string& filePath)
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

    auto FlushCurrent = [&]()
    {
        if (inSection && !current.key.empty())
        {
            results.push_back(current);
        }
    };

    auto IsSkippedSection = [](const std::string& name)
    {
        const std::string lowerName = Utils::ToLower(name);
        return lowerName == "steamachievements" || lowerName == "steam64" || lowerName == "steam";
    };

    while (std::getline(file, line))
    {
        // Strip trailing CR and spaces
        line = Utils::TrimWhitespace(line);

        if (line.empty() || line.front() == ';')
        {
            continue;
        }

        // Section header: [ACH_KEY]
        if (line.size() >= 3 && line.front() == '[' && line.back() == ']')
        {
            FlushCurrent();
            current = {};
            current.key = line.substr(1, line.size() - 2);
            if (IsSkippedSection(current.key))
            {
                inSection = false;
                continue;
            }
            inSection = true;
            continue;
        }

        if (!inSection)
        {
            continue;
        }

        // Key=Value pair, split on first '=' only
        const size_t eq = line.find('=');
        if (eq == std::string::npos)
        {
            continue;
        }

        const std::string key = line.substr(0, eq);
        const std::string val = line.substr(eq + 1);

        // Achieved - primary: State (hex LE, 1 = unlocked)
        //            possible alternatives: Achieved / HaveAchieved / Unlocked / unlocked / earned / achieved / value
        if (key == "State")
        {
            current.achieved = Utils::IsHexLeUint32(val) ? (Utils::ParseHexLeUint32(val) == 1) : (val == "1");
        }
        else if (key == "Achieved" ||
                key == "achieved" ||
                key == "HaveAchieved" ||
                key == "Unlocked" ||
                key == "unlocked" ||
                key == "earned" ||
                key == "value")
        {
            current.achieved = (std::stoi(val) == 1);
        }
        // UnlockTime - primary: Time (hex LE)
        //              possible alternatives: UnlockTime / unlocktime / unlock_time / HaveAchievedTime / HaveHaveAchievedTime / earned_time
        else if (key == "Time")
        {
            current.unlockTime = Utils::IsHexLeUint32(val) ? static_cast<int64_t>(Utils::ParseHexLeUint32(val)) : std::stoll(val);
        }
        else if (key == "UnlockTime" ||
                key == "unlocktime" ||
                key == "unlock_time" ||
                key == "HaveAchievedTime" ||
                key == "HaveHaveAchievedTime" ||
                key == "earned_time")
        {
            current.unlockTime = std::stoll(val);
        }
        // CurProgress, hex LE or decimal
        else if (key == "CurProgress" || key == "progress")
        {
            current.curProgress = Utils::IsHexLeUint32(val) ? static_cast<int>(Utils::ParseHexLeUint32(val)) : std::stoi(val);
        }
        // MaxProgress, hex LE or decimal
        else if (key == "MaxProgress" || key == "max_progress")
        {
            current.maxProgress = Utils::IsHexLeUint32(val) ? static_cast<int>(Utils::ParseHexLeUint32(val)) : std::stoi(val);
        }
    }

    // File end
    FlushCurrent();
    return results;
}
