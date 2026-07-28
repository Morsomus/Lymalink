/////////////////////////////////////////////////////////
// File: AppIdDirectoryFinder.h
// Date: 2026-07-28
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Declares frontend APPID directory finder
/////////////////////////////////////////////////////////

#pragma once

#include <QSet>
#include <QString>
#include <QVariantList>

class QDeadlineTimer;
class QFileInfo;

class AppIdDirectoryFinder
{
public:
    AppIdDirectoryFinder();
    ~AppIdDirectoryFinder();

    QVariantList Search(const QString &rootPath, bool *timedOut, QString *error) const;

private:
    bool DeadlineExpired(const QDeadlineTimer &deadline, bool *timedOut) const;
    bool IsNumericAppId(const QString &name) const;
    QString DetectEmulatorTypeFromFolderName(const QString &folderName) const;
    QString EmulatorLabel(const QString &emulatorType) const;
    void AddResult(QVariantList &results, QSet<QString> &seen, const QString &appId, const QString &emulatorType, const QString &path) const;
    void SortResults(QVariantList &results) const;
    bool IsReloadedShape(const QFileInfo &info) const;
    void FindNumericChildrenOfKnownEmulatorDir(const QString &directoryPath, QVariantList &results, QSet<QString> &seen, const QDeadlineTimer &deadline, bool *timedOut) const;

#if defined(Q_OS_WIN)
    QString EnvPath(const QString &name) const;
    void SearchWindowsAppDataRoot(const QString &rootPath, QVariantList &results, QSet<QString> &seen, const QDeadlineTimer &deadline, bool *timedOut) const;
    void SearchWindowsPublicDocuments(QVariantList &results, QSet<QString> &seen, const QDeadlineTimer &deadline, bool *timedOut) const;
    void SearchWindowsReloaded(QVariantList &results, QSet<QString> &seen, const QDeadlineTimer &deadline, bool *timedOut) const;
#else
    bool ShouldSkipLinuxDirectory(const QFileInfo &info) const;
    void SearchLinuxDirectory(const QString &directoryPath, QVariantList &results, QSet<QString> &seen, const QDeadlineTimer &deadline, bool *timedOut) const;
    void SearchLinuxRoot(const QString &rootPath, QVariantList &results, QSet<QString> &seen, const QDeadlineTimer &deadline, bool *timedOut, QString *error) const;
#endif
};
