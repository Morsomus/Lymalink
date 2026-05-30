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
#include <system_error>

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
    LOG_BE(Urgency::Info, "Targets set: %zu", m_targets.size());
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
            results.push_back(AppIdDirPathScanResult{target.targetId, foundPath, emuType});
            
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
