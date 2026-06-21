/////////////////////////////////////////////////////////
// File: WinOverlayStub.h
// Date: 2026-06-20
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Declares TEMPORARY no-op Windows overlay placeholder
/////////////////////////////////////////////////////////

#pragma once

#include "notification/AchievementNotificationService.h"

// Windows core milestone deliberately ships without injected overlay support.
// Stub preserves shared daemon orchestration without opening overlay IPC.
class WinOverlayStub : public IDesktopNotificationService
{
public:
    bool Init() { return true; }
    void Shutdown() {}
    void SetSocketPaused(bool) {}
    void ClearSharedMemoryNotification() {}
    bool ShowAchievementToast(const AchievementNotification&) override { return false; }
    bool ShowAchievementToastSharedMemory(const AchievementNotification&) { return false; }
    bool ShowAchievementToastSocket(const AchievementNotification&) { return false; }
    bool HasSocketClient() const { return false; }
};
