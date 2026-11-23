/////////////////////////////////////////////////////////
// File: SovereignParser.cpp
// Date: 2026-11-22
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Implements parser for SOVEREIGN achievement files
//
// Format:
// [Achievements]
// ACHIEVEMENT_KEY=1785652344
// or achievements.ini:
// [ACHIEVEMENT_KEY]
// Unlocked=1785652344
/////////////////////////////////////////////////////////

#include "SovereignParser.h"
#include "../Utils.h"

#include <filesystem>
#include <fstream>

/////////////////////////////////////////////////////////////////////

SovereignParser::SovereignParser()
{
    // Constructor
}

SovereignParser::~SovereignParser()
{
    // Destructor
}

/////////////////////////////////////////////////////////////////////
////////////////////////////// PUBLIC ///////////////////////////////
/////////////////////////////////////////////////////////////////////

bool SovereignParser::HasAchievementsSection(const std::string& filePath)
{
    // SOVEREIGN.ini: stores under [Achievements]
    std::ifstream file(filePath);
    std::string line;
    while (std::getline(file, line))
    {
        line = Utils::TrimWhitespace(line);
        if (line.size() >= 2 && line.front() == '[' && line.back() == ']' && Utils::ToLower(Utils::TrimWhitespace(line.substr(1, line.size() - 2))) == "achievements")
        {
            return true;
        }
    }

    return false;
}

/////////////////////////////////////////////////////////////////////

std::string SovereignParser::FindFileNameInDirectory(const std::string& directoryPath)
{
    // Check this directory only - a config-only SOVEREIGN.ini points to achievements.ini
    const std::filesystem::path directory(directoryPath);
    const std::filesystem::path sovereignFile = directory / "SOVEREIGN.ini";
    std::error_code ec;
    const bool sovereignExists = std::filesystem::is_regular_file(sovereignFile, ec);
    if (sovereignExists && HasAchievementsSection(sovereignFile.string()))
    {
        return "SOVEREIGN.ini";
    }

    ec.clear();
    if (std::filesystem::is_regular_file(directory / "achievements.ini", ec) || sovereignExists)
    {
        return "achievements.ini";
    }

    return "";
}

/////////////////////////////////////////////////////////////////////

std::string SovereignParser::GetFileName() const
{
    return "SOVEREIGN.ini";
}

/////////////////////////////////////////////////////////////////////

std::vector<AchievementData> SovereignParser::Parse(const std::string& filePath)
{
    std::vector<AchievementData> results;

    std::ifstream file(filePath);
    if (!file.is_open())
    {
        return results;
    }

    // SOVEREIGN.ini: [Achievements] KEY=0|epoch
    // achievements.ini: [KEY] Unlocked=0|epoch
    const bool sectionEntries = std::filesystem::path(filePath).filename() == "achievements.ini";
    bool inAchievements = false;
    std::string sectionKey;
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
            const std::string section = Utils::TrimWhitespace(line.substr(1, line.size() - 2));
            inAchievements = Utils::ToLower(section) == "achievements";
            sectionKey = section;
            continue;
        }

        // SOVEREIGN.ini: reads only [Achievements]
        // achievements.ini: uses section name as key
        if ((!sectionEntries && !inAchievements) || (sectionEntries && sectionKey.empty()))
        {
            continue;
        }

        const size_t equals = line.find('=');
        if (equals == std::string::npos)
        {
            continue;
        }

        const std::string key = Utils::TrimWhitespace(line.substr(0, equals));
        const std::string value = Utils::TrimWhitespace(line.substr(equals + 1));
        // Require a named field and a decimal timestamp
        if (key.empty() || (sectionEntries && Utils::ToLower(key) != "unlocked") || value.empty() || value.find_first_not_of("0123456789") != std::string::npos)
        {
            continue;
        }

        const int64_t unlockTime = Utils::ToInt64(value);
        if (value != "0" && unlockTime <= 0)
        {
            continue;
        }

        AchievementData data{};
        data.key = sectionEntries ? sectionKey : key;
        data.achieved = unlockTime > 0;
        data.unlockTime = unlockTime;
        results.push_back(data);
    }

    return results;
}
