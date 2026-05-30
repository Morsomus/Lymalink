/////////////////////////////////////////////////////////
// File: Logger.cpp
// Date: 2026-05-29
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Implements process-wide Qt message logger
/////////////////////////////////////////////////////////

#include "Logger.h"
#include "../Defines.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <iostream>

/////////////////////////////////////////////////////////////////////

Logger &Logger::Instance()
{
    static Logger inst;
    return inst;
}

/////////////////////////////////////////////////////////////////////

void Logger::Install()
{
    qInstallMessageHandler(Logger::MessageHandler);
}

/////////////////////////////////////////////////////////////////////

void Logger::SetLogFile(const QString &path)
{
    QMutexLocker lock(&m_mutex);

    if (m_file.isOpen())
    {
        m_file.close();
    }

    m_logPath = path;
    QDir().mkpath(QFileInfo(path).absolutePath());

    // Rotate at startup in case previous run filled active log
    RotateLogs(path);

    m_file.setFileName(path);
    if (!m_file.open(QIODevice::Append | QIODevice::Text))
    {
        std::cerr << "Logger: cannot open log file: " << path.toStdString() << "\n";
        return;
    }

    m_stream.setDevice(&m_file);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    m_stream.setEncoding(QStringConverter::Utf8);
#else
    m_stream.setCodec("UTF-8");
#endif
}

/////////////////////////////////////////////////////////////////////

QString Logger::DefaultLinuxLogPath(const QString &appName)
{
    QString stateHome = qEnvironmentVariable("XDG_STATE_HOME");
    if (stateHome.isEmpty())
    {
        stateHome = QDir::homePath() + "/.local/state";
    }

    return QString("%1/%2/%2-frontend.log").arg(stateHome, appName);
}

/////////////////////////////////////////////////////////////////////
///////////////////////////// PRIVATE ///////////////////////////////
/////////////////////////////////////////////////////////////////////

Logger::Logger(QObject *parent) : QObject(parent)
{
    m_logPath = "";
}

/////////////////////////////////////////////////////////////////////

void Logger::MessageHandler(QtMsgType type, const QMessageLogContext &ctx, const QString &msg)
{
    #ifndef QT_DEBUG
        if (type == QtDebugMsg)
        {
            return;
        }
    #endif

    Logger &log = Logger::Instance();
    QMutexLocker lock(&log.m_mutex);

    const QString ts = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz");

    const char *level = [type]() -> const char * {
        switch (type)
        {
            case QtDebugMsg:
                return "DEBUG   ";
            case QtInfoMsg:
                return "INFO    ";
            case QtWarningMsg:
                return "WARNING ";
            case QtCriticalMsg:
                return "CRITICAL";
            case QtFatalMsg:
                return "FATAL   ";
            default:
                return "UNKNOWN ";
        }
    }();

    QString location;
    if (ctx.file)
    {
        location = QString(" [%1:%2]").arg(ctx.file).arg(ctx.line);
    }

    const QString line = QString("%1 | %2 | %3%4").arg(ts).arg(level).arg(msg).arg(location);

    // Rotate during runtime when active file reaches configured size
    if (log.m_file.isOpen() && log.m_file.size() >= LOG_LYMALINK_FRONTEND_MAX_SIZE)
    {
        log.m_stream.flush();
        log.m_file.close();
        log.RotateLogs(log.m_logPath);
        log.m_file.setFileName(log.m_logPath);
        if (!log.m_file.open(QIODevice::Append | QIODevice::Text))
        {
            std::cerr << "Logger: cannot reopen log file after rotation: " << log.m_logPath.toStdString() << "\n";
        }
        else
        {
            log.m_stream.setDevice(&log.m_file);
        }
    }

    if (log.m_file.isOpen())
    {
        log.m_stream << line << "\n";
        // Flush each line so potential crashes keep last messages
        log.m_stream.flush();
    }

    #ifdef QT_DEBUG
        std::cerr << line.toStdString() << "\n";
    #endif

    if (type == QtFatalMsg)
    {
        abort();
    }
}

/////////////////////////////////////////////////////////////////////

void Logger::RotateLogs(const QString &path)
{
    if (!QFileInfo::exists(path))
    {
        return;
    }

    if (QFileInfo(path).size() < LOG_LYMALINK_FRONTEND_MAX_SIZE)
    {
        return;
    }

    for (int i = LOG_LYMALINK_FRONTEND_MAX_BACKUPS; i >= 1; --i)
    {
        const QString older = QString("%1.%2").arg(path).arg(i);
        const QString newer = (i == 1) ? path : QString("%1.%2").arg(path).arg(i - 1);
        if (QFileInfo::exists(older))
        {
            QFile::remove(older);
        }

        if (QFileInfo::exists(newer))
        {
            QFile::rename(newer, older);
        }
    }
}
