/////////////////////////////////////////////////////////
// File: Logger.h
// Date: 2026-05-26
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Small stderr logger for injected overlay code
/////////////////////////////////////////////////////////

#pragma once

#include <string>

#ifdef LYMALINK_OVERLAY_DISABLE_LOGGING

#define LYMALINK_LOG(...) ((void)sizeof(__VA_ARGS__))

#else

class Logger
{
public:
    static void Log(const std::string& msg);
};

#define LYMALINK_LOG(message) Logger::Log(message)

#endif
