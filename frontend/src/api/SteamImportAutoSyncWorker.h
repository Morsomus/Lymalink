/////////////////////////////////////////////////////////
// File: SteamImportAutoSyncWorker.h
// Date: 2026-07-31
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Declares Steam automatic import sync worker
/////////////////////////////////////////////////////////

#pragma once

#include "../database/SQLiteManager.h"
#include "SteamApi.h"

#include <QObject>
#include <QString>
#include <QVariantMap>

class SteamImportAutoSyncWorker : public QObject
{
    Q_OBJECT

public:
    explicit SteamImportAutoSyncWorker(QObject *parent = nullptr);
    ~SteamImportAutoSyncWorker();

public slots:
    void Run(const QString &databasePath, const QString &steamId, const QString &apiKey);

signals:
    void signalFinished(QVariantMap payload);
    void signalError(QString title, QString message);

private:
    QVariantMap UpdateChangedGames(SQLiteManager &databaseManager, const QString &connectionName, const QList<SteamOwnedGameData> &games, const QString &steamId, const QString &apiKey);
    QString SteamFetchErrorText(Error error) const;
};
