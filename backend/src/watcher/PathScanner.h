/////////////////////////////////////////////////////////
// File: PathScanner.h
// Date: 2026-05-22
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Declares PathScanner class for scanning
//              specific paths for target files
/////////////////////////////////////////////////////////

#pragma once

#include <string>
#include <vector>

struct AppIdDirPathScanTarget
{
    int targetId = 0;
    std::string appId;
    std::string prefixLocation;
    std::string executableLocation;
    std::string installationDir;
    std::string dataOpt;
};

struct AppIdDirPathScanResult
{
    int targetId = 0;
    std::string appidDirLocation;
    std::string emulatorType;
    std::string dataOpt;
    bool appidDirFound = false;
};

class PathScanner
{
public:
    PathScanner();
    ~PathScanner();

    void SetTargets(const std::vector<AppIdDirPathScanTarget>& targets);
    std::vector<AppIdDirPathScanResult> ScanOnceForAppIdDir() const;

private:
    std::vector<AppIdDirPathScanTarget> m_targets;

    std::string DetectEmulatorType(const std::string& appidDirLocation) const;
    std::string FindNemirtingasDir(const AppIdDirPathScanTarget& target) const;
    std::string FindGogPrefixAppIdDir(const AppIdDirPathScanTarget& target, const std::vector<std::string>& gogIds) const;
    std::vector<std::string> FindGogIds(const std::string& installationDir) const;
    std::string JoinIds(const std::vector<std::string>& ids) const;
};
