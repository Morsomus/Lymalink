/////////////////////////////////////////////////////////
// File: VulkanOverlayRenderer.cpp
// Date: 2026-06-27
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Implements Windows Vulkan resources used
//              by the implicit overlay layer.
/////////////////////////////////////////////////////////

#include "VulkanOverlayRenderer.h"

#include "FontEmbedded.h"
#include "WinLogger.h"
#include "WinOverlaySharedMemoryState.h"
#include "imgui_impl_vulkan.h"

#include <cstring>
#include <string>

/////////////////////////////////////////////////////////////////////

static ImTextureID DescriptorSetToTextureId(VkDescriptorSet descriptorSet)
{
    // ImGui stores texture handles as opaque pointers; cast through uintptr_t on 64-bit Windows
#if defined(_WIN64)
    return static_cast<ImTextureID>(reinterpret_cast<uintptr_t>(descriptorSet));
#else
    return static_cast<ImTextureID>(descriptorSet);
#endif
}

/////////////////////////////////////////////////////////////////////

static PFN_vkVoidFunction ImGuiVulkanLoader(const char* functionName, void* userData)
{
    auto* info = static_cast<VulkanOverlayRendererInitInfo*>(userData);
    const std::string name = functionName ? functionName : "";

    // Prefer device dispatch for device-level commands
    if (info->getDeviceProcAddr && info->device != VK_NULL_HANDLE)
    {
        if (PFN_vkVoidFunction fn = info->getDeviceProcAddr(info->device, functionName))
        {
            return fn;
        }
    }
    // Fall back to instance dispatch for instance/global commands
    if (info->getInstanceProcAddr && info->instance != VK_NULL_HANDLE)
    {
        if (PFN_vkVoidFunction fn = info->getInstanceProcAddr(info->instance, functionName))
        {
            return fn;
        }
    }

    const bool global =
        name == "vkCreateInstance" ||
        name == "vkEnumerateInstanceExtensionProperties" ||
        name == "vkEnumerateInstanceLayerProperties" ||
        name == "vkGetInstanceProcAddr";
    // Only true global functions may be queried with a null instance
    if (global && info->getInstanceProcAddr)
    {
        return info->getInstanceProcAddr(VK_NULL_HANDLE, functionName);
    }
    return nullptr;
}

/////////////////////////////////////////////////////////////////////

#define VK_CHECK(call, msg) \
    do { \
        VkResult _r = (call); \
        if (_r != VK_SUCCESS) { \
            LYMALINK_LOG(std::string("[VulkanOverlayRenderer] ") + (msg) + " VkResult=" + std::to_string(_r)); \
            return false; \
        } \
    } while (0)

#define LOAD_DEVICE_FN(name) \
    do { \
        m_##name = reinterpret_cast<PFN_##name>(m_info.getDeviceProcAddr(m_device, #name)); \
        if (!m_##name) { \
            LYMALINK_LOG("[VulkanOverlayRenderer][LoadVulkanFunctions] missing " #name "."); \
            return false; \
        } \
    } while (0)

/////////////////////////////////////////////////////////////////////

VulkanOverlayRenderer::VulkanOverlayRenderer()
{
    m_device = VK_NULL_HANDLE;
    m_graphicsQueue = VK_NULL_HANDLE;
    m_renderPass = VK_NULL_HANDLE;
    m_descriptorPool = VK_NULL_HANDLE;
    m_commandPool = VK_NULL_HANDLE;
    m_imguiBackendReady = false;

    m_iconImage = VK_NULL_HANDLE;
    m_iconMemory = VK_NULL_HANDLE;
    m_iconView = VK_NULL_HANDLE;
    m_iconSampler = VK_NULL_HANDLE;
    m_iconDescriptor = VK_NULL_HANDLE;
    m_iconGeneration = 0;

    m_framebuffers = {};
    m_commandBuffers = {};
    m_fences = {};
    m_renderFinished = {};

    m_info = {};
    m_ready = false;
}

VulkanOverlayRenderer::~VulkanOverlayRenderer()
{
    Shutdown();
}

/////////////////////////////////////////////////////////////////////
////////////////////////////// PUBLIC ///////////////////////////////
/////////////////////////////////////////////////////////////////////

bool VulkanOverlayRenderer::Initialize(const VulkanOverlayRendererInitInfo& info)
{
    m_info = info;
    m_device = info.device;
    m_graphicsQueue = info.graphicsQueue;

    if (m_device == VK_NULL_HANDLE || m_graphicsQueue == VK_NULL_HANDLE)
    {
        LYMALINK_LOG("[VulkanOverlayRenderer][Initialize] invalid device or queue.");
        return false;
    }
    if (m_info.imageCount == 0 || m_info.swapchainImages.empty() || m_info.swapchainViews.empty())
    {
        LYMALINK_LOG("[VulkanOverlayRenderer][Initialize] invalid swapchain images.");
        return false;
    }
    if (!m_info.getMemoryProperties)
    {
        LYMALINK_LOG("[VulkanOverlayRenderer][Initialize] memory properties resolver missing.");
        return false;
    }
    if (!LoadVulkanFunctions())
    {
        return false;
    }

    // Create Vulkan/ImGui resources in dependency order; any failure tears down partial state
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

    VkResult waitResult = m_vkDeviceWaitIdle(m_device);
    if (waitResult != VK_SUCCESS)
    {
        LYMALINK_LOG("[VulkanOverlayRenderer][Shutdown] vkDeviceWaitIdle failed result=" + std::to_string(waitResult));
    }

    DestroyIconTexture();

    if (m_imguiBackendReady)
    {
        // ImGui backend owns descriptor/font resources inside the descriptor pool
        ImGui_ImplVulkan_Shutdown();
        m_imguiBackendReady = false;
    }

    for (VkSemaphore semaphore : m_renderFinished)
    {
        if (semaphore)
        {
            m_vkDestroySemaphore(m_device, semaphore, nullptr);
        }
    }
    m_renderFinished.clear();

    for (VkFence fence : m_fences)
    {
        if (fence)
        {
            m_vkDestroyFence(m_device, fence, nullptr);
        }
    }
    m_fences.clear();

    if (m_commandPool)
    {
        // Command buffers must be freed before their command pool is destroyed
        if (!m_commandBuffers.empty())
        {
            m_vkFreeCommandBuffers(m_device, m_commandPool, static_cast<uint32_t>(m_commandBuffers.size()), m_commandBuffers.data());
        }
        m_commandBuffers.clear();
        m_vkDestroyCommandPool(m_device, m_commandPool, nullptr);
        m_commandPool = VK_NULL_HANDLE;
    }

    DestroySwapchainFramebuffers();

    if (m_renderPass)
    {
        m_vkDestroyRenderPass(m_device, m_renderPass, nullptr);
        m_renderPass = VK_NULL_HANDLE;
    }
    if (m_descriptorPool)
    {
        m_vkDestroyDescriptorPool(m_device, m_descriptorPool, nullptr);
        m_descriptorPool = VK_NULL_HANDLE;
    }

    m_ready = false;
    m_device = VK_NULL_HANDLE;
}

/////////////////////////////////////////////////////////////////////

ImTextureID VulkanOverlayRenderer::EnsureIconTexture(const std::vector<uint8_t>& rgbaPixels, uint64_t generation)
{
    if (!m_ready || rgbaPixels.size() != OVERLAY_ICON_DATA_SIZE)
    {
        return ImTextureID_Invalid;
    }
    if (m_iconDescriptor != VK_NULL_HANDLE && m_iconGeneration == generation)
    {
        // Existing descriptor still matches the UI icon generation
        return DescriptorSetToTextureId(m_iconDescriptor);
    }

    // Recreate image resources when receiver publishes a new icon payload
    DestroyIconTexture();
    if (!CreateIconImage() || !UploadIconPixels(rgbaPixels))
    {
        DestroyIconTexture();
        return ImTextureID_Invalid;
    }

    // Register the sampled image with ImGui and cache generation for future frames
    m_iconDescriptor = ImGui_ImplVulkan_AddTexture(m_iconSampler, m_iconView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    m_iconGeneration = generation;
    return DescriptorSetToTextureId(m_iconDescriptor);
}

/////////////////////////////////////////////////////////////////////

VkSemaphore VulkanOverlayRenderer::RenderDrawData(VkQueue presentQueue, const VkPresentInfoKHR* presentInfo, ImDrawData* drawData)
{
    if (!m_ready || !drawData)
    {
        return VK_NULL_HANDLE;
    }
    if (!presentInfo || presentInfo->swapchainCount == 0 || !presentInfo->pImageIndices)
    {
        LYMALINK_LOG("[VulkanOverlayRenderer][RenderDrawData] invalid present info.");
        return VK_NULL_HANDLE;
    }

    const uint32_t imageIndex = presentInfo->pImageIndices[0];
    if (imageIndex >= m_commandBuffers.size() || imageIndex >= m_fences.size() ||
        imageIndex >= m_renderFinished.size() || imageIndex >= m_framebuffers.size() ||
        imageIndex >= m_info.swapchainImages.size())
    {
        LYMALINK_LOG("[VulkanOverlayRenderer][RenderDrawData] image index out of range index=" + std::to_string(imageIndex));
        return VK_NULL_HANDLE;
    }
    if (presentQueue == VK_NULL_HANDLE)
    {
        LYMALINK_LOG("[VulkanOverlayRenderer][RenderDrawData] present queue null.");
        return VK_NULL_HANDLE;
    }
    if (presentInfo->waitSemaphoreCount > 0 && !presentInfo->pWaitSemaphores)
    {
        LYMALINK_LOG("[VulkanOverlayRenderer][RenderDrawData] wait semaphores missing.");
        return VK_NULL_HANDLE;
    }

    VkCommandBuffer commandBuffer = m_commandBuffers[imageIndex];
    VkFence fence = m_fences[imageIndex];

    // Wait for previous overlay work that used this swapchain image slot
    VkResult result = m_vkWaitForFences(m_device, 1, &fence, VK_TRUE, UINT64_MAX);
    if (result != VK_SUCCESS)
    {
        LYMALINK_LOG("[VulkanOverlayRenderer][RenderDrawData] vkWaitForFences failed result=" + std::to_string(result));
        return VK_NULL_HANDLE;
    }
    result = m_vkResetFences(m_device, 1, &fence);
    if (result != VK_SUCCESS)
    {
        LYMALINK_LOG("[VulkanOverlayRenderer][RenderDrawData] vkResetFences failed result=" + std::to_string(result));
        return VK_NULL_HANDLE;
    }
    result = m_vkResetCommandBuffer(commandBuffer, 0);
    if (result != VK_SUCCESS)
    {
        LYMALINK_LOG("[VulkanOverlayRenderer][RenderDrawData] vkResetCommandBuffer failed result=" + std::to_string(result));
        return VK_NULL_HANDLE;
    }

    // Record one-time commands that composite ImGui over the application's backbuffer
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    result = m_vkBeginCommandBuffer(commandBuffer, &beginInfo);
    if (result != VK_SUCCESS)
    {
        LYMALINK_LOG("[VulkanOverlayRenderer][RenderDrawData] vkBeginCommandBuffer failed result=" + std::to_string(result));
        return VK_NULL_HANDLE;
    }

    // Transition swapchain image from present layout into color-attachment layout
    VkImageMemoryBarrier barrierIn{};
    barrierIn.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrierIn.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    barrierIn.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    barrierIn.oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    barrierIn.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    barrierIn.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrierIn.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrierIn.image = m_info.swapchainImages[imageIndex];
    barrierIn.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    m_vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrierIn);

    // Load existing game frame and draw overlay without clearing the image
    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = m_renderPass;
    renderPassInfo.framebuffer = m_framebuffers[imageIndex];
    renderPassInfo.renderArea.extent = { m_info.width, m_info.height };
    m_vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    ImGui_ImplVulkan_RenderDrawData(drawData, commandBuffer);
    m_vkCmdEndRenderPass(commandBuffer);

    // Transition image back so the application's present call can display it
    VkImageMemoryBarrier barrierOut{};
    barrierOut.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrierOut.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    barrierOut.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    barrierOut.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    barrierOut.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    barrierOut.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrierOut.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrierOut.image = m_info.swapchainImages[imageIndex];
    barrierOut.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    m_vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrierOut);

    result = m_vkEndCommandBuffer(commandBuffer);
    if (result != VK_SUCCESS)
    {
        LYMALINK_LOG("[VulkanOverlayRenderer][RenderDrawData] vkEndCommandBuffer failed result=" + std::to_string(result));
        return VK_NULL_HANDLE;
    }

    std::vector<VkPipelineStageFlags> waitStages(presentInfo->waitSemaphoreCount, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount = presentInfo->waitSemaphoreCount;
    submitInfo.pWaitSemaphores = presentInfo->pWaitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages.empty() ? nullptr : waitStages.data();
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &m_renderFinished[imageIndex];

    // Wait on application semaphores, signal overlay semaphore for the forwarded present call
    result = m_vkQueueSubmit(presentQueue, 1, &submitInfo, fence);
    if (result != VK_SUCCESS)
    {
        LYMALINK_LOG("[VulkanOverlayRenderer][RenderDrawData] vkQueueSubmit failed result=" + std::to_string(result));
        return VK_NULL_HANDLE;
    }
    return m_renderFinished[imageIndex];
}

/////////////////////////////////////////////////////////////////////
///////////////////////////// PRIVATE ///////////////////////////////
/////////////////////////////////////////////////////////////////////

bool VulkanOverlayRenderer::CreateRenderPass()
{
    // Single color attachment, LOAD to preserve game frame and STORE for present
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

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorRef;

    // Dependency prevents color write hazards around the external swapchain image
    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &colorAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    VK_CHECK(m_vkCreateRenderPass(m_device, &renderPassInfo, nullptr, &m_renderPass), "vkCreateRenderPass failed");
    return true;
}

/////////////////////////////////////////////////////////////////////

bool VulkanOverlayRenderer::CreateSwapchainFramebuffers()
{
    if (m_info.swapchainViews.size() < m_info.imageCount)
    {
        LYMALINK_LOG("[VulkanOverlayRenderer][CreateSwapchainFramebuffers] not enough image views.");
        return false;
    }

    m_framebuffers.resize(m_info.imageCount, VK_NULL_HANDLE);
    for (uint32_t i = 0; i < m_info.imageCount; ++i)
    {
        // Pair each swapchain image view with one framebuffer used by ImGui render pass
        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = m_renderPass;
        framebufferInfo.attachmentCount = 1;
        framebufferInfo.pAttachments = &m_info.swapchainViews[i];
        framebufferInfo.width = m_info.width;
        framebufferInfo.height = m_info.height;
        framebufferInfo.layers = 1;
        VK_CHECK(m_vkCreateFramebuffer(m_device, &framebufferInfo, nullptr, &m_framebuffers[i]), "vkCreateFramebuffer failed");
    }
    return true;
}

/////////////////////////////////////////////////////////////////////

bool VulkanOverlayRenderer::CreateCommandPool()
{
    // Command pool must target the same graphics family used for overlay submissions
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = m_info.graphicsFamily;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    VK_CHECK(m_vkCreateCommandPool(m_device, &poolInfo, nullptr, &m_commandPool), "vkCreateCommandPool failed");
    return true;
}

/////////////////////////////////////////////////////////////////////

bool VulkanOverlayRenderer::CreateCommandBuffers()
{
    // Allocate one command buffer per swapchain image for independent frame-in-flight slots
    m_commandBuffers.resize(m_info.imageCount);
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = m_commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = m_info.imageCount;
    VK_CHECK(m_vkAllocateCommandBuffers(m_device, &allocInfo, m_commandBuffers.data()), "vkAllocateCommandBuffers failed");
    return true;
}

/////////////////////////////////////////////////////////////////////

bool VulkanOverlayRenderer::CreateFrameSyncObjects()
{
    // One fence and semaphore per swapchain image keeps overlay submissions ordered
    m_fences.resize(m_info.imageCount, VK_NULL_HANDLE);
    m_renderFinished.resize(m_info.imageCount, VK_NULL_HANDLE);

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    for (uint32_t i = 0; i < m_info.imageCount; ++i)
    {
        VK_CHECK(m_vkCreateFence(m_device, &fenceInfo, nullptr, &m_fences[i]), "vkCreateFence failed");
        VK_CHECK(m_vkCreateSemaphore(m_device, &semaphoreInfo, nullptr, &m_renderFinished[i]), "vkCreateSemaphore failed");
    }
    return true;
}

/////////////////////////////////////////////////////////////////////

bool VulkanOverlayRenderer::CreateImGuiDescriptorPool()
{
    // Pool includes extra sampled-image/sampler descriptors for notification icon textures
    VkDescriptorPoolSize poolSizes[3]{};
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[0].descriptorCount = 32;
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    poolSizes[1].descriptorCount = 32;
    poolSizes[2].type = VK_DESCRIPTOR_TYPE_SAMPLER;
    poolSizes[2].descriptorCount = 32;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    poolInfo.maxSets = 32;
    poolInfo.poolSizeCount = 3;
    poolInfo.pPoolSizes = poolSizes;

    VK_CHECK(m_vkCreateDescriptorPool(m_device, &poolInfo, nullptr, &m_descriptorPool), "vkCreateDescriptorPool failed");
    return true;
}

/////////////////////////////////////////////////////////////////////

bool VulkanOverlayRenderer::LoadVulkanFunctions()
{
    if (!m_info.getDeviceProcAddr)
    {
        LYMALINK_LOG("[VulkanOverlayRenderer][LoadVulkanFunctions] getDeviceProcAddr missing.");
        return false;
    }

    LOAD_DEVICE_FN(vkAllocateCommandBuffers);
    LOAD_DEVICE_FN(vkAllocateMemory);
    LOAD_DEVICE_FN(vkBeginCommandBuffer);
    LOAD_DEVICE_FN(vkBindBufferMemory);
    LOAD_DEVICE_FN(vkBindImageMemory);
    LOAD_DEVICE_FN(vkCmdBeginRenderPass);
    LOAD_DEVICE_FN(vkCmdCopyBufferToImage);
    LOAD_DEVICE_FN(vkCmdEndRenderPass);
    LOAD_DEVICE_FN(vkCmdPipelineBarrier);
    LOAD_DEVICE_FN(vkCreateBuffer);
    LOAD_DEVICE_FN(vkCreateCommandPool);
    LOAD_DEVICE_FN(vkCreateDescriptorPool);
    LOAD_DEVICE_FN(vkCreateFence);
    LOAD_DEVICE_FN(vkCreateFramebuffer);
    LOAD_DEVICE_FN(vkCreateImage);
    LOAD_DEVICE_FN(vkCreateImageView);
    LOAD_DEVICE_FN(vkCreateRenderPass);
    LOAD_DEVICE_FN(vkCreateSampler);
    LOAD_DEVICE_FN(vkCreateSemaphore);
    LOAD_DEVICE_FN(vkDestroyBuffer);
    LOAD_DEVICE_FN(vkDestroyCommandPool);
    LOAD_DEVICE_FN(vkDestroyDescriptorPool);
    LOAD_DEVICE_FN(vkDestroyFence);
    LOAD_DEVICE_FN(vkDestroyFramebuffer);
    LOAD_DEVICE_FN(vkDestroyImage);
    LOAD_DEVICE_FN(vkDestroyImageView);
    LOAD_DEVICE_FN(vkDestroyRenderPass);
    LOAD_DEVICE_FN(vkDestroySampler);
    LOAD_DEVICE_FN(vkDestroySemaphore);
    LOAD_DEVICE_FN(vkDeviceWaitIdle);
    LOAD_DEVICE_FN(vkEndCommandBuffer);
    LOAD_DEVICE_FN(vkFreeCommandBuffers);
    LOAD_DEVICE_FN(vkFreeMemory);
    LOAD_DEVICE_FN(vkGetBufferMemoryRequirements);
    LOAD_DEVICE_FN(vkGetImageMemoryRequirements);
    LOAD_DEVICE_FN(vkMapMemory);
    LOAD_DEVICE_FN(vkQueueSubmit);
    LOAD_DEVICE_FN(vkQueueWaitIdle);
    LOAD_DEVICE_FN(vkResetCommandBuffer);
    LOAD_DEVICE_FN(vkResetFences);
    LOAD_DEVICE_FN(vkUnmapMemory);
    LOAD_DEVICE_FN(vkWaitForFences);
    return true;
}

/////////////////////////////////////////////////////////////////////

bool VulkanOverlayRenderer::InitializeImGuiVulkanBackend()
{
    // Create ImGui context here because Windows layer has no separate receiver-owned context
    if (!ImGui::GetCurrentContext())
    {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        OverlayFonts::EnsureEmbeddedFontLoaded();
        ImGui::GetIO().IniFilename = nullptr;
        ImGui::StyleColorsDark();
    }

    // Wire the backend to this swapchain render pass and descriptor pool
    ImGui_ImplVulkan_InitInfo initInfo{};
    initInfo.Instance = m_info.instance;
    initInfo.PhysicalDevice = m_info.physicalDevice;
    initInfo.Device = m_device;
    initInfo.QueueFamily = m_info.graphicsFamily;
    initInfo.Queue = m_graphicsQueue;
    initInfo.DescriptorPool = m_descriptorPool;
    initInfo.ApiVersion = VK_API_VERSION_1_0;
    initInfo.MinImageCount = 2;
    initInfo.ImageCount = m_info.imageCount;
    initInfo.PipelineInfoMain.RenderPass = m_renderPass;
    initInfo.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

    if (!ImGui_ImplVulkan_LoadFunctions(initInfo.ApiVersion, ImGuiVulkanLoader, &m_info))
    {
        LYMALINK_LOG("[VulkanOverlayRenderer][InitializeImGuiVulkanBackend] ImGui_ImplVulkan_LoadFunctions failed.");
        return false;
    }
    if (!ImGui_ImplVulkan_Init(&initInfo))
    {
        LYMALINK_LOG("[VulkanOverlayRenderer][InitializeImGuiVulkanBackend] ImGui_ImplVulkan_Init failed.");
        return false;
    }
    m_imguiBackendReady = true;
    return true;
}

/////////////////////////////////////////////////////////////////////

bool VulkanOverlayRenderer::CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& memory)
{
    // Helper for staging buffers used by texture uploads
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    VK_CHECK(m_vkCreateBuffer(m_device, &bufferInfo, nullptr, &buffer), "vkCreateBuffer failed");

    VkMemoryRequirements requirements{};
    m_vkGetBufferMemoryRequirements(m_device, buffer, &requirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = requirements.size;
    allocInfo.memoryTypeIndex = FindMemoryType(requirements.memoryTypeBits, properties);
    VK_CHECK(m_vkAllocateMemory(m_device, &allocInfo, nullptr, &memory), "vkAllocateMemory buffer failed");
    VK_CHECK(m_vkBindBufferMemory(m_device, buffer, memory, 0), "vkBindBufferMemory failed");
    return true;
}

/////////////////////////////////////////////////////////////////////

bool VulkanOverlayRenderer::CreateIconImage()
{
    // Create device-local 2D RGBA image matching fixed shared-memory icon dimensions
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent = { OVERLAY_ICON_SIZE, OVERLAY_ICON_SIZE, 1 };
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    VK_CHECK(m_vkCreateImage(m_device, &imageInfo, nullptr, &m_iconImage), "vkCreateImage icon failed");

    VkMemoryRequirements requirements{};
    m_vkGetImageMemoryRequirements(m_device, m_iconImage, &requirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = requirements.size;
    allocInfo.memoryTypeIndex = FindMemoryType(requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    VK_CHECK(m_vkAllocateMemory(m_device, &allocInfo, nullptr, &m_iconMemory), "vkAllocateMemory icon failed");
    VK_CHECK(m_vkBindImageMemory(m_device, m_iconImage, m_iconMemory, 0), "vkBindImageMemory icon failed");

    // Create view and sampler used by ImGui texture descriptor
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = m_iconImage;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.layerCount = 1;
    VK_CHECK(m_vkCreateImageView(m_device, &viewInfo, nullptr, &m_iconView), "vkCreateImageView icon failed");

    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.maxLod = 1.0f;
    VK_CHECK(m_vkCreateSampler(m_device, &samplerInfo, nullptr, &m_iconSampler), "vkCreateSampler icon failed");
    return true;
}

/////////////////////////////////////////////////////////////////////

bool VulkanOverlayRenderer::UploadIconPixels(const std::vector<uint8_t>& rgbaPixels)
{
    // Upload through host-visible staging buffer before copying into device-local image.
    VkBuffer stagingBuffer = VK_NULL_HANDLE;
    VkDeviceMemory stagingMemory = VK_NULL_HANDLE;
    if (!CreateBuffer(rgbaPixels.size(), VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        stagingBuffer, stagingMemory))
    {
        return false;
    }

    void* mapped = nullptr;
    VkResult mapResult = m_vkMapMemory(m_device, stagingMemory, 0, rgbaPixels.size(), 0, &mapped);
    if (mapResult != VK_SUCCESS)
    {
        m_vkDestroyBuffer(m_device, stagingBuffer, nullptr);
        m_vkFreeMemory(m_device, stagingMemory, nullptr);
        LYMALINK_LOG("[VulkanOverlayRenderer][UploadIconPixels] vkMapMemory failed result=" + std::to_string(mapResult));
        return false;
    }
    std::memcpy(mapped, rgbaPixels.data(), rgbaPixels.size());
    m_vkUnmapMemory(m_device, stagingMemory);

    // Small context object keeps lambda signature C-style and avoids capturing
    struct UploadData
    {
        VulkanOverlayRenderer* renderer;
        VkImage image;
        VkBuffer buffer;
    } data{ this, m_iconImage, stagingBuffer };

    const bool ok = RunOneTimeCommands([](VkCommandBuffer commandBuffer, void* user) {
        auto* upload = static_cast<UploadData*>(user);

        // Prepare icon image as transfer destination
        VkImageMemoryBarrier barrierIn{};
        barrierIn.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrierIn.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrierIn.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrierIn.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrierIn.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrierIn.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrierIn.image = upload->image;
        barrierIn.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
        upload->renderer->m_vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrierIn);

        VkBufferImageCopy copyRegion{};
        copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copyRegion.imageSubresource.layerCount = 1;
        copyRegion.imageExtent = { OVERLAY_ICON_SIZE, OVERLAY_ICON_SIZE, 1 };
        upload->renderer->m_vkCmdCopyBufferToImage(commandBuffer, upload->buffer, upload->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

        // Make uploaded pixels shader-readable for ImGui sampling
        VkImageMemoryBarrier barrierOut{};
        barrierOut.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrierOut.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrierOut.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        barrierOut.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrierOut.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrierOut.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrierOut.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrierOut.image = upload->image;
        barrierOut.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
        upload->renderer->m_vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrierOut);
    }, &data);

    m_vkDestroyBuffer(m_device, stagingBuffer, nullptr);
    m_vkFreeMemory(m_device, stagingMemory, nullptr);
    return ok;
}

/////////////////////////////////////////////////////////////////////

bool VulkanOverlayRenderer::RunOneTimeCommands(void (*record)(VkCommandBuffer, void*), void* userData)
{
    // Allocate, submit, wait, then free a temporary command buffer for setup transfers
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = m_commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
    VK_CHECK(m_vkAllocateCommandBuffers(m_device, &allocInfo, &commandBuffer), "vkAllocateCommandBuffers one-time failed");

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    VK_CHECK(m_vkBeginCommandBuffer(commandBuffer, &beginInfo), "vkBeginCommandBuffer one-time failed");
    record(commandBuffer, userData);
    VK_CHECK(m_vkEndCommandBuffer(commandBuffer), "vkEndCommandBuffer one-time failed");

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;
    VK_CHECK(m_vkQueueSubmit(m_graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE), "vkQueueSubmit one-time failed");
    VK_CHECK(m_vkQueueWaitIdle(m_graphicsQueue), "vkQueueWaitIdle one-time failed");

    m_vkFreeCommandBuffers(m_device, m_commandPool, 1, &commandBuffer);
    return true;
}

/////////////////////////////////////////////////////////////////////

uint32_t VulkanOverlayRenderer::FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const
{
    // Match Vulkan memory type mask with required host/device property flags
    VkPhysicalDeviceMemoryProperties memoryProperties{};
    m_info.getMemoryProperties(m_info.physicalDevice, &memoryProperties);
    for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; ++i)
    {
        if ((typeFilter & (1u << i)) && (memoryProperties.memoryTypes[i].propertyFlags & properties) == properties)
        {
            return i;
        }
    }
    LYMALINK_LOG("[VulkanOverlayRenderer][FindMemoryType] no matching memory type.");
    return 0;
}

/////////////////////////////////////////////////////////////////////

void VulkanOverlayRenderer::DestroyIconTexture()
{
    // Descriptor must be unregistered before destroying underlying image/sampler objects
    if (m_iconDescriptor != VK_NULL_HANDLE)
    {
        ImGui_ImplVulkan_RemoveTexture(m_iconDescriptor);
        m_iconDescriptor = VK_NULL_HANDLE;
    }
    if (m_iconSampler)
    {
        m_vkDestroySampler(m_device, m_iconSampler, nullptr);
        m_iconSampler = VK_NULL_HANDLE;
    }
    if (m_iconView)
    {
        m_vkDestroyImageView(m_device, m_iconView, nullptr);
        m_iconView = VK_NULL_HANDLE;
    }
    if (m_iconImage)
    {
        m_vkDestroyImage(m_device, m_iconImage, nullptr);
        m_iconImage = VK_NULL_HANDLE;
    }
    if (m_iconMemory)
    {
        m_vkFreeMemory(m_device, m_iconMemory, nullptr);
        m_iconMemory = VK_NULL_HANDLE;
    }
    m_iconGeneration = 0;
}

/////////////////////////////////////////////////////////////////////

void VulkanOverlayRenderer::DestroySwapchainFramebuffers()
{
    // Framebuffers are rebuilt whenever swapchain images/views change
    for (VkFramebuffer framebuffer : m_framebuffers)
    {
        if (framebuffer)
        {
            m_vkDestroyFramebuffer(m_device, framebuffer, nullptr);
        }
    }
    m_framebuffers.clear();
}
