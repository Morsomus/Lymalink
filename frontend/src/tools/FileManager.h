/////////////////////////////////////////////////////////
// File: FileManager.h
// Date: 2026-05-14
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Declares FileManager class
/////////////////////////////////////////////////////////

#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

class FileManager : public QObject
{
    Q_OBJECT

public:
    explicit FileManager(QObject *parent = nullptr);
    ~FileManager();

    bool DeleteFile(const QString &filePath) const;
    bool DeleteFolder(const QString &folderPath) const;
    bool MoveFile(const QString &sourceFilePath, const QString &destinationFilePath) const;
    bool MoveFolder(const QString &sourceFolderPath, const QString &destinationFolderPath) const;
    bool RenameFile(const QString &filePath, const QString &newFilePath) const;
    bool RenameFolder(const QString &folderPath, const QString &newFolderPath) const;
    QStringList FileListCreate(const QString &folderPath) const;
    QStringList FolderListCreate(const QString &folderPath) const;
    QString LocalFileSource(const QString &filePath) const;
    QString FirstImageInDirectory(const QString &directoryPath, bool logMissingDir = true) const;

private:
    bool CopyFolder(const QString &sourceFolderPath, const QString &destinationFolderPath) const;
};
