/////////////////////////////////////////////////////////
// File: VulkanOverlayRenderer.cpp
// Date: 2026-05-26
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Implements Vulkan resources and ImGui
//              backend for the overlay layer.
/////////////////////////////////////////////////////////

#include "VulkanOverlayRenderer.h"
#include "Logger.h"
#include "FontEmbedded.h"

#include "imgui.h"
#include "imgui_impl_vulkan.h"
#include <array>
#include <cstring>

/////////////////////////////////////////////////////////////////////

// Custom function loader for ImGui to dynamically fetch Vulkan API function pointers
static PFN_vkVoidFunction ImGuiVulkanLoader(const char* functionName, void* userData)
{
    auto* info = static_cast<VulkanOverlayRendererInitInfo*>(userData);

    // Try loading via device function pointer first if device is valid
    if (info->getDeviceProcAddr && info->device != VK_NULL_HANDLE)
    {
        if (PFN_vkVoidFunction fn = info->getDeviceProcAddr(info->device, functionName))
        {
            return fn;
        }
    }

    // Fallback to instance function pointer if device lookup failed
    if (info->getInstanceProcAddr && info->instance != VK_NULL_HANDLE)
    {
        if (PFN_vkVoidFunction fn = info->getInstanceProcAddr(info->instance, functionName))
        {
            return fn;
        }
    }

    // Final fallback for global Vulkan functions
    if (info->getInstanceProcAddr)
    {
        return info->getInstanceProcAddr(VK_NULL_HANDLE, functionName);
    }

    return nullptr;
}

/////////////////////////////////////////////////////////////////////

// Helper macro to validate Vulkan API results
#define VK_CHECK(call, msg) \
    do { \
        VkResult _r = (call); \
        if (_r != VK_SUCCESS) { \
            Logger::Log(std::string("[VulkanOverlayRenderer] ") + (msg) + \
                        " VkResult=" + std::to_string(_r)); \
            return false; \
        } \
    } while (0)

/////////////////////////////////////////////////////////////////////

VulkanOverlayRenderer::VulkanOverlayRenderer()
{
    // Constructor
}

VulkanOverlayRenderer::~VulkanOverlayRenderer()
{
    Shutdown();
}

/////////////////////////////////////////////////////////////////////
////////////////////////////// PUBLIC ///////////////////////////////
/////////////////////////////////////////////////////////////////////

// Main initialization method that orchestrates the creation of all required Vulkan objects
bool VulkanOverlayRenderer::Initialize(const VulkanOverlayRendererInitInfo& info)
{
    m_info = info;
    m_device = info.device;
    m_graphicsQueue = info.graphicsQueue;

    // Sanity check to prevent initialization with invalid core Vulkan handles
    if (m_device == VK_NULL_HANDLE || m_graphicsQueue == VK_NULL_HANDLE)
    {
        Logger::Log("[VulkanOverlayRenderer][Initialize] invalid device or graphics queue.");
        return false;
    }
    // Verify that host application swapchain data pointers are valid
    if (m_info.imageCount == 0 || m_info.swapchainImages.empty() || m_info.swapchainViews.empty())
    {
        Logger::Log("[VulkanOverlayRenderer][Initialize] invalid swapchain image data.");
        return false;
    }

    // Setup of the rendering pipeline
    if (!CreateImGuiDescriptorPool()) { Shutdown(); return false; }
    if (!CreateRenderPass()) { Shutdown(); return false; }
    if (!CreateSwapchainFramebuffers()) { Shutdown(); return false; }
    if (!CreateCommandPool()) { Shutdown(); return false; }
    if (!CreateCommandBuffers()) { Shutdown(); return false; }
    if (!CreateFrameSyncObjects()) { Shutdown(); return false; }
    if (!InitializeImGuiVulkanBackend()) { Shutdown(); return false; }

    m_ready = true;
    return true;
}

/////////////////////////////////////////////////////////////////////

void VulkanOverlayRenderer::Shutdown()
{
    if (m_device == VK_NULL_HANDLE)
    {
        return;
    }
    VkResult waitResult = vkDeviceWaitIdle(m_device);
    if (waitResult != VK_SUCCESS)
    {
        Logger::Log("[VulkanOverlayRenderer][Shutdown] vkDeviceWaitIdle failed result=" + std::to_string(waitResult));
    }

    // Unregister and clean up the ImGui Vulkan context
    if (m_imguiBackendReady)
    {
        ImGui_ImplVulkan_Shutdown();
        m_imguiBackendReady = false;
    }

    for (auto s : m_renderFinished)
    {
        if (s)
        {
            vkDestroySemaphore(m_device, s, nullptr);
        }   
    }
    m_renderFinished.clear();

    for (auto f : m_fences)
    {
        if (f)
        {
            vkDestroyFence(m_device, f, nullptr);
        }
    }
        
    m_fences.clear();

    // Free all command buffers and destroy the command pool
    if (m_commandPool)
    {
        if (!m_commandBuffers.empty())
        {
            vkFreeCommandBuffers(m_device, m_commandPool, static_cast<uint32_t>(m_commandBuffers.size()), m_commandBuffers.data());
        }
        m_commandBuffers.clear();
        vkDestroyCommandPool(m_device, m_commandPool, nullptr);
        m_commandPool = VK_NULL_HANDLE;
    }

    DestroySwapchainFramebuffers();

    // Clean up the object
    if (m_renderPass)
    {
        vkDestroyRenderPass(m_device, m_renderPass, nullptr);
        m_renderPass = VK_NULL_HANDLE;
    }

    // Clean up memory pool
    if (m_descriptorPool)
    {
        vkDestroyDescriptorPool(m_device, m_descriptorPool, nullptr);
        m_descriptorPool = VK_NULL_HANDLE;
    }

    m_ready = false;
    m_device = VK_NULL_HANDLE;
}

/////////////////////////////////////////////////////////////////////

// Primary render loop method hooked into the host application's present framework
void VulkanOverlayRenderer::RenderDrawData(VkQueue presentQueue, const VkPresentInfoKHR* pPresentInfo, ImDrawData* drawData)
{
    if (!m_ready || !drawData)
    {
        return;
    }

    // Use the first swapchain image index from the present info
    if (!pPresentInfo || pPresentInfo->swapchainCount == 0 || !pPresentInfo->pImageIndices)
    {
        Logger::Log("[VulkanOverlayRenderer][RenderDrawData] invalid present info.");
        return;
    }

    // Retrieve active swapchain backbuffer index being prepared for screen presentation
    const uint32_t imgIdx = pPresentInfo->pImageIndices[0];
    if (imgIdx >= static_cast<uint32_t>(m_commandBuffers.size()) ||
        imgIdx >= static_cast<uint32_t>(m_fences.size()) ||
        imgIdx >= static_cast<uint32_t>(m_renderFinished.size()) ||
        imgIdx >= static_cast<uint32_t>(m_framebuffers.size()) ||
        imgIdx >= static_cast<uint32_t>(m_info.swapchainImages.size()))
    {
        Logger::Log("[VulkanOverlayRenderer][RenderDrawData] image index out of range index=" + std::to_string(imgIdx));
        return;
    }
    if (presentQueue == VK_NULL_HANDLE)
    {
        Logger::Log("[VulkanOverlayRenderer][RenderDrawData] present queue is null.");
        return;
    }
    if (pPresentInfo->waitSemaphoreCount > 0 && !pPresentInfo->pWaitSemaphores)
    {
        Logger::Log("[VulkanOverlayRenderer][RenderDrawData] waitSemaphoreCount is non-zero but pWaitSemaphores is null.");
        return;
    }

    VkCommandBuffer cmd = m_commandBuffers[imgIdx];
    VkFence fence = m_fences[imgIdx];

    // Wait for any previous render on this image slot to finish
    VkResult result = vkWaitForFences(m_device, 1, &fence, VK_TRUE, UINT64_MAX);
    if (result != VK_SUCCESS)
    {
        Logger::Log("[VulkanOverlayRenderer][RenderDrawData] vkWaitForFences failed result=" + std::to_string(result));
        return;
    }
    result = vkResetFences(m_device, 1, &fence);
    if (result != VK_SUCCESS)
    {
        Logger::Log("[VulkanOverlayRenderer][RenderDrawData] vkResetFences failed result=" + std::to_string(result));
        return;
    }
    result = vkResetCommandBuffer(cmd, 0);
    if (result != VK_SUCCESS)
    {
        Logger::Log("[VulkanOverlayRenderer][RenderDrawData] vkResetCommandBuffer failed result=" + std::to_string(result));
        return;
    }

    // Record
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    result = vkBeginCommandBuffer(cmd, &beginInfo);
    if (result != VK_SUCCESS)
    {
        Logger::Log("[VulkanOverlayRenderer][RenderDrawData] vkBeginCommandBuffer failed result=" + std::to_string(result));
        return;
    }

    // Transition image layout: PRESENT_SRC -> COLOR_ATTACHMENT_OPTIMAL
    VkImageMemoryBarrier barrierIn{};
    barrierIn.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrierIn.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    barrierIn.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    barrierIn.oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    barrierIn.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    barrierIn.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrierIn.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrierIn.image = m_info.swapchainImages[imgIdx];
    barrierIn.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

    vkCmdPipelineBarrier(cmd,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        0, 0, nullptr, 0, nullptr, 1, &barrierIn);

    // Begin render pass - LOAD_OP_LOAD preserves game's frame underneath
    VkRenderPassBeginInfo rpInfo{};
    rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rpInfo.renderPass = m_renderPass;
    rpInfo.framebuffer = m_framebuffers[imgIdx];
    rpInfo.renderArea.offset = { 0, 0 };
    rpInfo.renderArea.extent = { m_info.width, m_info.height };
    rpInfo.clearValueCount = 0; // No clear, overlay is additive
    vkCmdBeginRenderPass(cmd, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);

    ImGui_ImplVulkan_RenderDrawData(drawData, cmd);

    vkCmdEndRenderPass(cmd);

    // Transition back: COLOR_ATTACHMENT_OPTIMAL -> PRESENT_SRC
    VkImageMemoryBarrier barrierOut{};
    barrierOut.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrierOut.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    barrierOut.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    barrierOut.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    barrierOut.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    barrierOut.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrierOut.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrierOut.image = m_info.swapchainImages[imgIdx];
    barrierOut.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

    vkCmdPipelineBarrier(cmd,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
        0, 0, nullptr, 0, nullptr, 1, &barrierOut);

    result = vkEndCommandBuffer(cmd);
    if (result != VK_SUCCESS)
    {
        Logger::Log("[VulkanOverlayRenderer][RenderDrawData] vkEndCommandBuffer failed result=" + std::to_string(result));
        return;
    }

    // Submit - wait on the game's own semaphores, signal ours
    std::vector<VkPipelineStageFlags> waitStages(pPresentInfo->waitSemaphoreCount, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount = pPresentInfo->waitSemaphoreCount;
    submitInfo.pWaitSemaphores = pPresentInfo->pWaitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages.empty() ? nullptr : waitStages.data();
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmd;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &m_renderFinished[imgIdx];

    // Dispatch the overlay render work to the hardware queue
    result = vkQueueSubmit(presentQueue, 1, &submitInfo, fence);
    if (result != VK_SUCCESS)
    {
        Logger::Log("[VulkanOverlayRenderer][RenderDrawData] vkQueueSubmit failed result=" + std::to_string(result));
        return;
    }

    // Patch the present info so the compositor waits on our semaphore, not the game's - ensures overlay is visible before the flip
    // We cast away const here deliberately, the present call happens immediately after RenderDrawData returns in the hook
    auto* mutablePresent = const_cast<VkPresentInfoKHR*>(pPresentInfo);
    mutablePresent->waitSemaphoreCount = 1;
    mutablePresent->pWaitSemaphores = &m_renderFinished[imgIdx];
}

/////////////////////////////////////////////////////////////////////
///////////////////////////// PRIVATE ///////////////////////////////
/////////////////////////////////////////////////////////////////////

bool VulkanOverlayRenderer::CreateRenderPass()
{
    // Single colour attachment, LOAD to keep game frame, STORE to present
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = m_info.swapchainFormat;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorRef{};
    colorRef.attachment = 0;
    colorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    // Link subpass to graphics binding point
    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorRef;

    // Declare subpass dependencies to avoid color write race conditions during synchronization
    VkSubpassDependency dep{};
    dep.srcSubpass = VK_SUBPASS_EXTERNAL;
    dep.dstSubpass = 0;
    dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo rpInfo{};
    rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rpInfo.attachmentCount = 1;
    rpInfo.pAttachments = &colorAttachment;
    rpInfo.subpassCount = 1;
    rpInfo.pSubpasses = &subpass;
    rpInfo.dependencyCount = 1;
    rpInfo.pDependencies = &dep;

    VK_CHECK(vkCreateRenderPass(m_device, &rpInfo, nullptr, &m_renderPass), "vkCreateRenderPass failed");
    return true;
}

/////////////////////////////////////////////////////////////////////

// Allocates individual framebuffers mapped to the swapchain image views
bool VulkanOverlayRenderer::CreateSwapchainFramebuffers()
{
    if (m_info.swapchainViews.size() < m_info.imageCount)
    {
        Logger::Log("[VulkanOverlayRenderer][CreateSwapchainFramebuffers] not enough swapchain image views.");
        return false;
    }

    m_framebuffers.resize(m_info.imageCount);
    for (uint32_t i = 0; i < m_info.imageCount; ++i)
    {
        VkFramebufferCreateInfo fbInfo{};
        fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbInfo.renderPass = m_renderPass;
        fbInfo.attachmentCount = 1;
        fbInfo.pAttachments = &m_info.swapchainViews[i];
        fbInfo.width = m_info.width;
        fbInfo.height = m_info.height;
        fbInfo.layers = 1;
        VK_CHECK(vkCreateFramebuffer(m_device, &fbInfo, nullptr, &m_framebuffers[i]), "vkCreateFramebuffer failed");
    }
    return true;
}

/////////////////////////////////////////////////////////////////////

// Clear individual framebuffers when reinitializing or shutting down
void VulkanOverlayRenderer::DestroySwapchainFramebuffers()
{
    for (auto fb : m_framebuffers)
    {
        if (fb)
        {
            vkDestroyFramebuffer(m_device, fb, nullptr);
        }
    }   
    m_framebuffers.clear();
}

/////////////////////////////////////////////////////////////////////

// Allocates the command pool matching the target graphics queue family index
bool VulkanOverlayRenderer::CreateCommandPool()
{
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = m_info.graphicsFamily;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    VK_CHECK(vkCreateCommandPool(m_device, &poolInfo, nullptr, &m_commandPool), "vkCreateCommandPool failed");
    return true;
}

/////////////////////////////////////////////////////////////////////

// Allocates primary command buffers corresponding to the total swapchain image count
bool VulkanOverlayRenderer::CreateCommandBuffers()
{
    m_commandBuffers.resize(m_info.imageCount);
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = m_commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = m_info.imageCount;
    VK_CHECK(vkAllocateCommandBuffers(m_device, &allocInfo, m_commandBuffers.data()), "vkAllocateCommandBuffers failed");
    return true;
}

/////////////////////////////////////////////////////////////////////

// Generates structural primitives needed for multi-buffered hardware synchronization
bool VulkanOverlayRenderer::CreateFrameSyncObjects()
{
    m_fences.resize(m_info.imageCount, VK_NULL_HANDLE);
    m_renderFinished.resize(m_info.imageCount, VK_NULL_HANDLE);

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT; // Start signalled, no wait on first frame

    VkSemaphoreCreateInfo semInfo{};
    semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    for (uint32_t i = 0; i < m_info.imageCount; ++i)
    {
        VK_CHECK(vkCreateFence(m_device, &fenceInfo, nullptr, &m_fences[i]), "vkCreateFence failed");
        VK_CHECK(vkCreateSemaphore(m_device, &semInfo, nullptr, &m_renderFinished[i]), "vkCreateSemaphore failed");
    }
    return true;
}

/////////////////////////////////////////////////////////////////////

// Configures a descriptor pool for storing ImGui interface font samplers and UI imagery
bool VulkanOverlayRenderer::CreateImGuiDescriptorPool()
{
    // ImGui needs a combined image sampler per font/texture
    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSize.descriptorCount = 8;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    poolInfo.maxSets = 8;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;

    VK_CHECK(vkCreateDescriptorPool(m_device, &poolInfo, nullptr, &m_descriptorPool), "vkCreateDescriptorPool failed");
    return true;
}

/////////////////////////////////////////////////////////////////////

// Attaches and configures backend parameters required by the external ImGui dependency
bool VulkanOverlayRenderer::InitializeImGuiVulkanBackend()
{
    // Make init order robust - backend is initialised before receiver touches ImGui
    if (!ImGui::GetCurrentContext())
    {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        OverlayFonts::EnsureEmbeddedFontLoaded();
    }

    if (m_imguiBackendReady)
    {
        ImGui_ImplVulkan_Shutdown();
        m_imguiBackendReady = false;
    }

    // Map structural Vulkan settings from host initialization data into the ImGui configuration structure
    ImGui_ImplVulkan_InitInfo vkInfo{};
    vkInfo.Instance = m_info.instance;
    vkInfo.PhysicalDevice = m_info.physicalDevice;
    vkInfo.Device = m_device;
    vkInfo.QueueFamily = m_info.graphicsFamily;
    vkInfo.Queue = m_graphicsQueue;
    vkInfo.DescriptorPool = m_descriptorPool;
    vkInfo.MinImageCount = 2;
    vkInfo.ImageCount = m_info.imageCount;
    vkInfo.PipelineInfoMain.RenderPass = m_renderPass;
    vkInfo.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

    // Load Vulkan extension function hooks using localized function resolver
    if (!ImGui_ImplVulkan_LoadFunctions(vkInfo.ApiVersion, ImGuiVulkanLoader, &m_info))
    {
        Logger::Log("[VulkanOverlayRenderer][InitializeImGuiVulkanBackend] ImGui_ImplVulkan_LoadFunctions failed.");
        return false;
    }

    // Execute standard library backend init call
    if (!ImGui_ImplVulkan_Init(&vkInfo))
    {
        Logger::Log("[VulkanOverlayRenderer][InitializeImGuiVulkanBackend] ImGui_ImplVulkan_Init failed.");
        return false;
    }
    m_imguiBackendReady = true;
    return true;
}

/////////////////////////////////////////////////////////////////////
