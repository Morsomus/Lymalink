/////////////////////////////////////////////////////////
// File: GoGNParser.h
// Date: 2026-06-13
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Declares parser for Nemirtingas GOG
//              achievement JSON files
/////////////////////////////////////////////////////////

#pragma once

#include "AchievementParser.h"

class GoGNParser : public AchievementParser
{
public:
    GoGNParser();
    ~GoGNParser() override;

    std::string GetFileName() const override;
    std::vector<AchievementData> Parse(const std::string& filePath) override;
};
