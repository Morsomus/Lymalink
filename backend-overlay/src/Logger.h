/////////////////////////////////////////////////////////
// File: Logger.h
// Date: 2026-05-26
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Small stderr logger for injected overlay code
/////////////////////////////////////////////////////////

#pragma once

#include <string>

class Logger
{
public:
    static void Log(const std::string& msg);
};
