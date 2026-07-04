cmake_minimum_required(VERSION 3.21)
project(lymalinkd LANGUAGES CXX)

if(NOT WIN32)
    message(FATAL_ERROR "This CMake entrypoint is for the Windows backend.")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(MINIAUDIO_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/miniaudio")
if(NOT EXISTS "${MINIAUDIO_DIR}/miniaudio.h")
    message(FATAL_ERROR "Miniaudio is missing. For automatic download of the library - Run '.\\build.ps1 release' build process instead")
endif()

if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/nlohmann/json.hpp")
    message(FATAL_ERROR "nlohmann/json is missing. For automatic download of the library - Run '.\\build.ps1 release' build process instead")
endif()

find_package(Qt6 REQUIRED COMPONENTS Core Network Sql)
find_package(SQLite3 REQUIRED)
qt_standard_project_setup()

qt_add_executable(lymalinkd
    src/main.cpp
    src/Lymalinkd.cpp src/Lymalinkd.h
    src/ipc/WinSocketServer.cpp src/ipc/WinSocketServer.h
    src/notification/AchievementNotificationService.cpp src/notification/AchievementNotificationService.h
    src/notification/WinNotificationService.cpp src/notification/WinNotificationService.h
    src/notification/WinSoundService.cpp src/notification/WinSoundService.h
    src/overlay/WinOverlayInjector.cpp src/overlay/WinOverlayInjector.h
    src/overlay/WinOverlayNotifier.cpp src/overlay/WinOverlayNotifier.h
    src/database/SQLiteManager.cpp src/database/SQLiteManager.h
    src/tools/Utils.cpp src/tools/Utils.h
    src/tools/Logger.cpp src/tools/Logger.h
    src/watcher/PathScanner.cpp src/watcher/PathScanner.h
    src/watcher/ProcessWatcher.cpp src/watcher/ProcessWatcher.h
    src/watcher/AchievementHandlerWin.cpp src/watcher/AchievementHandler.h
    src/tools/parsers/RUNECodexParser.cpp src/tools/parsers/RUNECodexParser.h
    src/tools/parsers/GoldbergParser.cpp src/tools/parsers/GoldbergParser.h
    src/tools/parsers/GoGNParser.cpp src/tools/parsers/GoGNParser.h
)
target_include_directories(lymalinkd PRIVATE src ../frontend/src ../backend-overlay/include ${MINIAUDIO_DIR})
target_link_libraries(lymalinkd PRIVATE Qt6::Core Qt6::Network Qt6::Sql SQLite::SQLite3 ole32 windowscodecs advapi32)
set_target_properties(lymalinkd PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(lymalinkd PRIVATE BACKEND_DEBUG)
endif()
