/////////////////////////////////////////////////////////
// File: Logger.cpp
// Date: 2026-05-26
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Small stderr logger for injected overlay code
/////////////////////////////////////////////////////////

#include "Logger.h"

#include <cstdio>
#include <ctime>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>

namespace
{
constexpr const char* LOG_PATH = "/tmp/lymalink-overlay.log";
}

void Logger::Log(const std::string& msg)
{
    std::time_t now = std::time(nullptr);
    struct tm localTime {};
    char timestamp[64] = {};
    if (localtime_r(&now, &localTime))
    {
        std::strftime(timestamp, sizeof(timestamp), "%a %b %d %H:%M:%S %Y", &localTime);
    }
    else
    {
        std::snprintf(timestamp, sizeof(timestamp), "unknown-time");
    }

    char line[4096] = {};
    const int len = std::snprintf(line, sizeof(line), "%s pid=%ld %s\n", timestamp, static_cast<long>(getpid()), msg.c_str());
    if (len <= 0)
    {
        return;
    }
    const size_t lineLen = static_cast<size_t>(len) < sizeof(line) ? static_cast<size_t>(len) : sizeof(line) - 1;

    const int fd = open(LOG_PATH, O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC, 0644);
    if (fd >= 0)
    {
        (void)write(fd, line, lineLen);
        close(fd);
    }

    (void)write(STDERR_FILENO, line, lineLen);
}

__attribute__((constructor))
static void OnOverlayLibraryLoaded()
{
    Logger::Log("[OverlayLibrary] Loaded");
}

__attribute__((destructor))
static void OnOverlayLibraryUnloaded()
{
    Logger::Log("[OverlayLibrary] Unloaded");
}