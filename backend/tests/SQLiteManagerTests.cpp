/////////////////////////////////////////////////////////
// File: SQLiteManagerTests.cpp
// Date: 2026-05-23
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Tests SQLiteManager database helpers
/////////////////////////////////////////////////////////

#if __has_include(<catch2/catch_test_macros.hpp>)
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#else
#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#endif

#include "database/SQLiteManager.h"
#include "tools/parsers/GoGNParser.h"
#include "watcher/PathScanner.h"

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace
{
const std::string CONN = "test_conn";
const std::string DB_PATH = "/tmp/test_sqlitemgr.db";
const std::string TABLE = "items";

const std::vector<std::string> COLS = {
    "id    INTEGER PRIMARY KEY AUTOINCREMENT",
    "name  TEXT    NOT NULL",
    "qty   INTEGER NOT NULL DEFAULT 0",
    "price REAL"
};

void cleanupDb(const std::string &path);
DbRecord item(const std::string &name, int64_t qty = 1, double price = 0.0);

struct TestDb
{
    SQLiteManager mgr;
    std::string path;

    explicit TestDb(const std::string &dbPath = DB_PATH);
    ~TestDb();

    bool insert(const std::string &name, int64_t qty = 1, double price = 0.0);
};
}

/////////////////////////////////////////////////////////////////////

TEST_CASE("createDatabase_opensConnectionAndCreatesFile", "[database]")
{
    cleanupDb(DB_PATH);
    SQLiteManager m;

    REQUIRE(m.OpenDatabase(CONN, DB_PATH));
    CHECK(m.IsDatabaseOpen(CONN));
    CHECK(fs::exists(DB_PATH));

    m.CloseDatabase(CONN);
    CHECK_FALSE(m.IsDatabaseOpen(CONN));
}

/////////////////////////////////////////////////////////////////////

TEST_CASE("createTable_insertAndSelect_roundTripsValues", "[crud]")
{
    TestDb t;

    REQUIRE(t.insert("apple", 5, 1.50));
    CHECK(t.mgr.Count(CONN, TABLE) == 1);

    const DbRows rows = t.mgr.SelectAll(CONN, TABLE, {"name", "qty", "price"});
    REQUIRE(rows.size() == 1);

    const DbRow &row = rows.front();
    CHECK(std::get<std::string>(row.at("name")) == "apple");
    CHECK(std::get<int64_t>(row.at("qty")) == 5);
    CHECK_THAT(std::get<double>(row.at("price")), Catch::Matchers::WithinAbs(1.50, 1e-9));

    const DbRecord first = t.mgr.SelectFirst(CONN, TABLE, "name = ?", {std::string("apple")});
    CHECK(std::get<std::string>(first.at("name")) == "apple");
    CHECK(std::get<int64_t>(first.at("qty")) == 5);
}

/////////////////////////////////////////////////////////////////////

TEST_CASE("updateAndRemove_applyWhereBindings", "[crud]")
{
    TestDb t;
    REQUIRE(t.insert("apple", 5));
    REQUIRE(t.insert("banana", 3));

    REQUIRE(t.mgr.Update(CONN, TABLE, {{"qty", int64_t{99}}}, "name = ?", {std::string("apple")}));
    CHECK(std::get<int64_t>(t.mgr.SelectFirst(CONN, TABLE, "name = ?", {std::string("apple")}).at("qty")) == 99);
    CHECK(std::get<int64_t>(t.mgr.SelectFirst(CONN, TABLE, "name = ?", {std::string("banana")}).at("qty")) == 3);

    REQUIRE(t.mgr.Remove(CONN, TABLE, "name = ?", {std::string("banana")}));
    CHECK(t.mgr.Count(CONN, TABLE) == 1);
    CHECK(t.mgr.Count(CONN, TABLE, "name = ?", {std::string("banana")}) == 0);
}

/////////////////////////////////////////////////////////////////////

TEST_CASE("transactionRollback_discardsInsertedRows", "[transaction]")
{
    TestDb t;

    REQUIRE(t.mgr.BeginTransaction(CONN));
    REQUIRE(t.insert("should_disappear"));
    CHECK(t.mgr.Count(CONN, TABLE) == 1);
    REQUIRE(t.mgr.RollbackTransaction(CONN));
    CHECK(t.mgr.Count(CONN, TABLE) == 0);
}

/////////////////////////////////////////////////////////////////////

TEST_CASE("dropTable_removesExistingTable", "[schema]")
{
    TestDb t;

    REQUIRE(t.mgr.TableExists(CONN, TABLE));
    REQUIRE(t.mgr.DropTable(CONN, TABLE));
    CHECK_FALSE(t.mgr.TableExists(CONN, TABLE));
}

/////////////////////////////////////////////////////////////////////

TEST_CASE("invalidQuery_setsLastError", "[error]")
{
    TestDb t;

    CHECK_FALSE(t.mgr.ExecuteSql(CONN, "SELECT * FROM missing_table"));
    CHECK_FALSE(t.mgr.LastError().empty());
}

/////////////////////////////////////////////////////////////////////
// Connection
/////////////////////////////////////////////////////////////////////

TEST_CASE("openDatabase_alreadyOpen_returnsTrue", "[connection]")
{
    TestDb t;
    CHECK(t.mgr.OpenDatabase(CONN, DB_PATH));
}

/////////////////////////////////////////////////////////////////////

TEST_CASE("closeDatabase_unknownConnection_doesNotThrow", "[connection]")
{
    SQLiteManager m;
    REQUIRE_NOTHROW(m.CloseDatabase("nonexistent"));
}

/////////////////////////////////////////////////////////////////////

TEST_CASE("isDatabaseOpen_emptyConnection_usesDefaultConnection", "[connection]")
{
    cleanupDb(DB_PATH);
    SQLiteManager m;

    REQUIRE(m.OpenDatabase("", DB_PATH));
    CHECK(m.IsDatabaseOpen(""));

    m.CloseDatabase("");
    CHECK_FALSE(m.IsDatabaseOpen(""));
}

/////////////////////////////////////////////////////////////////////

TEST_CASE("isDatabaseOpen_afterClose_returnsFalse", "[connection]")
{
    TestDb t;

    REQUIRE(t.mgr.IsDatabaseOpen(CONN));
    t.mgr.CloseDatabase(CONN);
    CHECK_FALSE(t.mgr.IsDatabaseOpen(CONN));
}

/////////////////////////////////////////////////////////////////////
// File management
/////////////////////////////////////////////////////////////////////

TEST_CASE("createDatabase_nestedDirectories_createsPathAndFile", "[database]")
{
    const std::string root = "/tmp/sqlitemgr_test_dir";
    const std::string nested = root + "/sub/test.db";
    fs::remove_all(root);

    SQLiteManager m;
    REQUIRE(m.CreateDatabase(CONN, nested));
    CHECK(fs::exists(nested));

    m.CloseDatabase(CONN);
    fs::remove_all(root);
}

/////////////////////////////////////////////////////////////////////

TEST_CASE("deleteDatabase_removesWalAndShmSideFiles", "[database]")
{
    TestDb t;
    REQUIRE(t.insert("dummy"));

    REQUIRE(t.mgr.DeleteDatabase(CONN, DB_PATH));
    CHECK_FALSE(fs::exists(DB_PATH));
    CHECK_FALSE(fs::exists(DB_PATH + "-wal"));
    CHECK_FALSE(fs::exists(DB_PATH + "-shm"));
}

/////////////////////////////////////////////////////////////////////

TEST_CASE("databaseFileExists_missingFile_returnsFalse", "[database]")
{
    SQLiteManager m;
    CHECK_FALSE(m.DatabaseFileExists("/tmp/this_file_should_not_exist_lymalink.sqlite"));
}

/////////////////////////////////////////////////////////////////////
// CRUD edge cases
/////////////////////////////////////////////////////////////////////

TEST_CASE("insert_multipleRows_incrementsCount", "[crud]")
{
    TestDb t;

    REQUIRE(t.insert("apple", 5, 1.50));
    REQUIRE(t.insert("banana", 3, 0.75));
    REQUIRE(t.insert("cherry", 10, 2.00));

    CHECK(t.mgr.Count(CONN, TABLE) == 3);
}

/////////////////////////////////////////////////////////////////////

TEST_CASE("insert_emptyMap_returnsFalse", "[crud]")
{
    TestDb t;

    CHECK_FALSE(t.mgr.Insert(CONN, TABLE, {}));
    CHECK_FALSE(t.mgr.LastError().empty());
}

/////////////////////////////////////////////////////////////////////

TEST_CASE("insert_nullValue_roundTripsAsNull", "[crud]")
{
    TestDb t;
    REQUIRE(t.mgr.Insert(CONN, TABLE, {
        {"name", std::string("nullprice")},
        {"qty", int64_t{1}},
        {"price", DbNull{}}
    }));

    const DbRecord row = t.mgr.SelectFirst(CONN, TABLE, "name = ?", {std::string("nullprice")});
    REQUIRE_FALSE(row.empty());
    CHECK(std::holds_alternative<DbNull>(row.at("price")));
}

/////////////////////////////////////////////////////////////////////

TEST_CASE("insert_closedDatabase_returnsFalse", "[crud]")
{
    SQLiteManager m;
    CHECK_FALSE(m.Insert("closed", TABLE, item("x")));
}

/////////////////////////////////////////////////////////////////////

TEST_CASE("update_withoutWhere_updatesAllRows", "[crud]")
{
    TestDb t;
    REQUIRE(t.insert("a", 1));
    REQUIRE(t.insert("b", 2));
    REQUIRE(t.insert("c", 3));

    REQUIRE(t.mgr.Update(CONN, TABLE, {{"qty", int64_t{0}}}, {}));
    CHECK(t.mgr.Count(CONN, TABLE, "qty = ?", {int64_t{0}}) == 3);
}

/////////////////////////////////////////////////////////////////////

TEST_CASE("update_emptyMap_returnsFalse", "[crud]")
{
    TestDb t;
    CHECK_FALSE(t.mgr.Update(CONN, TABLE, {}, "id = ?", {int64_t{1}}));
}

/////////////////////////////////////////////////////////////////////

TEST_CASE("update_closedDatabase_returnsFalse", "[crud]")
{
    SQLiteManager m;
    CHECK_FALSE(m.Update("closed", TABLE, {{"qty", int64_t{1}}}, {}));
}

/////////////////////////////////////////////////////////////////////

TEST_CASE("remove_withoutWhere_deletesAllRows", "[crud]")
{
    TestDb t;
    REQUIRE(t.insert("x"));
    REQUIRE(t.insert("y"));

    REQUIRE(t.mgr.Remove(CONN, TABLE, {}));
    CHECK(t.mgr.Count(CONN, TABLE) == 0);
}

/////////////////////////////////////////////////////////////////////

TEST_CASE("remove_closedDatabase_returnsFalse", "[crud]")
{
    SQLiteManager m;
    CHECK_FALSE(m.Remove("closed", TABLE, {}));
}

/////////////////////////////////////////////////////////////////////

TEST_CASE("selectAll_multipleRows_returnsAllRows", "[crud]")
{
    TestDb t;
    REQUIRE(t.insert("a"));
    REQUIRE(t.insert("b"));
    REQUIRE(t.insert("c"));

    CHECK(t.mgr.SelectAll(CONN, TABLE).size() == 3);
}

/////////////////////////////////////////////////////////////////////

TEST_CASE("selectAll_emptyTable_returnsEmptyList", "[crud]")
{
    TestDb t;
    CHECK(t.mgr.SelectAll(CONN, TABLE).empty());
}

/////////////////////////////////////////////////////////////////////

TEST_CASE("selectWhere_noMatch_returnsEmptyList", "[crud]")
{
    TestDb t;
    REQUIRE(t.insert("apple"));

    CHECK(t.mgr.SelectWhere(CONN, TABLE, "name = ?", {std::string("mango")}).empty());
}

/////////////////////////////////////////////////////////////////////

TEST_CASE("selectFirst_noMatch_returnsEmptyMap", "[crud]")
{
    TestDb t;
    CHECK(t.mgr.SelectFirst(CONN, TABLE, "name = ?", {std::string("ghost")}).empty());
}

/////////////////////////////////////////////////////////////////////

TEST_CASE("selectWhere_withColumnFilter_returnsOnlyRequestedColumns", "[crud]")
{
    TestDb t;
    REQUIRE(t.insert("apple", 5, 1.50));

    const DbRows rows = t.mgr.SelectWhere(CONN, TABLE, "name = ?", {std::string("apple")}, {"name", "qty"});
    REQUIRE(rows.size() == 1);

    CHECK(rows.front().contains("name"));
    CHECK(rows.front().contains("qty"));
    CHECK_FALSE(rows.front().contains("price"));
    CHECK_FALSE(rows.front().contains("id"));
}

/////////////////////////////////////////////////////////////////////

TEST_CASE("selectFirst_matchingRows_returnsFirstRow", "[crud]")
{
    TestDb t;
    REQUIRE(t.insert("alpha"));
    REQUIRE(t.insert("alpha"));

    const DbRecord row = t.mgr.SelectFirst(CONN, TABLE, "name = ?", {std::string("alpha")});
    REQUIRE_FALSE(row.empty());
    CHECK(std::get<std::string>(row.at("name")) == "alpha");
}

/////////////////////////////////////////////////////////////////////

TEST_CASE("selectAll_closedDatabase_returnsEmptyList", "[crud]")
{
    SQLiteManager m;
    CHECK(m.SelectAll("closed", TABLE).empty());
}

/////////////////////////////////////////////////////////////////////

TEST_CASE("selectFirst_closedDatabase_returnsEmptyMap", "[crud]")
{
    SQLiteManager m;
    CHECK(m.SelectFirst("closed", TABLE).empty());
}

/////////////////////////////////////////////////////////////////////

TEST_CASE("count_withAndWithoutWhere", "[crud]")
{
    TestDb t;
    REQUIRE(t.insert("x"));
    REQUIRE(t.insert("x"));
    REQUIRE(t.insert("y"));

    CHECK(t.mgr.Count(CONN, TABLE) == 3);
    CHECK(t.mgr.Count(CONN, TABLE, "name = ?", {std::string("x")}) == 2);
    CHECK(t.mgr.Count(CONN, TABLE, "name = ?", {std::string("y")}) == 1);
    CHECK(t.mgr.Count(CONN, TABLE, "name = ?", {std::string("z")}) == 0);
}

/////////////////////////////////////////////////////////////////////

TEST_CASE("count_emptyTable_returnsZero", "[crud]")
{
    TestDb t;
    CHECK(t.mgr.Count(CONN, TABLE) == 0);
}

/////////////////////////////////////////////////////////////////////

TEST_CASE("count_closedDatabase_returnsNegativeOne", "[crud]")
{
    SQLiteManager m;
    CHECK(m.Count("closed", TABLE) == -1);
}

/////////////////////////////////////////////////////////////////////

TEST_CASE("bindValues_parameterCountMismatch_fails", "[crud]")
{
    TestDb t;
    REQUIRE(t.insert("x"));

    CHECK(t.mgr.Count(CONN, TABLE, "name = ?") == -1);
    CHECK(t.mgr.Count(CONN, TABLE, "name = ?", {std::string("x"), int64_t{1}}) == -1);
    CHECK_FALSE(t.mgr.LastError().empty());
}

/////////////////////////////////////////////////////////////////////

TEST_CASE("dbValueTypes_insertAndSelect_roundTrip", "[crud]")
{
    TestDb t;
    REQUIRE(t.mgr.Insert(CONN, TABLE, {
        {"name", std::string("typed")},
        {"qty", int64_t{42}},
        {"price", 3.14}
    }));

    const DbRecord row = t.mgr.SelectFirst(CONN, TABLE, "name = ?", {std::string("typed")});
    REQUIRE_FALSE(row.empty());
    CHECK(std::get<std::string>(row.at("name")) == "typed");
    CHECK(std::get<int64_t>(row.at("qty")) == 42);
    CHECK_THAT(std::get<double>(row.at("price")), Catch::Matchers::WithinAbs(3.14, 1e-9));
}

/////////////////////////////////////////////////////////////////////

TEST_CASE("dbValueTypes_textWithEmbeddedNull_roundTrips", "[crud]")
{
    TestDb t;
    const std::string name{"ab\0cd", 5};

    REQUIRE(t.mgr.Insert(CONN, TABLE, {
        {"name", name},
        {"qty", int64_t{7}},
        {"price", 1.25}
    }));

    const DbRecord row = t.mgr.SelectFirst(CONN, TABLE, "name = ?", {name});
    REQUIRE_FALSE(row.empty());
    CHECK(std::get<std::string>(row.at("name")) == name);
}

/////////////////////////////////////////////////////////////////////

TEST_CASE("dbValueTypes_blob_roundTrips", "[crud]")
{
    TestDb t;
    REQUIRE(t.mgr.ExecuteSql(CONN, "CREATE TABLE files (id INTEGER PRIMARY KEY, data BLOB NOT NULL)"));

    const DbBlob bytes{std::byte{0x00}, std::byte{0x41}, std::byte{0xff}};
    REQUIRE(t.mgr.Insert(CONN, "files", {{"id", int64_t{1}}, {"data", bytes}}));
    REQUIRE(t.mgr.Insert(CONN, "files", {{"id", int64_t{2}}, {"data", DbBlob{}}}));

    const DbRecord nonEmpty = t.mgr.SelectFirst(CONN, "files", "id = ?", {int64_t{1}});
    REQUIRE_FALSE(nonEmpty.empty());
    CHECK(std::get<DbBlob>(nonEmpty.at("data")) == bytes);

    const DbRecord empty = t.mgr.SelectFirst(CONN, "files", "id = ?", {int64_t{2}});
    REQUIRE_FALSE(empty.empty());
    CHECK(std::get<DbBlob>(empty.at("data")).empty());
}

/////////////////////////////////////////////////////////////////////
// Transactions
/////////////////////////////////////////////////////////////////////

TEST_CASE("transactionCommit_persistsInsertedRows", "[transaction]")
{
    TestDb t;

    REQUIRE(t.mgr.BeginTransaction(CONN));
    REQUIRE(t.insert("tx_row"));
    REQUIRE(t.mgr.CommitTransaction(CONN));

    t.mgr.CloseDatabase(CONN);
    REQUIRE(t.mgr.OpenDatabase(CONN, DB_PATH));
    CHECK(t.mgr.Count(CONN, TABLE) == 1);
}

/////////////////////////////////////////////////////////////////////

TEST_CASE("transaction_closedDatabase_returnsFalse", "[transaction]")
{
    SQLiteManager m;

    CHECK_FALSE(m.BeginTransaction("closed"));
    CHECK_FALSE(m.CommitTransaction("closed"));
    CHECK_FALSE(m.RollbackTransaction("closed"));
}

/////////////////////////////////////////////////////////////////////
// Schema
/////////////////////////////////////////////////////////////////////

TEST_CASE("createTable_idempotent_secondCallSucceeds", "[schema]")
{
    TestDb t;
    CHECK(t.mgr.CreateTable(CONN, TABLE, COLS));
}

/////////////////////////////////////////////////////////////////////

TEST_CASE("tableExists_nonExistentTable_returnsFalse", "[schema]")
{
    TestDb t;
    CHECK_FALSE(t.mgr.TableExists(CONN, "no_such_table"));
}

/////////////////////////////////////////////////////////////////////

TEST_CASE("dropTable_nonExistentTable_returnsTrue", "[schema]")
{
    TestDb t;
    CHECK(t.mgr.DropTable(CONN, "ghost_table"));
}

/////////////////////////////////////////////////////////////////////

TEST_CASE("executeSql_createTable_succeeds", "[schema]")
{
    TestDb t;

    CHECK(t.mgr.ExecuteSql(CONN, "CREATE TABLE IF NOT EXISTS extra (x INTEGER)"));
    CHECK(t.mgr.TableExists(CONN, "extra"));
}

/////////////////////////////////////////////////////////////////////

TEST_CASE("executeSql_badSql_setsLastError", "[schema]")
{
    TestDb t;

    CHECK_FALSE(t.mgr.ExecuteSql(CONN, "THIS IS NOT SQL"));
    CHECK_FALSE(t.mgr.LastError().empty());
}

/////////////////////////////////////////////////////////////////////

TEST_CASE("insert_missingTable_setsLastError", "[error]")
{
    TestDb t;

    CHECK_FALSE(t.mgr.Insert(CONN, "no_table", {{"x", std::string("y")}}));
    CHECK_FALSE(t.mgr.LastError().empty());
}

/////////////////////////////////////////////////////////////////////

TEST_CASE("gognParser_arrayFormat_parsesUnlockedAchievements", "[gogn]")
{
    const fs::path root = "/tmp/lymalink_gogn_parser_array";
    fs::remove_all(root);
    fs::create_directories(root);
    const fs::path filePath = root / "achievements.json";
    {
        std::ofstream file(filePath);
        file << R"([
            {"earned_time": 1781358362, "name": "NEW_ACHIEVEMENT_1_31"},
            {"earned_time": "1781359761", "name": "NEW_ACHIEVEMENT_1_3"}
        ])";
    }

    GoGNParser parser;
    const std::vector<AchievementData> parsed = parser.Parse(filePath.string());

    REQUIRE(parsed.size() == 2);
    CHECK(parsed[0].key == "NEW_ACHIEVEMENT_1_31");
    CHECK(parsed[0].achieved);
    CHECK(parsed[0].unlockTime == 1781358362);
    CHECK(parsed[1].key == "NEW_ACHIEVEMENT_1_3");
    CHECK(parsed[1].achieved);
    CHECK(parsed[1].unlockTime == 1781359761);

    fs::remove_all(root);
}

/////////////////////////////////////////////////////////////////////

TEST_CASE("gognParser_objectFormat_acceptsLegacyTimestampKeys", "[gogn]")
{
    const fs::path root = "/tmp/lymalink_gogn_parser_object";
    fs::remove_all(root);
    fs::create_directories(root);
    const fs::path filePath = root / "achievements.json";
    {
        std::ofstream file(filePath);
        file << R"({
            "ACH_UNLOCK_TIME": {"unlock_time": 1710000000},
            "ACH_UNLOCK_TIME_CAMEL": {"unlockTime": "1710500000"},
            "ACH_UNLOCK_DATE": {"unlock_date": 1710600000}
        })";
    }

    GoGNParser parser;
    const std::vector<AchievementData> parsed = parser.Parse(filePath.string());

    REQUIRE(parsed.size() == 3);
    CHECK(parsed[0].key == "ACH_UNLOCK_DATE");
    CHECK(parsed[0].achieved);
    CHECK(parsed[0].unlockTime == 1710600000);
    CHECK(parsed[1].key == "ACH_UNLOCK_TIME");
    CHECK(parsed[1].achieved);
    CHECK(parsed[1].unlockTime == 1710000000);
    CHECK(parsed[2].key == "ACH_UNLOCK_TIME_CAMEL");
    CHECK(parsed[2].achieved);
    CHECK(parsed[2].unlockTime == 1710500000);

    fs::remove_all(root);
}

/////////////////////////////////////////////////////////////////////

TEST_CASE("gognParser_missingOrInvalidFile_returnsEmpty", "[gogn]")
{
    const fs::path root = "/tmp/lymalink_gogn_parser_invalid";
    fs::remove_all(root);
    fs::create_directories(root);
    const fs::path filePath = root / "achievements.json";
    {
        std::ofstream file(filePath);
        file << "not json";
    }

    GoGNParser parser;
    CHECK(parser.Parse("/tmp/lymalink_missing_achievements.json").empty());
    CHECK(parser.Parse(filePath.string()).empty());

    fs::remove_all(root);
}

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

/////////////////////////////////////////////////////////////////////
///////////////////////////// PRIVATE ///////////////////////////////
/////////////////////////////////////////////////////////////////////

namespace
{
TestDb::TestDb(const std::string &dbPath)
    : path(dbPath)
{
    cleanupDb(path);
    REQUIRE(mgr.CreateDatabase(CONN, path));
    REQUIRE(mgr.CreateTable(CONN, TABLE, COLS));
}

/////////////////////////////////////////////////////////////////////

TestDb::~TestDb()
{
    mgr.CloseDatabase(CONN);
    cleanupDb(path);
}

/////////////////////////////////////////////////////////////////////

bool TestDb::insert(const std::string &name, int64_t qty, double price)
{
    return mgr.Insert(CONN, TABLE, item(name, qty, price));
}

/////////////////////////////////////////////////////////////////////

void cleanupDb(const std::string &path)
{
    for (const auto &file : {path, path + "-wal", path + "-shm"})
    {
        if (fs::exists(file))
        {
            fs::remove(file);
        }
    }
}

/////////////////////////////////////////////////////////////////////

DbRecord item(const std::string &name, int64_t qty, double price)
{
    return {
        {"name", name},
        {"qty", qty},
        {"price", price}
    };
}
}
