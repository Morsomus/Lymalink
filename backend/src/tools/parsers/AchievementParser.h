/////////////////////////////////////////////////////////
// File: AchievementParser.h
// Date: 2026-05-26
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Abstract base class for emulator-specific
//              achievement file parsers
/////////////////////////////////////////////////////////

#pragma once

#include "../../watcher/AchievementHandler.h"

#include <string>
#include <vector>

/////////////////////////////////////////////////////////////////////

class AchievementParser
{
public:
    virtual ~AchievementParser() = default;

    // Returns the expected achievement filename for this emulator type
    virtual std::string GetFileName() const = 0;

    // Parses the achievement file and returns all achievement states
    virtual std::vector<AchievementData> Parse(const std::string& filePath) = 0;
};
