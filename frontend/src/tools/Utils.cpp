/////////////////////////////////////////////////////////
// File: Utils.cpp
// Date: 2026-05-17
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Implements shared utility functions
/////////////////////////////////////////////////////////

#include "Utils.h"

#include <QDateTime>
#include <QLocale>
#include <QObject>
#include <QTimeZone>
#include <Qt>
#include <QFile>

/////////////////////////////////////////////////////////////////////

namespace Utils
{

qint64 IsoDateToEpoch(const QString &isoDate)
{
    const QDateTime dateTime = QDateTime::fromString(isoDate, Qt::ISODate);
    if (!dateTime.isValid())
    {
        return -1;
    }

    return dateTime.toUTC().toSecsSinceEpoch();
}

/////////////////////////////////////////////////////////////////////

QString EpochToIsoDate(qint64 epochTime)
{
    return QDateTime::fromSecsSinceEpoch(epochTime, QTimeZone::UTC).toString(Qt::ISODate);
}

/////////////////////////////////////////////////////////////////////

QString RelativeTime(qint64 epochSeconds)
{
    if (epochSeconds <= 0)
    {
        return QString();
    }

    const qint64 elapsedSeconds = qMax<qint64>(0, QDateTime::fromSecsSinceEpoch(epochSeconds).secsTo(QDateTime::currentDateTime()));
    const qint64 elapsedMinutes = qMax<qint64>(1, elapsedSeconds / 60);
    if (elapsedMinutes < 60)
    {
        return QObject::tr("%n minute(s) ago", nullptr, static_cast<int>(elapsedMinutes));
    }

    const qint64 elapsedHours = elapsedMinutes / 60;
    if (elapsedHours < 24)
    {
        return QObject::tr("%n hour(s) ago", nullptr, static_cast<int>(elapsedHours));
    }

    const qint64 elapsedDays = qMax<qint64>(1, elapsedHours / 24);
    return QObject::tr("%n day(s) ago", nullptr, static_cast<int>(elapsedDays));
}

/////////////////////////////////////////////////////////////////////

QString LocalDate(qint64 epochSeconds)
{
    if (epochSeconds <= 0)
    {
        return QString();
    }

    const QDate date = QDateTime::fromSecsSinceEpoch(epochSeconds).date();
    return QLocale::system().toString(date, QLocale::ShortFormat);
}

/////////////////////////////////////////////////////////////////////

QString MapStringValue(const QVariantMap &row, const QString &key)
{
    const QVariant value = row.value(key);
    return value.isNull() ? QString() : value.toString();
}

/////////////////////////////////////////////////////////////////////

int MapIntValue(const QVariantMap &row, const QString &key)
{
    const QVariant value = row.value(key);
    return value.isNull() ? 0 : value.toInt();
}

/////////////////////////////////////////////////////////////////////

QString ReadTextResource(const QString& resourcePath)
{
    QFile file(resourcePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        qWarning() << "Failed to read resource:" << resourcePath << file.errorString();
        return {};
    }

    return QString::fromUtf8(file.readAll());
}

/////////////////////////////////////////////////////////////////////

}