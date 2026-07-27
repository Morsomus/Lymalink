/////////////////////////////////////////////////////////
// File: PathScanner.cpp
// Date: 2026-05-22
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Implements PathScanner class for scanning
//              specific paths for target files
/////////////////////////////////////////////////////////

#include "PathScanner.h"
#include "Defines.h"
#include "../tools/Logger.h"

#include <algorithm>
#include <filesystem>
#include <ranges>
#include <cctype>
#include <regex>
#include <system_error>
#include <unordered_set>
#if defined(_WIN32)
    #include <cstdlib>
#endif

namespace fs = std::filesystem;

#define COMPONENT "PathScanner"

PathScanner::PathScanner()
{
    m_targets = {};
}

PathScanner::~PathScanner()
{
    // Destructor
}

/////////////////////////////////////////////////////////////////////
////////////////////////////// PUBLIC ///////////////////////////////
/////////////////////////////////////////////////////////////////////

void PathScanner::SetTargets(const std::vector<AppIdDirPathScanTarget>& targets)
{
    // Replace scan targets with latest database state from daemon
    m_targets = targets;
    LOG_BE(Urgency::Debug, "Targets set: %zu", m_targets.size());
}

/////////////////////////////////////////////////////////////////////

std::vector<AppIdDirPathScanResult> PathScanner::ScanOnceForAppIdDir() const
{
    std::vector<AppIdDirPathScanResult> results = {};

    // Scan each active target prefix for matching AppId directory
    for (const auto& target : m_targets)
    {
#if defined(_WIN32)
        if (target.appId.empty())
#else
        if (target.appId.empty() || target.prefixLocation.empty())
#endif
        {
            continue;
        }

        std::string joinedGogIds = "";
        if (!target.installationDir.empty())
        {
            // GOG Emu req files handling - Start
            const std::vector<std::string> gogIds = FindGogIds(target.installationDir);
            joinedGogIds = JoinIds(gogIds);
            if (!joinedGogIds.empty() && joinedGogIds != target.dataOpt)
            {
                results.push_back(AppIdDirPathScanResult{target.targetId, "", "", joinedGogIds, false});
                LOG_BE(Urgency::Info, "Found GOG ids: targetId=%d ids=%s", target.targetId, joinedGogIds.c_str());
            }

            const std::string nemirtingasDir = FindNemirtingasDir(target);
            if (!nemirtingasDir.empty())
            {
                results.push_back(AppIdDirPathScanResult{target.targetId, nemirtingasDir, "GOG-N", joinedGogIds, true});
                LOG_BE(Urgency::Info, "Found Nemirtingas dir: targetId=%d path=%s", target.targetId, nemirtingasDir.c_str());
                continue;
            }

            // TODO: Currently disabled because need to make sure if this structure is even possible, and also contains risk of false positives
            // const std::string gogPrefixDir = FindGogPrefixAppIdDir(target, gogIds);
            // if (!gogPrefixDir.empty())
            // {
            //     results.push_back(AppIdDirPathScanResult{target.targetId, gogPrefixDir, "GOG-N", joinedGogIds, true});
            //     LOG_BE(Urgency::Info, "Found GOG prefix dir: targetId=%d path=%s", target.targetId, gogPrefixDir.c_str());
            //     continue;
            // }
            // GOG Emu req files handling - End
        }

#if defined(_WIN32)
        // RLD uses fixed ProgramData\Steam storage, separate from AppData/Public Documents emulator folders
        const std::string reloadedDir = FindWindowsReloadedDir(target);
        if (!reloadedDir.empty())
        {
            results.push_back(AppIdDirPathScanResult{target.targetId, reloadedDir, "RLD", joinedGogIds, true});
            LOG_BE(Urgency::Info, "Found Reloaded dir: targetId=%d path=%s", target.targetId, reloadedDir.c_str());
            continue;
        }

        std::string emulatorType;
        const std::string appIdDir = FindWindowsAppIdDir(target, emulatorType);
        if (!appIdDir.empty())
        {
            results.push_back(AppIdDirPathScanResult{target.targetId, appIdDir, emulatorType, joinedGogIds, true});
            LOG_BE(Urgency::Info, "Found APPID dir: targetId=%d path=%s emu=%s", target.targetId, appIdDir.c_str(), emulatorType.c_str());
        }
#else
        // Validate prefix before recursive traversal
        std::error_code ec;
        if (!fs::exists(target.prefixLocation, ec) || !fs::is_directory(target.prefixLocation, ec))
        {
            LOG_BE(Urgency::Warning, "Prefix missing or not directory: targetId=%d prefix=%s", target.targetId, target.prefixLocation.c_str());
            continue;
        }

        fs::recursive_directory_iterator it(target.prefixLocation, fs::directory_options::skip_permission_denied, ec);
        fs::recursive_directory_iterator end;

        // Walk prefix tree until exact AppId folder name is found
        for (; it != end && !ec; it.increment(ec))
        {
            if (!it->is_directory(ec))
            {
                continue;
            }

            if (ShouldSkipLinuxPrefixScanDirectory(*it, ec))
            {
                it.disable_recursion_pending();
                continue;
            }

            if (it->path().filename().string() != target.appId)
            {
                continue;
            }

            const fs::path parent = it->path().parent_path();
            const fs::path grandParent = parent.parent_path();
            // RLD Wine path shape: .../ProgramData/Steam/<username>/<AppId>/stats/achievements.ini
            const bool reloadedShape = grandParent.filename().string() == "Steam" && grandParent.parent_path().filename().string() == "ProgramData";
            if (reloadedShape)
            {
                const fs::path statsDir = it->path() / "stats";
                const fs::path achievementFile = statsDir / "achievements.ini";
                std::error_code fileEc;
                if (fs::is_regular_file(achievementFile, fileEc))
                {
                    const std::string foundPath = statsDir.string();
                    results.push_back(AppIdDirPathScanResult{target.targetId, foundPath, "RLD", joinedGogIds, true});
                    LOG_BE(Urgency::Info, "Found Reloaded dir: targetId=%d path=%s", target.targetId, foundPath.c_str());
                    break;
                }
                continue;
            }

            const std::string foundPath = it->path().string();

            // Store discovered path and inferred emulator type
            std::string emuType = DetectEmulatorType(foundPath);
            results.push_back(AppIdDirPathScanResult{target.targetId, foundPath, emuType, joinedGogIds, true});
            
            LOG_BE(Urgency::Info, "Found APPID dir: targetId=%d path=%s emu=%s", target.targetId, foundPath.c_str(), emuType.c_str());
            break;
        }

        if (ec)
        {
            LOG_BE(Urgency::Critical, "Scan error: targetId=%d error=%s", target.targetId, ec.message().c_str());
        }
#endif
    }

    return results;
}

/////////////////////////////////////////////////////////////////////
///////////////////////////// PRIVATE ///////////////////////////////
/////////////////////////////////////////////////////////////////////

std::string PathScanner::DetectEmulatorType(const std::string& appidDirLocation) const
{
    // Emulator folder is parent directory of discovered AppId folder
    const fs::path path(appidDirLocation);
    return DetectEmulatorTypeFromFolderName(path.parent_path().filename().string());
}

/////////////////////////////////////////////////////////////////////

std::string PathScanner::DetectEmulatorTypeFromFolderName(const std::string& folderName) const
{
    // Compare lower-case folder names against known emulator aliases
    std::string lower = folderName;
    std::ranges::transform(lower, lower.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });

    if (lower == "codex")                               return "CODEX";
    if (lower == "rune")                                return "RUNE";
    if (lower == "empress")                             return "EMPRESS";
    if (lower == "skidrow" || lower == "skid-row")      return "SKIDROW";
    if (lower == "onlinefix" || lower == "online-fix")  return "OnlineFix";
    if (lower == "goldberg" || lower.find("goldberg") != std::string::npos ||
        lower == "gse saves" || lower == "gsesaves" ||
        lower == "goldberg steamemu saves")             return "GOLDBERG";
    if (lower == "smartsteamemu" || lower == "sse")     return "SmartSteamEmu";
    if (lower == "creamapi" || lower == "cream api")    return "CreamAPI";
    if (lower == "rld!" || lower == "rld" || 
        lower.find("reloaded") != std::string::npos)    return "RLD";
    if (lower == ".1911" || lower == "1911")            return "1911";
    if (lower == "cpy")                                 return "CPY";
    if (lower == "steampunks" ||
        lower == "steam punks")                         return "STEAMPUNKS";

    return "UNKNOWN";
}

/////////////////////////////////////////////////////////////////////

#if !defined(_WIN32)
bool PathScanner::ShouldSkipLinuxPrefixScanDirectory(const fs::directory_entry& entry, std::error_code& ec) const
{
    if (entry.is_symlink(ec))
    {
        return true;
    }
    ec.clear();

    const std::string name = entry.path().filename().string();
    return name == "dosdevices" || name == "pfx";
}
#endif

/////////////////////////////////////////////////////////////////////

#if defined(_WIN32)
std::string PathScanner::FindWindowsReloadedDir(const AppIdDirPathScanTarget& target) const
{
    // Inspect direct user folders under C:\ProgramData\Steam
    const fs::path steamRoot = "C:\\ProgramData\\Steam";

    std::error_code ec;
    if (!fs::exists(steamRoot, ec) || !fs::is_directory(steamRoot, ec))
    {
        return "";
    }

    fs::directory_iterator it(steamRoot, fs::directory_options::skip_permission_denied, ec);
    const fs::directory_iterator end;
    for (; it != end && !ec; it.increment(ec))
    {
        if (!it->is_directory(ec))
        {
            continue;
        }

        const fs::path statsDir = it->path() / target.appId / "stats";
        const fs::path achievementFile = statsDir / "achievements.ini";
        std::error_code fileEc;
        if (fs::is_regular_file(achievementFile, fileEc))
        {
            return statsDir.string();
        }
    }

    if (ec)
    {
        LOG_BE(Urgency::Warning, "Scan error: targetId=%d root=%s error=%s", target.targetId, steamRoot.string().c_str(), ec.message().c_str());
    }

    return "";
}
#endif

/////////////////////////////////////////////////////////////////////

#if defined(_WIN32)
std::string PathScanner::FindWindowsAppIdDir(const AppIdDirPathScanTarget& target, std::string& emulatorType) const
{
    // Check <emulator folder>\\<AppId>
    auto findAppIdBelowEmulatorDir = [&](const fs::path& directory) -> std::string
    {
        const std::string foundEmulatorType = DetectEmulatorTypeFromFolderName(directory.filename().string());
        if (foundEmulatorType == "UNKNOWN")
        {
            return "";
        }

        const fs::path candidate = directory / target.appId;
        std::error_code candidateEc;
        if (!fs::is_directory(candidate, candidateEc))
        {
            return "";
        }

        emulatorType = foundEmulatorType;
        return candidate.string();
    };

    // AppData roots; direct folders
    std::vector<fs::path> appDataRoots;
    if (const char* appData = std::getenv("APPDATA"); appData && *appData)
    {
        appDataRoots.emplace_back(appData);
    }
    if (const char* localAppData = std::getenv("LOCALAPPDATA"); localAppData && *localAppData)
    {
        appDataRoots.emplace_back(localAppData);
    }

    // Non-recursive AppData scan
    for (const fs::path& root : appDataRoots)
    {
        std::error_code ec;
        if (!fs::exists(root, ec) || !fs::is_directory(root, ec))
        {
            continue;
        }

        fs::directory_iterator it(root, fs::directory_options::skip_permission_denied, ec);
        const fs::directory_iterator end;
        for (; it != end && !ec; it.increment(ec))
        {
            if (!it->is_directory(ec))
            {
                continue;
            }

            const std::string appIdDir = findAppIdBelowEmulatorDir(it->path());
            if (!appIdDir.empty())
            {
                return appIdDir;
            }
        }

        if (ec)
        {
            LOG_BE(Urgency::Warning, "Scan error: targetId=%d root=%s error=%s", target.targetId, root.string().c_str(), ec.message().c_str());
        }
    }

    // Recursive Public Documents scan; known emulator folders only
    const fs::path publicDocuments = "C:\\Users\\Public\\Documents";
    std::error_code ec;
    if (!fs::exists(publicDocuments, ec) || !fs::is_directory(publicDocuments, ec))
    {
        return "";
    }

    fs::recursive_directory_iterator it(publicDocuments, fs::directory_options::skip_permission_denied, ec);
    const fs::recursive_directory_iterator end;
    for (; it != end && !ec; it.increment(ec))
    {
        if (!it->is_directory(ec))
        {
            continue;
        }

        const std::string appIdDir = findAppIdBelowEmulatorDir(it->path());
        if (!appIdDir.empty())
        {
            return appIdDir;
        }
    }

    if (ec)
    {
        LOG_BE(Urgency::Warning, "Scan error: targetId=%d root=%s error=%s", target.targetId, publicDocuments.string().c_str(), ec.message().c_str());
    }

    return "";
}
#endif

/////////////////////////////////////////////////////////////////////

std::string PathScanner::FindNemirtingasDir(const AppIdDirPathScanTarget& target) const
{
#if defined(_WIN32)
    std::vector<fs::path> scanRoots;
    std::vector<fs::path> candidates;
    if (!target.executableLocation.empty())
    {
        candidates.push_back(fs::path(target.executableLocation).parent_path() / "ngalaxye_settings");
    }

    std::error_code candidateEc;
    for (const fs::path& candidate : candidates)
    {
        if (candidate.empty())
        {
            continue;
        }

        if (fs::exists(candidate, candidateEc) && fs::is_directory(candidate, candidateEc))
        {
            return candidate.string();
        }
        candidateEc.clear();
    }

    if (!target.installationDir.empty())
    {
        scanRoots.emplace_back(target.installationDir);
    }
    if (const char* appData = std::getenv("APPDATA"); appData && *appData)
    {
        scanRoots.emplace_back(appData);
    }
    if (const char* localAppData = std::getenv("LOCALAPPDATA"); localAppData && *localAppData)
    {
        scanRoots.emplace_back(localAppData);
    }
    scanRoots.emplace_back("C:\\Users\\Public");

    for (const fs::path& root : scanRoots)
    {
        std::error_code ec;
        if (!fs::exists(root, ec) || !fs::is_directory(root, ec))
        {
            continue;
        }

        fs::recursive_directory_iterator it(root, fs::directory_options::skip_permission_denied, ec);
        const fs::recursive_directory_iterator end;
        for (; it != end && !ec; it.increment(ec))
        {
            if (it->is_directory(ec) && it->path().filename().string() == "ngalaxye_settings")
            {
                return it->path().string();
            }
        }
    }

    return "";
#else
    std::vector<fs::path> candidates = {};
    if (!target.executableLocation.empty())
    {
        candidates.push_back(fs::path(target.executableLocation).parent_path() / "ngalaxye_settings");
    }

    std::error_code ec;
    for (const fs::path& candidate : candidates)
    {
        if (candidate.empty())
        {
            continue;
        }

        if (fs::exists(candidate, ec) && fs::is_directory(candidate, ec))
        {
            return candidate.string();
        }
    }

    if (target.installationDir.empty() || !fs::exists(target.installationDir, ec) || !fs::is_directory(target.installationDir, ec))
    {
        return "";
    }

    fs::recursive_directory_iterator it(target.installationDir, fs::directory_options::skip_permission_denied, ec);
    fs::recursive_directory_iterator end;
    for (; it != end && !ec; it.increment(ec))
    {
        if (!it->is_directory(ec))
        {
            continue;
        }

        if (it->path().filename().string() == "ngalaxye_settings")
        {
            return it->path().string();
        }
    }

    return "";
#endif
}

/////////////////////////////////////////////////////////////////////

std::string PathScanner::FindGogPrefixAppIdDir(const AppIdDirPathScanTarget& target, const std::vector<std::string>& gogIds) const
{
#if defined(_WIN32)
    if (gogIds.empty())
    {
        return "";
    }

    std::vector<fs::path> scanRoots;
    if (const char* appData = std::getenv("APPDATA"); appData && *appData)
    {
        scanRoots.emplace_back(appData);
    }
    if (const char* localAppData = std::getenv("LOCALAPPDATA"); localAppData && *localAppData)
    {
        scanRoots.emplace_back(localAppData);
    }
    scanRoots.emplace_back("C:\\Users\\Public");

    const std::unordered_set<std::string> idSet(gogIds.begin(), gogIds.end());
    for (const fs::path& root : scanRoots)
    {
        std::error_code ec;
        if (!fs::exists(root, ec) || !fs::is_directory(root, ec))
        {
            continue;
        }

        fs::recursive_directory_iterator it(root, fs::directory_options::skip_permission_denied, ec);
        const fs::recursive_directory_iterator end;
        for (; it != end && !ec; it.increment(ec))
        {
            if (it->is_directory(ec) && idSet.contains(it->path().filename().string()))
            {
                return it->path().string();
            }
        }
    }

    return "";
#else
    if (target.prefixLocation.empty() || gogIds.empty())
    {
        return "";
    }

    std::error_code ec;
    if (!fs::exists(target.prefixLocation, ec) || !fs::is_directory(target.prefixLocation, ec))
    {
        return "";
    }

    const std::unordered_set<std::string> idSet(gogIds.begin(), gogIds.end());
    fs::recursive_directory_iterator it(target.prefixLocation, fs::directory_options::skip_permission_denied, ec);
    fs::recursive_directory_iterator end;
    for (; it != end && !ec; it.increment(ec))
    {
        if (!it->is_directory(ec))
        {
            continue;
        }

        if (ShouldSkipLinuxPrefixScanDirectory(*it, ec))
        {
            it.disable_recursion_pending();
            continue;
        }

        if (!idSet.contains(it->path().filename().string()))
        {
            continue;
        }

        return it->path().string();
    }

    return "";
#endif
}

/////////////////////////////////////////////////////////////////////

std::vector<std::string> PathScanner::FindGogIds(const std::string& installationDir) const
{
    std::vector<std::string> ids = {};
    if (installationDir.empty())
    {
        return ids;
    }

    std::error_code ec;
    if (!fs::exists(installationDir, ec) || !fs::is_directory(installationDir, ec))
    {
        return ids;
    }

    std::unordered_set<std::string> seen;
    const std::regex gogFilePattern(R"(^goggame-([0-9]+)\.(hashdb|info)$)", std::regex::icase);

    for (const fs::directory_entry& entry : fs::directory_iterator(installationDir, fs::directory_options::skip_permission_denied, ec))
    {
        if (ec)
        {
            break;
        }

        if (!entry.is_regular_file(ec))
        {
            continue;
        }

        const std::string fileName = entry.path().filename().string();
        std::smatch match;
        if (!std::regex_match(fileName, match, gogFilePattern))
        {
            continue;
        }

        const std::string id = match[1].str();
        if (seen.insert(id).second)
        {
            ids.push_back(id);
        }
    }

    std::ranges::sort(ids);
    return ids;
}

/////////////////////////////////////////////////////////////////////

std::string PathScanner::JoinIds(const std::vector<std::string>& ids) const
{
    std::string joined = "";
    for (const std::string& id : ids)
    {
        if (!joined.empty())
        {
            joined += ",";
        }
        joined += id;
    }
    return joined;
}
