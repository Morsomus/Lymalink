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

    bool DeleteFile(const QString &filePath);
    bool DeleteFolder(const QString &folderPath);
    bool MoveFile(const QString &sourceFilePath, const QString &destinationFilePath);
    bool MoveFolder(const QString &sourceFolderPath, const QString &destinationFolderPath);
    bool RenameFile(const QString &filePath, const QString &newFilePath);
    bool RenameFolder(const QString &folderPath, const QString &newFolderPath);
    QStringList FileListCreate(const QString &folderPath);
    QStringList FolderListCreate(const QString &folderPath);

private:
    bool CopyFolder(const QString &sourceFolderPath, const QString &destinationFolderPath);
};
