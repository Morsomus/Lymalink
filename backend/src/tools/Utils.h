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
std::string TrimWhitespace(const std::string& value);

}
