/////////////////////////////////////////////////////////
// File: Utils.h
// Date: 2026-05-17
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Declares shared utility functions
/////////////////////////////////////////////////////////

#pragma once

#include <QString>

namespace Utils
{

qint64 IsoDateToEpoch(const QString &isoDate);
QString EpochToIsoDate(qint64 epochTime);

}
