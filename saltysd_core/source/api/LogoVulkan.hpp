#pragma once
#include "Logo.hpp"
#include <glad/vulkan.h>

namespace LogoVK {
	typedef void* (*DeviceResolver)(VkDevice device, const char* name);
	typedef void* (*InstanceResolver)(const char* name);

	void OnDeviceCreated(VkPhysicalDevice physicalDevice, VkDevice device, DeviceResolver deviceResolve, InstanceResolver instanceResolve);
	void OnDeviceQueue(VkDevice device, uint32_t queueFamily, VkQueue queue);

	// Usage bits to OR into the swapchain's imageUsage (0 when the surface doesn't support them).
	VkImageUsageFlags ExtraSwapchainUsage(VkDevice device, VkSurfaceKHR surface);
	void OnSwapchainCreated(VkDevice device, const VkSwapchainCreateInfoKHR* info, VkSwapchainKHR swapchain,
	                        uint32_t imageCount, const VkImage* images);
	void OnSwapchainDestroyed(VkDevice device, VkSwapchainKHR swapchain);
	// Visible part of the swapchain image set by the game with nn::vi::SetLayerCrop, width/height 0 = whole image.
	void SetCrop(int x, int y, int width, int height);

	VkSemaphore Draw(VkQueue queue, VkSwapchainKHR swapchain, uint32_t imageIndex,
	                 uint32_t waitCount, const VkSemaphore* waits, float time);

	// Waits for our work and destroys every object.
	void Release();
}