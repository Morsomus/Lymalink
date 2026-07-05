/////////////////////////////////////////////////////////
// File: WinOverlayReceiver.h
// Date: 2026-06-27
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Declares Windows overlay notification
//              receiver and ImGui UI coordinator.
/////////////////////////////////////////////////////////

#pragma once

#include "WinOverlaySharedMemoryState.h"
#include "WinOverlayUi.h"

#include "imgui.h"

#include <cstdint>
#include <vector>

class WinOverlayReceiver
{
public:
    WinOverlayReceiver();
    ~WinOverlayReceiver();

    void Shutdown();

    bool BeginFrame();
    void Draw(uint32_t framebufferWidth, uint32_t framebufferHeight, ImTextureID iconTexture);

    inline const std::vector<uint8_t>& IconPixels() const { return m_ui.IconPixels(); }
    inline uint64_t IconGeneration() const { return m_ui.IconGeneration(); }

private:
    HANDLE m_mapping;
    WinOverlaySharedMemoryState* m_state;
    WinOverlayUi m_ui;
    bool m_loggedMappingMissing;
    bool m_loggedDaemonInactive;

    bool Open();
    bool TryClaim(WinOverlayNotification& notification);
    void Close();
};
