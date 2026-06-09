/////////////////////////////////////////////////////////
// File: VulkanOverlayLayer.cpp
// Date: 2026-05-26
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Vulkan implicit layer implementation.
//              Hooks vkCreateDevice, vkCreateSwapchainKHR
//              and vkQueuePresentKHR to drive the overlay
//              renderer inside the game process.
/////////////////////////////////////////////////////////

#include "VulkanOverlayLayer.h"
#include "OverlayReceiver.h"
#include "VulkanOverlayRenderer.h"
#include "Logger.h"

#include "imgui.h"
#include "imgui_impl_vulkan.h"
#include <vulkan/vk_layer.h>
#include <vulkan/vulkan.h>
#include <mutex>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

// Layer name must match the JSON manifest
static constexpr char LAYER_NAME[] = "VK_LAYER_LYMALINK_overlay";

// Single overlay service shared by all hooks
// Declared before s_devices so it outlives any backend objects that reference it during shutdown.
static OverlayReceiver s_overlay;
static std::once_flag s_overlayInitFlag;

// Per-instance and per-device dispatch tables
// The loader patches the first pointer in every dispatchable handle to a unique key we can use as a map index.
struct InstanceData
{
    VkInstance instance;
    VkPhysicalDevice physicalDevice; // first physical device seen
    PFN_vkGetInstanceProcAddr getProcAddr;
    PFN_vkDestroyInstance destroyInstance;
};

struct DeviceData
{
    VkDevice device;
    VkPhysicalDevice physicalDevice;
    PFN_vkGetDeviceProcAddr getProcAddr;
    PFN_vkDestroyDevice destroyDevice;
    PFN_vkQueuePresentKHR queuePresent;
    PFN_vkGetSwapchainImagesKHR getSwapchainImages;
    PFN_vkCreateImageView createImageView;
    PFN_vkDestroyImageView destroyImageView;

    uint32_t graphicsFamily = 0;
    VkQueue graphicsQueue = VK_NULL_HANDLE;

    // Swapchain state - rebuilt on vkCreateSwapchainKHR
    VkFormat swapchainFormat = VK_FORMAT_UNDEFINED;
    uint32_t swapchainWidth = 0;
    uint32_t swapchainHeight = 0;
    std::vector<VkImage> swapchainImages;
    std::vector<VkImageView> swapchainViews;

    std::unique_ptr<VulkanOverlayRenderer> backend = std::make_unique<VulkanOverlayRenderer>();
};

static std::mutex s_instanceMtx;
static std::unordered_map<void*, InstanceData> s_instances;

static std::mutex s_deviceMtx;
static std::unordered_map<void*, DeviceData> s_devices;

/////////////////////////////////////////////////////////////////////

static void DestroySwapchainImageViews(DeviceData& dev)
{
    if (!dev.destroyImageView)
    {
        dev.swapchainViews.clear();
        return;
    }

    for (VkImageView view : dev.swapchainViews)
    {
        if (view != VK_NULL_HANDLE)
        {
            dev.destroyImageView(dev.device, view, nullptr);
        }
    }
    dev.swapchainViews.clear();
}

/////////////////////////////////////////////////////////////////////

// Returns the loader dispatch key stored in the first pointer slot of any dispatchable Vulkan handle (VkInstance, VkDevice, VkQueue, etc.)
static inline void* DispatchKey(void* handle)
{
    return *reinterpret_cast<void**>(handle);
}

/////////////////////////////////////////////////////////////////////

// Iterates queue family properties to find the first family that supports graphics operations
// Falls back to family 0 if none is found
static uint32_t FindGraphicsFamily(VkPhysicalDevice physDev, PFN_vkGetPhysicalDeviceQueueFamilyProperties getQueueFamilyProps)
{
    uint32_t count = 0;
    getQueueFamilyProps(physDev, &count, nullptr);
    std::vector<VkQueueFamilyProperties> props(count);
    getQueueFamilyProps(physDev, &count, props.data());

    for (uint32_t i = 0; i < count; ++i)
    {
        if (props[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
        {
            return i;
        }
    }

    LYMALINK_LOG("[VulkanOverlayLayer][FindGraphicsFamily] no graphics queue family found; falling back to family 0.");
    return 0;
}

/////////////////////////////////////////////////////////////////////

// Intercepts instance creation to:
//   - advance the loader chain so the next layer/driver also sees the call
//   - snapshot the first physical device for later backend init
//   - store the instance dispatch table in s_instances
static VKAPI_ATTR VkResult VKAPI_CALL Hook_vkCreateInstance(const VkInstanceCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkInstance* pInstance)
{
    // Walk the pNext chain to find the loader-injected link info
    auto* linkInfo = static_cast<VkLayerInstanceCreateInfo*>(const_cast<void*>(pCreateInfo->pNext));

    while (linkInfo && !(linkInfo->sType == VK_STRUCTURE_TYPE_LOADER_INSTANCE_CREATE_INFO && linkInfo->function == VK_LAYER_LINK_INFO))
    {
        linkInfo = static_cast<VkLayerInstanceCreateInfo*>(const_cast<void*>(linkInfo->pNext));
    }

    if (!linkInfo)
    {
        LYMALINK_LOG("[VulkanOverlayLayer][Hook_vkCreateInstance] missing loader link info.");
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    // Grab next layer's GIPA, then advance the chain pointer so downstream layers get their own entry
    PFN_vkGetInstanceProcAddr nextGIPA = linkInfo->u.pLayerInfo->pfnNextGetInstanceProcAddr;
    linkInfo->u.pLayerInfo = linkInfo->u.pLayerInfo->pNext;

    auto createInstance = reinterpret_cast<PFN_vkCreateInstance>(nextGIPA(VK_NULL_HANDLE, "vkCreateInstance"));
    if (!createInstance)
    {
        LYMALINK_LOG("[VulkanOverlayLayer][Hook_vkCreateInstance] next vkCreateInstance missing.");
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    // Forward the actual instance creation to the next layer/driver
    VkResult result = createInstance(pCreateInfo, pAllocator, pInstance);
    if (result != VK_SUCCESS)
    {
        LYMALINK_LOG("[VulkanOverlayLayer][Hook_vkCreateInstance] next vkCreateInstance failed result=" + std::to_string(result));
        return result;
    }

    // Snapshot the first available physical device so Hook_vkCreateDevice can match it back to this instance without a second enumeration later
    VkPhysicalDevice physDev = VK_NULL_HANDLE;
    {
        uint32_t devCount = 1;
        auto enumPhys = reinterpret_cast<PFN_vkEnumeratePhysicalDevices>(nextGIPA(*pInstance, "vkEnumeratePhysicalDevices"));
        if (enumPhys)
        {
            VkResult enumResult = enumPhys(*pInstance, &devCount, &physDev);
            if (enumResult != VK_SUCCESS)
            {
                LYMALINK_LOG("[VulkanOverlayLayer][Hook_vkCreateInstance] vkEnumeratePhysicalDevices failed result=" + std::to_string(enumResult));
            }
        }
        else
        {
            LYMALINK_LOG("[VulkanOverlayLayer][Hook_vkCreateInstance] next vkEnumeratePhysicalDevices missing.");
        }
    }

    // Store instance dispatch table keyed by the loader dispatch pointer
    {
        std::lock_guard<std::mutex> lock(s_instanceMtx);
        InstanceData data{};
        data.instance = *pInstance;
        data.physicalDevice = physDev;
        data.getProcAddr = nextGIPA;
        data.destroyInstance = reinterpret_cast<PFN_vkDestroyInstance>(nextGIPA(*pInstance, "vkDestroyInstance"));
        s_instances[DispatchKey(*pInstance)] = std::move(data);
    }

    return VK_SUCCESS;
}

/////////////////////////////////////////////////////////////////////

// Removes the instance from the tracking map then forwards to the real destroy
static VKAPI_ATTR void VKAPI_CALL Hook_vkDestroyInstance(VkInstance instance, const VkAllocationCallbacks* pAllocator)
{
    void* key = DispatchKey(instance);
    PFN_vkDestroyInstance fn = nullptr;

    {
        std::lock_guard<std::mutex> lock(s_instanceMtx);
        auto it = s_instances.find(key);
        if (it != s_instances.end())
        {
            fn = it->second.destroyInstance;
            s_instances.erase(it);
        }
    }

    if (fn)
    {
        fn(instance, pAllocator);
    }
}

/////////////////////////////////////////////////////////////////////

// Intercepts device creation to:
//   - advance the loader chain for the next layer/driver
//   - resolve and cache per-device function pointers
//   - locate the graphics queue that the overlay will submit on
//   - trigger one-time OverlayReceiver connection setup
static VKAPI_ATTR VkResult VKAPI_CALL Hook_vkCreateDevice(VkPhysicalDevice physicalDevice, const VkDeviceCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDevice* pDevice)
{
    // Walk pNext chain for the loader device link info
    auto* linkInfo = static_cast<VkLayerDeviceCreateInfo*>(const_cast<void*>(pCreateInfo->pNext));

    while (linkInfo && !(linkInfo->sType == VK_STRUCTURE_TYPE_LOADER_DEVICE_CREATE_INFO && linkInfo->function == VK_LAYER_LINK_INFO))
    {
        linkInfo = static_cast<VkLayerDeviceCreateInfo*>(const_cast<void*>(linkInfo->pNext));
    }

    if (!linkInfo)
    {
        LYMALINK_LOG("[VulkanOverlayLayer][Hook_vkCreateDevice] missing loader link info.");
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    // Save both proc-addr getters, then step the chain forward
    PFN_vkGetInstanceProcAddr nextGIPA = linkInfo->u.pLayerInfo->pfnNextGetInstanceProcAddr;
    PFN_vkGetDeviceProcAddr nextGDPA = linkInfo->u.pLayerInfo->pfnNextGetDeviceProcAddr;
    linkInfo->u.pLayerInfo = linkInfo->u.pLayerInfo->pNext;

    auto createDevice = reinterpret_cast<PFN_vkCreateDevice>(nextGIPA(VK_NULL_HANDLE, "vkCreateDevice"));
    if (!createDevice)
    {
        LYMALINK_LOG("[VulkanOverlayLayer][Hook_vkCreateDevice] next vkCreateDevice missing.");
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    // Forward the actual device creation to the next layer/driver
    VkResult result = createDevice(physicalDevice, pCreateInfo, pAllocator, pDevice);
    if (result != VK_SUCCESS)
    {
        LYMALINK_LOG("[VulkanOverlayLayer][Hook_vkCreateDevice] next vkCreateDevice failed result=" + std::to_string(result));
        return result;
    }

    // Pick up any tracked instance handle to resolve instance-level functions
    VkInstance inst = VK_NULL_HANDLE;
    {
        std::lock_guard<std::mutex> ilock(s_instanceMtx);
        for (auto& [k, idata] : s_instances)
        {
            inst = idata.instance;
            break;
        }
    }
    auto getQueueFamilyProps = reinterpret_cast<PFN_vkGetPhysicalDeviceQueueFamilyProperties>(nextGIPA(inst, "vkGetPhysicalDeviceQueueFamilyProperties"));
    if (!getQueueFamilyProps)
    {
        LYMALINK_LOG("[VulkanOverlayLayer][Hook_vkCreateDevice] getQueueFamilyProps null, skipping graphics family detection.");
    }
    // Determine which queue family handles graphics, index 0 used as fallback
    const uint32_t gfxFamily = getQueueFamilyProps ? FindGraphicsFamily(physicalDevice, getQueueFamilyProps) : 0;

    // Retrieve the VkQueue handle at queue index 0 within the graphics family
    VkQueue gfxQueue = VK_NULL_HANDLE;
    auto getDevQueue = reinterpret_cast<PFN_vkGetDeviceQueue>(nextGDPA(*pDevice, "vkGetDeviceQueue"));
    if (getDevQueue)
    {
        getDevQueue(*pDevice, gfxFamily, 0, &gfxQueue);
    }
    else
    {
        LYMALINK_LOG("[VulkanOverlayLayer][Hook_vkCreateDevice] next vkGetDeviceQueue missing.");
    }

    // Cache device dispatch table and queue info for use in later hooks
    {
        std::lock_guard<std::mutex> lock(s_deviceMtx);
        DeviceData data{};
        data.device = *pDevice;
        data.physicalDevice = physicalDevice;
        data.getProcAddr = nextGDPA;
        data.graphicsFamily = gfxFamily;
        data.graphicsQueue = gfxQueue;
        data.destroyDevice = reinterpret_cast<PFN_vkDestroyDevice>(nextGDPA(*pDevice, "vkDestroyDevice"));
        data.queuePresent = reinterpret_cast<PFN_vkQueuePresentKHR>(nextGDPA(*pDevice, "vkQueuePresentKHR"));
        data.getSwapchainImages = reinterpret_cast<PFN_vkGetSwapchainImagesKHR>(nextGDPA(*pDevice, "vkGetSwapchainImagesKHR"));
        data.createImageView = reinterpret_cast<PFN_vkCreateImageView>(nextGDPA(*pDevice, "vkCreateImageView"));
        data.destroyImageView = reinterpret_cast<PFN_vkDestroyImageView>(nextGDPA(*pDevice, "vkDestroyImageView"));
        s_devices.insert_or_assign(DispatchKey(*pDevice), std::move(data));
    }

    if (!gfxQueue)
    {
        LYMALINK_LOG("[VulkanOverlayLayer][Hook_vkCreateDevice] graphics queue is null for family=" + std::to_string(gfxFamily));
    }

    // Connect the overlay receiver exactly once, regardless of how many devices are created
    std::call_once(s_overlayInitFlag, []() {
        s_overlay.InitConnection();
    });

    return VK_SUCCESS;
}

/////////////////////////////////////////////////////////////////////

// Shuts down the overlay backend for this device before forwarding the real destroy, ensuring GPU resources are freed in the correct order
static VKAPI_ATTR void VKAPI_CALL Hook_vkDestroyDevice(VkDevice device, const VkAllocationCallbacks* pAllocator)
{
    void* key = DispatchKey(device);
    PFN_vkDestroyDevice fn = nullptr;

    {
        std::lock_guard<std::mutex> lock(s_deviceMtx);
        auto it = s_devices.find(key);
        if (it != s_devices.end())
        {
            // Tear down ImGui/Vulkan backend before the device is gone
            if (it->second.backend)
            {
                s_overlay.InvalidateVulkanResources();
                it->second.backend->Shutdown();
            }
            DestroySwapchainImageViews(it->second);
            fn = it->second.destroyDevice;
            s_devices.erase(it);
        }
    }

    if (fn)
    {
        fn(device, pAllocator);
    }
}

/////////////////////////////////////////////////////////////////////

// Called on swapchain creation or resize
// Rebuilds the overlay backend with fresh swapchain images, views, and dimensions so every present hook frame targets the correct render targets
static VKAPI_ATTR VkResult VKAPI_CALL Hook_vkCreateSwapchainKHR(VkDevice device, const VkSwapchainCreateInfoKHR* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSwapchainKHR* pSwapchain)
{
    void* key = DispatchKey(device);

    // Resolve next-layer's vkCreateSwapchainKHR via the stored device proc addr
    PFN_vkCreateSwapchainKHR createSwapchain = nullptr;
    {
        std::lock_guard<std::mutex> lock(s_deviceMtx);
        auto it = s_devices.find(key);
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

    // Let the driver create the real swapchain first
    VkResult result = createSwapchain(device, pCreateInfo, pAllocator, pSwapchain);
    if (result != VK_SUCCESS)
    {
        LYMALINK_LOG("[VulkanOverlayLayer][Hook_vkCreateSwapchainKHR] next vkCreateSwapchainKHR failed result=" + std::to_string(result));
        return result;
    }

    VulkanOverlayRenderer* backend = nullptr;
    VulkanOverlayRendererInitInfo backendInfo{};
    uint32_t swapchainWidth = 0;
    uint32_t swapchainHeight = 0;

    {
        std::lock_guard<std::mutex> lock(s_deviceMtx);

        auto it = s_devices.find(key);
        if (it == s_devices.end())
        {
            LYMALINK_LOG("[VulkanOverlayLayer][Hook_vkCreateSwapchainKHR] device not tracked after swapchain creation.");
            return result;
        }

        DeviceData& dev = it->second;
        backend = dev.backend.get();

        if (!dev.getSwapchainImages || !dev.createImageView)
        {
            LYMALINK_LOG("[VulkanOverlayLayer][Hook_vkCreateSwapchainKHR] required swapchain device functions missing.");
            return result;
        }

        // Two-call idiom: first get count, then fill the image array
        uint32_t imgCount = 0;
        VkResult imageResult = dev.getSwapchainImages(device, *pSwapchain, &imgCount, nullptr);
        if (imageResult != VK_SUCCESS)
        {
            LYMALINK_LOG("[VulkanOverlayLayer][Hook_vkCreateSwapchainKHR] vkGetSwapchainImagesKHR count failed result=" + std::to_string(imageResult));
            return result;
        }
        dev.swapchainImages.resize(imgCount);
        imageResult = dev.getSwapchainImages(device, *pSwapchain, &imgCount, dev.swapchainImages.data());
        if (imageResult != VK_SUCCESS)
        {
            LYMALINK_LOG("[VulkanOverlayLayer][Hook_vkCreateSwapchainKHR] vkGetSwapchainImagesKHR data failed result=" + std::to_string(imageResult));
            dev.swapchainImages.clear();
            return result;
        }

        s_overlay.InvalidateVulkanResources();
        if (backend)
        {
            backend->Shutdown();
        }
        DestroySwapchainImageViews(dev);

        // Cache format and dimensions for the present hook and ImGui DisplaySize
        dev.swapchainFormat = pCreateInfo->imageFormat;
        dev.swapchainWidth = pCreateInfo->imageExtent.width;
        dev.swapchainHeight = pCreateInfo->imageExtent.height;
        swapchainWidth = dev.swapchainWidth;
        swapchainHeight = dev.swapchainHeight;

        // Create a 2D color view for each swapchain image so the renderer can write into them
        dev.swapchainViews.resize(imgCount, VK_NULL_HANDLE);
        bool imageViewsReady = true;
        for (uint32_t i = 0; i < imgCount; ++i)
        {
            VkImageViewCreateInfo viewInfo{};
            viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            viewInfo.image = dev.swapchainImages[i];
            viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            viewInfo.format = dev.swapchainFormat;
            viewInfo.components = { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY };
            viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            viewInfo.subresourceRange.baseMipLevel = 0;
            viewInfo.subresourceRange.levelCount = 1;
            viewInfo.subresourceRange.baseArrayLayer = 0;
            viewInfo.subresourceRange.layerCount = 1;
            VkResult viewResult = dev.createImageView(device, &viewInfo, nullptr, &dev.swapchainViews[i]);
            if (viewResult != VK_SUCCESS)
            {
                LYMALINK_LOG("[VulkanOverlayLayer][Hook_vkCreateSwapchainKHR] vkCreateImageView failed image=" + std::to_string(i) + " result=" + std::to_string(viewResult));
                imageViewsReady = false;
            }
        }
        if (!imageViewsReady)
        {
            DestroySwapchainImageViews(dev);
            return result;
        }

        // Locate the parent instance by matching physicalDevice
        // Linear scan is acceptable because there is almost always only one instance per process
        VkInstance instance = VK_NULL_HANDLE;
        PFN_vkGetInstanceProcAddr getInstanceProcAddr = nullptr;
        {
            std::lock_guard<std::mutex> ilock(s_instanceMtx);
            for (auto& [k, inst] : s_instances)
            {
                if (inst.physicalDevice == dev.physicalDevice)
                {
                    instance = inst.instance;
                    getInstanceProcAddr = inst.getProcAddr;
                    break;
                }
            }
        }

        // Fill the backend init struct with everything the renderer needs
        backendInfo.instance = instance;
        backendInfo.physicalDevice = dev.physicalDevice;
        backendInfo.device = device;
        backendInfo.graphicsQueue = dev.graphicsQueue;
        backendInfo.getInstanceProcAddr = getInstanceProcAddr;
        backendInfo.getDeviceProcAddr = dev.getProcAddr;
        backendInfo.graphicsFamily = dev.graphicsFamily;
        backendInfo.swapchainFormat = dev.swapchainFormat;
        backendInfo.imageCount = imgCount;
        backendInfo.width = dev.swapchainWidth;
        backendInfo.height = dev.swapchainHeight;
        backendInfo.swapchainImages = dev.swapchainImages;
        backendInfo.swapchainViews = dev.swapchainViews;
    }

    // Initialize backend first: command pool and descriptor pool are created here
    const bool backendReady = backend && backend->Initialize(backendInfo);
    if (!backendReady)
    {
        LYMALINK_LOG("[VulkanOverlayLayer][Hook_vkCreateSwapchainKHR] overlay backend init failed for swapchain images=" + std::to_string(backendInfo.imageCount) + " size=" + std::to_string(swapchainWidth) + "x" + std::to_string(swapchainHeight));
        return result;
    }

    // Resolve vkGetPhysicalDeviceMemoryProperties from the matched instance GIPA
    PFN_vkGetPhysicalDeviceMemoryProperties getMemProps = nullptr;
    {
        std::lock_guard<std::mutex> ilock(s_instanceMtx);
        for (auto& [k, inst] : s_instances)
        {
            if (inst.physicalDevice == backendInfo.physicalDevice)
            {
                getMemProps = reinterpret_cast<PFN_vkGetPhysicalDeviceMemoryProperties>(
                    inst.getProcAddr(inst.instance, "vkGetPhysicalDeviceMemoryProperties"));
                break;
            }
        }
    }

    if (!getMemProps)
    {
        LYMALINK_LOG("[VulkanOverlayLayer][Hook_vkCreateSwapchainKHR] vkGetPhysicalDeviceMemoryProperties not resolved.");
    }

    // Feed runtime Vulkan handles to OverlayReceiver after backend init, when command pool is valid
    s_overlay.EnsureVulkanImGuiContext(
        backendInfo.device,
        backendInfo.physicalDevice,
        backendInfo.graphicsQueue,
        backendInfo.graphicsFamily,
        backend->GetCommandPool(),
        getMemProps);

    ImGui::GetIO().DisplaySize = ImVec2(static_cast<float>(swapchainWidth), static_cast<float>(swapchainHeight));

    return result;
}

/////////////////////////////////////////////////////////////////////

// Called every frame just before the image is shown on screen
// If the overlay backend is ready it composites the notification UI into the swapchain image and patches the present semaphores accordingly
static VKAPI_ATTR VkResult VKAPI_CALL Hook_vkQueuePresentKHR(VkQueue queue, const VkPresentInfoKHR* pPresentInfo)
{
    // Queue dispatch key may differ from device key, fall back to the sole device if not found
    void* key = DispatchKey(queue);

    PFN_vkQueuePresentKHR nextPresent = nullptr;
    VulkanOverlayRenderer* backend = nullptr;
    uint32_t w = 0, h = 0;

    {
        std::lock_guard<std::mutex> lock(s_deviceMtx);
        auto it = s_devices.find(key);
        if (it != s_devices.end())
        {
            nextPresent = it->second.queuePresent;
            backend = it->second.backend.get();
            w = it->second.swapchainWidth;
            h = it->second.swapchainHeight;
        }
        else if (s_devices.size() == 1)
        {
            // Single-device fallback: queue key wasn't indexed but there's only one device
            DeviceData& dev = s_devices.begin()->second;
            nextPresent = dev.queuePresent;
            backend = dev.backend.get();
            w = dev.swapchainWidth;
            h = dev.swapchainHeight;
        }
    }

    const VkPresentInfoKHR* presentInfoToSubmit = pPresentInfo;
    VkPresentInfoKHR overlayPresentInfo{};
    VkSemaphore overlayWaitSemaphore = VK_NULL_HANDLE;

    // Render overlay into the swapchain image before forwarding the present call
    if (backend && backend->IsReady())
    {
        ImGuiIO& io = ImGui::GetIO();
        io.DisplaySize = ImVec2(static_cast<float>(w), static_cast<float>(h));

        // Begin a new ImGui frame for this present
        ImGui_ImplVulkan_NewFrame();
        ImGui::NewFrame();

        // Poll shared memory / socket for new notifications and emit draw commands
        s_overlay.RenderNotificationFrame(w, h);

        // Finalise ImGui draw lists, then submit them to the swapchain image
        ImGui::Render();
        overlayWaitSemaphore = backend->RenderDrawData(queue, pPresentInfo, ImGui::GetDrawData());
        if (overlayWaitSemaphore != VK_NULL_HANDLE)
        {
            overlayPresentInfo = *pPresentInfo;
            overlayPresentInfo.waitSemaphoreCount = 1;
            overlayPresentInfo.pWaitSemaphores = &overlayWaitSemaphore;
            presentInfoToSubmit = &overlayPresentInfo;
        }
    }

    if (nextPresent)
    {
        return nextPresent(queue, presentInfoToSubmit);
    }

    LYMALINK_LOG("[VulkanOverlayLayer][Hook_vkQueuePresentKHR] next vkQueuePresentKHR missing.");

    return VK_ERROR_DEVICE_LOST;
}

/////////////////////////////////////////////////////////////////////

// Central dispatch table for this layer
// Returns our hook for any function we intercept, nullptr for everything else (caller then forwards to the next layer)
static PFN_vkVoidFunction GetLayerProc(const char* name)
{
    const std::string n(name);
    if (n == "vkCreateInstance")      return reinterpret_cast<PFN_vkVoidFunction>(Hook_vkCreateInstance);
    if (n == "vkDestroyInstance")     return reinterpret_cast<PFN_vkVoidFunction>(Hook_vkDestroyInstance);
    if (n == "vkCreateDevice")        return reinterpret_cast<PFN_vkVoidFunction>(Hook_vkCreateDevice);
    if (n == "vkDestroyDevice")       return reinterpret_cast<PFN_vkVoidFunction>(Hook_vkDestroyDevice);
    if (n == "vkCreateSwapchainKHR")  return reinterpret_cast<PFN_vkVoidFunction>(Hook_vkCreateSwapchainKHR);
    if (n == "vkQueuePresentKHR")     return reinterpret_cast<PFN_vkVoidFunction>(Hook_vkQueuePresentKHR);
    if (n == "vkGetInstanceProcAddr") return reinterpret_cast<PFN_vkVoidFunction>(LymalinkLayer_vkGetInstanceProcAddr);
    if (n == "vkGetDeviceProcAddr")   return reinterpret_cast<PFN_vkVoidFunction>(LymalinkLayer_vkGetDeviceProcAddr);
    if (n == "vkGetPhysicalDeviceProcAddr") return reinterpret_cast<PFN_vkVoidFunction>(LymalinkLayer_vkGetPhysicalDeviceProcAddr);

    return nullptr;
}

/////////////////////////////////////////////////////////////////////

// Looks up the next-layer GIPA stored for this instance and forwards the call
// Used by GetDeviceProcAddr and GetPhysicalDeviceProcAddr for non-hooked functions
static PFN_vkVoidFunction ForwardInstanceProc(VkInstance instance, const char* pName)
{
    std::lock_guard<std::mutex> lock(s_instanceMtx);
    auto it = s_instances.find(DispatchKey(instance));
    if (it != s_instances.end())
    {
        return it->second.getProcAddr(instance, pName);
    }

    return nullptr;
}

/////////////////////////////////////////////////////////////////////
// Exported entry points
/////////////////////////////////////////////////////////////////////

extern "C"
{

// Called by the loader during layer discovery
// Advertises interface version 2 and registers the three proc-addr entry points the loader needs
VKAPI_ATTR VkResult VKAPI_CALL LymalinkLayer_vkNegotiateLoaderLayerInterfaceVersion(VkNegotiateLayerInterface* pVersionStruct)
{
    if (pVersionStruct->loaderLayerInterfaceVersion >= 2)
    {
        pVersionStruct->loaderLayerInterfaceVersion = 2;
        pVersionStruct->pfnGetInstanceProcAddr = LymalinkLayer_vkGetInstanceProcAddr;
        pVersionStruct->pfnGetDeviceProcAddr = LymalinkLayer_vkGetDeviceProcAddr;
        pVersionStruct->pfnGetPhysicalDeviceProcAddr = LymalinkLayer_vkGetPhysicalDeviceProcAddr;
    }
    return VK_SUCCESS;
}

// Returns our hook if we intercept the function, otherwise forwards to the next layer via the stored instance proc addr
// Handles null instance gracefully
VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL LymalinkLayer_vkGetInstanceProcAddr(VkInstance  instance, const char* pName)
{
    if (!pName)
    {
        return nullptr;
    }

    // Return our own hook if this is a function we intercept
    if (PFN_vkVoidFunction fn = GetLayerProc(pName))
    {
        return fn;
    }

    if (instance == VK_NULL_HANDLE)
    {
        static bool s_loggedNullMiss = false;
        if (!s_loggedNullMiss)
        {
            s_loggedNullMiss = true;
        }
        return nullptr;
    }
    return ForwardInstanceProc(instance, pName);
}

// Returns our hook for intercepted device functions; forwards everything else
// to the next layer via the stored device proc addr
VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL LymalinkLayer_vkGetDeviceProcAddr(VkDevice device, const char* pName)
{
    if (!pName)
    {
        return nullptr;
    }

    // Return our own hook if this is a function we intercept
    if (PFN_vkVoidFunction fn = GetLayerProc(pName))
    {
        return fn;
    }

    if (device == VK_NULL_HANDLE)
    {
        return nullptr;
    }
        
    std::lock_guard<std::mutex> lock(s_deviceMtx);
    auto it = s_devices.find(DispatchKey(device));
    if (it != s_devices.end())
    {
        return it->second.getProcAddr(device, pName);
    }

    return nullptr;
}

// Physical-device functions are instance-scoped; just forward through the instance chain
VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL LymalinkLayer_vkGetPhysicalDeviceProcAddr(VkInstance instance, const char* pName)
{
    if (!pName)
    {
        return nullptr;
    }

    if (instance == VK_NULL_HANDLE)
    {
        return nullptr;
    }

    return ForwardInstanceProc(instance, pName);
}

} // extern "C"
