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
#include <QStringList>

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
        qWarning() << "Tools:ReadTextResource: Failed to read resource:" << resourcePath << file.errorString();
        return {};
    }

    return QString::fromUtf8(file.readAll());
}

/////////////////////////////////////////////////////////////////////

std::function<bool(const QVariant &, const QVariant &)> CreateVariantMapComparator(const QString &primaryKey, const QString &secondaryKey, const QString &tertiaryKey)
{
    QStringList keys;
    for (const QString &key : {primaryKey, secondaryKey, tertiaryKey})
    {
        if (!key.isEmpty())
        {
            keys.append(key);
        }
    }

    return [keys](const QVariant &leftValue, const QVariant &rightValue) {
        const auto compareStrings = [](const QString &leftValue, const QString &rightValue, bool &handled) {
            const QString left = leftValue.trimmed();
            const QString right = rightValue.trimmed();
            handled = false;

            if (left.isEmpty() && right.isEmpty())
            {
                return false;
            }
            if (left.isEmpty())
            {
                handled = true;
                return false;
            }
            if (right.isEmpty())
            {
                handled = true;
                return true;
            }

            bool leftNumberOk = false;
            bool rightNumberOk = false;
            const int leftNumber = left.toInt(&leftNumberOk);
            const int rightNumber = right.toInt(&rightNumberOk);
            if (leftNumberOk && rightNumberOk && leftNumber != rightNumber)
            {
                handled = true;
                return leftNumber < rightNumber;
            }

            const int textCompare = QString::compare(left, right, Qt::CaseInsensitive);
            if (textCompare != 0)
            {
                handled = true;
                return textCompare < 0;
            }

            return false;
        };

        const QVariantMap left = leftValue.toMap();
        const QVariantMap right = rightValue.toMap();
        for (int keyIndex = 0; keyIndex < keys.size(); ++keyIndex)
        {
            QString currentLeftValue = left.value(keys.at(keyIndex)).toString();
            QString currentRightValue = right.value(keys.at(keyIndex)).toString();
            if (keyIndex == 0 && keys.size() > 1)
            {
                if (currentLeftValue.trimmed().isEmpty())
                {
                    currentLeftValue = left.value(keys.at(1)).toString();
                }
                if (currentRightValue.trimmed().isEmpty())
                {
                    currentRightValue = right.value(keys.at(1)).toString();
                }
            }

            bool handled = false;
            const bool result = compareStrings(currentLeftValue, currentRightValue, handled);
            if (handled)
            {
                return result;
            }
        }

        return false;
    };
}

/////////////////////////////////////////////////////////////////////

}