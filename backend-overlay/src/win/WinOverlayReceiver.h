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

class WinOverlayReceiver
{
public:
    WinOverlayReceiver();
    ~WinOverlayReceiver();

    void Shutdown();

    void BeginFrame();
    ImTextureID PrepareIconTexture(class VulkanOverlayRenderer& renderer);
    void Draw(uint32_t framebufferWidth, uint32_t framebufferHeight, ImTextureID iconTexture);

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
