/////////////////////////////////////////////////////////
// File: Utils.cpp
// Date: 2026-05-24
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Implements shared backend utility functions
/////////////////////////////////////////////////////////

#include "Utils.h"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <fstream>
#include <sstream>

/////////////////////////////////////////////////////////////////////

namespace Utils
{

int64_t NowEpoch()
{
    return static_cast<int64_t>(time(nullptr));
}

/////////////////////////////////////////////////////////////////////

uint64_t NowMs()
{
    using namespace std::chrono;
    return static_cast<uint64_t>(
        duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count()
    );
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

/////////////////////////////////////////////////////////////////////

std::string ReadTextFile(const std::string& path)
{
    std::ifstream file(path);
    if (!file.is_open())
    {
        return {};
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

/////////////////////////////////////////////////////////////////////

std::string TrimTrailingWhitespace(std::string value)
{
    while (!value.empty() && (value.back() == '\n' || value.back() == '\r' || value.back() == '\0' || value.back() == ' '))
    {
        value.pop_back();
    }
    return value;
}

/////////////////////////////////////////////////////////////////////

std::string ReadProcessComm()
{
    return TrimTrailingWhitespace(ReadTextFile("/proc/self/comm"));
}

/////////////////////////////////////////////////////////////////////

std::string ReadProcessCmdline()
{
    std::string cmdline = ReadTextFile("/proc/self/cmdline");
    std::replace(cmdline.begin(), cmdline.end(), '\0', ' ');
    return TrimTrailingWhitespace(cmdline);
}

/////////////////////////////////////////////////////////////////////

std::string ReadIniValue(const std::string& configPath, const std::string& section, const std::string& key)
{
    std::ifstream configFile(configPath);
    if (!configFile.is_open())
    {
        return {};
    }

    bool inTargetSection = false;
    std::string line;
    while (std::getline(configFile, line))
    {
        line = TrimWhitespace(line);
        if (line.empty() || line[0] == ';' || line[0] == '#')
        {
            continue;
        }

        if (line.front() == '[' && line.back() == ']')
        {
            inTargetSection = TrimWhitespace(line.substr(1, line.size() - 2)) == section;
            continue;
        }

        if (!inTargetSection)
        {
            continue;
        }

        const size_t separator = line.find('=');
        if (separator == std::string::npos)
        {
            continue;
        }

        if (TrimWhitespace(line.substr(0, separator)) == key)
        {
            return TrimWhitespace(line.substr(separator + 1));
        }
    }

    return {};
}

}
