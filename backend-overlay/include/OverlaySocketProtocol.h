/////////////////////////////////////////////////////////
// File: OverlaySocketProtocol.h
// Date: 2026-05-27
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Unix socket notification packet used by
//              lymalinkd and flatpak-loaded overlay.
/////////////////////////////////////////////////////////

#pragma once

#include "OverlaySharedMemoryState.h"

#include <cstdint>

constexpr const char* OVERLAY_SOCKET_FILENAME = "lymalink-overlay.sock";
constexpr uint32_t OVERLAY_SOCKET_VERSION = 1;

// 64x64 RGBA, matches the scale used by EnsureOpenGLIconTexture / EnsureVulkanIconTexture
constexpr uint32_t OVERLAY_ICON_SIZE = 64;
constexpr uint32_t OVERLAY_ICON_STRIDE = OVERLAY_ICON_SIZE * 4; // bytes per row
constexpr uint32_t OVERLAY_ICON_DATA_SIZE = OVERLAY_ICON_SIZE * OVERLAY_ICON_STRIDE; // 16 384 bytes

struct OverlaySocketPacket
{
    uint32_t version = OVERLAY_SOCKET_VERSION;
    uint32_t durationMs = 0;
    uint64_t timestamp = 0;
    char title[256] {};
    char description[512] {};
    char iconPath[1024] {};   // kept for non-Flatpak fallback / logging
    char appIconPath[1024] {};
    uint32_t notificationPosition = static_cast<uint32_t>(OverlayNotificationPosition::BottomRight);

    // Embedded icon pixels (RGBA, 64×64, pre-scaled by the daemon)
    // hasIconPixels == 1 -> use iconPixels, ignore iconPath on receiver side
    // hasIconPixels == 0 -> iconPixels is zeroed, receiver falls back to iconPath
    uint32_t hasIconPixels = 0;
    uint8_t iconPixels[OVERLAY_ICON_DATA_SIZE] {};
};
