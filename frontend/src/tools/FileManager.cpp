/////////////////////////////////////////////////////////
// File: FileManager.cpp
// Date: 2026-05-14
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Implements FileManager class
/////////////////////////////////////////////////////////

#include "FileManager.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QUrl>

/////////////////////////////////////////////////////////////////////

FileManager::FileManager(QObject *parent) : QObject(parent)
{
    // Constructor
}

FileManager::~FileManager()
{
    // Destructor
}

/////////////////////////////////////////////////////////////////////
////////////////////////////// PUBLIC ///////////////////////////////
/////////////////////////////////////////////////////////////////////

bool FileManager::DeleteFile(const QString &filePath)
{
    QFile file(filePath);
    if (file.remove())
    {
        qDebug() << "Successfully deleted file:" << filePath;
        return true;
    }

    qDebug() << "Failed to delete file:" << filePath << "Error:" << file.errorString();
    return false;
}

/////////////////////////////////////////////////////////////////////

bool FileManager::DeleteFolder(const QString &folderPath)
{
    QDir folder(folderPath);
    if (folder.removeRecursively())
    {
        qDebug() << "Successfully deleted folder:" << folderPath;
        return true;
    }

    qDebug() << "Failed to delete folder:" << folderPath;
    return false;
}

/////////////////////////////////////////////////////////////////////

bool FileManager::MoveFile(const QString &sourceFilePath, const QString &destinationFilePath)
{
    QFile sourceFile(sourceFilePath);

    // Rename first, then copy for cross-device moves.
    if (sourceFile.rename(destinationFilePath))
    {
        qDebug() << "Successfully moved file:" << sourceFilePath << "to" << destinationFilePath;
        return true;
    }

    if (!QFile::copy(sourceFilePath, destinationFilePath))
    {
        qDebug() << "Failed to move file:" << sourceFilePath << "to" << destinationFilePath;
        return false;
    }

    if (QFile::remove(sourceFilePath))
    {
        qDebug() << "Successfully moved file:" << sourceFilePath << "to" << destinationFilePath;
        return true;
    }

    QFile::remove(destinationFilePath);
    qDebug() << "Failed to remove source file after copy:" << sourceFilePath;
    return false;
}

/////////////////////////////////////////////////////////////////////

bool FileManager::MoveFolder(const QString &sourceFolderPath, const QString &destinationFolderPath)
{
    QDir folder;

    // Rename first, then copy for cross-device moves.
    if (folder.rename(sourceFolderPath, destinationFolderPath))
    {
        qDebug() << "Successfully moved folder:" << sourceFolderPath << "to" << destinationFolderPath;
        return true;
    }

    if (!CopyFolder(sourceFolderPath, destinationFolderPath))
    {
        QDir(destinationFolderPath).removeRecursively();
        qDebug() << "Failed to move folder:" << sourceFolderPath << "to" << destinationFolderPath;
        return false;
    }

    if (QDir(sourceFolderPath).removeRecursively())
    {
        qDebug() << "Successfully moved folder:" << sourceFolderPath << "to" << destinationFolderPath;
        return true;
    }

    qDebug() << "Failed to remove source folder after copy:" << sourceFolderPath;
    return false;
}

/////////////////////////////////////////////////////////////////////

bool FileManager::RenameFile(const QString &filePath, const QString &newFilePath)
{
    QFile file(filePath);
    if (file.rename(newFilePath))
    {
        qDebug() << "Successfully renamed file:" << filePath << "to" << newFilePath;
        return true;
    }

    qDebug() << "Failed to rename file:" << filePath << "to" << newFilePath << "Error:" << file.errorString();
    return false;
}

/////////////////////////////////////////////////////////////////////

bool FileManager::RenameFolder(const QString &folderPath, const QString &newFolderPath)
{
    QDir folder;
    if (folder.rename(folderPath, newFolderPath))
    {
        qDebug() << "Successfully renamed folder:" << folderPath << "to" << newFolderPath;
        return true;
    }

    qDebug() << "Failed to rename folder:" << folderPath << "to" << newFolderPath;
    return false;
}

/////////////////////////////////////////////////////////////////////

QStringList FileManager::FileListCreate(const QString &folderPath)
{
    QStringList fileList;
    QDir folder(folderPath);
    const QFileInfoList entries = folder.entryInfoList(QDir::Files | QDir::NoDotAndDotDot, QDir::Name);
    for (const QFileInfo &entry : entries)
    {
        fileList.append(entry.absoluteFilePath());
    }

    return fileList;
}

/////////////////////////////////////////////////////////////////////

QStringList FileManager::FolderListCreate(const QString &folderPath)
{
    QStringList folderList;
    QDir folder(folderPath);
    const QFileInfoList entries = folder.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const QFileInfo &entry : entries)
    {
        folderList.append(entry.absoluteFilePath());
    }

    return folderList;
}

/////////////////////////////////////////////////////////////////////

QString FileManager::LocalFileSource(const QString &filePath)
{
    return filePath.isEmpty() ? QString() : QUrl::fromLocalFile(filePath).toString();
}

/////////////////////////////////////////////////////////////////////

QString FileManager::FirstImageInDirectory(const QString &directoryPath)
{
    QDir directory(directoryPath);
    if (!directory.exists())
    {
        return QString();
    }

    const QStringList imageFilters = {
        "*.jpg", "*.jpeg", "*.png", "*.webp", "*.bmp"
    };

    const QFileInfoList imageFiles = directory.entryInfoList(imageFilters, QDir::Files, QDir::Name | QDir::IgnoreCase);
    return imageFiles.isEmpty() ? QString() : LocalFileSource(imageFiles.first().absoluteFilePath());
}

/////////////////////////////////////////////////////////////////////
///////////////////////////// PRIVATE ///////////////////////////////
/////////////////////////////////////////////////////////////////////

bool FileManager::CopyFolder(const QString &sourceFolderPath, const QString &destinationFolderPath)
{
    QDir sourceFolder(sourceFolderPath);
    if (!sourceFolder.exists())
    {
        return false;
    }

    QDir destinationFolder(destinationFolderPath);
    if (!destinationFolder.exists() && !QDir().mkpath(destinationFolderPath))
    {
        return false;
    }

    const QFileInfoList entries = sourceFolder.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QFileInfo &entry : entries)
    {
        const QString sourcePath = entry.absoluteFilePath();
        const QString destinationPath = destinationFolder.filePath(entry.fileName());
        if (entry.isDir())
        {
            if (!CopyFolder(sourcePath, destinationPath))
            {
                return false;
            }
        }
        else if (!QFile::copy(sourcePath, destinationPath))
        {
            return false;
        }
    }

    return true;
}
