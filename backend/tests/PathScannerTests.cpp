/////////////////////////////////////////////////////////
// File: PathScannerTests.cpp
// Date: 2026-08-02
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Tests AppId directory path scanner
/////////////////////////////////////////////////////////

#if __has_include(<catch2/catch_test_macros.hpp>)
#include <catch2/catch_test_macros.hpp>
#else
#include <catch2/catch.hpp>
#endif

#include "watcher/PathScanner.h"

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

/////////////////////////////////////////////////////////////////////

TEST_CASE("pathScanner_detectsNemirtingasBelowInstallationDir", "[pathscanner]")
{
    const fs::path root = "/tmp/lymalink_pathscanner_nemirtingas";
    fs::remove_all(root);
    const fs::path installDir = root / "Game";
    const fs::path exeDir = installDir / "Binaries" / "Win64";
    fs::create_directories(exeDir / "ngalaxye_settings");
    const fs::path exePath = installDir / "Launcher.exe";
    {
        std::ofstream file(exePath);
        file << "";
    }

    PathScanner scanner;
    scanner.SetTargets({AppIdDirPathScanTarget{123, "123", root.string(), exePath.string(), installDir.string(), ""}});
    const std::vector<AppIdDirPathScanResult> results = scanner.ScanOnceForAppIdDir();

    REQUIRE(results.size() == 1);
    CHECK(results[0].targetId == 123);
    CHECK(results[0].appidDirFound);
    CHECK(results[0].emulatorType == "GOG-N");
    CHECK(fs::path(results[0].appidDirLocation).filename() == "ngalaxye_settings");

    fs::remove_all(root);
}

/////////////////////////////////////////////////////////////////////

TEST_CASE("pathScanner_skipsGogScanWhenInstallationDirEmpty", "[pathscanner]")
{
    const fs::path root = "/tmp/lymalink_pathscanner_gog_disabled";
    fs::remove_all(root);
    const fs::path installDir = root / "Game";
    const fs::path prefix = root / "prefix";
    fs::create_directories(installDir / "ngalaxye_settings");
    fs::create_directories(prefix / "Goldberg" / "123");
    const fs::path exePath = installDir / "Game.exe";
    {
        std::ofstream file(exePath);
        file << "";
    }

    PathScanner scanner;
    scanner.SetTargets({AppIdDirPathScanTarget{123, "123", prefix.string(), exePath.string(), "", ""}});
    const std::vector<AppIdDirPathScanResult> results = scanner.ScanOnceForAppIdDir();

    REQUIRE(results.size() == 1);
    CHECK(results[0].targetId == 123);
    CHECK(results[0].appidDirFound);
    CHECK(results[0].emulatorType == "GOLDBERG");
    CHECK(fs::path(results[0].appidDirLocation).filename() == "123");

    fs::remove_all(root);
}

/////////////////////////////////////////////////////////////////////

TEST_CASE("pathScanner_detectsTenokeSteamDataBelowInstallationDir", "[pathscanner]")
{
    const fs::path root = "/tmp/lymalink_pathscanner_tenoke_root";
    fs::remove_all(root);
    const fs::path installDir = root / "Game";
    const fs::path steamDataDir = installDir / "SteamData";
    const fs::path prefix = root / "prefix";
    fs::create_directories(steamDataDir);
    fs::create_directories(prefix);
    {
        std::ofstream file(steamDataDir / "user_stats.ini");
        file << "[ACHIEVEMENTS]\n";
    }

    PathScanner scanner;
    scanner.SetTargets({AppIdDirPathScanTarget{123, "123", prefix.string(), "", installDir.string(), ""}});
    const std::vector<AppIdDirPathScanResult> results = scanner.ScanOnceForAppIdDir();

    REQUIRE(results.size() == 1);
    CHECK(results[0].targetId == 123);
    CHECK(results[0].appidDirFound);
    CHECK(results[0].emulatorType == "Tenoke");
    CHECK(fs::path(results[0].appidDirLocation) == steamDataDir);

    fs::remove_all(root);
}

/////////////////////////////////////////////////////////////////////

TEST_CASE("pathScanner_detectsNestedTenokeSteamDataBelowInstallationDir", "[pathscanner]")
{
    const fs::path root = "/tmp/lymalink_pathscanner_tenoke_nested";
    fs::remove_all(root);
    const fs::path installDir = root / "Game";
    const fs::path steamDataDir = installDir / "Sub" / "SteamData";
    const fs::path prefix = root / "prefix";
    fs::create_directories(steamDataDir);
    fs::create_directories(prefix);
    {
        std::ofstream file(steamDataDir / "user_stats.ini");
        file << "[ACHIEVEMENTS]\n";
    }

    PathScanner scanner;
    scanner.SetTargets({AppIdDirPathScanTarget{123, "123", prefix.string(), "", installDir.string(), ""}});
    const std::vector<AppIdDirPathScanResult> results = scanner.ScanOnceForAppIdDir();

    REQUIRE(results.size() == 1);
    CHECK(results[0].targetId == 123);
    CHECK(results[0].appidDirFound);
    CHECK(results[0].emulatorType == "Tenoke");
    CHECK(fs::path(results[0].appidDirLocation) == steamDataDir);

    fs::remove_all(root);
}

/////////////////////////////////////////////////////////////////////

TEST_CASE("pathScanner_ignoresTenokeSteamDataWithoutUserStats", "[pathscanner]")
{
    const fs::path root = "/tmp/lymalink_pathscanner_tenoke_missing_file";
    fs::remove_all(root);
    const fs::path installDir = root / "Game";
    const fs::path prefix = root / "prefix";
    fs::create_directories(installDir / "SteamData");
    fs::create_directories(prefix);

    PathScanner scanner;
    scanner.SetTargets({AppIdDirPathScanTarget{123, "123", prefix.string(), "", installDir.string(), ""}});
    const std::vector<AppIdDirPathScanResult> results = scanner.ScanOnceForAppIdDir();

    CHECK(results.empty());

    fs::remove_all(root);
}

/////////////////////////////////////////////////////////////////////

TEST_CASE("pathScanner_skipsTenokeWhenInstallationDirEmpty", "[pathscanner]")
{
    const fs::path root = "/tmp/lymalink_pathscanner_tenoke_disabled";
    fs::remove_all(root);
    const fs::path prefix = root / "prefix";
    fs::create_directories(prefix / "Game" / "SteamData");
    {
        std::ofstream file(prefix / "Game" / "SteamData" / "user_stats.ini");
        file << "[ACHIEVEMENTS]\n";
    }

    PathScanner scanner;
    scanner.SetTargets({AppIdDirPathScanTarget{123, "123", prefix.string(), "", "", ""}});
    const std::vector<AppIdDirPathScanResult> results = scanner.ScanOnceForAppIdDir();

    CHECK(results.empty());

    fs::remove_all(root);
}

/////////////////////////////////////////////////////////////////////

TEST_CASE("pathScanner_prefersNemirtingasOverTenokeInInstallationDir", "[pathscanner]")
{
    const fs::path root = "/tmp/lymalink_pathscanner_gogn_before_tenoke";
    fs::remove_all(root);
    const fs::path installDir = root / "Game";
    const fs::path prefix = root / "prefix";
    fs::create_directories(installDir / "ngalaxye_settings");
    fs::create_directories(installDir / "SteamData");
    fs::create_directories(prefix);
    {
        std::ofstream file(installDir / "SteamData" / "user_stats.ini");
        file << "[ACHIEVEMENTS]\n";
    }

    PathScanner scanner;
    scanner.SetTargets({AppIdDirPathScanTarget{123, "123", prefix.string(), "", installDir.string(), ""}});
    const std::vector<AppIdDirPathScanResult> results = scanner.ScanOnceForAppIdDir();

    REQUIRE(results.size() == 1);
    CHECK(results[0].appidDirFound);
    CHECK(results[0].emulatorType == "GOG-N");
    CHECK(fs::path(results[0].appidDirLocation).filename() == "ngalaxye_settings");

    fs::remove_all(root);
}

/////////////////////////////////////////////////////////////////////

#if !defined(_WIN32)
TEST_CASE("pathScanner_detectsReloadedWineProgramDataSteamStatsDir", "[pathscanner]")
{
    const fs::path root = "/tmp/lymalink_pathscanner_rld";
    fs::remove_all(root);
    const fs::path scanRoot = root / "selected_root";
    const fs::path driveC = scanRoot / "things" / "Heroic" / "Prefixes" / "Game123" / "drive_c";
    const fs::path statsDir = driveC / "ProgramData" / "Steam" / "UserA" / "217980" / "stats";
    fs::create_directories(statsDir);
    fs::create_directories(driveC / "ProgramData" / "Steam" / "UserA" / "999999" / "stats");
    fs::create_directories(driveC / "ProgramData" / "Steam" / "UserB" / "217980" / "stats");
    {
        std::ofstream file(statsDir / "achievements.ini");
        file << "";
    }
    {
        std::ofstream file(driveC / "ProgramData" / "Steam" / "UserA" / "999999" / "stats" / "achievements.ini");
        file << "";
    }

    PathScanner scanner;
    scanner.SetTargets({AppIdDirPathScanTarget{217, "217980", scanRoot.string(), "", "", ""}});
    const std::vector<AppIdDirPathScanResult> results = scanner.ScanOnceForAppIdDir();

    REQUIRE(results.size() == 1);
    CHECK(results[0].targetId == 217);
    CHECK(results[0].appidDirFound);
    CHECK(results[0].emulatorType == "RLD");
    CHECK(fs::path(results[0].appidDirLocation) == statsDir);

    fs::remove_all(root);
}

/////////////////////////////////////////////////////////////////////

TEST_CASE("pathScanner_ignoresReloadedShapeWithoutAchievementFile", "[pathscanner]")
{
    const fs::path root = "/tmp/lymalink_pathscanner_rld_missing_file";
    fs::remove_all(root);
    const fs::path scanRoot = root / "selected_root";
    const fs::path driveC = scanRoot / "things" / "Heroic" / "Prefixes" / "Game123" / "drive_c";
    fs::create_directories(driveC / "ProgramData" / "Steam" / "UserA" / "217980" / "stats");
    fs::create_directories(driveC / "ProgramData" / "Steam" / "UserA" / "999999" / "stats");
    {
        std::ofstream file(driveC / "ProgramData" / "Steam" / "UserA" / "999999" / "stats" / "achievements.ini");
        file << "";
    }

    PathScanner scanner;
    scanner.SetTargets({AppIdDirPathScanTarget{217, "217980", scanRoot.string(), "", "", ""}});
    const std::vector<AppIdDirPathScanResult> results = scanner.ScanOnceForAppIdDir();

    CHECK(results.empty());

    fs::remove_all(root);
}

/////////////////////////////////////////////////////////////////////

TEST_CASE("pathScanner_skipsLinuxPrefixLoopAndDosdevicesDirs", "[pathscanner]")
{
    const fs::path root = "/tmp/lymalink_pathscanner_linux_prefix_guards";
    fs::remove_all(root);
    const fs::path prefix = root / "prefix";
    fs::create_directories(prefix / "dosdevices" / "Goldberg" / "123");
    fs::create_directories(prefix / "drive_c" / "users" / "steamuser" / "Goldberg" / "123");
    fs::create_directory_symlink(".", prefix / "pfx");

    PathScanner scanner;
    scanner.SetTargets({AppIdDirPathScanTarget{123, "123", prefix.string(), "", "", ""}});
    const std::vector<AppIdDirPathScanResult> results = scanner.ScanOnceForAppIdDir();

    REQUIRE(results.size() == 1);
    CHECK(results[0].targetId == 123);
    CHECK(results[0].appidDirFound);
    CHECK(results[0].emulatorType == "GOLDBERG");
    CHECK(fs::path(results[0].appidDirLocation).string().find("dosdevices") == std::string::npos);
    CHECK(fs::path(results[0].appidDirLocation).string().find("pfx") == std::string::npos);

    fs::remove_all(root);
}
#endif

/////////////////////////////////////////////////////////////////////

// Disabled while FindGogPrefixAppIdDir() is intentionally not used.
// Current scanner only records GOG IDs; prefix-dir matching was parked due false-positive risk.
TEST_CASE("pathScanner_collectsGogIdsAndMatchesPrefixAchievementDir", "[pathscanner][.]")
{
    const fs::path root = "/tmp/lymalink_pathscanner_gog_ids";
    fs::remove_all(root);
    const fs::path installDir = root / "Game";
    const fs::path prefix = root / "prefix";
    fs::create_directories(installDir);
    fs::create_directories(installDir / "Nested");
    fs::create_directories(prefix / "users" / "steamuser" / "AppData" / "1986509485");
    {
        std::ofstream file(installDir / "goggame-1986509485.hashdb");
        file << "";
    }
    {
        std::ofstream file(installDir / "goggame-200.info");
        file << "";
    }
    {
        std::ofstream file(installDir / "Nested" / "goggame-999.info");
        file << "";
    }

    PathScanner scanner;
    scanner.SetTargets({AppIdDirPathScanTarget{456, "456", prefix.string(), (installDir / "Game.exe").string(), installDir.string(), ""}});
    const std::vector<AppIdDirPathScanResult> results = scanner.ScanOnceForAppIdDir();

    REQUIRE(results.size() == 2);
    CHECK(results[0].targetId == 456);
    CHECK_FALSE(results[0].appidDirFound);
    CHECK(results[0].dataOpt == "1986509485,200");
    CHECK(results[1].targetId == 456);
    CHECK(results[1].appidDirFound);
    CHECK(results[1].emulatorType == "GOG-N");
    CHECK(results[1].dataOpt == "1986509485,200");
    CHECK(fs::path(results[1].appidDirLocation).filename() == "1986509485");

    fs::remove_all(root);
}
