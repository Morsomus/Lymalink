/////////////////////////////////////////////////////////
// File: WinOverlayUi.h
// Date: 2026-06-21
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Declares ImGui state and rendering for
//              Windows overlay notifications.
/////////////////////////////////////////////////////////

#pragma once

#include "WinOverlayTypes.h"

#include "imgui.h"

#include <cstdint>
#include <vector>

class WinOverlayUi
{
public:
    WinOverlayUi();
    ~WinOverlayUi();

    inline bool IsIdle() const { return !m_visible && !m_fadingOut; }
    void Update(const WinOverlayNotification* notification);
    void Draw(uint32_t framebufferWidth, uint32_t framebufferHeight, ImTextureID iconTexture);

    // Renderer uses icon pixels and generation to refresh GPU texture
    inline const std::vector<uint8_t>& IconPixels() const { return m_notification.iconPixels; }
    inline uint64_t IconGeneration() const { return m_iconGeneration; }

private:
    WinOverlayNotification m_notification;
    bool m_visible;
    bool m_fadingOut;
    float m_alpha;
    float m_slide;
    uint64_t m_lastFrameMs;
    uint64_t m_iconGeneration;
};
