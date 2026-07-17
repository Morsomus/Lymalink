/////////////////////////////////////////////////////////
// File: WinLogger.h
// Date: 2026-06-21
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Declares Windows debug-output logger for
//              injected overlay components.
/////////////////////////////////////////////////////////

#pragma once

#include <string>

#ifdef LYMALINK_OVERLAY_DISABLE_LOGGING

#define LYMALINK_LOG(...) ((void)sizeof(__VA_ARGS__))

#else

class WinLogger
{
public:
    static void Log(const std::string& msg);
};

#define LYMALINK_LOG(message) WinLogger::Log(message)

#endif
