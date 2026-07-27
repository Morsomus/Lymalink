/////////////////////////////////////////////////////////
// File: RLDParser.h
// Date: 2026-07-27
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Declares parser for Reloaded INI
//              achievement files
/////////////////////////////////////////////////////////

#pragma once

#include "AchievementParser.h"

/////////////////////////////////////////////////////////////////////

class RLDParser : public AchievementParser
{
public:
    RLDParser();
    ~RLDParser() override;

    std::string GetFileName() const override;
    std::vector<AchievementData> Parse(const std::string& filePath) override;
};
