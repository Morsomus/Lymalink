/////////////////////////////////////////////////////////
// File: OverlayReceiver.h
// Date: 2026-05-26
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Declares in-process overlay notification
//              renderer for Vulkan and OpenGL targets.
//              Injected via LD_PRELOAD / Vulkan implicit
//              layer. Reads notification state from a
//              POSIX shared memory segment or socket 
//              written by lymalinkd 
/////////////////////////////////////////////////////////

#pragma once

#include "OverlaySocketProtocol.h"
#include "OverlaySharedMemoryState.h"

#include <vulkan/vulkan.h>
#include <cstdint>
#include <string>
#include <vector>

class OverlayReceiver
{
public:
    OverlayReceiver();
    ~OverlayReceiver();

    // Called once when the injected library initialises inside the game process
    // Opens the shared memory segment OR socket created by lymalinkd
    // Returns false if shared memory is not yet available, overlay will retry on the next frame so startup order does not matter
    bool InitConnection();

    void Shutdown();

    // Called every frame from the Vulkan / OpenGL hook before the swap
    // Reads shared state or socket, drives fade animation and emits ImGui notification UI
    void RenderNotificationFrame(uint32_t framebufferWidth, uint32_t framebufferHeight);

    // Called once after vkCreateDevice is known
    // Stores Vulkan handles required for icon texture upload
    void EnsureVulkanImGuiContext(
        VkDevice             device,
        VkPhysicalDevice     physicalDevice,
        VkQueue              graphicsQueue,
        uint32_t             graphicsQueueFamily,
        VkCommandPool        commandPool,
        PFN_vkGetPhysicalDeviceMemoryProperties getMemProps);

    // OpenGL entry points, called from the dlsym hook
    void EnsureOpenGLImGuiContext();  // Called once on first swap

private:
    struct ActiveNotification
    {
        bool visible = false;
        uint64_t shownAtMs = 0;
        uint32_t durationMs = 5000;
        std::string title;
        std::string description;
        std::string iconPath;
        std::string appIconPath;
        OverlayNotificationPosition position = OverlayNotificationPosition::BottomRight;
        // Embedded icon pixels from socket packet, non-empty when hasIconPixels == 1.
        std::vector<uint8_t> iconPixels; // RGBA
    };

    // Animation
    float m_alpha = 0.0f;       // 0.0 - 1.0
    float m_slideOffset = 0.0f; // pixels, animates to 0
    bool m_fadingOut = false;

    uint32_t m_fbWidth = 0;
    uint32_t m_fbHeight = 0;

    // Internal shared memory state
    int m_shmFd = -1;
    OverlaySharedMemoryState* m_shm = nullptr;

    bool m_vulkanReady = false;
    bool m_openGLReady = false;
    bool m_imguiReady = false;
    bool m_ownsImguiContext = false;

    // Socket IPC
    bool m_useSocket = false;
    bool m_socketConnecting = false;
    bool m_hasSocketPending = false;
    bool m_loggedSocketFailed = false;
    int m_socketFd = -1;
    uint64_t m_nextSocketConnectAttemptMs = 0;
    std::string m_socketPath;
    ActiveNotification m_currentActiveNotification;
    ActiveNotification m_socketPending;
    std::string m_loadedIconPath;
    uint32_t m_iconTextureId = 0;

    // Vulkan handles, not owned, provided by VulkanOverlayLayer
    VkDevice m_vkDevice = VK_NULL_HANDLE;
    VkPhysicalDevice m_vkPhysicalDevice = VK_NULL_HANDLE;
    VkQueue m_vkQueue = VK_NULL_HANDLE;
    VkCommandPool m_vkCommandPool = VK_NULL_HANDLE;
    PFN_vkGetPhysicalDeviceMemoryProperties m_vkGetPhysicalDeviceMemoryProperties = nullptr;

    // Vulkan icon texture, owned here
    VkImage m_vkIconImage = VK_NULL_HANDLE;
    VkDeviceMemory m_vkIconMemory = VK_NULL_HANDLE;
    VkImageView m_vkIconImageView = VK_NULL_HANDLE;
    VkSampler m_vkIconSampler = VK_NULL_HANDLE;
    VkDescriptorSet m_vkIconDescSet = VK_NULL_HANDLE;

    // Shared memory
    bool SharedMemoryOpen();
    void SharedMemoryClose();
    bool SharedMemoryClaimPendingNotification();   // Copies from shm into m_currentActiveNotification if active

    // Socket IPC
    bool SocketConnect();
    void SocketClose();
    bool SocketIsConnected();
    void SocketDrain();
    bool SocketClaimPendingNotification();
    std::string SocketDetectFlatpakPath() const;
    bool SocketIsBlockedFlatpakLauncherProcess(const std::string& appId) const;
    ActiveNotification SocketPacketToNotification(const OverlaySocketPacket& packet) const;

    // Rendering
    void DrawNotificationWindow();
    void UpdateNotificationAnimation(float deltaSeconds);
    bool EnsureVulkanIconTexture(const std::string& iconPath);
    void DestroyVulkanIconTexture();
    uint32_t VulkanFindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags props) const;
    bool EnsureOpenGLIconTexture(const std::string& iconPath);
    void DestroyOpenGLIconTexture();
};
