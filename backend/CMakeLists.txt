cmake_minimum_required(VERSION 3.21)

file(STRINGS "${CMAKE_CURRENT_LIST_DIR}/../VERSION" VERSION_DATA)
list(GET VERSION_DATA 0 LYMALINK_VERSION)
string(STRIP "${LYMALINK_VERSION}" LYMALINK_VERSION)

project(lymalinkd VERSION ${LYMALINK_VERSION} LANGUAGES CXX)

if(NOT WIN32)
    message(FATAL_ERROR "This CMake entrypoint is for the Windows backend.")
endif()

enable_language(RC)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(LYMALINK_VERSION_TWEAK_VALUE ${PROJECT_VERSION_TWEAK})
if("${LYMALINK_VERSION_TWEAK_VALUE}" STREQUAL "")
    set(LYMALINK_VERSION_TWEAK_VALUE 0)
endif()

if(PROJECT_VERSION VERSION_LESS "1.0.0")
    set(LYMALINK_PRERELEASE_FLAG 0x00000002L)
else()
    set(LYMALINK_PRERELEASE_FLAG 0)
endif()

set(MINIAUDIO_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/miniaudio")
if(NOT EXISTS "${MINIAUDIO_DIR}/miniaudio.h")
    message(FATAL_ERROR "Miniaudio is missing. For automatic download of the library - Run '.\\build.ps1 release' build process instead")
endif()

if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/nlohmann/json.hpp")
    message(FATAL_ERROR "nlohmann/json is missing. For automatic download of the library - Run '.\\build.ps1 release' build process instead")
endif()

find_package(Qt6 REQUIRED COMPONENTS Core Gui Network Sql Widgets)
find_package(SQLite3 REQUIRED)
qt_standard_project_setup()

qt_add_executable(lymalinkd
    src/main.cpp
    res/windows/Lymalinkd.rc
    src/Lymalinkd.cpp src/Lymalinkd.h
    src/ipc/WinSocketServer.cpp src/ipc/WinSocketServer.h
    src/notification/AchievementNotificationService.cpp src/notification/AchievementNotificationService.h
    src/notification/WinNotificationService.cpp src/notification/WinNotificationService.h
    src/notification/WinSoundService.cpp src/notification/WinSoundService.h
    src/overlay/WinOverlayInjector.cpp src/overlay/WinOverlayInjector.h
    src/overlay/WinOverlayNotifier.cpp src/overlay/WinOverlayNotifier.h
    src/database/SQLiteManager.cpp src/database/SQLiteManager.h
    src/tools/Utils.cpp src/tools/Utils.h
    src/tools/AchievementKeyResolver.cpp src/tools/AchievementKeyResolver.h
    src/tools/Logger.cpp src/tools/Logger.h
    src/tray/BackendTrayIcon.cpp src/tray/BackendTrayIcon.h
    src/watcher/PathScanner.cpp src/watcher/PathScanner.h
    src/watcher/ProcessWatcher.cpp src/watcher/ProcessWatcher.h
    src/watcher/AchievementHandlerWin.cpp src/watcher/AchievementHandler.h
    src/tools/parsers/RUNECodexParser.cpp src/tools/parsers/RUNECodexParser.h
    src/tools/parsers/GoldbergParser.cpp src/tools/parsers/GoldbergParser.h
    src/tools/parsers/GoGNParser.cpp src/tools/parsers/GoGNParser.h
    src/tools/parsers/RLDParser.cpp src/tools/parsers/RLDParser.h
    src/tools/parsers/SSEParser.cpp src/tools/parsers/SSEParser.h
)
target_include_directories(lymalinkd PRIVATE src ../frontend/src ../backend-overlay/include ${MINIAUDIO_DIR})
target_link_libraries(lymalinkd PRIVATE Qt6::Core Qt6::Gui Qt6::Network Qt6::Sql Qt6::Widgets SQLite::SQLite3 ole32 windowscodecs advapi32)
target_compile_definitions(lymalinkd PRIVATE
    LYMALINK_PROJECT_URL="https://github.com/Morsomus/Lymalink"
    LYMALINK_LEGAL_COPYRIGHT="Licensed under GNU GPLv3"
    LYMALINK_PRERELEASE_FLAG=${LYMALINK_PRERELEASE_FLAG}
    LYMALINK_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
    LYMALINK_VERSION_MINOR=${PROJECT_VERSION_MINOR}
    LYMALINK_VERSION_PATCH=${PROJECT_VERSION_PATCH}
    LYMALINK_VERSION_TWEAK=${LYMALINK_VERSION_TWEAK_VALUE}
    LYMALINK_VERSION_STRING="${PROJECT_VERSION}"
)
set_target_properties(lymalinkd PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

set_target_properties(lymalinkd PROPERTIES WIN32_EXECUTABLE TRUE)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(lymalinkd PRIVATE BACKEND_DEBUG)
endif()
