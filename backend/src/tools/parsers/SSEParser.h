/////////////////////////////////////////////////////////
// File: SSEParser.h
// Date: 2026-08-01
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Declares parser for SmartSteamEmu binary
//              achievement files
/////////////////////////////////////////////////////////

#pragma once

#include "AchievementParser.h"

#include <string>
#include <vector>

/////////////////////////////////////////////////////////////////////

class SSEParser : public AchievementParser
{
public:
    SSEParser();
    ~SSEParser() override;

    std::string GetFileName() const override;
    std::vector<AchievementData> Parse(const std::string& filePath) override;
};
