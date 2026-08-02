/////////////////////////////////////////////////////////
// File: TenokeParser.h
// Date: 2026-08-02
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Declares parser for Tenoke achievement files
/////////////////////////////////////////////////////////

#pragma once

#include "AchievementParser.h"

class TenokeParser : public AchievementParser
{
public:
    TenokeParser();
    ~TenokeParser() override;

    std::string GetFileName() const override;
    std::vector<AchievementData> Parse(const std::string& filePath) override;
};
