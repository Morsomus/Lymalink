/////////////////////////////////////////////////////////
// File: Lymalink.h
// Date: 2026-05-14
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Declares Lymalink backend orchestrator 
/////////////////////////////////////////////////////////

#pragma once

#include "Error.h"
#include "api/SteamApiWorker.h"
#include "database/SQLiteManager.h"

#include <QObject>
#include <QThread>
#include <QVariantList>
#include <QString>

class Lymalink : public QObject
{
    Q_OBJECT

public:
    explicit Lymalink(QObject *parent = nullptr);
    ~Lymalink();

    Error Initialize();
    
    Q_INVOKABLE void SearchSteamAppIds(const QString &term);
    Q_INVOKABLE void CancelSteamAppIdSearch();

signals:
    void signalSteamAppIdsReady(bool success, bool cancelled, QVariantList results);

    // Internal worker signals
    void signalRequestSearchSteamAppIds(const QString &term);
    void signalRequestCancel();
    
private:
    Error DatabaseInit();

    SQLiteManager m_databaseManager;
    QString m_databaseConnectionName;
    QString m_databasePath;

    QThread m_workerThread;
    SteamApiWorker *m_steamApiWorker = nullptr;
};
