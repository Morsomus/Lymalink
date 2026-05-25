/////////////////////////////////////////////////////////
// File: Utils.cpp
// Date: 2026-05-24
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Implements shared backend utility functions
/////////////////////////////////////////////////////////

#include "Utils.h"

#include <ctime>

/////////////////////////////////////////////////////////////////////

namespace Utils
{

int64_t NowEpoch()
{
    return static_cast<int64_t>(time(nullptr));
}

/////////////////////////////////////////////////////////////////////

std::string TrimWhitespace(const std::string& value)
{
    const size_t begin = value.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos)
    {
        return {};
    }

    const size_t end = value.find_last_not_of(" \t\r\n");
    return value.substr(begin, end - begin + 1);
}

}
