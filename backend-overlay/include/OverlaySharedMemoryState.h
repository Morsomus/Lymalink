/////////////////////////////////////////////////////////
// File: OverlaySharedMemoryState.h
// Date: 2026-05-26
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Shared memory layout used by backend
//              service and overlay.
//              Currently not used in code,
//              left for future fallback
/////////////////////////////////////////////////////////

#pragma once

#include <atomic>
#include <cstdint>

constexpr const char* OVERLAY_SHM_NAME = "/lymalink_overlay";
constexpr uint32_t OVERLAY_SHM_VERSION = 3;

enum class OverlayNotificationPosition : uint32_t
{
    TopLeft = 0,
    TopCenter = 1,
    TopRight = 2,
    BottomRight = 3,
    BottomLeft = 4
};

struct OverlaySharedMemoryState
{
    uint32_t version = OVERLAY_SHM_VERSION;
    std::atomic<bool> daemonActive{false};
    std::atomic<bool> active{false};
    uint64_t timestamp = 0;
    uint32_t durationMs = 0;
    uint32_t notificationPosition = static_cast<uint32_t>(OverlayNotificationPosition::BottomRight);
    char title[256]{};
    char description[512]{};
    char iconPath[1024]{};
    char appIconPath[1024]{};
};
