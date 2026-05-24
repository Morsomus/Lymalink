/////////////////////////////////////////////////////////
// File: PathScanner.cpp
// Date: 2026-05-22
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Implements PathScanner class for scanning
//              specific paths for target files
/////////////////////////////////////////////////////////

#include "PathScanner.h"
#include "../tools/Logger.h"

#include <algorithm>
#include <filesystem>
#include <ranges>
#include <cctype>
#include <system_error>

namespace fs = std::filesystem;

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
    m_targets = targets;
    Logger::Log("[PathScanner] Targets set: " + std::to_string(m_targets.size()));
}

/////////////////////////////////////////////////////////////////////

std::vector<AppIdDirPathScanResult> PathScanner::ScanOnceForAppIdDir() const
{
    std::vector<AppIdDirPathScanResult> results;

    for (const auto& target : m_targets)
    {
        if (target.appId.empty() || target.prefixLocation.empty())
        {
            continue;
        }

        std::error_code ec;
        if (!fs::exists(target.prefixLocation, ec) || !fs::is_directory(target.prefixLocation, ec))
        {
            Logger::Log("[PathScanner] Prefix missing or not directory: targetId=" + std::to_string(target.targetId) + " prefix=" + target.prefixLocation);
            continue;
        }

        fs::recursive_directory_iterator it(target.prefixLocation, fs::directory_options::skip_permission_denied, ec);
        fs::recursive_directory_iterator end;

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
            results.push_back(AppIdDirPathScanResult{target.targetId, foundPath, DetectEmulatorType(foundPath)});
            Logger::Log("[PathScanner] Found APPID dir: targetId=" + std::to_string(target.targetId) + " path=" + foundPath);
            break;
        }

        if (ec)
        {
            Logger::Log("[PathScanner] Scan error: targetId=" + std::to_string(target.targetId) + " error=" + ec.message());
        }
    }

    return results;
}

/////////////////////////////////////////////////////////////////////
///////////////////////////// PRIVATE ///////////////////////////////
/////////////////////////////////////////////////////////////////////

std::string PathScanner::DetectEmulatorType(const std::string& appidDirLocation) const
{
    const fs::path path(appidDirLocation);
    std::string folderName = path.parent_path().filename().string();

    EmulatorType type = GetEmulatorEnum(folderName);
    switch (type)
    {
        case EmulatorType::CODEX:
            return "CODEX";

        case EmulatorType::RUNE:
            return "RUNE";

        case EmulatorType::EMPRESS:
            return "EMPRESS";

        case EmulatorType::SKIDROW:
            return "SKIDROW";

        case EmulatorType::ONLINEFIX:
            return "OnlineFix";

        case EmulatorType::GOLDBERG:
            return "GOLDBERG";

        case EmulatorType::SMARTSTEAMEMU:
            return "SmartSteamEmu";

        case EmulatorType::CREAMAPI:
            return "CreamAPI";

        case EmulatorType::RLD:
            return "RLD!";

        case EmulatorType::_1911:
            return "1911";

        case EmulatorType::CPY:
            return "CPY";

        case EmulatorType::STEAMPUNKS:
            return "STEAMPUNKS";

        case EmulatorType::UNKNOWN:
        default:
            return folderName.empty() ? "UNKNOWN" : folderName;
    }
}

/////////////////////////////////////////////////////////////////////

EmulatorType PathScanner::GetEmulatorEnum(const std::string& folderName) const
{
    std::string lower = folderName;
    std::ranges::transform(lower, lower.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });

    if (lower == "codex")                               return EmulatorType::CODEX;
    if (lower == "rune")                                return EmulatorType::RUNE;
    if (lower == "empress")                             return EmulatorType::EMPRESS;
    if (lower == "skidrow" || lower == "skid-row")      return EmulatorType::SKIDROW;
    if (lower == "onlinefix" || lower == "online-fix")  return EmulatorType::ONLINEFIX;
    if (lower == "goldberg" || lower.find("goldberg") != std::string::npos ||
        lower == "gse saves" || lower == "gsesaves" ||
        lower == "goldberg steamemu saves")             return EmulatorType::GOLDBERG;
    if (lower == "smartsteamemu" || lower == "sse")     return EmulatorType::SMARTSTEAMEMU;
    if (lower == "creamapi" || lower == "cream api")    return EmulatorType::CREAMAPI;
    if (lower == "rld!" || lower == "rld" || 
        lower.find("reloaded") != std::string::npos)    return EmulatorType::RLD;
    if (lower == ".1911" || lower == "1911")            return EmulatorType::_1911;
    if (lower == "cpy")                                 return EmulatorType::CPY;
    if (lower == "steampunks" ||
        lower == "steam punks")                         return EmulatorType::STEAMPUNKS;

    return EmulatorType::UNKNOWN;
}