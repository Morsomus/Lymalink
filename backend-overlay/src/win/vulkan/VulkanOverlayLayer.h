/////////////////////////////////////////////////////////
// File: VulkanOverlayLayer.h
// Date: 2026-06-27
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Declares Windows Vulkan implicit layer
//              exports for the overlay DLL.
/////////////////////////////////////////////////////////

#pragma once

#ifndef VK_NO_PROTOTYPES
    #define VK_NO_PROTOTYPES
#endif

#include <vulkan/vk_layer.h>
#include <vulkan/vulkan.h>

#define LYMALINK_WIN_VK_EXPORT extern "C" __declspec(dllexport)

LYMALINK_WIN_VK_EXPORT VKAPI_ATTR VkResult VKAPI_CALL LymalinkLayer_vkNegotiateLoaderLayerInterfaceVersion(VkNegotiateLayerInterface* version);
LYMALINK_WIN_VK_EXPORT VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL LymalinkLayer_vkGetInstanceProcAddr(VkInstance instance, const char* name);
LYMALINK_WIN_VK_EXPORT VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL LymalinkLayer_vkGetDeviceProcAddr(VkDevice device, const char* name);
LYMALINK_WIN_VK_EXPORT VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL LymalinkLayer_vkGetPhysicalDeviceProcAddr(VkInstance instance, const char* name);
