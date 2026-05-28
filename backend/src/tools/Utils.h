/////////////////////////////////////////////////////////
// File: Utils.h
// Date: 2026-05-24
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Declares shared backend utility functions
/////////////////////////////////////////////////////////

#pragma once

#include <cstdint>
#include <string>

namespace Utils
{

int64_t NowEpoch();
uint64_t NowMs();
std::string TrimWhitespace(const std::string& value);
std::string ReadTextFile(const std::string& path);
std::string TrimTrailingWhitespace(std::string value);
std::string ReadProcessComm();
std::string ReadProcessCmdline();
std::string ReadIniValue(const std::string& configPath, const std::string& section, const std::string& key);

}
