/////////////////////////////////////////////////////////
// File: Lymalink.h
// Date: 2026-05-14
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Declares Lymalink backend orchestrator 
/////////////////////////////////////////////////////////

#pragma once

#include "Error.h"
#include "api/SteamApi.h"
#include "database/SQLiteManager.h"

#include <QObject>
#include <QVariantList>
#include <QVariantMap>
#include <QString>

class Lymalink : public QObject
{
    Q_OBJECT

public:
    explicit Lymalink(QObject *parent = nullptr);
    ~Lymalink();

    Error Initialize();
    Q_INVOKABLE QVariantList SearchSteamAppIds(const QString &term);

signals:
    
private:
    Error DatabaseInit();

    SQLiteManager m_databaseManager;
    SteamApi m_steamApi;
    QString m_databaseConnectionName;
    QString m_databasePath;
};
