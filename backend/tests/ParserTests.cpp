/////////////////////////////////////////////////////////
// File: ParserTests.cpp
// Date: 2026-08-02
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Tests achievement file parsers
/////////////////////////////////////////////////////////

#if __has_include(<catch2/catch_test_macros.hpp>)
#include <catch2/catch_test_macros.hpp>
#else
#include <catch2/catch.hpp>
#endif

#include "tools/Utils.h"
#include "tools/parsers/GoGNParser.h"
#include "tools/parsers/RLDParser.h"
#include "tools/parsers/SSEParser.h"
#include "tools/parsers/TenokeParser.h"

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

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

TEST_CASE("tenokeParser_exampleOne_parsesUnlockedAchievementsAndIgnoresStats", "[tenoke]")
{
    const fs::path root = "/tmp/lymalink_tenoke_parser_example1";
    fs::remove_all(root);
    fs::create_directories(root);
    const fs::path filePath = root / "user_stats.ini";
    {
        std::ofstream file(filePath);
        file << R"([STATS]
"stat_collectable" = 1.000000

[ACHIEVEMENTS]
"awakened" = {unlocked = true, time = 1785652344}
"new_achievement_1_12" = {unlocked = true, time = 1785652392}
"new_achievement_1_27" = {unlocked = true, time = 1785652537}
"new_achievement_1_26" = {unlocked = true, time = 1785652563}
)";
    }

    TenokeParser parser;
    const std::vector<AchievementData> parsed = parser.Parse(filePath.string());

    REQUIRE(parsed.size() == 4);
    CHECK(parsed[0].key == "awakened");
    CHECK(parsed[0].achieved);
    CHECK(parsed[0].unlockTime == 1785652344);
    CHECK_FALSE(parsed[0].hasCurProgress);
    CHECK_FALSE(parsed[0].hasMaxProgress);
    CHECK(parsed[1].key == "new_achievement_1_12");
    CHECK(parsed[1].unlockTime == 1785652392);
    CHECK(parsed[2].key == "new_achievement_1_27");
    CHECK(parsed[2].unlockTime == 1785652537);
    CHECK(parsed[3].key == "new_achievement_1_26");
    CHECK(parsed[3].unlockTime == 1785652563);

    fs::remove_all(root);
}

/////////////////////////////////////////////////////////////////////

TEST_CASE("tenokeParser_exampleTwo_parsesUnlockedAchievements", "[tenoke]")
{
    const fs::path root = "/tmp/lymalink_tenoke_parser_example2";
    fs::remove_all(root);
    fs::create_directories(root);
    const fs::path filePath = root / "user_stats.ini";
    {
        std::ofstream file(filePath);
        file << R"([ACHIEVEMENTS]
"first_fusion" = {unlocked = true, time = 1781702106}
"first_evo" = {unlocked = true, time = 1781702331}
)";
    }

    TenokeParser parser;
    const std::vector<AchievementData> parsed = parser.Parse(filePath.string());

    REQUIRE(parsed.size() == 2);
    CHECK(parsed[0].key == "first_fusion");
    CHECK(parsed[0].achieved);
    CHECK(parsed[0].unlockTime == 1781702106);
    CHECK(parsed[1].key == "first_evo");
    CHECK(parsed[1].achieved);
    CHECK(parsed[1].unlockTime == 1781702331);

    fs::remove_all(root);
}

/////////////////////////////////////////////////////////////////////

TEST_CASE("tenokeParser_lockedAchievement_hasNoProgress", "[tenoke]")
{
    const fs::path root = "/tmp/lymalink_tenoke_parser_locked";
    fs::remove_all(root);
    fs::create_directories(root);
    const fs::path filePath = root / "user_stats.ini";
    {
        std::ofstream file(filePath);
        file << R"([ACHIEVEMENTS]
"still_locked" = {unlocked = false, time = 0}
"numeric_locked" = {unlocked = 0, time = 0}
)";
    }

    TenokeParser parser;
    const std::vector<AchievementData> parsed = parser.Parse(filePath.string());

    REQUIRE(parsed.size() == 2);
    CHECK(parsed[0].key == "still_locked");
    CHECK_FALSE(parsed[0].achieved);
    CHECK(parsed[0].unlockTime == 0);
    CHECK_FALSE(parsed[0].hasCurProgress);
    CHECK_FALSE(parsed[0].hasMaxProgress);
    CHECK(parsed[1].key == "numeric_locked");
    CHECK_FALSE(parsed[1].achieved);

    fs::remove_all(root);
}

/////////////////////////////////////////////////////////////////////

TEST_CASE("tenokeParser_missingOrMalformedFile_returnsEmpty", "[tenoke]")
{
    const fs::path root = "/tmp/lymalink_tenoke_parser_invalid";
    fs::remove_all(root);
    fs::create_directories(root);
    const fs::path filePath = root / "user_stats.ini";
    {
        std::ofstream file(filePath);
        file << R"([ACHIEVEMENTS]
malformed = true
"missing_time" = {unlocked = true}
[STATS]
"stat_collectable" = 1.000000
)";
    }

    TenokeParser parser;
    CHECK(parser.Parse((root / "missing.ini").string()).empty());
    CHECK(parser.Parse(filePath.string()).empty());

    fs::remove_all(root);
}

/////////////////////////////////////////////////////////////////////

TEST_CASE("rldParser_firstAchievementFormat_parsesTimestampProgressUnlock", "[rld]")
{
    const fs::path root = "/tmp/lymalink_rld_parser_first";
    fs::remove_all(root);
    fs::create_directories(root);
    const fs::path filePath = root / "achievements.ini";
    {
        std::ofstream file(filePath);
        file << R"([ACHIEVEMENT_23]
Time=2073676A00
CurProgress=0000000000
MaxProgress=0000000000
)";
    }

    RLDParser parser;
    const std::vector<AchievementData> parsed = parser.Parse(filePath.string());

    REQUIRE(parsed.size() == 1);
    CHECK(parsed[0].key == "ACHIEVEMENT_23");
    CHECK(parsed[0].achieved);
    CHECK(parsed[0].unlockTime == 1785164576);
    CHECK(parsed[0].hasCurProgress);
    CHECK(parsed[0].curProgress == 0);
    CHECK(parsed[0].hasMaxProgress);
    CHECK(parsed[0].maxProgress == 0);

    fs::remove_all(root);
}

/////////////////////////////////////////////////////////////////////

TEST_CASE("rldParser_secondAchievementFormat_parsesBothUnlocks", "[rld]")
{
    const fs::path root = "/tmp/lymalink_rld_parser_second";
    fs::remove_all(root);
    fs::create_directories(root);
    const fs::path filePath = root / "achievements.ini";
    {
        std::ofstream file(filePath);
        file << R"([ACHIEVEMENT_23]
Time=2073676A00
CurProgress=0000000000
MaxProgress=0000000000

[ACHIEVEMENT_33]
Time=2073676A00
CurProgress=0000000000
MaxProgress=0000000000
)";
    }

    RLDParser parser;
    const std::vector<AchievementData> parsed = parser.Parse(filePath.string());

    REQUIRE(parsed.size() == 2);
    CHECK(parsed[0].key == "ACHIEVEMENT_23");
    CHECK(parsed[0].achieved);
    CHECK(parsed[0].unlockTime == 1785164576);
    CHECK(parsed[1].key == "ACHIEVEMENT_33");
    CHECK(parsed[1].achieved);

    fs::remove_all(root);
}

/////////////////////////////////////////////////////////////////////

TEST_CASE("utils_crc32_matchesKnownCheckValue", "[utils]")
{
    CHECK(Utils::ToUpperHexUint32(Utils::Crc32("123456789")) == "CBF43926");
}

/////////////////////////////////////////////////////////////////////

TEST_CASE("sseParser_singleAchievementBinary_parsesUnlockedAchievement", "[ste]")
{
    const fs::path root = "/tmp/lymalink_ste_parser_single";
    fs::remove_all(root);
    fs::create_directories(root);
    const fs::path filePath = root / "stats.bin";
    {
        const std::vector<unsigned char> bytes = {
            0x01, 0x00, 0x00, 0x00,
            0xD1, 0x0C, 0xC8, 0x1B, 0x8C, 0x02, 0x00, 0x00,
            0xDC, 0x94, 0x6D, 0x6A, 0x00, 0x00, 0x00, 0x00,
            0x01, 0x74, 0xA7, 0x49, 0x01, 0x00, 0x00, 0x00
        };
        std::ofstream file(filePath, std::ios::binary);
        file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    }

    SSEParser parser;
    const std::vector<AchievementData> parsed = parser.Parse(filePath.string());

    REQUIRE(parsed.size() == 1);
    CHECK(parsed[0].key == "crc32:1BC80CD1");
    CHECK(parsed[0].achieved);
    CHECK(parsed[0].unlockTime == 1785566428);

    fs::remove_all(root);
}

/////////////////////////////////////////////////////////////////////

TEST_CASE("sseParser_twoAchievementBinary_parsesBothUnlocks", "[ste]")
{
    const fs::path root = "/tmp/lymalink_ste_parser_double";
    fs::remove_all(root);
    fs::create_directories(root);
    const fs::path filePath = root / "stats.bin";
    {
        const std::vector<unsigned char> bytes = {
            0x02, 0x00, 0x00, 0x00,
            0xD1, 0x0C, 0xC8, 0x1B, 0xC9, 0x01, 0x00, 0x00,
            0x81, 0x90, 0x6D, 0x6A, 0x00, 0x00, 0x00, 0x00,
            0x01, 0x24, 0x18, 0xCD, 0x01, 0x00, 0x00, 0x00,
            0xF7, 0x6A, 0xCC, 0xD1, 0xC9, 0x01, 0x00, 0x00,
            0x11, 0x90, 0x6D, 0x6A, 0x00, 0x00, 0x00, 0x00,
            0x01, 0x77, 0x1F, 0xCD, 0x01, 0x00, 0x00, 0x00
        };
        std::ofstream file(filePath, std::ios::binary);
        file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    }

    SSEParser parser;
    const std::vector<AchievementData> parsed = parser.Parse(filePath.string());

    REQUIRE(parsed.size() == 2);
    CHECK(parsed[0].key == "crc32:1BC80CD1");
    CHECK(parsed[0].achieved);
    CHECK(parsed[0].unlockTime == 1785565313);
    CHECK(parsed[1].key == "crc32:D1CC6AF7");
    CHECK(parsed[1].achieved);
    CHECK(parsed[1].unlockTime == 1785565201);

    fs::remove_all(root);
}

/////////////////////////////////////////////////////////////////////

TEST_CASE("sseParser_mixedValues_skipsStatsAndKeepsLockedAchievements", "[ste]")
{
    const fs::path root = "/tmp/lymalink_ste_parser_mixed";
    fs::remove_all(root);
    fs::create_directories(root);
    const fs::path filePath = root / "stats.bin";
    {
        const std::vector<unsigned char> bytes = {
            0x03, 0x00, 0x00, 0x00,
            0x11, 0x22, 0x33, 0x44, 0x00, 0x00, 0x00, 0x00,
            0x64, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x55, 0x66, 0x77, 0x88, 0x00, 0x00, 0x00, 0x00,
            0xC8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
            0x99, 0xAA, 0xBB, 0xCC, 0x00, 0x00, 0x00, 0x00,
            0x2C, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00
        };
        std::ofstream file(filePath, std::ios::binary);
        file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    }

    SSEParser parser;
    const std::vector<AchievementData> parsed = parser.Parse(filePath.string());

    REQUIRE(parsed.size() == 2);
    CHECK(parsed[0].key == "crc32:44332211");
    CHECK_FALSE(parsed[0].achieved);
    CHECK(parsed[0].unlockTime == 100);
    CHECK(parsed[1].key == "crc32:88776655");
    CHECK(parsed[1].achieved);
    CHECK(parsed[1].unlockTime == 200);

    fs::remove_all(root);
}

/////////////////////////////////////////////////////////////////////

TEST_CASE("sseParser_missingOrMalformedFile_returnsEmpty", "[ste]")
{
    const fs::path root = "/tmp/lymalink_ste_parser_invalid";
    fs::remove_all(root);
    fs::create_directories(root);

    SSEParser parser;
    CHECK(parser.Parse((root / "missing.bin").string()).empty());

    {
        const std::vector<unsigned char> bytes = {0x01, 0x00, 0x00};
        std::ofstream file(root / "too_short.bin", std::ios::binary);
        file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    }
    CHECK(parser.Parse((root / "too_short.bin").string()).empty());

    {
        const std::vector<unsigned char> bytes = {0x02, 0x00, 0x00, 0x00};
        std::ofstream file(root / "truncated.bin", std::ios::binary);
        file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    }
    CHECK(parser.Parse((root / "truncated.bin").string()).empty());

    fs::remove_all(root);
}
