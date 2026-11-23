/////////////////////////////////////////////////////////
// File: AchievementHandlerTests.cpp
// Date: 2026-11-23
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Tests achievement handler
/////////////////////////////////////////////////////////

#if __has_include(<catch2/catch_test_macros.hpp>)
#include <catch2/catch_test_macros.hpp>
#else
#include <catch2/catch.hpp>
#endif

#include "watcher/AchievementHandler.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <thread>

namespace fs = std::filesystem;

/////////////////////////////////////////////////////////

TEST_CASE("sovereignWatcher_tracksCustomFolderAndDirectFile", "[sovereign][watcher]")
{
    struct FileCase
    {
        const char* fileName;
        const char* contents;
        const char* key;
    };
    const FileCase files[] = {
        {"SOVEREIGN.ini", "[Achievements]\nSOV_KEY=1785652344\n", "SOV_KEY"},
        {"achievements.ini", "[INI_KEY]\nUnlocked=1785652344\n", "INI_KEY"}
    };

    const fs::path root = "/tmp/lymalink_sovereign_custom_watcher";
    fs::remove_all(root);
    for (const auto& file : files)
    {
        for (const bool directFile : {false, true})
        {
            const fs::path caseRoot = root / file.fileName / (directFile ? "direct" : "folder");
            const fs::path selectedDir = caseRoot / "selected";
            const fs::path stagingDir = caseRoot / "staging";
            fs::create_directories(selectedDir);
            fs::create_directories(stagingDir);
            {
                std::ofstream staged(stagingDir / file.fileName);
                staged << file.contents;
            }

            AchievementHandler handler;
            handler.Init();
            handler.AddTarget(1, selectedDir.string(), "SOVEREIGN", directFile ? file.fileName : "");
            handler.Start();
            fs::rename(stagingDir / file.fileName, selectedDir / file.fileName);

            bool detected = false;
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
            while (!detected && std::chrono::steady_clock::now() < deadline)
            {
                for (const AchievementData& achievement : handler.PollUnhandled(1))
                {
                    detected |= achievement.key == file.key && achievement.achieved;
                }
                if (!detected)
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(25));
                }
            }
            CHECK(detected);

            bool found = false;
            const auto scanned = handler.ReadAchievementFileOnce(1, selectedDir.string(), "SOVEREIGN", file.fileName, &found);
            CHECK(found);
            REQUIRE(scanned.size() == 1);
            CHECK(scanned[0].key == file.key);

            handler.Stop();
            if (directFile)
            {
                AchievementHandler existingFileHandler;
                existingFileHandler.Init();
                existingFileHandler.AddTarget(2, selectedDir.string(), "SOVEREIGN", file.fileName);
                const auto initial = existingFileHandler.PollUnhandled(2);
                REQUIRE(initial.size() == 1);
                CHECK(initial[0].key == file.key);
                existingFileHandler.Start();
                existingFileHandler.Stop();
            }
        }
    }
    fs::remove_all(root);
}
