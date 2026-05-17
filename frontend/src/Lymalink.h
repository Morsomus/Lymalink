/////////////////////////////////////////////////////////
// File: Lymalink.h
// Date: 2026-05-14
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Declares Lymalink backend orchestrator 
/////////////////////////////////////////////////////////

#pragma once

#include "Error.h"
#include "database/SQLiteManager.h"

#include <QObject>
#include <QString>

class Lymalink : public QObject
{
    Q_OBJECT

public:
    explicit Lymalink(QObject *parent = nullptr);
    ~Lymalink();

    Error Initialize();

signals:
    
private:
    Error DatabaseInit();

    SQLiteManager m_databaseManager;
    QString m_databaseConnectionName;
    QString m_databasePath;
};
