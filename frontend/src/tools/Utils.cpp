/////////////////////////////////////////////////////////
// File: Utils.cpp
// Date: 2026-05-17
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Implements shared utility functions
/////////////////////////////////////////////////////////

#include "Utils.h"

#include <QDateTime>
#include <QTimeZone>
#include <Qt>

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

}
