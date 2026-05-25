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
    m_lastDownloadedUrl = "";
    m_lastDownloadedData = {};

    // Keep image download attempts bounded so hydration cannot hang forever
    m_network.setTransferTimeout(15000);
}

ImageCacheManager::~ImageCacheManager()
{
    // Destructor
}

/////////////////////////////////////////////////////////////////////

Error ImageCacheManager::DownloadAndCache(const QString &url, const QString &savePath, const QSize &targetSize, QString &cachedPath, const QString &newName)
{
    Error downloadResult = Error::NoError;

    // Caller receives empty path unless image exists or save completes
    cachedPath.clear();

    if (url.isEmpty() || savePath.isEmpty() || !targetSize.isValid())
    {
        qWarning() << "Invalid image cache parameters:" << url << savePath << targetSize;
        downloadResult = Error::InvalidParameter;
        return downloadResult;
    }

    const QString finalPath = BuildFinalPath(savePath, newName, url, targetSize);

    // Reuse existing scaled image when present on disk
    if (QFileInfo::exists(finalPath))
    {
        qDebug() << "Cache hit:" << finalPath;
        cachedPath = finalPath;
        return downloadResult;
    }

    if (!QDir().mkpath(savePath))
    {
        qWarning() << "Cannot create directory:" << savePath;
        downloadResult = Error::FileSystemError;
        return downloadResult;
    }

    QByteArray data;
    if (m_lastDownloadedUrl == url && !m_lastDownloadedData.isEmpty())
    {
        // Reuse prior network payload for multiple target sizes of same image
        qDebug() << "Reusing downloaded image data:" << url;
        data = m_lastDownloadedData;
    }
    else
    {
        // Download original image data before decoding/scaling
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
            downloadResult = Error::NotFound;
            return downloadResult;
        }

        data = reply->readAll();
        if (data.isEmpty())
        {
            qWarning() << "Empty response:" << url;
            downloadResult = Error::NotFound;
            return downloadResult;
        }

        m_lastDownloadedUrl = url;
        m_lastDownloadedData = data;
    }

    // Decode by content so Steam/CDN file extension does not matter
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
        downloadResult = Error::ParseError;
        return downloadResult;
    }

    // Scale down only; avoid upscaling smaller source images
    QImage result;
    if (image.width() > targetSize.width() || image.height() > targetSize.height())
    {
        result = image.scaled(targetSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
    else
    {
        result = std::move(image);
    }

    // Write temp file first so failed saves do not corrupt existing cache file
    const QString tmpPath = QDir(TempDir()).filePath(QFileInfo(finalPath).fileName() + ".tmp");
    if (!result.save(tmpPath, "JPG", 90))
    {
        qWarning() << "Failed to save temp file:" << tmpPath;
        downloadResult = Error::FileSystemError;
        return downloadResult;
    }

    // Replace final cache file atomically from temp path where possible
    QFile::remove(finalPath);
    if (!QFile::rename(tmpPath, finalPath))
    {
        QFile::remove(tmpPath);
        qWarning() << "Failed to move image to:" << finalPath;
        downloadResult = Error::FileSystemError;
    }

    qDebug() << "Saved:" << finalPath;
    cachedPath = finalPath;
    return downloadResult;
}

/////////////////////////////////////////////////////////////////////

void ImageCacheManager::ClearMemoryCache()
{
    // Drop in-memory source payload after hydration batch completes
    m_lastDownloadedUrl.clear();
    m_lastDownloadedData.clear();
}

/////////////////////////////////////////////////////////////////////
///////////////////////////// PRIVATE ///////////////////////////////
/////////////////////////////////////////////////////////////////////

QString ImageCacheManager::TempDir() const
{
    // Use OS temp location for intermediate image writes
    QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    return tempDir;
}

/////////////////////////////////////////////////////////////////////

QString ImageCacheManager::BuildFinalPath(const QString &savePath, const QString &newName, const QString &url, const QSize &targetSize) const
{
    QString finalPath = "";
    QString stem = "";

    // Prefer caller-provided asset name; otherwise hash URL and requested size
    if (!newName.isEmpty())
    {
        stem = newName;
    }
    else
    {
        const QString input = url + QString::number(targetSize.width()) + "x" + QString::number(targetSize.height());
        stem = QCryptographicHash::hash(input.toUtf8(), QCryptographicHash::Sha1).toHex();
    }

    finalPath = QDir(savePath).filePath(stem + ".jpg");
    return finalPath;
}
