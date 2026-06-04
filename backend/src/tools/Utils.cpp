/////////////////////////////////////////////////////////
// File: Utils.cpp
// Date: 2026-05-24
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Implements shared backend utility functions
/////////////////////////////////////////////////////////

#include "Utils.h"

#include <algorithm>
#include <cctype>
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

std::string ToLower(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(),
        [](unsigned char c)
        {
            return static_cast<char>(std::tolower(c));
        });

    return value;
}

/////////////////////////////////////////////////////////////////////

int64_t ToInt64(const std::string& value)
{
    try
    {
        return std::stoll(TrimWhitespace(value));
    }
    catch (...)
    {
        return 0;
    }
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

/////////////////////////////////////////////////////////////////////

bool IsHexLeUint32(const std::string& val)
{
    if (val.size() != 8)
    {
        return false;
    }
    for (char c : val)
    {
        if (!std::isxdigit(static_cast<unsigned char>(c)))
        {
            return false;
        }
    }
    return true;
}

/////////////////////////////////////////////////////////////////////

uint32_t ParseHexLeUint32(const std::string& val)
{
    uint32_t result = 0;
    for (int i = 0; i < 4; ++i)
    {
        uint8_t byte = static_cast<uint8_t>(std::stoul(val.substr(i * 2, 2), nullptr, 16));
        result |= static_cast<uint32_t>(byte) << (i * 8);
    }
    return result;
}

/////////////////////////////////////////////////////////////////////

std::string BuildString(std::initializer_list<std::string_view> parts)
{
    std::ostringstream oss;
    for (std::string_view part : parts)
    {
        oss << part;
    }
    return oss.str();
}

}
