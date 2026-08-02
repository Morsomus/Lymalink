/////////////////////////////////////////////////////////
// File: AchievementKeyResolverTests.cpp
// Date: 2026-08-02
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Tests achievement key resolver helpers
/////////////////////////////////////////////////////////

#if __has_include(<catch2/catch_test_macros.hpp>)
#include <catch2/catch_test_macros.hpp>
#else
#include <catch2/catch.hpp>
#endif

#include "Defines.h"
#include "database/SQLiteManager.h"
#include "tools/AchievementKeyResolver.h"

#include <cstdint>
#include <filesystem>
#include <string>

namespace fs = std::filesystem;

namespace
{
const std::string CONN = "test_conn";

void cleanupDb(const std::string& path);
}

/////////////////////////////////////////////////////////////////////

TEST_CASE("achievementKeyResolver_resolvesPreparedCRCKey", "[resolver]")
{
    const std::string dbPath = "/tmp/lymalink_achievement_key_resolver.db";
    cleanupDb(dbPath);

    SQLiteManager mgr;
    REQUIRE(mgr.CreateDatabase(CONN, dbPath));
    REQUIRE(mgr.CreateTable(CONN, DATABASE_TABLE_EMU_ACHIEVEMENTS, {
        "id INTEGER",
        "achievement_key TEXT",
        "cur_progress INTEGER DEFAULT 0",
        "max_progress INTEGER DEFAULT 0",
        "date_unlocked INTEGER DEFAULT 0"
    }));
    REQUIRE(mgr.Insert(CONN, DATABASE_TABLE_EMU_ACHIEVEMENTS, {
        {"id", int64_t{1366540}},
        {"achievement_key", std::string("ACHIEVEMENT_1")},
        {"cur_progress", int64_t{0}},
        {"max_progress", int64_t{40}},
        {"date_unlocked", int64_t{0}}
    }));

    AchievementKeyResolver resolver(mgr, CONN);
    resolver.PrepareTargetKeys(1366540);

    CHECK(resolver.ResolveKey(1366540, "crc32:689F7FF3") == "ACHIEVEMENT_1");
    CHECK(resolver.ResolveKey(1366540, "crc32:8CB98988") == "crc32:8CB98988");

    mgr.CloseDatabase(CONN);
    cleanupDb(dbPath);
}

/////////////////////////////////////////////////////////////////////
///////////////////////////// PRIVATE ///////////////////////////////
/////////////////////////////////////////////////////////////////////

namespace
{
void cleanupDb(const std::string& path)
{
    for (const auto& file : {path, path + "-wal", path + "-shm"})
    {
        if (fs::exists(file))
        {
            fs::remove(file);
        }
    }
}
}
