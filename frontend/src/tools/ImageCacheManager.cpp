/////////////////////////////////////////////////////////
// File: ImageCacheManager.cpp
// Date: 2026-05-20
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Implements ImageCacheManager for downloading
//              and handling image assets
/////////////////////////////////////////////////////////

#include "ImageCacheManager.h"

#include <QNetworkReply>
#include <QNetworkRequest>
#include <QEventLoop>
#include <QImage>
#include <QImageReader>
#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <QBuffer>
#include <QCryptographicHash>
#include <QStandardPaths>
#include <QUrl>

/////////////////////////////////////////////////////////////////////

ImageCacheManager::ImageCacheManager(QObject *parent) : QObject(parent)
{
    m_network.setTransferTimeout(15000);
}

ImageCacheManager::~ImageCacheManager()
{
    // Destructor
}

/////////////////////////////////////////////////////////////////////

Error ImageCacheManager::DownloadAndCache(const QString &url, const QString &savePath, const QSize &targetSize, QString &cachedPath, const QString &newName)
{
    cachedPath.clear();

    if (url.isEmpty() || savePath.isEmpty() || !targetSize.isValid())
    {
        qWarning() << "Invalid image cache parameters:" << url << savePath << targetSize;
        return Error::InvalidParameter;
    }

    const QString finalPath = BuildFinalPath(savePath, newName, url, targetSize);

    // Check for cache hit
    if (QFileInfo::exists(finalPath))
    {
        qDebug() << "Cache hit:" << finalPath;
        cachedPath = finalPath;
        return Error::NoError;
    }

    if (!QDir().mkpath(savePath))
    {
        qWarning() << "Cannot create directory:" << savePath;
        return Error::FileSystemError;
    }

    QByteArray data;
    if (m_lastDownloadedUrl == url && !m_lastDownloadedData.isEmpty())
    {
        qDebug() << "Reusing downloaded image data:" << url;
        data = m_lastDownloadedData;
    }
    else
    {
        // Download
        QNetworkRequest request{QUrl(url)};
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);

        QNetworkReply *reply = m_network.get(request);

        QEventLoop loop;
        QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        loop.exec();

        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError)
        {
            qWarning() << "Network error:" << reply->errorString() << url;
            return Error::NotFound;
        }

        data = reply->readAll();
        if (data.isEmpty())
        {
            qWarning() << "Empty response:" << url;
            return Error::NotFound;
        }

        m_lastDownloadedUrl = url;
        m_lastDownloadedData = data;
    }

    // Decode
    QBuffer buf;
    buf.setData(data);
    buf.open(QIODevice::ReadOnly);

    QImageReader reader(&buf);
    reader.setAutoTransform(true);
    reader.setDecideFormatFromContent(true);

    QImage image = reader.read();
    if (image.isNull())
    {
        qWarning() << "Invalid image data:" << reader.errorString() << url;
        return Error::ParseError;
    }

    // Scale if required
    QImage result;
    if (image.width() > targetSize.width() || image.height() > targetSize.height())
    {
        result = image.scaled(targetSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
    else
    {
        result = std::move(image);
    }

    // Write to temp
    const QString tmpPath = QDir(TempDir()).filePath(QFileInfo(finalPath).fileName() + ".tmp");
    if (!result.save(tmpPath, "JPG", 90))
    {
        qWarning() << "Failed to save temp file:" << tmpPath;
        return Error::FileSystemError;
    }

    QFile::remove(finalPath);
    if (!QFile::rename(tmpPath, finalPath))
    {
        QFile::remove(tmpPath);
        qWarning() << "Failed to move image to:" << finalPath;
        return Error::FileSystemError;
    }

    qDebug() << "Saved:" << finalPath;
    cachedPath = finalPath;
    return Error::NoError;
}

/////////////////////////////////////////////////////////////////////

void ImageCacheManager::ClearMemoryCache()
{
    m_lastDownloadedUrl.clear();
    m_lastDownloadedData.clear();
}

/////////////////////////////////////////////////////////////////////
///////////////////////////// PRIVATE ///////////////////////////////
/////////////////////////////////////////////////////////////////////

QString ImageCacheManager::TempDir() const
{
    return QStandardPaths::writableLocation(QStandardPaths::TempLocation);
}

/////////////////////////////////////////////////////////////////////

QString ImageCacheManager::BuildFinalPath(const QString &savePath, const QString &newName, const QString &url, const QSize &targetSize) const
{
    QString stem;

    if (!newName.isEmpty())
    {
        stem = newName;
    }
    else
    {
        const QString input = url + QString::number(targetSize.width()) + "x" + QString::number(targetSize.height());
        stem = QCryptographicHash::hash(input.toUtf8(), QCryptographicHash::Sha1).toHex();
    }

    return QDir(savePath).filePath(stem + ".jpg");
}
