/////////////////////////////////////////////////////////
// File: WinOverlayNotifier.h
// Date: 2026-06-21
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Declares Windows shared-memory producer
//              for injected overlay processes.
/////////////////////////////////////////////////////////

#pragma once

#include "notification/AchievementNotificationService.h"
#include "WinOverlaySharedMemoryState.h"

#include <cstdint>
#include <filesystem>
#include <mutex>
#include <string>
#include <unordered_map>

class WinOverlayNotifier : public IDesktopNotificationService
{
public:
    WinOverlayNotifier();
    ~WinOverlayNotifier() override;

    bool Init();
    void Shutdown();

    // Creates or removes per-process shared memory used by injected overlay
    bool RegisterProcess(int targetId, uint32_t pid);
    void UnregisterProcess(int targetId);

    // Clears active notifications from all registered shared-memory mappings
    void ClearSharedMemoryNotification();
    bool ShowAchievementToast(const AchievementNotification& notification) override;

private:
    struct Mapping
    {
        uint32_t pid = 0;
        HANDLE handle = nullptr;
        WinOverlaySharedMemoryState* state = nullptr;
    };

    std::mutex m_mutex;
    std::unordered_map<int, Mapping> m_mappings;
    bool m_initialised;
    uint64_t m_activeTimeoutMs;

    // Loads achievement icon, or app icon fallback, as RGBA pixels
    static bool LoadIconPixels(const AchievementNotification& notification, uint8_t* destination);
    static bool LoadIconFile(const std::filesystem::path& path, uint8_t* destination);
    template <typename T>
    static void ReleaseCom(T*& value);
    static void CopyText(char* destination, size_t destinationSize, const std::string& source);
    static void CloseMapping(Mapping& mapping);
    static void SetVulkanLayersEnabled(bool enabled);
    static OverlayNotificationPosition ResolveNotificationPosition();
    static OverlayNotificationPosition ParseNotificationPosition(const std::string& value);
    static std::string ResolveConfigPath();
};
