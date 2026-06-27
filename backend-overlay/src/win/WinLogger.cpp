/////////////////////////////////////////////////////////
// File: WinLogger.cpp
// Date: 2026-06-21
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Implements Windows debug-output logging
//              for injected overlay components.
/////////////////////////////////////////////////////////

#include "WinLogger.h"

#ifndef LYMALINK_OVERLAY_DISABLE_LOGGING

#ifndef NOMINMAX
    #define NOMINMAX
#endif
#include <windows.h>

#include <cstdint>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <string>

namespace
{
std::filesystem::path ResolveLogPath()
{
    char localAppData[MAX_PATH]{};
    const DWORD length = GetEnvironmentVariableA("LOCALAPPDATA", localAppData, MAX_PATH);
    if (length > 0 && length < MAX_PATH)
    {
        return std::filesystem::path(localAppData) / "Lymalink" / "logs" / "lymalink-overlay.log";
    }

    char tempPath[MAX_PATH]{};
    const DWORD tempLength = GetTempPathA(MAX_PATH, tempPath);
    if (tempLength > 0 && tempLength < MAX_PATH)
    {
        return std::filesystem::path(tempPath) / "lymalink-overlay.log";
    }

    return "lymalink-overlay.log";
}

const char* OverlayArch()
{
#if INTPTR_MAX == INT64_MAX
    return "x64";
#else
    return "x86";
#endif
}
}

/////////////////////////////////////////////////////////////////////
////////////////////////////// PUBLIC ///////////////////////////////
/////////////////////////////////////////////////////////////////////

void WinLogger::Log(const std::string& message)
{
    std::time_t now = std::time(nullptr);
    struct tm localTime{};
    char timestamp[64]{};
    if (localtime_s(&localTime, &now) == 0)
    {
        std::strftime(timestamp, sizeof(timestamp), "%a %b %d %H:%M:%S %Y", &localTime);
    }
    else
    {
        std::snprintf(timestamp, sizeof(timestamp), "unknown-time");
    }

    const std::string line = std::string(timestamp) + " pid=" + std::to_string(GetCurrentProcessId()) + " " + message + "\n";

    // OutputDebugStringA keeps logs visible to debuggers without a console window
    OutputDebugStringA(("[LymalinkOverlay] " + line).c_str());

    const std::filesystem::path logPath = ResolveLogPath();
    std::error_code ec;
    std::filesystem::create_directories(logPath.parent_path(), ec);

    std::ofstream out(logPath, std::ios::app);
    if (out.is_open())
    {
        out << line;
    }
}

/////////////////////////////////////////////////////////////////////

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved)
{
    (void)instance;
    (void)reserved;

    if (reason == DLL_PROCESS_ATTACH)
    {
        LYMALINK_LOG(std::string("[OverlayLibrary] Loaded arch=") + OverlayArch());
    }
    else if (reason == DLL_PROCESS_DETACH)
    {
        LYMALINK_LOG("[OverlayLibrary] Unloaded");
    }

    return TRUE;
}

#endif
