cmake_minimum_required(VERSION 3.16)

file(STRINGS "${CMAKE_CURRENT_LIST_DIR}/../VERSION" VERSION_DATA)
list(GET VERSION_DATA 0 LYMALINK_VERSION)
list(GET VERSION_DATA 1 LICENSE_VERSION)
string(STRIP "${LYMALINK_VERSION}" LYMALINK_VERSION)
string(STRIP "${LICENSE_VERSION}" LICENSE_VERSION)

project(Lymalink VERSION ${LYMALINK_VERSION} LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Qml Quick Widgets Sql Network)
find_package(OpenSSL REQUIRED)
qt_standard_project_setup()

qt_add_executable(Lymalink
    src/main.cpp
    src/Lymalink.h
    src/Lymalink.cpp
    src/Settings.h
    src/Settings.cpp
    src/SysTray.h
    src/SysTray.cpp
    src/database/SQLiteManager.h
    src/database/SQLiteManager.cpp
    src/tools/Encryption.h
    src/tools/Encryption.cpp
    src/tools/FileManager.h
    src/tools/FileManager.cpp
    src/tools/Utils.h
    src/tools/Utils.cpp
    src/api/SteamApi.h
    src/api/SteamApi.cpp
    src/api/SteamApiWorker.h
    src/api/SteamApiWorker.cpp
)

target_compile_definitions(Lymalink PRIVATE
    LYMALINK_VERSION="${LYMALINK_VERSION}"
    LICENSE_VERSION="${LICENSE_VERSION}"
)

set_target_properties(Lymalink PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

set_source_files_properties(Themes.qml PROPERTIES
    QT_QML_SINGLETON_TYPE TRUE
)

# TODO: Remove later
# Temporary non-commited files - Replace with your own dummy resources
set(EXTERNAL_ASSETS
    "${CMAKE_CURRENT_SOURCE_DIR}/../a_instr/temp/library_capsule_2x.jpg"
    "${CMAKE_CURRENT_SOURCE_DIR}/../a_instr/temp/library_600x900_2x.jpg"
    "${CMAKE_CURRENT_SOURCE_DIR}/../a_instr/temp/hero_capsule_2x.jpg"
    "${CMAKE_CURRENT_SOURCE_DIR}/../a_instr/temp/hero_capsule.jpg"
)
foreach(asset_path IN LISTS EXTERNAL_ASSETS)
    get_filename_component(asset_name ${asset_path} NAME)
    set_source_files_properties(${asset_path} PROPERTIES 
        QT_RESOURCE_ALIAS "res/img/${asset_name}"
    )
endforeach()

qt_add_qml_module(Lymalink
    URI Lymalink
    VERSION 1.0
    RESOURCE_PREFIX /qt/qml
    RESOURCES
        ${EXTERNAL_ASSETS}
        res/fonts/Inter/Inter-VariableFont_opsz,wght.ttf
        res/fonts/Inter/Inter-Italic-VariableFont_opsz,wght.ttf
        res/fonts/Inter/OFL.txt
        res/img-external/GitHub_Invertocat_White_Clearspace.png
        res/img/BlankBackground_MFC_Glow_00002_E.png
        res/img/BlankBackground_MFC_00002_E.png
        res/img/BlankBackground_MFC_Glow_00005_ED.png
        res/img/BlankBackground_MFC_Glow_00005_2_ED.png
        res/img/BlankBackground_MFC_Glow_00004_ED.png
        res/img/BlankBackground_MFC_Glow_00006_ED.png
        res/img/BlankBackground_MFC_Glow_00006_.png
        res/img/BlankBackground_MFC_Glow_00032_ED.png
        res/img/BlankBackground_MFC_Glow_00033_ED.png
    QML_FILES
        Main.qml 
        Themes.qml
        common/CustomBusyIndicator.qml
        common/CustomTooltip.qml
        common/ErrorImage.qml
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
