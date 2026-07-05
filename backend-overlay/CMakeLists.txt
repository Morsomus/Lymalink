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

set(MINHOOK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/minhook")
if(NOT EXISTS "${MINHOOK_DIR}/include/MinHook.h")
    message(FATAL_ERROR "MinHook is missing. For automatic download of the library - Run '.\\build.ps1 release' build process instead")
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

add_library(lymalink_minhook STATIC
    ${MINHOOK_DIR}/src/buffer.c
    ${MINHOOK_DIR}/src/hook.c
    ${MINHOOK_DIR}/src/trampoline.c
    ${MINHOOK_DIR}/src/hde/hde32.c
    ${MINHOOK_DIR}/src/hde/hde64.c
)
target_include_directories(lymalink_minhook PUBLIC ${MINHOOK_DIR}/include ${MINHOOK_DIR}/src/hde)

add_library(lymalink-overlay-vulkan SHARED
    src/win/vulkan/VulkanOverlayLayer.cpp
    src/win/vulkan/VulkanOverlayRenderer.cpp
    src/win/WinOverlayReceiver.cpp
    src/win/WinOverlayUi.cpp
    src/win/WinOverlayEntrypoint.cpp
    src/win/WinLogger.cpp
    ${IMGUI_DIR}/backends/imgui_impl_vulkan.cpp
)
target_include_directories(lymalink-overlay-vulkan PRIVATE include src/win/vulkan src/win ${IMGUI_DIR} ${IMGUI_DIR}/backends ${VULKAN_INCLUDE_DIR})
target_compile_definitions(lymalink-overlay-vulkan PRIVATE VK_NO_PROTOTYPES IMGUI_IMPL_VULKAN_NO_PROTOTYPES)
target_compile_definitions(lymalink-overlay-vulkan PRIVATE $<$<CONFIG:Release>:LYMALINK_OVERLAY_DISABLE_LOGGING>)
target_link_libraries(lymalink-overlay-vulkan PRIVATE lymalink_imgui)
set_target_properties(lymalink-overlay-vulkan PROPERTIES
    OUTPUT_NAME "lymalink-overlay-vulkan-${LYMALINK_ARCH}"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

add_library(lymalink-overlay-opengl SHARED
    src/win/opengl/OpenGLOverlayLayer.cpp
    src/win/WinOverlayReceiver.cpp
    src/win/WinOverlayUi.cpp
    src/win/WinOverlayEntrypoint.cpp
    src/win/WinLogger.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)
target_include_directories(lymalink-overlay-opengl PRIVATE include src/win/opengl src/win ${IMGUI_DIR} ${IMGUI_DIR}/backends ${MINHOOK_DIR}/include)
target_compile_definitions(lymalink-overlay-opengl PRIVATE LYMALINK_OVERLAY_ATTACH_HOOKS)
target_compile_definitions(lymalink-overlay-opengl PRIVATE $<$<CONFIG:Release>:LYMALINK_OVERLAY_DISABLE_LOGGING>)
target_link_libraries(lymalink-overlay-opengl PRIVATE lymalink_imgui lymalink_minhook opengl32 gdi32)
set_target_properties(lymalink-overlay-opengl PROPERTIES
    OUTPUT_NAME "lymalink-overlay-opengl-${LYMALINK_ARCH}"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

add_library(lymalink-overlay-dx9 SHARED
    src/win/dx9/Direct3D9OverlayLayer.cpp
    src/win/WinOverlayReceiver.cpp
    src/win/WinOverlayUi.cpp
    src/win/WinOverlayEntrypoint.cpp
    src/win/WinLogger.cpp
    ${IMGUI_DIR}/backends/imgui_impl_dx9.cpp
)
target_include_directories(lymalink-overlay-dx9 PRIVATE include src/win/dx9 src/win ${IMGUI_DIR} ${IMGUI_DIR}/backends ${MINHOOK_DIR}/include)
target_compile_definitions(lymalink-overlay-dx9 PRIVATE LYMALINK_OVERLAY_ATTACH_HOOKS)
target_compile_definitions(lymalink-overlay-dx9 PRIVATE $<$<CONFIG:Release>:LYMALINK_OVERLAY_DISABLE_LOGGING>)
target_link_libraries(lymalink-overlay-dx9 PRIVATE lymalink_imgui lymalink_minhook d3d9 user32 gdi32)
set_target_properties(lymalink-overlay-dx9 PROPERTIES
    OUTPUT_NAME "lymalink-overlay-dx9-${LYMALINK_ARCH}"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

add_library(lymalink-overlay-dx10 SHARED
    src/win/dx10/Direct3D10OverlayLayer.cpp
    src/win/WinOverlayReceiver.cpp
    src/win/WinOverlayUi.cpp
    src/win/WinOverlayEntrypoint.cpp
    src/win/WinLogger.cpp
    ${IMGUI_DIR}/backends/imgui_impl_dx10.cpp
)
target_include_directories(lymalink-overlay-dx10 PRIVATE include src/win/dx10 src/win ${IMGUI_DIR} ${IMGUI_DIR}/backends ${MINHOOK_DIR}/include)
target_compile_definitions(lymalink-overlay-dx10 PRIVATE LYMALINK_OVERLAY_ATTACH_HOOKS)
target_compile_definitions(lymalink-overlay-dx10 PRIVATE $<$<CONFIG:Release>:LYMALINK_OVERLAY_DISABLE_LOGGING>)
target_link_libraries(lymalink-overlay-dx10 PRIVATE lymalink_imgui lymalink_minhook d3d10 dxgi user32 gdi32)
set_target_properties(lymalink-overlay-dx10 PROPERTIES
    OUTPUT_NAME "lymalink-overlay-dx10-${LYMALINK_ARCH}"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

add_library(lymalink-overlay-dx11 SHARED
    src/win/dx11/Direct3D11OverlayLayer.cpp
    src/win/WinOverlayReceiver.cpp
    src/win/WinOverlayUi.cpp
    src/win/WinOverlayEntrypoint.cpp
    src/win/WinLogger.cpp
    ${IMGUI_DIR}/backends/imgui_impl_dx11.cpp
)
target_include_directories(lymalink-overlay-dx11 PRIVATE include src/win/dx11 src/win ${IMGUI_DIR} ${IMGUI_DIR}/backends ${MINHOOK_DIR}/include)
target_compile_definitions(lymalink-overlay-dx11 PRIVATE LYMALINK_OVERLAY_ATTACH_HOOKS)
target_compile_definitions(lymalink-overlay-dx11 PRIVATE $<$<CONFIG:Release>:LYMALINK_OVERLAY_DISABLE_LOGGING>)
target_link_libraries(lymalink-overlay-dx11 PRIVATE lymalink_imgui lymalink_minhook d3d11 dxgi user32 gdi32)
set_target_properties(lymalink-overlay-dx11 PROPERTIES
    OUTPUT_NAME "lymalink-overlay-dx11-${LYMALINK_ARCH}"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

add_executable(lymalink-overlay-injector
    src/win/opengl/OpenGLInjector.cpp
)
target_compile_definitions(lymalink-overlay-injector PRIVATE $<$<CONFIG:Release>:LYMALINK_OVERLAY_DISABLE_LOGGING>)
target_link_libraries(lymalink-overlay-injector PRIVATE advapi32)
set_target_properties(lymalink-overlay-injector PROPERTIES
    OUTPUT_NAME "lymalink-overlay-injector-${LYMALINK_ARCH}"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
