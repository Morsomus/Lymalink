/////////////////////////////////////////////////////////
// File: Logger.h
// Date: 2026-05-22
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Declares a singleton logging utility
/////////////////////////////////////////////////////////

#pragma once

#include <string>
#include <cstdarg>
#include <fstream>
#include <mutex>

enum class Urgency
{
    Debug,
    Info,
    Warning,
    Critical,
    Fatal
};

class Logger
{
public:
    // Returns the single Logger instance
    static Logger& Instance();

    // Non-copyable, non-movable
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
    Logger(Logger&&) = delete;
    Logger& operator=(Logger&&) = delete;

    void Init();

    // Logger::Instance().Log(Urgency::Warning, "Lymalinkd", "Main", "sigprocmask failed: %s", strerror(errno));
#if defined(_MSC_VER)
    void Log(Urgency level, const char* component, const char* function, const char* fmt, ...);
#else
    void Log(Urgency level, const char* component, const char* function, const char* fmt, ...) __attribute__((format(printf, 5, 6)));
#endif

private:
    std::string m_logPath;
    std::ofstream m_file;
    std::mutex m_mutex;

    Logger() = default;
    ~Logger();

    void Rotate();
    std::string Timestamp() const;
    const char* LevelStr(Urgency level) const;
    std::string ResolveLogPath() const;
};
