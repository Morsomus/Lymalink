/////////////////////////////////////////////////////////
// File: Logger.h
// Date: 2026-05-29
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Declares process-wide Qt message logger
/////////////////////////////////////////////////////////

#pragma once

#include <QFile>
#include <QMutex>
#include <QObject>
#include <QTextStream>
#include <QtMessageHandler>

class Logger : public QObject
{
    Q_OBJECT

public:
    static Logger &Instance();

    void Install();
    void SetLogFile(const QString &path);
#if defined(Q_OS_WIN)
    static QString DefaultWindowsLogPath(const QString &appName);
#else
    static QString DefaultLinuxLogPath(const QString &appName);
#endif

private:
    QString m_logPath;
    QFile m_file;
    QTextStream m_stream;
    QMutex m_mutex;

    explicit Logger(QObject *parent = nullptr);
    static void MessageHandler(QtMsgType type, const QMessageLogContext &ctx, const QString &msg);
    void RotateLogs(const QString &path);
};
