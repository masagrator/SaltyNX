#include "Vulkan.hpp"
#include "LogoVulkan.hpp"

namespace vk {

	void CmdSetViewport(VkCommandBuffer commandBuffer, uint32_t firstViewport, uint32_t viewportCount, const VkViewport* pViewports) {
		if (resolutionLookup) for (uint32_t i = 0; i < viewportCount; i++) {
			const VkViewport& viewport = pViewports[i];
			if (viewport.height > 1.f && viewport.width > 1.f && viewport.x == 0.f && viewport.y == 0.f) {
				uint32_t width = (uint32_t)viewport.width;
				uint32_t height = (uint32_t)viewport.height;
				NX_FPS_Math::addResToViewports(width, height);
				last_viewport = {width, height};
			}
		}
		return vkCmdSetViewport_0(commandBuffer, firstViewport, viewportCount, pViewports);
	}

	void CmdSetViewportWithCount(VkCommandBuffer commandBuffer, uint32_t viewportCount, const VkViewport* pViewports) {
		if (resolutionLookup) for (uint32_t i = 0; i < viewportCount; i++) {
			const VkViewport& viewport = pViewports[i];
			if (viewport.height > 1.f && viewport.width > 1.f && viewport.x == 0.f && viewport.y == 0.f) {
				uint32_t width = (uint32_t)viewport.width;
				uint32_t height = (uint32_t)viewport.height;
				NX_FPS_Math::addResToViewports(width, height);
				last_viewport = {width, height};
			}
		}
		return vkCmdSetViewportWithCount_0(commandBuffer, viewportCount, pViewports);
	}

	void CmdSetScissor(VkCommandBuffer commandBuffer, uint32_t firstScissor, uint32_t scissorCount, const VkRect2D* pScissors) {
		if (resolutionLookup) for (uint32_t i = 0; i < scissorCount; i++) {
			const VkRect2D& scissor = pScissors[i];
			if (scissor.extent.height > 1 && scissor.extent.width > 1 && scissor.offset.x == 0 && scissor.offset.y == 0 &&
			    scissor.extent.width != last_viewport.first && scissor.extent.height != last_viewport.second) {
				NX_FPS_Math::addResToViewports(scissor.extent.width, scissor.extent.height);
			}
		}
		return vkCmdSetScissor_0(commandBuffer, firstScissor, scissorCount, pScissors);
	}

	void CmdSetScissorWithCount(VkCommandBuffer commandBuffer, uint32_t scissorCount, const VkRect2D* pScissors) {
		if (resolutionLookup) for (uint32_t i = 0; i < scissorCount; i++) {
			const VkRect2D& scissor = pScissors[i];
			if (scissor.extent.height > 1 && scissor.extent.width > 1 && scissor.offset.x == 0 && scissor.offset.y == 0 &&
			    scissor.extent.width != last_viewport.first && scissor.extent.height != last_viewport.second) {
				NX_FPS_Math::addResToViewports(scissor.extent.width, scissor.extent.height);
			}
		}
		return vkCmdSetScissorWithCount_0(commandBuffer, scissorCount, pScissors);
	}

	namespace Logo {
		bool done = false;
		uint64_t startTick = 0;
		uint64_t endTick = 0;
		VkInstance lastInstance = VK_NULL_HANDLE;

		void* ResolveDevice(VkDevice device, const char* name) {
			void* address = vkGetDeviceProcAddr_0 ? (void*)vkGetDeviceProcAddr_0(device, name) : nullptr;
			if (!address) address = (void*)SaltySDCore_FindSymbolBuiltin(name);
			return address;
		}

		void* ResolveInstance(const char* name) {
			void* address = (void*)SaltySDCore_FindSymbolBuiltin(name);
			if (!address && vkGetInstanceProcAddr_0 && lastInstance) address = (void*)vkGetInstanceProcAddr_0(lastInstance, name);
			return address;
		}

		const VkPresentInfoKHR* Draw(VkQueue queue, const VkPresentInfoKHR* info, VkPresentInfoKHR* patched, VkSemaphore* semaphore) {
			const uint64_t now = Utils::_getSystemTick();
			if (endTick == 0) {
				startTick = now;
				endTick = now + systemtickfrequency * ::Logo::DURATION_SECONDS;
			}
			if (now >= endTick) {
				LogoVK::Release();
				done = true;
				return info;
			}
			const float time = (float)((double)(now - startTick) / (double)systemtickfrequency);
			for (uint32_t i = 0; i < info->swapchainCount; i++) {
				*semaphore = LogoVK::Draw(queue, info->pSwapchains[i], info->pImageIndices[i], info->waitSemaphoreCount, info->pWaitSemaphores, time);
				if (!*semaphore) continue;
				*patched = *info;
				patched->waitSemaphoreCount = 1;
				patched->pWaitSemaphores = semaphore;
				return patched;
			}
			return info;
		}
	}

	VkResult CreateDevice(VkPhysicalDevice physicalDevice, const VkDeviceCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDevice* pDevice) {
		const VkResult result = vkCreateDevice_0(physicalDevice, pCreateInfo, pAllocator, pDevice);
		if (result >= 0 && !Logo::done)
			LogoVK::OnDeviceCreated(physicalDevice, *pDevice, Logo::ResolveDevice, Logo::ResolveInstance);
		return result;
	}

	void GetDeviceQueue(VkDevice device, uint32_t queueFamilyIndex, uint32_t queueIndex, VkQueue* pQueue) {
		vkGetDeviceQueue_0(device, queueFamilyIndex, queueIndex, pQueue);
		if (!Logo::done) LogoVK::OnDeviceQueue(device, queueFamilyIndex, *pQueue);
	}

	void DestroySwapchain(VkDevice device, VkSwapchainKHR swapchain, const VkAllocationCallbacks* pAllocator) {
		// Our image views of its images have to go first.
		LogoVK::OnSwapchainDestroyed(device, swapchain);
		vkDestroySwapchainKHR_0(device, swapchain, pAllocator);
	}

	namespace Common {
		NOINLINE VkResult QueuePresent(VkQueue queue, const VkPresentInfoKHR* pPresentInfo, PFN_vkQueuePresentKHR pointer) {

			static bool check_redirection = false;
			//Fix for games in which subsdk redirects internally vkQueuePresentKHR to nv::Swapchain
			if (check_redirection == true) {
				return pointer(queue, pPresentInfo);
			}
			if (NX_FPS_Math::starttick == 0) [[unlikely]] {
				(Shared -> API) = 3;
				NX_FPS_Math::starttick = Utils::_getSystemTick();
				NX_FPS_Math::starttick2 = NX_FPS_Math::starttick;
			}

			NX_FPS_Math::PreFrame();
			VkPresentInfoKHR patchedInfo;
			VkSemaphore logoSemaphore = VK_NULL_HANDLE;
			if (!Logo::done) pPresentInfo = Logo::Draw(queue, pPresentInfo, &patchedInfo, &logoSemaphore);
			check_redirection = true;
			const VkResult vulkanResult = pointer(queue, pPresentInfo);
			check_redirection = false;
			if (vulkanResult >= 0) NX_FPS_Math::PostFrame();

			if (!NX_FPS_Math::new_fpslock) {
				NX_FPS_Math::FPStiming = 0;
				NX_FPS_Math::FPSlock = 0;
				changeFPS = false;
			}
			else {
				changeFPS = true;
				NX_FPS_Math::FPSlock = ((*sharedOperationMode == 1) ? (Shared -> FPSlockedDocked) : (Shared -> FPSlocked));
				if (NX_FPS_Math::new_fpslock != ((Shared -> currentRefreshRate) ? (Shared -> currentRefreshRate) : 60))
					NX_FPS_Math::FPStiming = (systemtickfrequency/NX_FPS_Math::new_fpslock) - 6000;
				else NX_FPS_Math::FPStiming = 0;
			}

			return vulkanResult;
		}

		NOINLINE VkResult CreateSwapchain(VkDevice device, const VkSwapchainCreateInfoKHR* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSwapchainKHR* pSwapchain, PFN_vkCreateSwapchainKHR pointer) {
			VkSwapchainCreateInfoKHR* m_createInfo = (VkSwapchainCreateInfoKHR*)pCreateInfo;
			if ((Shared -> SetBuffers) > 0) {
				m_createInfo->minImageCount = (Shared -> SetBuffers);
			}
			// The logo copies the area under the text out of the swapchain image.
			if (!Logo::done) m_createInfo->imageUsage |= LogoVK::ExtraSwapchainUsage(device, m_createInfo->surface);
			VkResult vulkanResult = pointer(device, pCreateInfo, pAllocator, pSwapchain);
			if (vulkanResult >= 0) {
				uint32_t numBuffers = 0;
				vkGetSwapchainImagesKHR_0(device, *pSwapchain, &numBuffers, nullptr);
				(Shared -> Buffers) = numBuffers;
				if (!Logo::done) {
					VkImage images[8]{};
					uint32_t count = numBuffers;
					if (count <= 8 && vkGetSwapchainImagesKHR_0(device, *pSwapchain, &count, images) >= 0)
						LogoVK::OnSwapchainCreated(device, m_createInfo, *pSwapchain, count, images);
				}
			}
			return vulkanResult;
		}

		std::array vk_replacements = {
			runtime_replace{"vkQueuePresentKHR", (uintptr_t*)&vkQueuePresentKHR_0, (void*)vk::QueuePresent},
			runtime_replace{"vkGetDeviceProcAddr", (uintptr_t*)&vkGetDeviceProcAddr_0, (void*)vk::GetDeviceProcAddr},
			runtime_replace{"vkCmdSetViewport", (uintptr_t*)&vkCmdSetViewport_0, (void*)CmdSetViewport},
			runtime_replace{"vkCmdSetViewportWithCount", (uintptr_t*)&vkCmdSetViewportWithCount_0, (void*)CmdSetViewportWithCount},
			runtime_replace{"vkCmdSetScissor", (uintptr_t*)&vkCmdSetScissor_0, (void*)CmdSetScissor},
			runtime_replace{"vkCmdSetScissorWithCount", (uintptr_t*)&vkCmdSetScissorWithCount_0, (void*)CmdSetScissorWithCount},
			runtime_replace{"vkCreateSwapchainKHR", (uintptr_t*)&vkCreateSwapchainKHR_0, (void*)vk::CreateSwapchain},
			runtime_replace{"vkGetSwapchainImagesKHR", (uintptr_t*)&vkGetSwapchainImagesKHR_0},
			runtime_replace{"vkCreateDevice", (uintptr_t*)&vkCreateDevice_0, (void*)vk::CreateDevice},
			runtime_replace{"vkGetDeviceQueue", (uintptr_t*)&vkGetDeviceQueue_0, (void*)vk::GetDeviceQueue},
			runtime_replace{"vkDestroySwapchainKHR", (uintptr_t*)&vkDestroySwapchainKHR_0, (void*)vk::DestroySwapchain}
		};

		NOINLINE PFN_vkVoidFunction GetDeviceProcAddr(VkDevice device, const char* pName, PFN_vkGetDeviceProcAddr pointer) {
			uintptr_t address = (uintptr_t)pointer(device, pName);
			if (!strcmp("vkGetDeviceProcAddr", pName)) {
				if (!vkGetDeviceProcAddr_0) {
					memcpy(&vkGetDeviceProcAddr_0, &address, sizeof(address));
				}
				return (PFN_vkVoidFunction)pointer;
			}

			for (const auto& replacement : vk_replacements) {
				if (!strcmp(replacement.name, pName)) {
					if (replacement.orig_ptr && *replacement.orig_ptr == 0) *replacement.orig_ptr = address;
					if (replacement.hook_ptr) return (PFN_vkVoidFunction)replacement.hook_ptr;
					break;
				}
			}
			return (PFN_vkVoidFunction)address;
		}

		NOINLINE PFN_vkVoidFunction GetInstanceProcAddr(VkInstance instance, const char* pName, PFN_vkGetInstanceProcAddr pointer) {
			uintptr_t address = (uintptr_t)pointer(instance, pName);
			if (instance) Logo::lastInstance = instance;

			for (const auto& replacement : vk_replacements) {
				if (!strcmp(replacement.name, pName)) {
					if (replacement.orig_ptr && *replacement.orig_ptr == 0) *replacement.orig_ptr = address;
					if (replacement.hook_ptr) return (PFN_vkVoidFunction)replacement.hook_ptr;
					break;
				}
			}
			return (PFN_vkVoidFunction)address;
		}
	}

	namespace nvSwapchain {
		VkResult QueuePresent(VkQueue queue, const VkPresentInfoKHR* pPresentInfo) {
			return vk::Common::QueuePresent(queue, pPresentInfo, nvSwapchainQueuePresentKHR_0);
		}

		VkResult CreateSwapchain(VkDevice device, const VkSwapchainCreateInfoKHR* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSwapchainKHR* pSwapchain) {
			return vk::Common::CreateSwapchain(device, pCreateInfo, pAllocator, pSwapchain, nvSwapchainCreateSwapchainKHR_0);
		}

		PFN_vkVoidFunction GetDeviceProcAddr(VkDevice device, const char* pName) {
			return vk::Common::GetDeviceProcAddr(device, pName, nvSwapchainGetDeviceProcAddr_0);
		}

		PFN_vkVoidFunction GetInstanceProcAddr(VkInstance instance, const char* pName) {
			return vk::Common::GetInstanceProcAddr(instance, pName, nvSwapchainGetInstanceProcAddr_0);
		}
	}

	VkResult QueuePresent(VkQueue queue, const VkPresentInfoKHR* pPresentInfo) {
		return vk::Common::QueuePresent(queue, pPresentInfo, vkQueuePresentKHR_0);
	}

	VkResult CreateSwapchain(VkDevice device, const VkSwapchainCreateInfoKHR* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSwapchainKHR* pSwapchain) {
		return vk::Common::CreateSwapchain(device, pCreateInfo, pAllocator, pSwapchain, vkCreateSwapchainKHR_0);
	}

	PFN_vkVoidFunction GetDeviceProcAddr(VkDevice device, const char* pName) {
		return vk::Common::GetDeviceProcAddr(device, pName, vkGetDeviceProcAddr_0);
	}

	PFN_vkVoidFunction GetInstanceProcAddr(VkInstance instance, const char* pName) {
		return vk::Common::GetInstanceProcAddr(instance, pName, vkGetInstanceProcAddr_0);
	}

	u32 LookupSymbol(uintptr_t* pOutAddress, const char* name) {
		if (!strcmp("vkGetInstanceProcAddr", name)) {
			if (!vkGetInstanceProcAddr_0)
				nn::roLookupSymbol_0((uintptr_t*)&vkGetInstanceProcAddr_0, name);
			*pOutAddress = (uintptr_t)&GetInstanceProcAddr;
			return 0;
		}
		if (!strcmp("vkGetDeviceProcAddr", name)) {
			if (!vkGetDeviceProcAddr_0)
				nn::roLookupSymbol_0((uintptr_t*)&vkGetDeviceProcAddr_0, name);
			*pOutAddress = (uintptr_t)&GetDeviceProcAddr;
			return 0;
		}
		return nn::roLookupSymbol_0(pOutAddress, name);
	}
}

void checkvkGetInstanceProcAddr(bool* out) {
	if (vk::vkGetInstanceProcAddr_0) *out = true;
	else *out = false;
}
