/////////////////////////////////////////////////////////
// File: WinOverlaySharedMemoryState.h
// Date: 2026-06-21
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Defines fixed Windows shared-memory ABI
//              used by lymalinkd and injected overlays.
/////////////////////////////////////////////////////////

#pragma once

#if !defined(_WIN32)
    #error "WinOverlaySharedMemoryState is Windows-only."
#endif

#include "OverlaySharedMemoryState.h"

#include <windows.h>
#include <cstddef>
#include <cstdint>
#include <string>

constexpr uint32_t WIN_OVERLAY_SHM_VERSION = 1;
constexpr wchar_t WIN_OVERLAY_SHM_PREFIX[] = L"Local\\LymalinkOverlay.v1.";

#pragma pack(push, 8)
struct alignas(8) WinOverlaySharedMemoryState
{
    // ABI metadata used by injected overlay to validate mapping layout.
    uint32_t version;
    uint32_t structSize;

    // Interlocked flags coordinating daemon and overlay notification state.
    volatile LONG daemonActive;
    volatile LONG active;
    uint64_t timestamp;
    uint32_t durationMs;
    uint32_t notificationPosition;
    char title[256];
    char description[512];
    char iconPath[1024];
    char appIconPath[1024];

    // Optional fixed-size 64x64 RGBA icon payload.
    uint32_t hasIconPixels;
    uint32_t reserved;
    uint8_t iconPixels[OVERLAY_ICON_DATA_SIZE];
};
#pragma pack(pop)

// Guard ABI offsets and total size across both supported architectures.
static_assert(alignof(WinOverlaySharedMemoryState) == 8);
static_assert(offsetof(WinOverlaySharedMemoryState, timestamp) == 16);
static_assert(offsetof(WinOverlaySharedMemoryState, title) == 32);
static_assert(offsetof(WinOverlaySharedMemoryState, iconPixels) == 2856);
static_assert(sizeof(WinOverlaySharedMemoryState) == 19240);

inline std::wstring WinOverlaySharedMemoryName(uint32_t pid)
{
    // Per-process names prevent notification payloads crossing game processes.
    return std::wstring(WIN_OVERLAY_SHM_PREFIX) + std::to_wstring(pid);
}
