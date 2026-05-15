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

    Q_INVOKABLE bool DeleteFile(const QString &filePath);
    Q_INVOKABLE bool DeleteFolder(const QString &folderPath);
    Q_INVOKABLE bool MoveFile(const QString &sourceFilePath, const QString &destinationFilePath);
    Q_INVOKABLE bool MoveFolder(const QString &sourceFolderPath, const QString &destinationFolderPath);
    Q_INVOKABLE bool RenameFile(const QString &filePath, const QString &newFilePath);
    Q_INVOKABLE bool RenameFolder(const QString &folderPath, const QString &newFolderPath);
    Q_INVOKABLE QStringList FileListCreate(const QString &folderPath);
    Q_INVOKABLE QStringList FolderListCreate(const QString &folderPath);

signals:
    void signalFileDeleted(const QString &filePath, bool success);

private:
    bool CopyFolder(const QString &sourceFolderPath, const QString &destinationFolderPath);
};
