/////////////////////////////////////////////////////////
// File: Logger.cpp
// Date: 2026-05-22
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Implementation of a simple logging utility
/////////////////////////////////////////////////////////

#include <iostream>
#include <fstream>
#include <ctime>
#include <filesystem>
#include <string>
#include <cstdarg>
#include <cstdio>
#include <mutex>
#include <vector>

#include "Logger.h"

namespace fs = std::filesystem;

std::ofstream logFile;
std::mutex logMutex;

/////////////////////////////////////////////////////////////////////
////////////////////////////// PUBLIC ///////////////////////////////
/////////////////////////////////////////////////////////////////////

void Logger::Init(const std::string& logPath)
{
    std::lock_guard<std::mutex> lock(logMutex);

    fs::path logP(logPath);
    if (!fs::exists(logP.parent_path()))
    {
        fs::create_directories(logP.parent_path());
    }

    logFile.open(logPath, std::ios::app);
    if (logFile.is_open())
    {
        std::cout.rdbuf(logFile.rdbuf());
        std::cerr.rdbuf(logFile.rdbuf());
    }
}

/////////////////////////////////////////////////////////////////////

void Logger::Log(const std::string& msg)
{
    std::lock_guard<std::mutex> lock(logMutex);

    std::time_t now = std::time(nullptr);
    std::string timestamp = std::ctime(&now);
    // remove trailing newline
    if (!timestamp.empty() && timestamp.back() == '\n') timestamp.pop_back();
    std::cout << timestamp << " " << msg << std::endl;
}

/////////////////////////////////////////////////////////////////////

void Logger::Log(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    const std::string msg = FormatMessage(fmt, args);
    va_end(args);
    Log(msg);
}

/////////////////////////////////////////////////////////////////////

void Logger::Error(const std::string& msg)
{
    std::lock_guard<std::mutex> lock(logMutex);

    std::time_t now = std::time(nullptr);
    std::string timestamp = std::ctime(&now);
    if (!timestamp.empty() && timestamp.back() == '\n') timestamp.pop_back();
    std::cerr << timestamp << " ERROR: " << msg << std::endl;
}

/////////////////////////////////////////////////////////////////////

void Logger::Error(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    const std::string msg = FormatMessage(fmt, args);
    va_end(args);
    Error(msg);
}

/////////////////////////////////////////////////////////////////////
///////////////////////////// PRIVATE ///////////////////////////////
/////////////////////////////////////////////////////////////////////

std::string Logger::FormatMessage(const char* fmt, va_list args)
{
    va_list argsCopy;
    va_copy(argsCopy, args);
    const int len = std::vsnprintf(nullptr, 0, fmt, argsCopy);
    va_end(argsCopy);

    if (len <= 0)
    {
        return {};
    }

    std::vector<char> buffer(static_cast<size_t>(len) + 1);
    std::vsnprintf(buffer.data(), buffer.size(), fmt, args);
    return std::string(buffer.data(), static_cast<size_t>(len));
}
