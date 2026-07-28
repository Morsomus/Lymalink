/////////////////////////////////////////////////////////
// File: AppIdDirectoryFinder.cpp
// Date: 2026-07-28
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Implements frontend APPID directory finder
/////////////////////////////////////////////////////////

#include "AppIdDirectoryFinder.h"

#include <QDeadlineTimer>
#include <QDebug>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QObject>
#include <QVariantMap>

#include <algorithm>

#if defined(Q_OS_WIN)
    #include <QProcessEnvironment>
#endif

/////////////////////////////////////////////////////////////////////

AppIdDirectoryFinder::AppIdDirectoryFinder()
{
    // Constructor
}

AppIdDirectoryFinder::~AppIdDirectoryFinder()
{
    // Destructor
}

/////////////////////////////////////////////////////////////////////
////////////////////////////// PUBLIC ///////////////////////////////
/////////////////////////////////////////////////////////////////////

QVariantList AppIdDirectoryFinder::Search(const QString &rootPath, bool *timedOut, QString *error) const
{
    // Reset output state for each one-shot search request
    if (timedOut)
    {
        *timedOut = false;
    }
    if (error)
    {
        error->clear();
    }

    QVariantList results;
    QSet<QString> seen;
    const int findTimeoutMs = 30000;
    const QDeadlineTimer deadline(findTimeoutMs);

#if defined(Q_OS_WIN)
    Q_UNUSED(rootPath);

    // Windows uses known emulator storage roots - Linux root is user-selected
    qDebug() << "AppIdDirectoryFinder::Search: search start mode=Windows system roots timeoutMs=" << findTimeoutMs;
    SearchWindowsAppDataRoot(EnvPath(QStringLiteral("APPDATA")), results, seen, deadline, timedOut);
    if (!DeadlineExpired(deadline, timedOut))
    {
        SearchWindowsAppDataRoot(EnvPath(QStringLiteral("LOCALAPPDATA")), results, seen, deadline, timedOut);
    }
    if (!DeadlineExpired(deadline, timedOut))
    {
        SearchWindowsPublicDocuments(results, seen, deadline, timedOut);
    }
    if (!DeadlineExpired(deadline, timedOut))
    {
        SearchWindowsReloaded(results, seen, deadline, timedOut);
    }
#else
    // Linux prefixes can live anywhere, so only search the selected root
    qDebug() << "AppIdDirectoryFinder::Search: search start mode=Linux root=" << rootPath << "timeoutMs=" << findTimeoutMs;
    SearchLinuxRoot(rootPath, results, seen, deadline, timedOut, error);
#endif

    // Keep result order stable for repeat finder runs and UI display
    SortResults(results);
    qDebug() << "AppIdDirectoryFinder::Search: search finish count=" << results.size() << "timedOut=" << (timedOut ? *timedOut : false) << "error=" << (error ? *error : QString());
    return results;
}

/////////////////////////////////////////////////////////////////////
///////////////////////////// PRIVATE ///////////////////////////////
/////////////////////////////////////////////////////////////////////

bool AppIdDirectoryFinder::DeadlineExpired(const QDeadlineTimer &deadline, bool *timedOut) const
{
    // Shared timeout guard for all traversal loops
    if (!deadline.hasExpired())
    {
        return false;
    }

    if (timedOut)
    {
        *timedOut = true;
    }

    qWarning() << "AppIdDirectoryFinder::DeadlineExpired: finder timeout reached";
    return true;
}

/////////////////////////////////////////////////////////////////////

bool AppIdDirectoryFinder::IsNumericAppId(const QString &name) const
{
    // APPID folder names must be positive canonical integer text
    bool ok = false;
    const qlonglong appId = name.toLongLong(&ok);
    bool isNumericAppId = false;

    if (ok)
    {
        const QString normalizedAppId = QString::number(appId);
        isNumericAppId = appId > 0;
        if (isNumericAppId)
        {
            isNumericAppId = normalizedAppId == name;
        }
    }

    return isNumericAppId;
}

/////////////////////////////////////////////////////////////////////

QString AppIdDirectoryFinder::DetectEmulatorTypeFromFolderName(const QString &folderName) const
{
    // Match backend emulator folder aliases
    const QString lower = folderName.toLower();

    if (lower == QStringLiteral("codex"))
        return QStringLiteral("CODEX");
    else if (lower == QStringLiteral("rune"))
        return QStringLiteral("RUNE");
    else if (lower == QStringLiteral("empress"))
        return QStringLiteral("EMPRESS");
    else if (lower == QStringLiteral("skidrow") || lower == QStringLiteral("skid-row"))
        return QStringLiteral("SKIDROW");
    else if (lower == QStringLiteral("onlinefix") || lower == QStringLiteral("online-fix"))
        return QStringLiteral("OnlineFix");
    else if (lower == QStringLiteral("goldberg") || lower.contains(QStringLiteral("goldberg")) ||
             lower == QStringLiteral("gse saves") || lower == QStringLiteral("gsesaves") ||
             lower == QStringLiteral("goldberg steamemu saves"))
        return QStringLiteral("GOLDBERG");
    else if (lower == QStringLiteral("smartsteamemu") || lower == QStringLiteral("sse"))
        return QStringLiteral("SmartSteamEmu");
    else if (lower == QStringLiteral("creamapi") || lower == QStringLiteral("cream api"))
        return QStringLiteral("CreamAPI");
    else if (lower == QStringLiteral("rld!") || lower == QStringLiteral("rld") || lower.contains(QStringLiteral("reloaded")))
        return QStringLiteral("RLD");
    else if (lower == QStringLiteral(".1911") || lower == QStringLiteral("1911"))
        return QStringLiteral("1911");
    else if (lower == QStringLiteral("cpy"))
        return QStringLiteral("CPY");
    else if (lower == QStringLiteral("steampunks") || lower == QStringLiteral("steam punks"))
        return QStringLiteral("STEAMPUNKS");

    return QStringLiteral("UNKNOWN");
}

/////////////////////////////////////////////////////////////////////

QString AppIdDirectoryFinder::EmulatorLabel(const QString &emulatorType) const
{
    // UI labels stay separate from raw emulator type values
    const QString normalized = emulatorType.trimmed().toUpper();

    if (normalized == QStringLiteral("GOG-N"))
        return QStringLiteral("GOG-Nemirtingas");
    else if (normalized == QStringLiteral("GOLDBERG"))
        return QStringLiteral("Goldberg");
    else if (normalized == QStringLiteral("CODEX"))
        return QStringLiteral("Codex");
    else if (normalized == QStringLiteral("RUNE"))
        return QStringLiteral("Rune");
    else if (normalized == QStringLiteral("RLD"))
        return QStringLiteral("Reloaded");

    return QStringLiteral("-");
}

/////////////////////////////////////////////////////////////////////

void AppIdDirectoryFinder::AddResult(QVariantList &results, QSet<QString> &seen, const QString &appId, const QString &emulatorType, const QString &path) const
{
    // Ignore non-APPID folders before building result payload
    if (!IsNumericAppId(appId))
    {
        return;
    }

    // Deduplicate exact discoveries from overlapping roots
    const QString key = appId + QStringLiteral("|") + emulatorType + QStringLiteral("|") + path;
    if (seen.contains(key))
    {
        return;
    }

    seen.insert(key);
    results.append(QVariantMap{
        {QStringLiteral("appId"), appId},
        {QStringLiteral("emulatorType"), emulatorType},
        {QStringLiteral("emulatorLabel"), EmulatorLabel(emulatorType)},
        {QStringLiteral("path"), path}
    });

    qDebug() << "AppIdDirectoryFinder::AddResult: found APPID folder" << "appId=" << appId << "emulatorType=" << emulatorType << "path=" << path;
}

/////////////////////////////////////////////////////////////////////

void AppIdDirectoryFinder::SortResults(QVariantList &results) const
{
    // Sort by APPID first, then emulator type, then full path
    std::sort(results.begin(), results.end(), [](const QVariant &leftValue, const QVariant &rightValue) {
        const QVariantMap left = leftValue.toMap();
        const QVariantMap right = rightValue.toMap();
        const qlonglong leftId = left.value(QStringLiteral("appId")).toString().toLongLong();
        const qlonglong rightId = right.value(QStringLiteral("appId")).toString().toLongLong();
        if (leftId != rightId)
        {
            const bool leftIdIsBefore = leftId < rightId;
            return leftIdIsBefore;
        }

        const int typeCompare = QString::compare(
            left.value(QStringLiteral("emulatorType")).toString(),
            right.value(QStringLiteral("emulatorType")).toString(),
            Qt::CaseInsensitive
        );
        if (typeCompare != 0)
        {
            const bool typeIsBefore = typeCompare < 0;
            return typeIsBefore;
        }

        const int pathCompare = QString::compare(
            left.value(QStringLiteral("path")).toString(),
            right.value(QStringLiteral("path")).toString(),
            Qt::CaseInsensitive
        );

        const bool pathIsBefore = pathCompare < 0;
        return pathIsBefore;
    });
}

/////////////////////////////////////////////////////////////////////

bool AppIdDirectoryFinder::IsReloadedShape(const QFileInfo &info) const
{
    // RLD shape is ProgramData/Steam/<user>/<appid>
    const QDir appIdDir(info.absoluteFilePath());
    const QFileInfo userDir(appIdDir.absolutePath());
    const QFileInfo steamDir(QDir(userDir.absolutePath()).absolutePath());
    const QFileInfo programDataDir(QDir(steamDir.absolutePath()).absolutePath());
    bool isReloadedShape = false;

    if (steamDir.fileName() == QStringLiteral("Steam"))
    {
        isReloadedShape = programDataDir.fileName() == QStringLiteral("ProgramData");
    }

    return isReloadedShape;
}

/////////////////////////////////////////////////////////////////////

void AppIdDirectoryFinder::FindNumericChildrenOfKnownEmulatorDir(const QString &directoryPath, QVariantList &results, QSet<QString> &seen, const QDeadlineTimer &deadline, bool *timedOut) const
{
    // Known emulator folders store APPID directories as direct children
    const QFileInfo directoryInfo(directoryPath);
    const QString emulatorType = DetectEmulatorTypeFromFolderName(directoryInfo.fileName());
    if (emulatorType == QStringLiteral("UNKNOWN"))
    {
        return;
    }

    const QDir directory(directoryPath);
    const QFileInfoList children = directory.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const QFileInfo &child : children)
    {
        if (DeadlineExpired(deadline, timedOut))
        {
            return;
        }

        AddResult(results, seen, child.fileName(), emulatorType, child.absoluteFilePath());
    }
}

/////////////////////////////////////////////////////////////////////

#if defined(Q_OS_WIN)
QString AppIdDirectoryFinder::EnvPath(const QString &name) const
{
    // Read Windows roots from current process environment
    const QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    const QString path = environment.value(name);
    return path;
}

/////////////////////////////////////////////////////////////////////

void AppIdDirectoryFinder::SearchWindowsAppDataRoot(const QString &rootPath, QVariantList &results, QSet<QString> &seen, const QDeadlineTimer &deadline, bool *timedOut) const
{
    // AppData search is shallow: emulator folder, then APPID child folders
    if (rootPath.isEmpty())
    {
        return;
    }

    const QDir root(rootPath);
    if (!root.exists())
    {
        qWarning() << "AppIdDirectoryFinder::SearchWindowsAppDataRoot: Windows root missing:" << rootPath;
        return;
    }

    qDebug() << "AppIdDirectoryFinder::SearchWindowsAppDataRoot: searching Windows AppData root:" << rootPath;
    const QFileInfoList entries = root.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const QFileInfo &entry : entries)
    {
        if (DeadlineExpired(deadline, timedOut))
        {
            return;
        }

        FindNumericChildrenOfKnownEmulatorDir(entry.absoluteFilePath(), results, seen, deadline, timedOut);
    }
}

/////////////////////////////////////////////////////////////////////

void AppIdDirectoryFinder::SearchWindowsPublicDocuments(QVariantList &results, QSet<QString> &seen, const QDeadlineTimer &deadline, bool *timedOut) const
{
    // Public Documents can contain nested emulator save folders
    const QString rootPath = QStringLiteral("C:/Users/Public/Documents");
    const QDir root(rootPath);
    if (!root.exists())
    {
        qWarning() << "AppIdDirectoryFinder::SearchWindowsPublicDocuments: Windows Public Documents missing:" << rootPath;
        return;
    }

    qDebug() << "AppIdDirectoryFinder::SearchWindowsPublicDocuments: searching Windows Public Documents:" << rootPath;
    QDirIterator it(rootPath, QDir::Dirs | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    while (it.hasNext())
    {
        if (DeadlineExpired(deadline, timedOut))
        {
            return;
        }

        FindNumericChildrenOfKnownEmulatorDir(it.next(), results, seen, deadline, timedOut);
    }
}

/////////////////////////////////////////////////////////////////////

void AppIdDirectoryFinder::SearchWindowsReloaded(QVariantList &results, QSet<QString> &seen, const QDeadlineTimer &deadline, bool *timedOut) const
{
    const QString rootPath = QStringLiteral("C:/ProgramData/Steam");
    const QDir root(rootPath);
    if (!root.exists())
    {
        qWarning() << "AppIdDirectoryFinder::SearchWindowsReloaded: Windows RLD root missing:" << rootPath;
        return;
    }

    qDebug() << "AppIdDirectoryFinder::SearchWindowsReloaded: searching Windows RLD root:" << rootPath;
    const QFileInfoList users = root.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const QFileInfo &user : users)
    {
        if (DeadlineExpired(deadline, timedOut))
        {
            return;
        }

        const QDir userDir(user.absoluteFilePath());
        const QFileInfoList appIdDirs = userDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
        for (const QFileInfo &appIdDir : appIdDirs)
        {
            if (DeadlineExpired(deadline, timedOut))
            {
                return;
            }

            AddResult(results, seen, appIdDir.fileName(), QStringLiteral("RLD"), appIdDir.absoluteFilePath());
        }
    }
}

#else

/////////////////////////////////////////////////////////////////////

bool AppIdDirectoryFinder::ShouldSkipLinuxDirectory(const QFileInfo &info) const
{
    // Avoid Wine prefix loops and helper folders during recursive search
    if (info.isSymLink())
    {
        qDebug() << "AppIdDirectoryFinder::ShouldSkipLinuxDirectory: skipped symlink:" << info.absoluteFilePath();
        return true;
    }

    const QString name = info.fileName();
    if (name == QStringLiteral("dosdevices") || name == QStringLiteral("pfx"))
    {
        qDebug() << "AppIdDirectoryFinder::ShouldSkipLinuxDirectory: skipped prefix helper dir:" << info.absoluteFilePath();
        return true;
    }

    return false;
}

/////////////////////////////////////////////////////////////////////

void AppIdDirectoryFinder::SearchLinuxDirectory(const QString &directoryPath, QVariantList &results, QSet<QString> &seen, const QDeadlineTimer &deadline, bool *timedOut) const
{
    // Recursively search selected Linux root because prefix locations are not knowable
    if (DeadlineExpired(deadline, timedOut))
    {
        return;
    }

    const QDir directory(directoryPath);
    const QFileInfo directoryInfo(directoryPath);
    if (!directoryInfo.isReadable())
    {
        qWarning() << "AppIdDirectoryFinder::SearchLinuxDirectory: skipped inaccessible folder:" << directoryPath;
        return;
    }

    const QFileInfoList entries = directory.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const QFileInfo &entry : entries)
    {
        if (DeadlineExpired(deadline, timedOut))
        {
            return;
        }

        if (ShouldSkipLinuxDirectory(entry))
        {
            continue;
        }

        if (!entry.isReadable())
        {
            qWarning() << "AppIdDirectoryFinder::SearchLinuxDirectory: skipped inaccessible folder:" << entry.absoluteFilePath();
            continue;
        }

        const QString appId = entry.fileName();
        if (IsNumericAppId(appId))
        {
            if (IsReloadedShape(entry))
            {
                AddResult(results, seen, appId, QStringLiteral("RLD"), entry.absoluteFilePath());
            }
            else
            {
                const QFileInfo parentInfo(entry.absolutePath());
                AddResult(results, seen, appId, DetectEmulatorTypeFromFolderName(parentInfo.fileName()), entry.absoluteFilePath());
            }
        }

        SearchLinuxDirectory(entry.absoluteFilePath(), results, seen, deadline, timedOut);
    }
}

/////////////////////////////////////////////////////////////////////

void AppIdDirectoryFinder::SearchLinuxRoot(const QString &rootPath, QVariantList &results, QSet<QString> &seen, const QDeadlineTimer &deadline, bool *timedOut, QString *error) const
{
    // Linux search requires explicit user-selected root folder
    const QString trimmedRoot = rootPath.trimmed();
    if (trimmedRoot.isEmpty())
    {
        if (error)
        {
            *error = QObject::tr("Select a directory to search.");
        }

        qWarning() << "AppIdDirectoryFinder::SearchLinuxRoot: missing Linux search root";
        return;
    }

    const QDir root(trimmedRoot);
    if (!root.exists())
    {
        if (error)
        {
            *error = QObject::tr("Selected search directory does not exist.");
        }

        qWarning() << "AppIdDirectoryFinder::SearchLinuxRoot: Linux root missing:" << trimmedRoot;
        return;
    }

    qDebug() << "AppIdDirectoryFinder::SearchLinuxRoot: searching Linux root:" << trimmedRoot;
    SearchLinuxDirectory(trimmedRoot, results, seen, deadline, timedOut);
}
#endif
