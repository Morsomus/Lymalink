/////////////////////////////////////////////////////////
// File: Utils.h
// Date: 2026-05-17
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Declares shared utility functions
/////////////////////////////////////////////////////////

#pragma once

#include <functional>

#include <QString>
#include <QVariant>
#include <QVariantMap>

namespace Utils
{

qint64 IsoDateToEpoch(const QString &isoDate);
QString RelativeTime(qint64 epochSeconds);
QString LocalDate(qint64 epochSeconds);
QString MapStringValue(const QVariantMap &row, const QString &key);
int MapIntValue(const QVariantMap &row, const QString &key);
QString ReadTextResource(const QString& resourcePath);
std::function<bool(const QVariant &, const QVariant &)> CreateVariantMapComparator(const QString &primaryKey, const QString &secondaryKey = QString(), const QString &tertiaryKey = QString());

}
