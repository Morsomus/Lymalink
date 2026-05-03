cmake_minimum_required(VERSION 3.16)

file(READ "${CMAKE_CURRENT_LIST_DIR}/../VERSION" LYMALINK_VERSION)
string(STRIP "${LYMALINK_VERSION}" LYMALINK_VERSION)

project(Lymalink VERSION ${LYMALINK_VERSION} LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Qml Quick Widgets)
qt_standard_project_setup()

qt_add_executable(Lymalink
    src/main.cpp
)

target_compile_definitions(Lymalink PRIVATE
    LYMALINK_VERSION="${LYMALINK_VERSION}"
)

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
    RESOURCES
        res/img-external/GitHub_Invertocat_White_Clearspace.png
        res/img/BlankBackground_MFC_Glow_00002_E.png
        res/img/BlankBackground_MFC_00002_E.png
        res/img/BlankBackground_MFC_Glow_00005_ED.png
        res/img/BlankBackground_MFC_Glow_00004_ED.png
    QML_FILES
        Main.qml 
        Themes.qml
        components/BackendServiceElement.qml
        components/GHElement.qml
        components/Sidebar.qml
        views/Dashboard.qml
        views/Settings.qml
)

target_link_libraries(Lymalink PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::Widgets
)
