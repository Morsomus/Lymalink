/////////////////////////////////////////////////////////
// File: GoldbergParser.h
// Date: 2026-06-02
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Declares parser, JSON achievement files
/////////////////////////////////////////////////////////

#pragma once

#include "AchievementParser.h"

/////////////////////////////////////////////////////////////////////

class GoldbergParser : public AchievementParser
{
public:
    GoldbergParser();
    ~GoldbergParser() override;

    std::string GetFileName() const override;
    std::vector<AchievementData> Parse(const std::string& filePath) override;

private:
    std::vector<AchievementData> ParseIni(std::istream& file);
};
