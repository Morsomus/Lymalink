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

bool FileManager::DeleteFile(const QString &filePath) const
{
    bool fileDeleted = false;

    // Delete single file and report QFile error when removal fails
    QFile file(filePath);
    if (file.remove())
    {
        qDebug() << "Successfully deleted file:" << filePath;
        fileDeleted = true;
        return fileDeleted;
    }

    qDebug() << "Failed to delete file:" << filePath << "Error:" << file.errorString();
    return fileDeleted;
}

/////////////////////////////////////////////////////////////////////

bool FileManager::DeleteFolder(const QString &folderPath) const
{
    bool folderDeleted = false;

    // Remove folder recursively because target folders can contain nested files
    QDir folder(folderPath);
    if (folder.removeRecursively())
    {
        qDebug() << "Successfully deleted folder:" << folderPath;
        folderDeleted = true;
        return folderDeleted;
    }

    qDebug() << "Failed to delete folder:" << folderPath;
    return folderDeleted;
}

/////////////////////////////////////////////////////////////////////

bool FileManager::MoveFile(const QString &sourceFilePath, const QString &destinationFilePath) const
{
    bool fileMoved = false;

    QFile sourceFile(sourceFilePath);

    // Rename first, then copy for cross-device moves.
    if (sourceFile.rename(destinationFilePath))
    {
        qDebug() << "Successfully moved file:" << sourceFilePath << "to" << destinationFilePath;
        fileMoved = true;
        return fileMoved;
    }

    if (!QFile::copy(sourceFilePath, destinationFilePath))
    {
        qDebug() << "Failed to move file:" << sourceFilePath << "to" << destinationFilePath;
        return fileMoved;
    }

    // Remove source only after destination copy succeeds
    if (QFile::remove(sourceFilePath))
    {
        qDebug() << "Successfully moved file:" << sourceFilePath << "to" << destinationFilePath;
        fileMoved = true;
        return fileMoved;
    }

    // Roll back copied destination when source cleanup fails
    QFile::remove(destinationFilePath);
    qDebug() << "Failed to remove source file after copy:" << sourceFilePath;
    return fileMoved;
}

/////////////////////////////////////////////////////////////////////

bool FileManager::MoveFolder(const QString &sourceFolderPath, const QString &destinationFolderPath) const
{
    bool folderMoved = false;

    QDir folder;

    // Rename first, then copy for cross-device moves.
    if (folder.rename(sourceFolderPath, destinationFolderPath))
    {
        qDebug() << "Successfully moved folder:" << sourceFolderPath << "to" << destinationFolderPath;
        folderMoved = true;
        return folderMoved;
    }

    if (!CopyFolder(sourceFolderPath, destinationFolderPath))
    {
        // Remove partial destination tree after failed recursive copy
        QDir(destinationFolderPath).removeRecursively();
        qDebug() << "Failed to move folder:" << sourceFolderPath << "to" << destinationFolderPath;
        return folderMoved;
    }

    // Remove source only after recursive copy succeeds
    if (QDir(sourceFolderPath).removeRecursively())
    {
        qDebug() << "Successfully moved folder:" << sourceFolderPath << "to" << destinationFolderPath;
        folderMoved = true;
        return folderMoved;
    }

    qDebug() << "Failed to remove source folder after copy:" << sourceFolderPath;
    return folderMoved;
}

/////////////////////////////////////////////////////////////////////

bool FileManager::RenameFile(const QString &filePath, const QString &newFilePath) const
{
    bool fileRenamed = false;

    // Rename file in-place and preserve QFile error text
    QFile file(filePath);
    if (file.rename(newFilePath))
    {
        qDebug() << "Successfully renamed file:" << filePath << "to" << newFilePath;
        fileRenamed = true;
        return fileRenamed;
    }

    qDebug() << "Failed to rename file:" << filePath << "to" << newFilePath << "Error:" << file.errorString();
    return fileRenamed;
}

/////////////////////////////////////////////////////////////////////

bool FileManager::RenameFolder(const QString &folderPath, const QString &newFolderPath) const
{
    bool folderRenamed = false;

    // Rename folder in-place through QDir
    QDir folder;
    if (folder.rename(folderPath, newFolderPath))
    {
        qDebug() << "Successfully renamed folder:" << folderPath << "to" << newFolderPath;
        folderRenamed = true;
        return folderRenamed;
    }

    qDebug() << "Failed to rename folder:" << folderPath << "to" << newFolderPath;
    return folderRenamed;
}

/////////////////////////////////////////////////////////////////////

QStringList FileManager::FileListCreate(const QString &folderPath) const
{
    QStringList fileList;

    // Return absolute paths for files directly under target folder
    QDir folder(folderPath);
    const QFileInfoList entries = folder.entryInfoList(QDir::Files | QDir::NoDotAndDotDot, QDir::Name);
    for (const QFileInfo &entry : entries)
    {
        fileList.append(entry.absoluteFilePath());
    }

    return fileList;
}

/////////////////////////////////////////////////////////////////////

QStringList FileManager::FolderListCreate(const QString &folderPath) const
{
    QStringList folderList;

    // Return absolute paths for child folders directly under target folder
    QDir folder(folderPath);
    const QFileInfoList entries = folder.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const QFileInfo &entry : entries)
    {
        folderList.append(entry.absoluteFilePath());
    }

    return folderList;
}

/////////////////////////////////////////////////////////////////////

QString FileManager::LocalFileSource(const QString &filePath) const
{
    QString fileSource = "";

    // Convert filesystem path into QML-friendly file URL
    if (!filePath.isEmpty())
    {
        fileSource = QUrl::fromLocalFile(filePath).toString();
    }

    return fileSource;
}

/////////////////////////////////////////////////////////////////////

QString FileManager::FirstImageInDirectory(const QString &directoryPath) const
{
    QString imagePath = "";

    // Search only existing directories for supported image extensions
    QDir directory(directoryPath);
    if (!directory.exists())
    {
        return imagePath;
    }

    const QStringList imageFilters = {
        "*.jpg", "*.jpeg", "*.png", "*.webp", "*.bmp"
    };

    const QFileInfoList imageFiles = directory.entryInfoList(imageFilters, QDir::Files, QDir::Name | QDir::IgnoreCase);
    if (!imageFiles.isEmpty())
    {
        imagePath = LocalFileSource(imageFiles.first().absoluteFilePath());
    }

    return imagePath;
}

/////////////////////////////////////////////////////////////////////
///////////////////////////// PRIVATE ///////////////////////////////
/////////////////////////////////////////////////////////////////////

bool FileManager::CopyFolder(const QString &sourceFolderPath, const QString &destinationFolderPath) const
{
    bool folderCopied = false;

    // Validate source before creating destination tree
    QDir sourceFolder(sourceFolderPath);
    if (!sourceFolder.exists())
    {
        return folderCopied;
    }

    QDir destinationFolder(destinationFolderPath);
    if (!destinationFolder.exists() && !QDir().mkpath(destinationFolderPath))
    {
        return folderCopied;
    }

    // Copy all files and folders recursively into destination
    const QFileInfoList entries = sourceFolder.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QFileInfo &entry : entries)
    {
        const QString sourcePath = entry.absoluteFilePath();
        const QString destinationPath = destinationFolder.filePath(entry.fileName());
        if (entry.isDir())
        {
            if (!CopyFolder(sourcePath, destinationPath))
            {
                return folderCopied;
            }
        }
        else if (!QFile::copy(sourcePath, destinationPath))
        {
            return folderCopied;
        }
    }

    folderCopied = true;
    return folderCopied;
}
