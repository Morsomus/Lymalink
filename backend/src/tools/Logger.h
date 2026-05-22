/////////////////////////////////////////////////////////
// File: Logger.h
// Date: 2026-05-22
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Declares a simple logging utility
/////////////////////////////////////////////////////////

#pragma once

#include <string>

class Logger
{
public:
    static void Init(const std::string& logPath);
    static void Log(const std::string& msg);
    static void Log(const char* fmt, ...);
    static void Error(const std::string& msg);
    static void Error(const char* fmt, ...);

private:
    static std::string FormatMessage(const char* fmt, va_list args);
};
