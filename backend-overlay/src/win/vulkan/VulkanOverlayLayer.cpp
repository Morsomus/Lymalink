/////////////////////////////////////////////////////////
// File: VulkanOverlayLayer.cpp
// Date: 2026-06-27
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Implements Windows Vulkan implicit layer
//              hooks and notification rendering.
/////////////////////////////////////////////////////////

#include "VulkanOverlayLayer.h"

#include "WinLogger.h"
#include "WinOverlayReceiver.h"
#include "VulkanOverlayRenderer.h"
#include "imgui.h"
#include "imgui_impl_vulkan.h"

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace
{
static WinOverlayReceiver s_overlay;            // One overlay receiver shared by every intercepted Vulkan device in this process
static std::mutex s_renderMutex;
static thread_local bool s_rendering = false;   // Prevents recursive vkQueuePresentKHR interception if rendering triggers driver present work

// Per-instance dispatch table and first physical device snapshot
struct InstanceData
{
    VkInstance instance = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    PFN_vkGetInstanceProcAddr getProcAddr = nullptr;
    PFN_vkDestroyInstance destroyInstance = nullptr;
    PFN_vkGetPhysicalDeviceMemoryProperties getMemoryProperties = nullptr;
};

// Per-device dispatch table plus swapchain objects owned by the overlay layer
struct DeviceData
{
    VkDevice device = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    PFN_vkGetDeviceProcAddr getProcAddr = nullptr;
    PFN_vkDestroyDevice destroyDevice = nullptr;
    PFN_vkQueuePresentKHR queuePresent = nullptr;
    PFN_vkGetSwapchainImagesKHR getSwapchainImages = nullptr;
    PFN_vkCreateImageView createImageView = nullptr;
    PFN_vkDestroyImageView destroyImageView = nullptr;

    uint32_t graphicsFamily = 0;
    VkQueue graphicsQueue = VK_NULL_HANDLE;
    VkFormat swapchainFormat = VK_FORMAT_UNDEFINED;
    uint32_t swapchainWidth = 0;
    uint32_t swapchainHeight = 0;
    std::vector<VkImage> swapchainImages;
    std::vector<VkImageView> swapchainViews;
    std::unique_ptr<VulkanOverlayRenderer> renderer = std::make_unique<VulkanOverlayRenderer>();
};

// Vulkan loader dispatch keys are used as stable map keys for dispatchable handles
static std::mutex s_instanceMutex;
static std::unordered_map<void*, InstanceData> s_instances;

static std::mutex s_deviceMutex;
static std::unordered_map<void*, DeviceData> s_devices;
static std::unordered_map<void*, void*> s_queueToDevice;    // Queue handles do not always share the same dispatch key as the device, so cache an explicit mapping

/////////////////////////////////////////////////////////////////////

void* DispatchKey(void* handle)
{
    // Loader stores the dispatch pointer in the first pointer-sized slot of each dispatchable handle
    return handle ? *reinterpret_cast<void**>(handle) : nullptr;
}

/////////////////////////////////////////////////////////////////////

void DestroySwapchainImageViews(DeviceData& device)
{
    if (!device.destroyImageView)
    {
        device.swapchainViews.clear();
        return;
    }
    for (VkImageView view : device.swapchainViews)
    {
        if (view != VK_NULL_HANDLE)
        {
            device.destroyImageView(device.device, view, nullptr);
        }
    }
    device.swapchainViews.clear();
}

/////////////////////////////////////////////////////////////////////

uint32_t FindGraphicsFamily(VkPhysicalDevice physicalDevice, PFN_vkGetPhysicalDeviceQueueFamilyProperties getQueueFamilyProps)
{
    // Prefer the first graphics-capable queue family; family 0 is the last-resort fallback
    uint32_t count = 0;
    getQueueFamilyProps(physicalDevice, &count, nullptr);
    std::vector<VkQueueFamilyProperties> properties(count);
    getQueueFamilyProps(physicalDevice, &count, properties.data());
    for (uint32_t i = 0; i < count; ++i)
    {
        if (properties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
        {
            return i;
        }
    }

    LYMALINK_LOG("[VulkanOverlayLayer][FindGraphicsFamily] no graphics queue, using family 0.");
    return 0;
}

/////////////////////////////////////////////////////////////////////

VkInstance FindInstanceForPhysicalDevice(VkPhysicalDevice physicalDevice, PFN_vkGetInstanceProcAddr* getInstanceProcAddr, PFN_vkGetPhysicalDeviceMemoryProperties* getMemoryProperties)
{
    // Match device creation back to the instance data captured during vkCreateInstance
    std::lock_guard<std::mutex> lock(s_instanceMutex);
    for (auto& [key, instance] : s_instances)
    {
        if (instance.physicalDevice == physicalDevice)
        {
            if (getInstanceProcAddr)
            {
                *getInstanceProcAddr = instance.getProcAddr;
            }
            if (getMemoryProperties)
            {
                *getMemoryProperties = instance.getMemoryProperties;
            }
            return instance.instance;
        }
    }
    return VK_NULL_HANDLE;
}

/////////////////////////////////////////////////////////////////////

DeviceData* FindDeviceForQueueLocked(VkQueue queue)
{
    // Normal path: map queue dispatch key to device dispatch key
    void* queueKey = DispatchKey(queue);
    auto qit = s_queueToDevice.find(queueKey);
    if (qit != s_queueToDevice.end())
    {
        auto dit = s_devices.find(qit->second);
        if (dit != s_devices.end())
        {
            return &dit->second;
        }
    }
    // Single-device fallback handles drivers whose queue dispatch key does not match our cache
    if (s_devices.size() == 1)
    {
        return &s_devices.begin()->second;
    }
    return nullptr;
}

/////////////////////////////////////////////////////////////////////

// Intercepts instance creation to advance the loader chain, create the real instance, and capture instance-level dispatch functions needed by later hooks
static VKAPI_ATTR VkResult VKAPI_CALL Hook_vkCreateInstance(const VkInstanceCreateInfo* createInfo, const VkAllocationCallbacks* allocator, VkInstance* instance)
{
    LYMALINK_LOG("[VulkanOverlayLayer][Hook_vkCreateInstance] called.");

    // Walk pNext chain to find the loader-injected link info for this layer
    auto* linkInfo = static_cast<VkLayerInstanceCreateInfo*>(const_cast<void*>(createInfo->pNext));
    while (linkInfo && !(linkInfo->sType == VK_STRUCTURE_TYPE_LOADER_INSTANCE_CREATE_INFO && linkInfo->function == VK_LAYER_LINK_INFO))
    {
        linkInfo = static_cast<VkLayerInstanceCreateInfo*>(const_cast<void*>(linkInfo->pNext));
    }
    if (!linkInfo)
    {
        LYMALINK_LOG("[VulkanOverlayLayer][Hook_vkCreateInstance] missing loader link info.");
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    PFN_vkGetInstanceProcAddr nextGIPA = linkInfo->u.pLayerInfo->pfnNextGetInstanceProcAddr;
    // Advance chain so downstream layers and the ICD see their own link info
    linkInfo->u.pLayerInfo = linkInfo->u.pLayerInfo->pNext;

    auto createInstance = reinterpret_cast<PFN_vkCreateInstance>(nextGIPA(VK_NULL_HANDLE, "vkCreateInstance"));
    if (!createInstance)
    {
        LYMALINK_LOG("[VulkanOverlayLayer][Hook_vkCreateInstance] next vkCreateInstance missing.");
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    VkResult result = createInstance(createInfo, allocator, instance);
    if (result != VK_SUCCESS)
    {
        LYMALINK_LOG("[VulkanOverlayLayer][Hook_vkCreateInstance] next vkCreateInstance failed result=" + std::to_string(result));
        return result;
    }

    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    // Snapshot the first physical device so device hooks can find matching instance state cheaply
    auto enumeratePhysicalDevices = reinterpret_cast<PFN_vkEnumeratePhysicalDevices>(nextGIPA(*instance, "vkEnumeratePhysicalDevices"));
    if (enumeratePhysicalDevices)
    {
        uint32_t count = 1;
        VkResult enumResult = enumeratePhysicalDevices(*instance, &count, &physicalDevice);
        if (enumResult != VK_SUCCESS && !(enumResult == VK_INCOMPLETE && physicalDevice != VK_NULL_HANDLE))
        {
            LYMALINK_LOG("[VulkanOverlayLayer][Hook_vkCreateInstance] vkEnumeratePhysicalDevices failed result=" + std::to_string(enumResult));
        }
    }

    InstanceData data{};
    data.instance = *instance;
    data.physicalDevice = physicalDevice;
    data.getProcAddr = nextGIPA;
    data.destroyInstance = reinterpret_cast<PFN_vkDestroyInstance>(nextGIPA(*instance, "vkDestroyInstance"));
    data.getMemoryProperties = reinterpret_cast<PFN_vkGetPhysicalDeviceMemoryProperties>(nextGIPA(*instance, "vkGetPhysicalDeviceMemoryProperties"));

    std::lock_guard<std::mutex> lock(s_instanceMutex);
    s_instances[DispatchKey(*instance)] = std::move(data);
    return VK_SUCCESS;
}

/////////////////////////////////////////////////////////////////////

// Removes tracked instance state before forwarding destruction to the next layer/driver
static VKAPI_ATTR void VKAPI_CALL Hook_vkDestroyInstance(VkInstance instance, const VkAllocationCallbacks* allocator)
{
    PFN_vkDestroyInstance destroyInstance = nullptr;
    {
        std::lock_guard<std::mutex> lock(s_instanceMutex);
        auto it = s_instances.find(DispatchKey(instance));
        if (it != s_instances.end())
        {
            destroyInstance = it->second.destroyInstance;
            s_instances.erase(it);
        }
    }
    if (destroyInstance)
    {
        destroyInstance(instance, allocator);
    }
}

/////////////////////////////////////////////////////////////////////

// Intercepts device creation to resolve device dispatch, select graphics queue, and prepare queue-to-device lookup for present hooks
static VKAPI_ATTR VkResult VKAPI_CALL Hook_vkCreateDevice(VkPhysicalDevice physicalDevice, const VkDeviceCreateInfo* createInfo, const VkAllocationCallbacks* allocator, VkDevice* device)
{
    // Walk pNext chain to find and advance the loader device link
    auto* linkInfo = static_cast<VkLayerDeviceCreateInfo*>(const_cast<void*>(createInfo->pNext));
    while (linkInfo && !(linkInfo->sType == VK_STRUCTURE_TYPE_LOADER_DEVICE_CREATE_INFO && linkInfo->function == VK_LAYER_LINK_INFO))
    {
        linkInfo = static_cast<VkLayerDeviceCreateInfo*>(const_cast<void*>(linkInfo->pNext));
    }
    if (!linkInfo)
    {
        LYMALINK_LOG("[VulkanOverlayLayer][Hook_vkCreateDevice] missing loader link info.");
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    PFN_vkGetInstanceProcAddr nextGIPA = linkInfo->u.pLayerInfo->pfnNextGetInstanceProcAddr;
    PFN_vkGetDeviceProcAddr nextGDPA = linkInfo->u.pLayerInfo->pfnNextGetDeviceProcAddr;
    linkInfo->u.pLayerInfo = linkInfo->u.pLayerInfo->pNext;

    auto createDevice = reinterpret_cast<PFN_vkCreateDevice>(nextGIPA(VK_NULL_HANDLE, "vkCreateDevice"));
    if (!createDevice)
    {
        LYMALINK_LOG("[VulkanOverlayLayer][Hook_vkCreateDevice] next vkCreateDevice missing.");
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    VkResult result = createDevice(physicalDevice, createInfo, allocator, device);
    if (result != VK_SUCCESS)
    {
        LYMALINK_LOG("[VulkanOverlayLayer][Hook_vkCreateDevice] next vkCreateDevice failed result=" + std::to_string(result));
        return result;
    }

    PFN_vkGetInstanceProcAddr instanceProcAddr = nullptr;
    VkInstance instance = FindInstanceForPhysicalDevice(physicalDevice, &instanceProcAddr, nullptr);
    auto getQueueFamilyProps = instanceProcAddr
        ? reinterpret_cast<PFN_vkGetPhysicalDeviceQueueFamilyProperties>(instanceProcAddr(instance, "vkGetPhysicalDeviceQueueFamilyProperties"))
        : nullptr;
    const uint32_t graphicsFamily = getQueueFamilyProps ? FindGraphicsFamily(physicalDevice, getQueueFamilyProps) : 0;

    VkQueue graphicsQueue = VK_NULL_HANDLE;
    // Use queue index 0 in the selected graphics family for overlay rendering
    auto getDeviceQueue = reinterpret_cast<PFN_vkGetDeviceQueue>(nextGDPA(*device, "vkGetDeviceQueue"));
    if (getDeviceQueue)
    {
        getDeviceQueue(*device, graphicsFamily, 0, &graphicsQueue);
    }

    DeviceData data{};
    data.device = *device;
    data.physicalDevice = physicalDevice;
    data.getProcAddr = nextGDPA;
    data.destroyDevice = reinterpret_cast<PFN_vkDestroyDevice>(nextGDPA(*device, "vkDestroyDevice"));
    data.queuePresent = reinterpret_cast<PFN_vkQueuePresentKHR>(nextGDPA(*device, "vkQueuePresentKHR"));
    data.getSwapchainImages = reinterpret_cast<PFN_vkGetSwapchainImagesKHR>(nextGDPA(*device, "vkGetSwapchainImagesKHR"));
    data.createImageView = reinterpret_cast<PFN_vkCreateImageView>(nextGDPA(*device, "vkCreateImageView"));
    data.destroyImageView = reinterpret_cast<PFN_vkDestroyImageView>(nextGDPA(*device, "vkDestroyImageView"));
    data.graphicsFamily = graphicsFamily;
    data.graphicsQueue = graphicsQueue;

    std::lock_guard<std::mutex> lock(s_deviceMutex);
    void* deviceKey = DispatchKey(*device);
    if (graphicsQueue != VK_NULL_HANDLE)
    {
        // Present hook receives a queue, so remember which device owns this queue
        s_queueToDevice[DispatchKey(graphicsQueue)] = deviceKey;
    }
    s_devices.insert_or_assign(deviceKey, std::move(data));

    return VK_SUCCESS;
}

/////////////////////////////////////////////////////////////////////

// Shuts down overlay resources before forwarding real device destruction
static VKAPI_ATTR void VKAPI_CALL Hook_vkDestroyDevice(VkDevice device, const VkAllocationCallbacks* allocator)
{
    PFN_vkDestroyDevice destroyDevice = nullptr;
    {
        std::lock_guard<std::mutex> lock(s_deviceMutex);
        auto it = s_devices.find(DispatchKey(device));
        if (it != s_devices.end())
        {
            if (it->second.renderer)
            {
                // Renderer owns Vulkan resources tied to this VkDevice
                it->second.renderer->Shutdown();
            }
            DestroySwapchainImageViews(it->second);
            // Drop any cached queue aliases belonging to the destroyed device
            for (auto qit = s_queueToDevice.begin(); qit != s_queueToDevice.end();)
            {
                if (qit->second == DispatchKey(device))
                {
                    qit = s_queueToDevice.erase(qit);
                }
                else
                {
                    ++qit;
                }
            }
            destroyDevice = it->second.destroyDevice;
            s_devices.erase(it);
        }
    }
    if (destroyDevice)
    {
        destroyDevice(device, allocator);
    }
}

/////////////////////////////////////////////////////////////////////

// Rebuilds overlay swapchain resources after application swapchain creation/resizing
static VKAPI_ATTR VkResult VKAPI_CALL Hook_vkCreateSwapchainKHR(VkDevice device, const VkSwapchainCreateInfoKHR* createInfo, const VkAllocationCallbacks* allocator, VkSwapchainKHR* swapchain)
{
    PFN_vkCreateSwapchainKHR createSwapchain = nullptr;
    {
        std::lock_guard<std::mutex> lock(s_deviceMutex);
        auto it = s_devices.find(DispatchKey(device));
        if (it != s_devices.end())
        {
            createSwapchain = reinterpret_cast<PFN_vkCreateSwapchainKHR>(it->second.getProcAddr(device, "vkCreateSwapchainKHR"));
        }
    }
    if (!createSwapchain)
    {
        LYMALINK_LOG("[VulkanOverlayLayer][Hook_vkCreateSwapchainKHR] next vkCreateSwapchainKHR missing.");
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    VkResult result = createSwapchain(device, createInfo, allocator, swapchain);
    if (result != VK_SUCCESS)
    {
        return result;
    }

    VulkanOverlayRenderer* renderer = nullptr;
    VulkanOverlayRendererInitInfo rendererInfo{};
    {
        std::lock_guard<std::mutex> lock(s_deviceMutex);
        auto it = s_devices.find(DispatchKey(device));
        if (it == s_devices.end())
        {
            return result;
        }

        DeviceData& dev = it->second;
        renderer = dev.renderer.get();
        if (!dev.getSwapchainImages || !dev.createImageView)
        {
            LYMALINK_LOG("[VulkanOverlayLayer][Hook_vkCreateSwapchainKHR] required swapchain functions missing.");
            return result;
        }

        // Two-call Vulkan idiom: query image count first, then fill image array
        uint32_t imageCount = 0;
        VkResult imageResult = dev.getSwapchainImages(device, *swapchain, &imageCount, nullptr);
        if (imageResult != VK_SUCCESS)
        {
            LYMALINK_LOG("[VulkanOverlayLayer][Hook_vkCreateSwapchainKHR] image count failed result=" + std::to_string(imageResult));
            return result;
        }
        dev.swapchainImages.resize(imageCount);
        imageResult = dev.getSwapchainImages(device, *swapchain, &imageCount, dev.swapchainImages.data());
        if (imageResult != VK_SUCCESS)
        {
            dev.swapchainImages.clear();
            LYMALINK_LOG("[VulkanOverlayLayer][Hook_vkCreateSwapchainKHR] image data failed result=" + std::to_string(imageResult));
            return result;
        }

        if (renderer)
        {
            // Old swapchain resources are invalid once a new swapchain is created
            renderer->Shutdown();
        }
        DestroySwapchainImageViews(dev);

        // Cache dimensions/format for present hook and ImGui display size
        dev.swapchainFormat = createInfo->imageFormat;
        dev.swapchainWidth = createInfo->imageExtent.width;
        dev.swapchainHeight = createInfo->imageExtent.height;
        dev.swapchainViews.resize(imageCount, VK_NULL_HANDLE);

        bool imageViewsReady = true;
        for (uint32_t i = 0; i < imageCount; ++i)
        {
            // Renderer needs one 2D color view per swapchain image
            VkImageViewCreateInfo viewInfo{};
            viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            viewInfo.image = dev.swapchainImages[i];
            viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            viewInfo.format = dev.swapchainFormat;
            viewInfo.components = { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY };
            viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            viewInfo.subresourceRange.levelCount = 1;
            viewInfo.subresourceRange.layerCount = 1;
            VkResult viewResult = dev.createImageView(device, &viewInfo, nullptr, &dev.swapchainViews[i]);
            if (viewResult != VK_SUCCESS)
            {
                imageViewsReady = false;
                LYMALINK_LOG("[VulkanOverlayLayer][Hook_vkCreateSwapchainKHR] vkCreateImageView failed result=" + std::to_string(viewResult));
            }
        }
        if (!imageViewsReady)
        {
            DestroySwapchainImageViews(dev);
            return result;
        }

        PFN_vkGetInstanceProcAddr instanceProcAddr = nullptr;
        PFN_vkGetPhysicalDeviceMemoryProperties getMemoryProperties = nullptr;
        // Resolve instance-scoped functions required by ImGui/Vulkan memory allocation
        VkInstance instance = FindInstanceForPhysicalDevice(dev.physicalDevice, &instanceProcAddr, &getMemoryProperties);

        rendererInfo.instance = instance;
        rendererInfo.physicalDevice = dev.physicalDevice;
        rendererInfo.device = device;
        rendererInfo.graphicsQueue = dev.graphicsQueue;
        rendererInfo.getInstanceProcAddr = instanceProcAddr;
        rendererInfo.getDeviceProcAddr = dev.getProcAddr;
        rendererInfo.getMemoryProperties = getMemoryProperties;
        rendererInfo.graphicsFamily = dev.graphicsFamily;
        rendererInfo.swapchainFormat = dev.swapchainFormat;
        rendererInfo.imageCount = imageCount;
        rendererInfo.width = dev.swapchainWidth;
        rendererInfo.height = dev.swapchainHeight;
        rendererInfo.swapchainImages = dev.swapchainImages;
        rendererInfo.swapchainViews = dev.swapchainViews;
    }

    if (renderer && !renderer->Initialize(rendererInfo))
    {
        LYMALINK_LOG("[VulkanOverlayLayer][Hook_vkCreateSwapchainKHR] renderer init failed.");
    }
    return result;
}

/////////////////////////////////////////////////////////////////////

// Renders ImGui overlay just before handing the present call to the real driver
static VKAPI_ATTR VkResult VKAPI_CALL Hook_vkQueuePresentKHR(VkQueue queue, const VkPresentInfoKHR* presentInfo)
{
    if (s_rendering)
    {
        // Re-entrant present from driver/backend path: bypass overlay rendering and forward
        std::lock_guard<std::mutex> lock(s_deviceMutex);
        DeviceData* dev = FindDeviceForQueueLocked(queue);
        return dev && dev->queuePresent ? dev->queuePresent(queue, presentInfo) : VK_ERROR_DEVICE_LOST;
    }

    PFN_vkQueuePresentKHR nextPresent = nullptr;
    VulkanOverlayRenderer* renderer = nullptr;
    uint32_t width = 0;
    uint32_t height = 0;
    {
        std::lock_guard<std::mutex> lock(s_deviceMutex);
        DeviceData* dev = FindDeviceForQueueLocked(queue);
        if (dev)
        {
            nextPresent = dev->queuePresent;
            renderer = dev->renderer.get();
            width = dev->swapchainWidth;
            height = dev->swapchainHeight;
        }
    }

    const VkPresentInfoKHR* submitPresentInfo = presentInfo;
    VkPresentInfoKHR overlayPresentInfo{};
    VkSemaphore overlaySemaphore = VK_NULL_HANDLE;

    if (renderer && renderer->IsReady() && width > 0 && height > 0)
    {
        // Build one ImGui frame and render it into the current swapchain image
        std::lock_guard<std::mutex> renderLock(s_renderMutex);
        s_rendering = true;

        ImGui::GetIO().DisplaySize = ImVec2(static_cast<float>(width), static_cast<float>(height));
        ImGui_ImplVulkan_NewFrame();
        ImGui::NewFrame();
        s_overlay.BeginFrame();
        ImTextureID icon = renderer->EnsureIconTexture(s_overlay.IconPixels(), s_overlay.IconGeneration());
        s_overlay.Draw(width, height, icon);
        ImGui::Render();

        overlaySemaphore = renderer->RenderDrawData(queue, presentInfo, ImGui::GetDrawData());
        if (overlaySemaphore != VK_NULL_HANDLE)
        {
            // Present waits on overlay render completion without mutating the application's struct
            overlayPresentInfo = *presentInfo;
            overlayPresentInfo.waitSemaphoreCount = 1;
            overlayPresentInfo.pWaitSemaphores = &overlaySemaphore;
            submitPresentInfo = &overlayPresentInfo;
        }
        s_rendering = false;
    }

    if (nextPresent)
    {
        return nextPresent(queue, submitPresentInfo);
    }
    LYMALINK_LOG("[VulkanOverlayLayer][Hook_vkQueuePresentKHR] next vkQueuePresentKHR missing.");
    return VK_ERROR_DEVICE_LOST;
}

/////////////////////////////////////////////////////////////////////

// Central dispatch table for functions this layer intercepts
PFN_vkVoidFunction GetLayerProc(const char* name)
{
    if (!name)
    {
        return nullptr;
    }
    const std::string n(name);
    if (n == "vkCreateInstance") return reinterpret_cast<PFN_vkVoidFunction>(Hook_vkCreateInstance);
    if (n == "vkDestroyInstance") return reinterpret_cast<PFN_vkVoidFunction>(Hook_vkDestroyInstance);
    if (n == "vkCreateDevice") return reinterpret_cast<PFN_vkVoidFunction>(Hook_vkCreateDevice);
    if (n == "vkDestroyDevice") return reinterpret_cast<PFN_vkVoidFunction>(Hook_vkDestroyDevice);
    if (n == "vkCreateSwapchainKHR") return reinterpret_cast<PFN_vkVoidFunction>(Hook_vkCreateSwapchainKHR);
    if (n == "vkQueuePresentKHR") return reinterpret_cast<PFN_vkVoidFunction>(Hook_vkQueuePresentKHR);
    if (n == "vkGetInstanceProcAddr") return reinterpret_cast<PFN_vkVoidFunction>(LymalinkLayer_vkGetInstanceProcAddr);
    if (n == "vkGetDeviceProcAddr") return reinterpret_cast<PFN_vkVoidFunction>(LymalinkLayer_vkGetDeviceProcAddr);
    if (n == "vkGetPhysicalDeviceProcAddr") return reinterpret_cast<PFN_vkVoidFunction>(LymalinkLayer_vkGetPhysicalDeviceProcAddr);
    return nullptr;
}

/////////////////////////////////////////////////////////////////////

// Forwards instance-level lookups through the next layer/driver captured at creation time
PFN_vkVoidFunction ForwardInstanceProc(VkInstance instance, const char* name)
{
    std::lock_guard<std::mutex> lock(s_instanceMutex);
    auto it = s_instances.find(DispatchKey(instance));
    return it != s_instances.end() ? it->second.getProcAddr(instance, name) : nullptr;
}
}

/////////////////////////////////////////////////////////////////////
// Exported entry points
/////////////////////////////////////////////////////////////////////

LYMALINK_WIN_VK_EXPORT VKAPI_ATTR VkResult VKAPI_CALL LymalinkLayer_vkNegotiateLoaderLayerInterfaceVersion(VkNegotiateLayerInterface* version)
{
    LYMALINK_LOG("[VulkanOverlayLayer][Negotiate] called.");

    // Vulkan loader calls this first so the layer can advertise proc-addr entry points
    if (version && version->loaderLayerInterfaceVersion >= 2)
    {
        version->loaderLayerInterfaceVersion = 2;
        version->pfnGetInstanceProcAddr = LymalinkLayer_vkGetInstanceProcAddr;
        version->pfnGetDeviceProcAddr = LymalinkLayer_vkGetDeviceProcAddr;
        version->pfnGetPhysicalDeviceProcAddr = LymalinkLayer_vkGetPhysicalDeviceProcAddr;
    }
    return VK_SUCCESS;
}

/////////////////////////////////////////////////////////////////////

LYMALINK_WIN_VK_EXPORT VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL LymalinkLayer_vkGetInstanceProcAddr(VkInstance instance, const char* name)
{
    if (PFN_vkVoidFunction fn = GetLayerProc(name))
    {
        return fn;
    }
    return instance != VK_NULL_HANDLE ? ForwardInstanceProc(instance, name) : nullptr;
}

/////////////////////////////////////////////////////////////////////

LYMALINK_WIN_VK_EXPORT VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL LymalinkLayer_vkGetDeviceProcAddr(VkDevice device, const char* name)
{
    if (PFN_vkVoidFunction fn = GetLayerProc(name))
    {
        return fn;
    }
    if (device == VK_NULL_HANDLE)
    {
        return nullptr;
    }
    std::lock_guard<std::mutex> lock(s_deviceMutex);
    auto it = s_devices.find(DispatchKey(device));
    return it != s_devices.end() ? it->second.getProcAddr(device, name) : nullptr;
}

/////////////////////////////////////////////////////////////////////

LYMALINK_WIN_VK_EXPORT VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL LymalinkLayer_vkGetPhysicalDeviceProcAddr(VkInstance instance, const char* name)
{
    // Physical-device functions are instance-scoped, so forward through the instance chain
    return instance != VK_NULL_HANDLE ? ForwardInstanceProc(instance, name) : nullptr;
}

/////////////////////////////////////////////////////////////////////

LYMALINK_WIN_VK_EXPORT VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL vkGetInstanceProcAddr(VkInstance instance, const char* name)
{
    // Export standard loader symbol as an alias for Windows layer discovery paths
    return LymalinkLayer_vkGetInstanceProcAddr(instance, name);
}

/////////////////////////////////////////////////////////////////////

LYMALINK_WIN_VK_EXPORT VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL vkGetDeviceProcAddr(VkDevice device, const char* name)
{
    // Export standard loader symbol as an alias for Windows layer discovery paths
    return LymalinkLayer_vkGetDeviceProcAddr(device, name);
}
