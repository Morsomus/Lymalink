cmake_minimum_required(VERSION 3.16)

file(STRINGS "${CMAKE_CURRENT_LIST_DIR}/../VERSION" VERSION_DATA)
list(GET VERSION_DATA 0 LYMALINK_VERSION)
string(STRIP "${LYMALINK_VERSION}" LYMALINK_VERSION)

project(Lymalink VERSION ${LYMALINK_VERSION} LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Qml Quick Widgets Sql Network)
find_package(OpenSSL REQUIRED)
qt_standard_project_setup()
qt_policy(SET QTP0004 NEW)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_package(Qt6 REQUIRED COMPONENTS DBus)
    set(PLATFORM_BACKEND_SOURCES
        src/ipc/DBusService.h
        src/ipc/DBusService.cpp
    )
elseif(WIN32)
    find_package(Qt6 REQUIRED COMPONENTS QuickControls2)
    set(PLATFORM_BACKEND_SOURCES
        src/ipc/WinSocketService.h
        src/ipc/WinSocketService.cpp
        res/windows/Lymalink.rc
    )
else()
    message(FATAL_ERROR "Currently only supports Linux and Windows")
endif()

qt_add_executable(Lymalink
    src/main.cpp
    src/Lymalink.h
    src/Lymalink.cpp
    src/Settings.h
    src/Settings.cpp
    src/SysTray.h
    src/SysTray.cpp
    src/data/DataTransporter.h
    src/data/DataTransporter.cpp
    src/ipc/BackendControl.h
    src/ipc/BackendControl.cpp
    ${PLATFORM_BACKEND_SOURCES}
    src/database/SQLiteManager.h
    src/database/SQLiteManager.cpp
    src/tools/Encryption.h
    src/tools/Encryption.cpp
    src/tools/FileManager.h
    src/tools/FileManager.cpp
    src/tools/ImageCacheManager.h
    src/tools/ImageCacheManager.cpp
    src/tools/Logger.h
    src/tools/Logger.cpp
    src/tools/Utils.h
    src/tools/Utils.cpp
    src/api/SteamApi.h
    src/api/SteamApi.cpp
    src/api/SteamApiSearchWorker.h
    src/api/SteamApiSearchWorker.cpp
    src/api/SteamApiHydrationWorker.h
    src/api/SteamApiHydrationWorker.cpp
)

target_compile_definitions(Lymalink PRIVATE
    LYMALINK_VERSION="${LYMALINK_VERSION}"
)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_options(Lymalink PRIVATE "-Wl,--no-undefined")
endif()

set_target_properties(Lymalink PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

set_source_files_properties(Themes.qml PROPERTIES
    QT_QML_SINGLETON_TYPE TRUE
)

qt_add_qml_module(Lymalink
    URI Lymalink
    VERSION 1.0
    RESOURCE_PREFIX /qt/qml
    NO_IMPORT_SCAN
    RESOURCES
        res/docs/CREDITS.md
        res/docs/THIRD-PARTY-LICENSES-LINUX.md
        res/docs/LICENSE.md
        res/docs/help/user-guide-0.8.x-beta.md
        res/fonts/Inter/Inter-VariableFont_opsz,wght.ttf
        res/fonts/Inter/Inter-Italic-VariableFont_opsz,wght.ttf
        res/fonts/Inter/OFL.txt
        res/img-external/GitHub_Invertocat_White_Clearspace.png
        res/img-external/GitHub_Invertocat_Black_Clearspace.png
        res/img/BlankBackground_MFC_Glow_00002_E.png
        res/img/BlankBackground_MFC_00002_E.png
        res/img/BlankBackground_MFC_Glow_00005_ED.png
        res/img/BlankBackground_MFC_Glow_00005_2_ED.png
        res/img/BlankBackground_MFC_Glow_00004_ED.png
        res/img/BlankBackground_MFC_Glow_00006_ED.png
        res/img/BlankBackground_MFC_Glow_00006_.png
        res/img/BlankBackground_MFC_Glow_00032_ED.png
        res/img/BlankBackground_MFC_Glow_00033_ED.png
        res/img/BlankBackground_MFC_Glow_00034_ED.png
        res/img/BlankBackground_MFC_Glow_00035_ED.png
        res/img/BlankBackground_MFC_Glow_00036_ED.png
        res/img/BlankBackground_MFC_Glow_00037_ED.png
        res/img/BlankBackground_MFC_Glow_00038_ED.png
        res/img/BlankBackground_MFC_Glow_00039_ED.png
        res/img/BlankBackground_MFC_Glow_00040_ED.png
    QML_FILES
        Main.qml 
        Themes.qml
        common/CustomBusyIndicator.qml
        common/CustomButton.qml
        common/CustomCheckBox.qml
        common/CustomComboBox.qml
        common/CustomSwitch.qml
        common/CustomTextField.qml
        common/CustomTooltip.qml
        common/ConflictResolutionPopup.qml
        common/ConfirmationPopup.qml
        common/ErrorImage.qml
        common/ErrorPopup.qml
        common/MarkdownDocumentPopup.qml
        views/Main/Dashboard/CardGrid/CardGrid.qml
        views/Main/Dashboard/CardGrid/Components/Card.qml
        views/Main/Dashboard/CardGrid/Components/CardSmall.qml
        views/Main/Dashboard/CardList/CardList.qml
        views/Main/Dashboard/CardList/Components/CardRow.qml
        views/Main/Dashboard/CardList/Components/CardRowDetailed.qml
        views/Main/Dashboard/NewTarget/NewTarget.qml
        views/Main/Dashboard/NewTarget/Targets/Custom.qml
        views/Main/Dashboard/NewTarget/Targets/Emulator.qml
        views/Main/Dashboard/NewTarget/Targets/SteamImport.qml
        views/Main/Dashboard/TargetDetails/TargetDetails.qml
        views/Main/Dashboard/TargetDetails/TargetAchievementEditPopup.qml
        views/Main/Dashboard/TargetDetails/TargetSettings.qml
        views/Main/Dashboard/Dashboard.qml
        views/Main/Dashboard/DashboardToolbar.qml
        views/Main/Settings/Settings.qml
        views/Sidebar/Components/BackendServiceElement.qml
        views/Sidebar/Components/GHElement.qml
        views/Sidebar/Components/SidebarButton.qml
        views/Sidebar/Sidebar.qml
)

target_link_libraries(Lymalink PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::Widgets
    Qt6::Sql
    Qt6::Network
    OpenSSL::Crypto
)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(Lymalink PRIVATE Qt6::DBus)
elseif(WIN32)
    target_link_libraries(Lymalink PRIVATE Qt6::QuickControls2)
endif()

##############################################################
## Testing
##############################################################

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    find_package(Qt6 REQUIRED COMPONENTS Test Sql)
    enable_testing()
    qt_add_executable(FileManagerTests
        tests/FileManagerTests.cpp
        src/tools/FileManager.h
        src/tools/FileManager.cpp
    )
    target_link_libraries(FileManagerTests PRIVATE
        Qt6::Core
        Qt6::Test
    )
    add_test(NAME FileManagerTests COMMAND FileManagerTests)

    qt_add_executable(EncryptionTests
        tests/EncryptionTests.cpp
        src/tools/Encryption.h
        src/tools/Encryption.cpp
    )
    target_link_libraries(EncryptionTests PRIVATE
        Qt6::Core
        Qt6::Test
        OpenSSL::Crypto
    )
    add_test(NAME EncryptionTests COMMAND EncryptionTests)

    qt_add_executable(SQLiteManagerTests
        tests/SQLiteManagerTests.cpp
        src/database/SQLiteManager.h
        src/database/SQLiteManager.cpp
    )
    target_link_libraries(SQLiteManagerTests PRIVATE
        Qt6::Core
        Qt6::Sql
        Qt6::Test
    )
    add_test(NAME SQLiteManagerTests COMMAND SQLiteManagerTests)

#    qt_add_executable(SteamApiTests
#        tests/SteamApiTests.cpp
#        src/api/SteamApi.h
#        src/api/SteamApi.cpp
#    )
#    target_link_libraries(SteamApiTests PRIVATE
#        Qt6::Core
#        Qt6::Network
#        Qt6::Test
#    )
#    add_test(NAME SteamApiTests COMMAND SteamApiTests)
endif()
