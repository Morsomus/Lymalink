/////////////////////////////////////////////////////////
// File: RUNECodexParser.h
// Date: 2026-05-26
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Declares parser, parses INI achievement files
/////////////////////////////////////////////////////////

#pragma once

#include "AchievementParser.h"

/////////////////////////////////////////////////////////////////////

class RUNECodexParser : public AchievementParser
{
public:
    RUNECodexParser();
    ~RUNECodexParser() override;

    std::string GetFileName() const override;
    std::vector<AchievementData> Parse(const std::string& filePath) override;
};