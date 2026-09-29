#include "NX-FPS-Common.hpp"
#include "api/NVN.hpp"
#include "api/EGL.hpp"
#include "api/Vulkan.hpp"

static ptrdiff_t SharedMemoryOffset = 1234;

extern "C" {

	void NX_FPS(SharedMemory* _sharedmemory, uint32_t* _sharedOperationMode) {

		sharedOperationMode = _sharedOperationMode;
		SaltySDCore_printf("NX-FPS: alive\n");
		NVN::Logo::LoadShaderFiles();
		LOCK::patcher.bindMainRegion(Utils::getMainAddress());
		SaltySDCore_printf("NX-FPS: found main at: 0x%lX\n", LOCK::patcher.mainRegion());
		Result ret = SaltySD_CheckIfSharedMemoryAvailable(&SharedMemoryOffset, sizeof(NxFpsSharedBlock));
		SaltySDCore_printf("NX-FPS: ret: 0x%X\n", ret);
		if (!ret) {
			SaltySDCore_printf("NX-FPS: MemoryOffset: %d\n", SharedMemoryOffset);

			Shared = (NxFpsSharedBlock*)__builtin_assume_aligned((const void*)((uintptr_t)shmemGetAddr(_sharedmemory) + SharedMemoryOffset), 4);
			Shared -> MAGIC = 0x465053;
			Shared->expectedSetBuffers = -1;

			//Putting this inside doesn't generate bloat in .init_array
			std::array replacements = {
				runtime_replace{"nvnBootstrapLoader", (uintptr_t*)&NVN::nvnBootstrapLoader_0, (void*)NVN::BootstrapLoader_1, nullptr},
				runtime_replace{"eglGetProcAddress", (uintptr_t*)&EGL::eglGetProcAddress_0, (void*)EGL::GetProc, nullptr},
				runtime_replace{"eglSwapBuffers", (uintptr_t*)&EGL::eglSwapBuffers_0, (void*)EGL::Swap, nullptr},
				runtime_replace{"eglSwapInterval", (uintptr_t*)&EGL::eglSwapInterval_0, (void*)EGL::Interval, nullptr},
				runtime_replace{"glViewport", (uintptr_t*)&EGL::glViewport_0, (void*)EGL::Viewport, nullptr},
				runtime_replace{"glViewportArrayv", (uintptr_t*)&EGL::glViewportArrayv_0, (void*)EGL::ViewportArrayv, nullptr},
				runtime_replace{"glViewportArrayvNV", (uintptr_t*)&EGL::glViewportArrayvNV_0, (void*)EGL::ViewportArrayvNV, nullptr},
				runtime_replace{"glViewportArrayvOES", (uintptr_t*)&EGL::glViewportArrayvOES_0, (void*)EGL::ViewportArrayvOES, nullptr},
				runtime_replace{"glViewportIndexedf", (uintptr_t*)&EGL::glViewportIndexedf_0, (void*)EGL::ViewportIndexedf, nullptr},
				runtime_replace{"glViewportIndexedfNV", (uintptr_t*)&EGL::glViewportIndexedfNV_0, (void*)EGL::ViewportIndexedfNV, nullptr},
				runtime_replace{"glViewportIndexedfOES", (uintptr_t*)&EGL::glViewportIndexedfOES_0, (void*)EGL::ViewportIndexedfOES, nullptr},
				runtime_replace{"glViewportIndexedfv", (uintptr_t*)&EGL::glViewportIndexedfv_0, (void*)EGL::ViewportIndexedfv, nullptr},
				runtime_replace{"glViewportIndexedfvNV", (uintptr_t*)&EGL::glViewportIndexedfvNV_0, (void*)EGL::ViewportIndexedfvNV, nullptr},
				runtime_replace{"glViewportIndexedfvOES", (uintptr_t*)&EGL::glViewportIndexedfvOES_0, (void*)EGL::ViewportIndexedfvOES, nullptr},
				
				runtime_replace{"vkQueuePresentKHR", (uintptr_t*)&vk::vkQueuePresentKHR_0, (void*)vk::QueuePresent, nullptr},
				runtime_replace{"_ZN11NvSwapchain15QueuePresentKHREP9VkQueue_TPK16VkPresentInfoKHR", (uintptr_t*)&vk::nvSwapchainQueuePresentKHR_0, (void*)vk::nvSwapchain::QueuePresent, nullptr},
				runtime_replace{"vkGetInstanceProcAddr", (uintptr_t*)&vk::vkGetInstanceProcAddr_0, (void*)vk::GetInstanceProcAddr, nullptr},
				runtime_replace{"vkCmdSetViewport", (uintptr_t*)&vk::vkCmdSetViewport_0, (void*)vk::CmdSetViewport, nullptr},
				runtime_replace{"vkCreateSwapchainKHR", (uintptr_t*)&vk::vkCreateSwapchainKHR_0, (void*)vk::CreateSwapchain, nullptr},
				runtime_replace{"vkGetDeviceProcAddr", (uintptr_t*)&vk::vkGetDeviceProcAddr_0, (void*)vk::GetDeviceProcAddr, nullptr},
				runtime_replace{"vkCmdSetViewportWithCount", (uintptr_t*)&vk::vkCmdSetViewportWithCount_0, (void*)vk::CmdSetViewportWithCount, nullptr},
				runtime_replace{"vkGetSwapchainImagesKHR", (uintptr_t*)&vk::vkGetSwapchainImagesKHR_0, nullptr, nullptr},
				runtime_replace{"vkCreateDevice", (uintptr_t*)&vk::vkCreateDevice_0, (void*)vk::CreateDevice, nullptr},
				runtime_replace{"vkGetDeviceQueue", (uintptr_t*)&vk::vkGetDeviceQueue_0, (void*)vk::GetDeviceQueue, nullptr},
				runtime_replace{"vkDestroySwapchainKHR", (uintptr_t*)&vk::vkDestroySwapchainKHR_0, (void*)vk::DestroySwapchain, nullptr},
				runtime_replace{"_ZN11NvSwapchain18CreateSwapchainKHREP10VkDevice_TPK24VkSwapchainCreateInfoKHRPK21VkAllocationCallbacksPP16VkSwapchainKHR_T", (uintptr_t*)&vk::nvSwapchainCreateSwapchainKHR_0, nullptr, nullptr},

				runtime_replace{"_ZN2nn2oe20SetFocusHandlingModeENS0_17FocusHandlingModeE", (uintptr_t*)&nn::SetFocusHandlingMode_0, (void*)nn::setFocusHandlingMode, nullptr},
				runtime_replace{"_ZN2nn2oe44SetUserInactivityDetectionTimeExtendedUnsafeEb", (uintptr_t*)&nn::SetUserInactivityDetectionTimeExtended_0, nullptr, nullptr},
				runtime_replace{
					#if defined(SWITCH32) || defined(OUNCE32)
					"_ZN2nn2ro12LookupSymbolEPjPKc",
					#else
					"_ZN2nn2ro12LookupSymbolEPmPKc",
					#endif
					(uintptr_t*)&nn::roLookupSymbol_0, (void*)vk::LookupSymbol, &checkvkGetInstanceProcAddr},
				runtime_replace{
					#if defined(SWITCH32) || defined(OUNCE32)
					"_ZN2nn2fs6detail12FileAccessor4ReadEPjxPvjRKNS0_10ReadOptionE",
					#else
					"_ZN2nn2fs6detail12FileAccessor4ReadEPmlPvmRKNS0_10ReadOptionE",
					#endif
					(uintptr_t*)&nn::FileAccessorRead_0, (void*)nn::FileAccessorRead, &checkReadFlag},
				runtime_replace{
					#if defined(SWITCH32) || defined(OUNCE32)
					"_ZN2nn2fs6detail12FileAccessor4ReadEPjxPvjRKNS0_10ReadOptionEPNS1_25FileDataCacheAccessResultE",
					#else
					"_ZN2nn2fs6detail12FileAccessor4ReadEPmlPvmRKNS0_10ReadOptionEPNS1_25FileDataCacheAccessResultE",
					#endif
					(uintptr_t*)&nn::FileAccessorReadCache_0, (void*)nn::FileAccessorReadCache, &checkReadFlag}
			};

			for (const auto& replacement: replacements) {
				if (replacement.orig_ptr) *replacement.orig_ptr = SaltySDCore_FindSymbolBuiltin(replacement.name);
				if (replacement.hook_ptr) {
					if (replacement.cond_check) {
						bool check = false;
						replacement.cond_check(&check);
						if (check == true) SaltySDCore_ReplaceImport(replacement.name, replacement.hook_ptr);
					}
					else SaltySDCore_ReplaceImport(replacement.name, replacement.hook_ptr);
				}
			}

			uint64_t titleid = 0;
			svcGetInfo(&titleid, InfoType_ProgramId, CUR_PROCESS_HANDLE, 0);
			char path[128];

			#if defined(SWITCH32) || defined(OUNCE32)
			npf_snprintf(path, sizeof(path), "sdmc:/SaltySD/triple_buffer/%016llX.flag", titleid);
			#else
			npf_snprintf(path, sizeof(path), "sdmc:/SaltySD/triple_buffer/%016lX.flag", titleid);
			#endif
			FILE* tb_file = SaltySDCore_fopen(path, "rb");
			if (tb_file) {
				SaltySDCore_fclose(tb_file);
				// Must be done before game starts, otherwise game can take whole available heap.
				NVN::TripleBuffer::requested = SaltySDCore_ReserveMemory(NVN::TripleBuffer::RESERVED_MEMORY_SIZE);
				SaltySDCore_printf("NX-FPS: TripleBuffer: memory reservation requested: %d\n", NVN::TripleBuffer::requested);
			}

			FILE* nvn_file = SaltySDCore_fopen("sdmc:/SaltySD/flags/nvncounters.flag", "rb");
			if  (nvn_file) {
				SaltySDCore_fclose(nvn_file);
				NVN::enableCounters = true;
			}

			#if defined(SWITCH32) || defined(OUNCE32)
			npf_snprintf(path, sizeof(path), "sdmc:/SaltySD/plugins/FPSLocker/%016llX.dat", titleid);
			#else
			npf_snprintf(path, sizeof(path), "sdmc:/SaltySD/plugins/FPSLocker/%016lX.dat", titleid);
			#endif
			FILE* file_dat = SaltySDCore_fopen(path, "rb");
			if (file_dat) {
				uint8_t temp = 0;
				SaltySDCore_fread(&temp, 1, 1, file_dat);
				(Shared -> FPSlocked) = temp;
				(Shared -> FPSlockedDocked) = temp;
				if (temp >= 40 && temp <= 120) {
					FILE* sync_file = SaltySDCore_fopen("sdmc:/SaltySD/flags/displaysync.flag", "rb");
					if  (sync_file) {
						SaltySDCore_fclose(sync_file);
						SaltySD_SetDisplaySync(true);
						Shared->displaySync.ds.handheld = true;
					}
					else SaltySD_SetDisplaySync(false);
					sync_file = SaltySDCore_fopen("sdmc:/SaltySD/flags/displaysyncdocked.flag", "rb");
					if  (sync_file) {
						SaltySDCore_fclose(sync_file);
						SaltySD_SetDisplaySyncDocked(true);
						Shared->displaySync.ds.docked = true;
					}
					else SaltySD_SetDisplaySyncDocked(false);
				}
				SaltySDCore_fread(&temp, 1, 1, file_dat);
				(Shared -> ZeroSync) = temp;
				if (SaltySDCore_fread(&temp, 1, 1, file_dat))
					(Shared -> SetBuffers) = temp;
				if (SaltySDCore_fread(&temp, 1, 1, file_dat))
					(Shared -> forceSuspend) = (bool)temp;
				if (SaltySDCore_fread(&temp, 1, 1, file_dat))
					(Shared -> FPSlockedDocked) = temp;
				SaltySDCore_fclose(file_dat);
			}

			u64 buildid = SaltySD_GetBID();
			if (!buildid) {
				SaltySDCore_printf("NX-FPS: getBID failed! Err: 0x%x\n", ret);
			}
			else {
				#if defined(SWITCH32) || defined(OUNCE32)
				npf_snprintf(path, sizeof(path), "sdmc:/SaltySD/plugins/FPSLocker/patches/%016llX/%016llX.bin", titleid, buildid);
				SaltySDCore_printf("NX-FPS: BID: %016llX\n", buildid);
				#else
				npf_snprintf(path, sizeof(path), "sdmc:/SaltySD/plugins/FPSLocker/patches/%016lX/%016lX.bin", titleid, buildid);
				SaltySDCore_printf("NX-FPS: BID: %016lX\n", buildid);
				#endif
				FILE* patch_file = SaltySDCore_fopen(path, "rb");
				if (patch_file) {
					SaltySDCore_fclose(patch_file);
					SaltySDCore_printf("NX-FPS: FPSLocker: successfully opened: %s\n", path);
					configRC = LOCK::patcher.loadFromFile(path);
					SaltySDCore_printf("NX-FPS: FPSLocker: readConfig rc: 0x%x\n", configRC);
					if (R_SUCCEEDED(configRC)) {
						if (LOCK::patcher.masterWriteApplied()) {
							(Shared -> patchApplied) = 2;
						}
						uint64_t alias_start = 0;
						uint64_t heap_start = 0;
						svcGetInfo(&alias_start, InfoType_AliasRegionAddress, CUR_PROCESS_HANDLE, 0);
						svcGetInfo(&heap_start, InfoType_HeapRegionAddress, CUR_PROCESS_HANDLE, 0);
						// Game's heap is moved by memory reserved for Core, keep heap offsets same as without it.
						size_t reserved_size = 0;
						SaltySDCore_GetReservedMemory(&reserved_size);
						heap_start += reserved_size;

						LOCK::patcher.bindDynamicRegions((uintptr_t)alias_start, (uintptr_t)heap_start);
					}

				}
				else SaltySDCore_printf("NX-FPS: FPSLocker: File not found: %s\n", path);
			}
		}
		SaltySDCore_printf("NX-FPS: injection finished\n");
	}
}
