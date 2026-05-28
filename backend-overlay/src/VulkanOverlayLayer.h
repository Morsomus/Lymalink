/////////////////////////////////////////////////////////
// File: VulkanOverlayLayer.h
// Date: 2026-05-26
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Declares the Vulkan implicit layer that
//              intercepts vkQueuePresentKHR and delegates
//              to OverlayReceiver for rendering.
/////////////////////////////////////////////////////////

#pragma once

#include <vulkan/vk_layer.h>
#include <vulkan/vulkan.h>

#define LYMALINK_VK_EXPORT __attribute__((visibility("default")))

// Exported entry points required by the Vulkan loader.
// Must have C linkage so the loader can dlsym() them by name.
extern "C"
{
    LYMALINK_VK_EXPORT VKAPI_ATTR VkResult VKAPI_CALL LymalinkLayer_vkNegotiateLoaderLayerInterfaceVersion(VkNegotiateLayerInterface* pVersionStruct);
    LYMALINK_VK_EXPORT VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL LymalinkLayer_vkGetInstanceProcAddr(VkInstance instance, const char* pName);
    LYMALINK_VK_EXPORT VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL LymalinkLayer_vkGetDeviceProcAddr(VkDevice device, const char* pName);
    LYMALINK_VK_EXPORT VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL LymalinkLayer_vkGetPhysicalDeviceProcAddr(VkInstance instance, const char* pName);
}
