cmake_minimum_required(VERSION 3.21)
project(lymalink_overlay_win LANGUAGES C CXX)

if(NOT WIN32)
    message(FATAL_ERROR "This CMake entrypoint is for the Windows Overlay.")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS OFF)

set(IMGUI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/imgui")
if(NOT EXISTS "${IMGUI_DIR}/imgui.h")
    message(FATAL_ERROR "Dear ImGui is missing. For automatic download of the library - Run '.\\build.ps1 release' build process instead")
endif()

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(LYMALINK_ARCH x64)
else()
    set(LYMALINK_ARCH x86)
endif()

find_path(VULKAN_INCLUDE_DIR vulkan/vulkan.h HINTS "$ENV{VULKAN_SDK}/Include")
if(NOT VULKAN_INCLUDE_DIR)
    message(FATAL_ERROR "Vulkan headers not found. Install the Vulkan SDK and set VULKAN_SDK.")
endif()

set(IMGUI_SOURCES
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
)

add_library(lymalink_imgui STATIC ${IMGUI_SOURCES})
target_include_directories(lymalink_imgui PUBLIC ${IMGUI_DIR} ${IMGUI_DIR}/backends)

add_library(lymalink-overlay-vulkan SHARED
    src/win/vulkan/VulkanOverlayLayer.cpp
    src/win/vulkan/VulkanOverlayRenderer.cpp
    src/win/WinOverlayReceiver.cpp
    src/win/WinOverlayUi.cpp
    src/win/WinLogger.cpp
    ${IMGUI_DIR}/backends/imgui_impl_vulkan.cpp
)
target_include_directories(lymalink-overlay-vulkan PRIVATE include src/win/vulkan src/win ${IMGUI_DIR} ${IMGUI_DIR}/backends ${VULKAN_INCLUDE_DIR})
target_compile_definitions(lymalink-overlay-vulkan PRIVATE VK_NO_PROTOTYPES IMGUI_IMPL_VULKAN_NO_PROTOTYPES)
target_link_libraries(lymalink-overlay-vulkan PRIVATE lymalink_imgui)
set_target_properties(lymalink-overlay-vulkan PROPERTIES
    OUTPUT_NAME "lymalink-overlay-vulkan-${LYMALINK_ARCH}"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
