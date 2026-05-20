/////////////////////////////////////////////////////////
// File: ImageCacheManager.h
// Date: 2026-05-20
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Declares ImageCacheManager for downloading
//              and handling image assets
/////////////////////////////////////////////////////////

#pragma once

#include "../Error.h"

#include <QObject>
#include <QNetworkAccessManager>
#include <QSize>
#include <QString>

class ImageCacheManager : public QObject
{
    Q_OBJECT

public:
    explicit ImageCacheManager(QObject *parent = nullptr);
    ~ImageCacheManager();

    Error DownloadAndCache(const QString &url, const QString &savePath, const QSize &targetSize, QString &cachedPath, const QString &newName = QString());

private:
    QNetworkAccessManager m_network;

    QString TempDir() const;
    QString BuildFinalPath(const QString &savePath, const QString &newName, const QString &url, const QSize &targetSize) const;
};
