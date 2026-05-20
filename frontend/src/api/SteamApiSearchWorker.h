/////////////////////////////////////////////////////////
// File: SteamApiSearchWorker.h
// Date: 2026-05-19
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Declares Steam API search worker
/////////////////////////////////////////////////////////

#pragma once

#include "../Error.h"
#include "SteamApi.h"

#include <QAtomicInt>
#include <QObject>
#include <QVariantList>

class SteamApiSearchWorker : public QObject
{
    Q_OBJECT
public:
    explicit SteamApiSearchWorker(QObject *parent = nullptr);
    ~SteamApiSearchWorker();

public slots:
    void Init();
    void SearchSteamAppIds(const QString &term);
    void Cancel();

signals:
    void signalSearchAppIdsFinished(bool success, bool cancelled, QVariantList results);

private:
    QAtomicInt m_cancelled{0};
    SteamApi *m_steamApi;
};
