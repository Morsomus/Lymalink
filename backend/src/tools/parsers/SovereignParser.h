/////////////////////////////////////////////////////////
// File: SovereignParser.h
// Date: 2026-11-22
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Declares parser for SOVEREIGN achievement files
/////////////////////////////////////////////////////////

#pragma once

#include "AchievementParser.h"

class SovereignParser : public AchievementParser
{
public:
    SovereignParser();
    ~SovereignParser() override;

    static bool HasAchievementsSection(const std::string& filePath);
    static std::string FindFileNameInDirectory(const std::string& directoryPath);

    std::string GetFileName() const override;
    std::vector<AchievementData> Parse(const std::string& filePath) override;
};
