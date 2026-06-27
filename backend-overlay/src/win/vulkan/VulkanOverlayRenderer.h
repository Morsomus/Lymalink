/////////////////////////////////////////////////////////
// File: VulkanOverlayRenderer.h
// Date: 2026-06-27
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Declares Windows Vulkan resources used to
//              render ImGui into a game swapchain.
/////////////////////////////////////////////////////////

#pragma once

#include "imgui.h"

#include <vulkan/vulkan.h>

#include <cstdint>
#include <vector>

struct VulkanOverlayRendererInitInfo
{
    VkInstance instance = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkQueue graphicsQueue = VK_NULL_HANDLE;
    PFN_vkGetInstanceProcAddr getInstanceProcAddr = nullptr;
    PFN_vkGetDeviceProcAddr getDeviceProcAddr = nullptr;
    PFN_vkGetPhysicalDeviceMemoryProperties getMemoryProperties = nullptr;
    uint32_t graphicsFamily = 0;
    VkFormat swapchainFormat = VK_FORMAT_UNDEFINED;
    uint32_t imageCount = 0;
    uint32_t width = 0;
    uint32_t height = 0;
    std::vector<VkImage> swapchainImages;
    std::vector<VkImageView> swapchainViews;
};

class VulkanOverlayRenderer
{
public:
    VulkanOverlayRenderer();
    ~VulkanOverlayRenderer();

    bool Initialize(const VulkanOverlayRendererInitInfo& info);
    void Shutdown();

    ImTextureID EnsureIconTexture(const std::vector<uint8_t>& rgbaPixels, uint64_t generation);
    VkSemaphore RenderDrawData(VkQueue presentQueue, const VkPresentInfoKHR* presentInfo, ImDrawData* drawData);

    inline bool IsReady() const { return m_ready; }
    inline VkCommandPool GetCommandPool() const { return m_commandPool; }

private:
    VkDevice m_device;
    VkQueue m_graphicsQueue;
    VkRenderPass m_renderPass;
    VkDescriptorPool m_descriptorPool;
    VkCommandPool m_commandPool;
    bool m_imguiBackendReady;

    VkImage m_iconImage;
    VkDeviceMemory m_iconMemory;
    VkImageView m_iconView;
    VkSampler m_iconSampler;
    VkDescriptorSet m_iconDescriptor;
    uint64_t m_iconGeneration;

    std::vector<VkFramebuffer> m_framebuffers;
    std::vector<VkCommandBuffer> m_commandBuffers;
    std::vector<VkFence> m_fences;
    std::vector<VkSemaphore> m_renderFinished;

    VulkanOverlayRendererInitInfo m_info;
    bool m_ready;

    bool CreateRenderPass();
    bool CreateSwapchainFramebuffers();
    bool CreateCommandPool();
    bool CreateCommandBuffers();
    bool CreateFrameSyncObjects();
    bool CreateImGuiDescriptorPool();
    bool InitializeImGuiVulkanBackend();
    bool LoadVulkanFunctions();

    bool CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& memory);
    bool CreateIconImage();
    bool UploadIconPixels(const std::vector<uint8_t>& rgbaPixels);
    bool RunOneTimeCommands(void (*record)(VkCommandBuffer, void*), void* userData);
    uint32_t FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const;

    void DestroyIconTexture();
    void DestroySwapchainFramebuffers();

    PFN_vkAllocateCommandBuffers m_vkAllocateCommandBuffers = nullptr;
    PFN_vkAllocateMemory m_vkAllocateMemory = nullptr;
    PFN_vkBeginCommandBuffer m_vkBeginCommandBuffer = nullptr;
    PFN_vkBindBufferMemory m_vkBindBufferMemory = nullptr;
    PFN_vkBindImageMemory m_vkBindImageMemory = nullptr;
    PFN_vkCmdBeginRenderPass m_vkCmdBeginRenderPass = nullptr;
    PFN_vkCmdCopyBufferToImage m_vkCmdCopyBufferToImage = nullptr;
    PFN_vkCmdEndRenderPass m_vkCmdEndRenderPass = nullptr;
    PFN_vkCmdPipelineBarrier m_vkCmdPipelineBarrier = nullptr;
    PFN_vkCreateBuffer m_vkCreateBuffer = nullptr;
    PFN_vkCreateCommandPool m_vkCreateCommandPool = nullptr;
    PFN_vkCreateDescriptorPool m_vkCreateDescriptorPool = nullptr;
    PFN_vkCreateFence m_vkCreateFence = nullptr;
    PFN_vkCreateFramebuffer m_vkCreateFramebuffer = nullptr;
    PFN_vkCreateImage m_vkCreateImage = nullptr;
    PFN_vkCreateImageView m_vkCreateImageView = nullptr;
    PFN_vkCreateRenderPass m_vkCreateRenderPass = nullptr;
    PFN_vkCreateSampler m_vkCreateSampler = nullptr;
    PFN_vkCreateSemaphore m_vkCreateSemaphore = nullptr;
    PFN_vkDestroyBuffer m_vkDestroyBuffer = nullptr;
    PFN_vkDestroyCommandPool m_vkDestroyCommandPool = nullptr;
    PFN_vkDestroyDescriptorPool m_vkDestroyDescriptorPool = nullptr;
    PFN_vkDestroyFence m_vkDestroyFence = nullptr;
    PFN_vkDestroyFramebuffer m_vkDestroyFramebuffer = nullptr;
    PFN_vkDestroyImage m_vkDestroyImage = nullptr;
    PFN_vkDestroyImageView m_vkDestroyImageView = nullptr;
    PFN_vkDestroyRenderPass m_vkDestroyRenderPass = nullptr;
    PFN_vkDestroySampler m_vkDestroySampler = nullptr;
    PFN_vkDestroySemaphore m_vkDestroySemaphore = nullptr;
    PFN_vkDeviceWaitIdle m_vkDeviceWaitIdle = nullptr;
    PFN_vkEndCommandBuffer m_vkEndCommandBuffer = nullptr;
    PFN_vkFreeCommandBuffers m_vkFreeCommandBuffers = nullptr;
    PFN_vkFreeMemory m_vkFreeMemory = nullptr;
    PFN_vkGetBufferMemoryRequirements m_vkGetBufferMemoryRequirements = nullptr;
    PFN_vkGetImageMemoryRequirements m_vkGetImageMemoryRequirements = nullptr;
    PFN_vkMapMemory m_vkMapMemory = nullptr;
    PFN_vkQueueSubmit m_vkQueueSubmit = nullptr;
    PFN_vkQueueWaitIdle m_vkQueueWaitIdle = nullptr;
    PFN_vkResetCommandBuffer m_vkResetCommandBuffer = nullptr;
    PFN_vkResetFences m_vkResetFences = nullptr;
    PFN_vkUnmapMemory m_vkUnmapMemory = nullptr;
    PFN_vkWaitForFences m_vkWaitForFences = nullptr;
};
