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

namespace fs = std::filesystem;

#define COMPONENT "PathScanner"

/////////////////////////////////////////////////////////////////////

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
        if (target.appId.empty() || target.prefixLocation.empty())
        {
            continue;
        }

        // GOG Emu req files handling - Start
        const std::vector<std::string> gogIds = FindGogIds(target.installationDir);
        const std::string joinedGogIds = JoinIds(gogIds);
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

        const std::string gogPrefixDir = FindGogPrefixAppIdDir(target, gogIds);
        if (!gogPrefixDir.empty())
        {
            results.push_back(AppIdDirPathScanResult{target.targetId, gogPrefixDir, "GOG-N", joinedGogIds, true});
            LOG_BE(Urgency::Info, "Found GOG prefix dir: targetId=%d path=%s", target.targetId, gogPrefixDir.c_str());
            continue;
        }
        // GOG Emu req files handling - End

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

            if (it->path().filename().string() != target.appId)
            {
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
    std::string folderName = path.parent_path().filename().string();

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
        lower.find("reloaded") != std::string::npos)    return "RLD!";
    if (lower == ".1911" || lower == "1911")            return "1911";
    if (lower == "cpy")                                 return "CPY";
    if (lower == "steampunks" ||
        lower == "steam punks")                         return "STEAMPUNKS";

    return folderName.empty() ? "UNKNOWN" : folderName;
}

/////////////////////////////////////////////////////////////////////

std::string PathScanner::FindNemirtingasDir(const AppIdDirPathScanTarget& target) const
{
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
}

/////////////////////////////////////////////////////////////////////

std::string PathScanner::FindGogPrefixAppIdDir(const AppIdDirPathScanTarget& target, const std::vector<std::string>& gogIds) const
{
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

        if (!idSet.contains(it->path().filename().string()))
        {
            continue;
        }

        return it->path().string();
    }

    return "";
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
