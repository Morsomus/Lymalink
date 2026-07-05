/////////////////////////////////////////////////////////
// File: WinOverlayReceiver.cpp
// Date: 2026-06-27
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Implements Windows overlay notification
//              receiver and ImGui UI coordinator.
/////////////////////////////////////////////////////////

#include "WinOverlayReceiver.h"

#include "WinLogger.h"
#include <windows.h>

#include <string>

/////////////////////////////////////////////////////////////////////

WinOverlayReceiver::WinOverlayReceiver()
{
    m_mapping = nullptr;
    m_state = nullptr;
    m_loggedMappingMissing = false;
    m_loggedDaemonInactive = false;
}

WinOverlayReceiver::~WinOverlayReceiver()
{
    Shutdown();
}

/////////////////////////////////////////////////////////////////////
////////////////////////////// PUBLIC ///////////////////////////////
/////////////////////////////////////////////////////////////////////

void WinOverlayReceiver::Shutdown()
{
    Close();
}

/////////////////////////////////////////////////////////////////////

bool WinOverlayReceiver::BeginFrame()
{
    // Only claim a new notification once the current UI animation has fully completed
    WinOverlayNotification notification;
    const bool hasNotification = m_ui.IsIdle() && TryClaim(notification);
    m_ui.Update(hasNotification ? &notification : nullptr);
    return hasNotification;
}

/////////////////////////////////////////////////////////////////////

void WinOverlayReceiver::Draw(uint32_t framebufferWidth, uint32_t framebufferHeight, ImTextureID iconTexture)
{
    m_ui.Draw(framebufferWidth, framebufferHeight, iconTexture);
}

/////////////////////////////////////////////////////////////////////
///////////////////////////// PRIVATE ///////////////////////////////
/////////////////////////////////////////////////////////////////////

bool WinOverlayReceiver::Open()
{
    // Injector creates one mapping per target PID so each game process receives only its own notification
    const std::wstring name = WinOverlaySharedMemoryName(GetCurrentProcessId());
    m_mapping = OpenFileMappingW(FILE_MAP_READ | FILE_MAP_WRITE, FALSE, name.c_str());
    if (!m_mapping)
    {
        if (!m_loggedMappingMissing)
        {
            LYMALINK_LOG("[WinOverlayReceiver][Open] OpenFileMappingW failed pid=" + std::to_string(GetCurrentProcessId()) + " error=" + std::to_string(GetLastError()));
            m_loggedMappingMissing = true;
        }
        return false;
    }
    m_loggedMappingMissing = false;

    m_state = static_cast<WinOverlaySharedMemoryState*>(MapViewOfFile(m_mapping, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, sizeof(WinOverlaySharedMemoryState)));
    if (!m_state)
    {
        LYMALINK_LOG("[WinOverlayReceiver][Open] MapViewOfFile failed error=" + std::to_string(GetLastError()));
        CloseHandle(m_mapping);
        m_mapping = nullptr;
        return false;
    }

    // Reject stale or incompatible mappings before reading string/icon payload fields
    if (m_state->version != WIN_OVERLAY_SHM_VERSION || m_state->structSize != sizeof(WinOverlaySharedMemoryState))
    {
        LYMALINK_LOG("[WinOverlayReceiver][Open] shared memory ABI mismatch version=" + std::to_string(m_state->version) +
            " expected=" + std::to_string(WIN_OVERLAY_SHM_VERSION) +
            " size=" + std::to_string(m_state->structSize) +
            " expectedSize=" + std::to_string(sizeof(WinOverlaySharedMemoryState)));
        Close();
        return false;
    }
    LYMALINK_LOG("[WinOverlayReceiver][Open] connected pid=" + std::to_string(GetCurrentProcessId()));
    return true;
}

/////////////////////////////////////////////////////////////////////

bool WinOverlayReceiver::TryClaim(WinOverlayNotification& notification)
{
    // Mapping may appear after the DLL is already loaded, so retry lazily every frame
    if (!m_state && !Open())
    {
        return false;
    }
    // Daemon owns lifetime; inactive daemon means the mapping should be dropped and reopened later
    if (InterlockedCompareExchange(&m_state->daemonActive, 0, 0) == 0)
    {
        if (!m_loggedDaemonInactive)
        {
            LYMALINK_LOG("[WinOverlayReceiver][TryClaim] daemon inactive.");
            m_loggedDaemonInactive = true;
        }
        Close();
        return false;
    }
    m_loggedDaemonInactive = false;
    // Atomically claim one pending notification - Expected active=1, write active=0 on success
    if (InterlockedCompareExchange(&m_state->active, 0, 1) != 1)
    {
        return false;
    }

    // Ensure payload writes from the notifier are visible after claiming the active flag
    MemoryBarrier();
    notification = {};
    notification.shownAtMs = GetTickCount64();
    notification.durationMs = m_state->durationMs == 0 ? 6000 : m_state->durationMs;
    notification.position = static_cast<OverlayNotificationPosition>(m_state->notificationPosition);
    notification.title = m_state->title;
    notification.description = m_state->description;
    if (m_state->hasIconPixels == 1)
    {
        notification.iconPixels.assign(m_state->iconPixels, m_state->iconPixels + OVERLAY_ICON_DATA_SIZE);
    }
    LYMALINK_LOG("[WinOverlayReceiver][TryClaim] claimed: " + notification.title);
    return true;
}

/////////////////////////////////////////////////////////////////////

void WinOverlayReceiver::Close()
{
    // Release view before mapping handle, matching Windows object lifetime rules
    if (m_state)
    {
        if (!UnmapViewOfFile(m_state))
        {
            LYMALINK_LOG("[WinOverlayReceiver][Close] UnmapViewOfFile failed error=" + std::to_string(GetLastError()));
        }
        m_state = nullptr;
    }
    if (m_mapping)
    {
        if (!CloseHandle(m_mapping))
        {
            LYMALINK_LOG("[WinOverlayReceiver][Close] CloseHandle failed error=" + std::to_string(GetLastError()));
        }
        m_mapping = nullptr;
    }
}
