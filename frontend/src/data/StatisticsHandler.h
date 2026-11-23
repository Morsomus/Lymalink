/////////////////////////////////////////////////////////
// File: StatisticsHandler.h
// Date: 2026-11-23
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Reads local statistics
/////////////////////////////////////////////////////////

#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

class StatisticsHandler : public QObject
{
    Q_OBJECT

public:
    explicit StatisticsHandler(QObject *parent = nullptr);
    ~StatisticsHandler();

    QVariantMap Read(const QString &databasePath) const;

private:
    bool TitleLess(const QVariant &left, const QVariant &right) const;
    bool HighlightTieLess(const QVariant &left, const QVariant &right) const;
    QVariantMap BuildSource(const QVariantList &gameRows, const QVariantList &achievementRows) const;
};
