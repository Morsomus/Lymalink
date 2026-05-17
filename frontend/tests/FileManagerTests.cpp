/////////////////////////////////////////////////////////
// File: FileManagerTests.cpp
// Date: 2026-05-14
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Tests FileManager filesystem helpers
/////////////////////////////////////////////////////////

#include "../src/tools/FileManager.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QtTest/QtTest>

class FileManagerTests : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();        // runs once before all tests
    // void cleanupTestCase();     // runs once after all tests (optional)
    // void init();                // runs before EACH test (optional)
    // void cleanup();             // runs after EACH test (optional)

    void deleteFile_removesExistingFile();
    void deleteFolder_removesNestedFolder();
    void moveFile_movesFile();
    void moveFolder_movesNestedFolder();
    void renameFile_renamesFile();
    void renameFolder_renamesFolder();
    void fileListCreate_returnsOnlyFiles();
    void folderListCreate_returnsOnlyFolders();

private:
    bool createFile(const QString &filePath, const QByteArray &data = "test data");
};

/////////////////////////////////////////////////////////////////////

void FileManagerTests::initTestCase()
{
    qputenv("QT_LOGGING_RULES", "*.debug=true");
    // qDebug() << "========= FileManager test suite starting =========";
}

// void FileManagerTests::cleanupTestCase()
// {
//     qDebug() << "========= FileManager test suite finished =========";
// }

// void FileManagerTests::init()
// {
//     qDebug() << "-- Starting:" << QTest::currentTestFunction();
// }

// void FileManagerTests::cleanup()
// {
//     qDebug() << "-- Finished:" << QTest::currentTestFunction() << (QTest::currentTestFailed() ? "[FAILED]" : "[PASSED]");
// }

/////////////////////////////////////////////////////////////////////

void FileManagerTests::deleteFile_removesExistingFile()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    FileManager fileManager;
    const QString filePath = QDir(tempDir.path()).filePath("delete.txt");
    QVERIFY(createFile(filePath));

    qDebug() << "Attempting to delete:" << filePath;
    QVERIFY(fileManager.DeleteFile(filePath));
    qDebug() << "File exists after delete:" << QFileInfo::exists(filePath);
}

/////////////////////////////////////////////////////////////////////

void FileManagerTests::deleteFolder_removesNestedFolder()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    FileManager fileManager;
    const QString folderPath = QDir(tempDir.path()).filePath("folder");
    const QString nestedPath = QDir(folderPath).filePath("nested");
    QVERIFY(QDir().mkpath(nestedPath));
    QVERIFY(createFile(QDir(nestedPath).filePath("data.txt")));

    QVERIFY(fileManager.DeleteFolder(folderPath));
    QVERIFY(!QFileInfo::exists(folderPath));
}

/////////////////////////////////////////////////////////////////////

void FileManagerTests::moveFile_movesFile()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    FileManager fileManager;
    QDir root(tempDir.path());
    const QString sourcePath = root.filePath("source.txt");
    const QString destinationPath = root.filePath("destination.txt");
    QVERIFY(createFile(sourcePath, "move file"));

    QVERIFY(fileManager.MoveFile(sourcePath, destinationPath));
    QVERIFY(!QFileInfo::exists(sourcePath));
    QVERIFY(QFileInfo::exists(destinationPath));
}

/////////////////////////////////////////////////////////////////////

void FileManagerTests::moveFolder_movesNestedFolder()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    FileManager fileManager;
    QDir root(tempDir.path());
    const QString sourcePath = root.filePath("source-folder");
    const QString destinationPath = root.filePath("destination-folder");
    const QString nestedPath = QDir(sourcePath).filePath("nested");
    QVERIFY(QDir().mkpath(nestedPath));
    QVERIFY(createFile(QDir(nestedPath).filePath("data.txt"), "move folder"));

    QVERIFY(fileManager.MoveFolder(sourcePath, destinationPath));
    QVERIFY(!QFileInfo::exists(sourcePath));
    QVERIFY(QFileInfo::exists(QDir(destinationPath).filePath("nested/data.txt")));
}

/////////////////////////////////////////////////////////////////////

void FileManagerTests::renameFile_renamesFile()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    FileManager fileManager;
    QDir root(tempDir.path());
    const QString oldPath = root.filePath("old.txt");
    const QString newPath = root.filePath("new.txt");
    QVERIFY(createFile(oldPath));

    QVERIFY(fileManager.RenameFile(oldPath, newPath));
    QVERIFY(!QFileInfo::exists(oldPath));
    QVERIFY(QFileInfo::exists(newPath));
}

/////////////////////////////////////////////////////////////////////

void FileManagerTests::renameFolder_renamesFolder()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    FileManager fileManager;
    QDir root(tempDir.path());
    const QString oldPath = root.filePath("old-folder");
    const QString newPath = root.filePath("new-folder");
    QVERIFY(QDir().mkpath(oldPath));

    QVERIFY(fileManager.RenameFolder(oldPath, newPath));
    QVERIFY(!QFileInfo::exists(oldPath));
    QVERIFY(QFileInfo::exists(newPath));
}

/////////////////////////////////////////////////////////////////////

void FileManagerTests::fileListCreate_returnsOnlyFiles()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    FileManager fileManager;
    QDir root(tempDir.path());
    const QString firstFile = root.filePath("a.txt");
    const QString secondFile = root.filePath("b.txt");
    QVERIFY(createFile(firstFile));
    QVERIFY(createFile(secondFile));
    QVERIFY(QDir().mkpath(root.filePath("folder")));

    const QStringList files = fileManager.FileListCreate(tempDir.path());
    QCOMPARE(files.size(), 2);
    QVERIFY(files.contains(QFileInfo(firstFile).absoluteFilePath()));
    QVERIFY(files.contains(QFileInfo(secondFile).absoluteFilePath()));
}

/////////////////////////////////////////////////////////////////////

void FileManagerTests::folderListCreate_returnsOnlyFolders()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    FileManager fileManager;
    QDir root(tempDir.path());
    const QString firstFolder = root.filePath("folder-a");
    const QString secondFolder = root.filePath("folder-b");
    QVERIFY(QDir().mkpath(firstFolder));
    QVERIFY(QDir().mkpath(secondFolder));
    QVERIFY(createFile(root.filePath("file.txt")));

    const QStringList folders = fileManager.FolderListCreate(tempDir.path());
    QCOMPARE(folders.size(), 2);
    QVERIFY(folders.contains(QFileInfo(firstFolder).absoluteFilePath()));
    QVERIFY(folders.contains(QFileInfo(secondFolder).absoluteFilePath()));
}


/////////////////////////////////////////////////////////////////////
///////////////////////////// PRIVATE ///////////////////////////////
/////////////////////////////////////////////////////////////////////

bool FileManagerTests::createFile(const QString &filePath, const QByteArray &data)
{
    QFile file(filePath);

    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }

    return file.write(data) == data.size();
}

/////////////////////////////////////////////////////////////////////

QTEST_MAIN(FileManagerTests)
#include "FileManagerTests.moc"
