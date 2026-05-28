/////////////////////////////////////////////////////////
// File: CodexParser.h
// Date: 2026-05-26
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Declares CodexParser, parses CODEX
//              emulator INI achievement files
/////////////////////////////////////////////////////////

#pragma once

#include "AchievementParser.h"

/////////////////////////////////////////////////////////////////////

class CodexParser : public AchievementParser
{
public:
    CodexParser();
    ~CodexParser() override;

    std::string GetFileName() const override;
    std::vector<AchievementData> Parse(const std::string& filePath) override;
};