#pragma once
#include "../NX-FPS-Common.hpp"
#include <glad/vulkan.h>

namespace vk {
	inline PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr_0;
	inline PFN_vkGetDeviceProcAddr vkGetDeviceProcAddr_0;
	inline PFN_vkCmdSetViewport vkCmdSetViewport_0;
	inline PFN_vkCmdSetViewportWithCount vkCmdSetViewportWithCount_0;
	inline PFN_vkCmdSetScissor vkCmdSetScissor_0;
	inline PFN_vkCmdSetScissorWithCount vkCmdSetScissorWithCount_0;
	inline PFN_vkCreateSwapchainKHR vkCreateSwapchainKHR_0;
	inline PFN_vkGetSwapchainImagesKHR vkGetSwapchainImagesKHR_0;
	inline PFN_vkQueuePresentKHR vkQueuePresentKHR_0;
	inline PFN_vkCreateDevice vkCreateDevice_0;
	inline PFN_vkGetDeviceQueue vkGetDeviceQueue_0;
	inline PFN_vkDestroySwapchainKHR vkDestroySwapchainKHR_0;
	inline PFN_vkQueuePresentKHR nvSwapchainQueuePresentKHR_0;
	inline PFN_vkCreateSwapchainKHR nvSwapchainCreateSwapchainKHR_0;
	inline PFN_vkGetDeviceProcAddr nvSwapchainGetDeviceProcAddr_0;
	inline PFN_vkGetInstanceProcAddr nvSwapchainGetInstanceProcAddr_0;

	VkResult QueuePresent(VkQueue queue, const VkPresentInfoKHR* pPresentInfo);
	VkResult CreateSwapchain(VkDevice device, const VkSwapchainCreateInfoKHR* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSwapchainKHR* pSwapchain);
	PFN_vkVoidFunction GetDeviceProcAddr(VkDevice device, const char* pName);
	PFN_vkVoidFunction GetInstanceProcAddr(VkInstance instance, const char* pName);

	void CmdSetViewport(VkCommandBuffer commandBuffer, uint32_t firstViewport, uint32_t viewportCount, const VkViewport* pViewports);
	void CmdSetViewportWithCount(VkCommandBuffer commandBuffer, uint32_t viewportCount, const VkViewport* pViewports);
	void CmdSetScissor(VkCommandBuffer commandBuffer, uint32_t firstScissor, uint32_t scissorCount, const VkRect2D* pScissors);
	void CmdSetScissorWithCount(VkCommandBuffer commandBuffer, uint32_t scissorCount, const VkRect2D* pScissors);
	u32 LookupSymbol(uintptr_t* pOutAddress, const char* name);
	VkResult CreateDevice(VkPhysicalDevice physicalDevice, const VkDeviceCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDevice* pDevice);
	void GetDeviceQueue(VkDevice device, uint32_t queueFamilyIndex, uint32_t queueIndex, VkQueue* pQueue);
	void DestroySwapchain(VkDevice device, VkSwapchainKHR swapchain, const VkAllocationCallbacks* pAllocator);

	namespace Logo {
		extern bool done; // finished, failed, or disabled by nologo.flag
		// cond_check for the hooks that exist only for the logo (vkCreateDevice, vkGetDeviceQueue, vkDestroySwapchainKHR)
		inline void check(bool* out) { *out = !done; }
	}

	namespace nvSwapchain {
		VkResult QueuePresent(VkQueue queue, const VkPresentInfoKHR* pPresentInfo);
		VkResult CreateSwapchain(VkDevice device, const VkSwapchainCreateInfoKHR* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSwapchainKHR* pSwapchain);
		PFN_vkVoidFunction GetDeviceProcAddr(VkDevice device, const char* pName);
		PFN_vkVoidFunction GetInstanceProcAddr(VkInstance instance, const char* pName);
	}
}

void checkvkGetInstanceProcAddr(bool* out);
