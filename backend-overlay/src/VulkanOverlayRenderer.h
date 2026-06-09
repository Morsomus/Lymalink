/////////////////////////////////////////////////////////
// File: VulkanOverlayRenderer.h
// Date: 2026-05-26
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Declares the Vulkan resources needed to
//              render ImGui into the swapchain from an
//              implicit layer. Owned and driven by
//              OverlayReceiver.
/////////////////////////////////////////////////////////

#pragma once

#include <vulkan/vulkan.h>
#include <vector>

// Forward declare so callers need not pull in imgui_impl_vulkan.h
struct ImDrawData;

struct VulkanOverlayRendererInitInfo
{
    VkInstance instance = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkQueue graphicsQueue = VK_NULL_HANDLE;
    PFN_vkGetInstanceProcAddr getInstanceProcAddr = nullptr;
    PFN_vkGetDeviceProcAddr getDeviceProcAddr = nullptr;
    uint32_t graphicsFamily = 0;
    VkFormat swapchainFormat;
    uint32_t imageCount = 0;
    uint32_t width = 0;
    uint32_t height = 0;
    // Swapchain images to render into, not owned here
    std::vector<VkImage> swapchainImages;
    std::vector<VkImageView> swapchainViews;
};

class VulkanOverlayRenderer
{
public:
    VulkanOverlayRenderer();
    ~VulkanOverlayRenderer();

    // Creates render pass, framebuffers, descriptor pool, command pool, semaphores and initialises ImGui Vulkan backend
    bool Initialize(const VulkanOverlayRendererInitInfo& info);
    void Shutdown();

    // Called from VulkanOverlayLayer before vkQueuePresentKHR
    // Records and submits a command buffer that renders ImGui draw data on top of the swapchain image at pPresentInfo->pImageIndices[0]
    VkSemaphore RenderDrawData(VkQueue presentQueue, const VkPresentInfoKHR* pPresentInfo, ImDrawData* drawData);
    inline bool IsReady() const { return m_ready; }
    inline VkCommandPool GetCommandPool() const { return m_commandPool; }
    inline VkDescriptorPool GetDescriptorPool() const { return m_descriptorPool; }

private:
    VkDevice m_device = VK_NULL_HANDLE;
    VkQueue m_graphicsQueue = VK_NULL_HANDLE;
    VkRenderPass m_renderPass = VK_NULL_HANDLE;
    VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;
    VkCommandPool m_commandPool = VK_NULL_HANDLE;
    bool m_imguiBackendReady = false;

    std::vector<VkFramebuffer> m_framebuffers;
    std::vector<VkCommandBuffer> m_commandBuffers;
    std::vector<VkFence> m_fences;
    std::vector<VkSemaphore> m_renderFinished;

    VulkanOverlayRendererInitInfo m_info{};
    bool m_ready = false;

    bool CreateRenderPass();
    bool CreateSwapchainFramebuffers();
    bool CreateCommandPool();
    bool CreateCommandBuffers();
    bool CreateFrameSyncObjects();
    bool CreateImGuiDescriptorPool();
    bool InitializeImGuiVulkanBackend();

    void DestroySwapchainFramebuffers();
};
