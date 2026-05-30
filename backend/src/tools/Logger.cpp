/////////////////////////////////////////////////////////
// File: Logger.cpp
// Date: 2026-05-22
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Implementation of a singleton logging utility
/////////////////////////////////////////////////////////

#include "Logger.h"
#include "Defines.h"

#include <cstdio>
#include <cstdarg>
#include <ctime>
#include <filesystem>
#include <iostream>
#include <vector>

namespace fs = std::filesystem;

/////////////////////////////////////////////////////////////////////
////////////////////////////// PUBLIC ///////////////////////////////
/////////////////////////////////////////////////////////////////////

Logger& Logger::Instance()
{
    static Logger instance;
    return instance;
}

/////////////////////////////////////////////////////////////////////

Logger::~Logger()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_file.is_open())
    {
        m_file.close();
    }
}

/////////////////////////////////////////////////////////////////////

void Logger::Init()
{
    std::lock_guard<std::mutex> lock(m_mutex);

    m_logPath = ResolveLogPath();
    fs::create_directories(fs::path(m_logPath).parent_path());

    Rotate();

    m_file.open(m_logPath, std::ios::app);
    if (!m_file.is_open())
    {
        std::cerr << "Logger: cannot open log file: " << m_logPath << "\n";
    }
}

/////////////////////////////////////////////////////////////////////

void Logger::Log(Urgency level, const char* component, const char* function, const char* fmt, ...)
{
    #ifndef BACKEND_DEBUG
        if (level == Urgency::Debug)
        {
            return;
        }
    #endif

    // Format the caller's message
    va_list args;
    va_start(args, fmt);

    va_list argsCopy;
    va_copy(argsCopy, args);
    const int len = std::vsnprintf(nullptr, 0, fmt, argsCopy);
    va_end(argsCopy);

    std::string userMsg;
    if (len > 0)
    {
        std::vector<char> buf(static_cast<size_t>(len) + 1);
        std::vsnprintf(buf.data(), buf.size(), fmt, args);
        userMsg.assign(buf.data(), static_cast<size_t>(len));
    }
    va_end(args);

    const std::string line = Timestamp() + " | " + LevelStr(level) + " | " + component + "::" + function + " | " + userMsg;

    {
        std::lock_guard<std::mutex> lock(m_mutex);

        // Rotate mid-run if the file has reached the size limit
        if (m_file.is_open())
        {
            const auto sz = static_cast<long long>(fs::file_size(fs::path(m_logPath)));
            if (sz >= LOG_LYMALINK_BACKEND_MAX_SIZE)
            {
                m_file.close();
                Rotate();
                m_file.open(m_logPath, std::ios::app);
            }
        }

        if (m_file.is_open())
        {
            m_file << line << "\n";
            m_file.flush();   // flush immediately so potential crash does not swallow the last lines
        }

        std::cerr << line << "\n";
    }

    if (level == Urgency::Fatal)
    {
        std::abort();
    }
}

/////////////////////////////////////////////////////////////////////
///////////////////////////// PRIVATE ///////////////////////////////
/////////////////////////////////////////////////////////////////////

// Caller must hold m_mutex
void Logger::Rotate()
{
    if (m_logPath.empty() || !fs::exists(m_logPath))
    {
        return;
    }

    if (static_cast<long long>(fs::file_size(m_logPath)) < LOG_LYMALINK_BACKEND_MAX_SIZE)
    {
        return;
    }

    for (int i = LOG_LYMALINK_BACKEND_MAX_BACKUPS; i >= 1; --i)
    {
        fs::path older = m_logPath + "." + std::to_string(i);
        fs::path newer = (i == 1) ? fs::path(m_logPath) : fs::path(m_logPath + "." + std::to_string(i - 1));

        try
        {
            if (fs::exists(older))
            {
                fs::remove(older);
            }
            if (fs::exists(newer))
            {
                fs::rename(newer, older);
            }
        }
        catch (const fs::filesystem_error& e)
        {
            std::cerr << "Logger::Rotate error: " << e.what() << "\n";
        }
    }
}

/////////////////////////////////////////////////////////////////////

std::string Logger::Timestamp() const
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);

    struct tm tm_info;
    localtime_r(&ts.tv_sec, &tm_info);

    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm_info);

    char full[64];
    std::snprintf(full, sizeof(full), "%s.%03ld", buf, ts.tv_nsec / 1'000'000);
    return full;
}

/////////////////////////////////////////////////////////////////////

const char* Logger::LevelStr(Urgency level) const
{
    switch (level)
    {
        #ifdef BACKEND_DEBUG
            case Urgency::Debug: return "DEBUG   ";
        #endif
        case Urgency::Info:
            return "INFO    ";
        case Urgency::Warning:
            return "WARNING ";
        case Urgency::Critical:
            return "CRITICAL";
        case Urgency::Fatal:
            return "FATAL   ";
        default:
            return "UNKNOWN ";
    }
    return "UNKNOWN ";
}

/////////////////////////////////////////////////////////////////////

std::string Logger::ResolveLogPath() const
{
#if defined(_WIN32)
    PWSTR wpath = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &wpath)))
    {
        char narrow[MAX_PATH];
        WideCharToMultiByte(CP_UTF8, 0, wpath, -1, narrow, MAX_PATH, nullptr, nullptr);
        CoTaskMemFree(wpath);
        return std::string(narrow) + "\\lymalink\\lymalink-backend.log";
    }
    return "C:\\lymalink\\lymalink-backend.log";

#else
    const char* xdgState = std::getenv("XDG_STATE_HOME");
    if (xdgState && xdgState[0] != '\0')
    {
        return std::string(xdgState) + "/lymalink/lymalink-backend.log";
    }

    const char* home = std::getenv("HOME");
    if (home && home[0] != '\0')
    {
        return std::string(home) + "/.local/state/lymalink/lymalink-backend.log";
    }

    return "/tmp/lymalink-backend.log";
#endif
}