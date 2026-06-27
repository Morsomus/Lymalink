/////////////////////////////////////////////////////////
// File: WinOverlayTypes.h
// Date: 2026-06-27
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Declares Windows overlay notification
//              value types shared by receiver and UI.
/////////////////////////////////////////////////////////

#pragma once

#include "WinOverlaySharedMemoryState.h"

#include <cstdint>
#include <string>
#include <vector>

struct WinOverlayNotification
{
    uint64_t shownAtMs = 0;
    uint32_t durationMs = 6000;
    OverlayNotificationPosition position = OverlayNotificationPosition::BottomRight;
    std::string title;
    std::string description;
    std::vector<uint8_t> iconPixels;
};
