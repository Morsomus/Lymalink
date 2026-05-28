/////////////////////////////////////////////////////////
// File: CodexParser.cpp
// Date: 2026-05-26
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Implements CodexParser, parses CODEX
//              emulator INI achievement files
//
// CODEX achievement INI format example:
//  [ACHIEVEMENT_KEY]
//  Achieved=1
//  CurProgress=0
//  MaxProgress=0
//  UnlockTime=1779755364
/////////////////////////////////////////////////////////

#include "CodexParser.h"

#include <fstream>
#include <string>

/////////////////////////////////////////////////////////////////////

CodexParser::CodexParser()
{
    // Constructor
}

CodexParser::~CodexParser()
{
    // Destructor
}

/////////////////////////////////////////////////////////////////////
////////////////////////////// PUBLIC ///////////////////////////////
/////////////////////////////////////////////////////////////////////

std::string CodexParser::GetFileName() const
{
    return "achievements.ini";
}

/////////////////////////////////////////////////////////////////////

std::vector<AchievementData> CodexParser::Parse(const std::string& filePath)
{
    std::vector<AchievementData> results;

    std::ifstream file(filePath);
    if (!file.is_open())
    {
        return results;
    }

    AchievementData current;
    bool inSection = false;
    std::string line;

    // Flush the current section into results once we hit the next header or EOF
    auto FlushCurrent = [&]()
    {
        if (inSection && !current.key.empty())
        {
            results.push_back(current);
        }
    };

    while (std::getline(file, line))
    {
        // Strip trailing CR and spaces
        while (!line.empty() && (line.back() == '\r' || line.back() == ' '))
        {
            line.pop_back();
        }

        if (line.empty() || line.front() == ';')
        {
            continue;
        }

        // Section header: [ACH_KEY]
        if (line.front() == '[' && line.back() == ']')
        {
            FlushCurrent();
            current = {};
            current.key = line.substr(1, line.size() - 2);
            if (current.key == "SteamAchievements")
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

        if (key == "Achieved")
        {
            current.achieved = (val == "1");
        }
        else if (key == "CurProgress")
        {
            current.curProgress = std::stoi(val);
        }
        else if (key == "MaxProgress")
        {
            current.maxProgress = std::stoi(val);
        }
        else if (key == "UnlockTime")
        {
            current.unlockTime = std::stoll(val);
        }
    }

    // File end
    FlushCurrent();
    return results;
}
