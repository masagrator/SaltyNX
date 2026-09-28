#if defined(SWITCH32)
#include <switch_min.h>
#define InfoType_ProgramId InfoType_TitleId
#elif defined(SWITCH)
#include <switch.h>
#else
#error "Unsupported base architecture!"
#endif
#include <arm_neon.h>
#include "saltysd_ipc.h"
#include "saltysd_dynamic.h"
#include "saltysd_core.h"
#include "lock.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <utility>
#include "nanoprintf.h"

namespace LOCK {
	constinit LOCK::Patcher patcher;
}

struct runtime_replace {
	const char* name;
	uintptr_t* orig_ptr;
	void* hook_ptr;
	void (*cond_check)(bool* check);
};

ptrdiff_t SharedMemoryOffset = 1234;
Result configRC = 1;

static uint32_t* sharedOperationMode = 0;

struct resolutionCalls {
	uint16_t width;
	uint16_t height;
	uint16_t calls;
};

bool resolutionLookup = false;
bool setNumActiveTexturesDetected = false;
uint8_t amountOfAvailableBuffers = 0;

resolutionCalls m_resolutionRenderCalls[8] = {0};
resolutionCalls m_resolutionViewportCalls[8] = {0};

struct NxFpsSharedBlock {
	uint32_t MAGIC; //0x465053 "\x00FPS" 
	uint8_t FPS;
	float FPSavg;
	bool pluginActive;
	uint8_t FPSlocked;
	uint8_t FPSmode;
	uint8_t ZeroSync;
	uint8_t patchApplied;
	uint8_t API; //1 - NVN, 2 - EGL, 3 - Vulkan
	uint32_t FPSticks[10];
	uint8_t Buffers;
	uint8_t SetBuffers;
	uint8_t ActiveBuffers;
	uint8_t SetActiveBuffers;
	union {
		struct {
			bool handheld: 1;
			bool docked: 1;
			bool reserved: 6;
		} ds;
		uint8_t general;
	} displaySync;
	resolutionCalls renderCalls[8];
	resolutionCalls viewportCalls[8];
	bool forceOriginalRefreshRate;
	bool dontForce60InDocked;
	bool forceSuspend;
	uint8_t currentRefreshRate;
	float readSpeedPerSecond;
	uint8_t FPSlockedDocked;
	uint64_t frameNumber;
	int8_t expectedSetBuffers;
	union {
		struct {
			uint64_t timestamp; //NX 1 tick = 1.625 ns | (x * 13 / 8) ns
			uint64_t samplesPassed;
			uint64_t inputVertices;
			uint64_t inputPrimitives;
			uint64_t vertexShaderInvocations;
			uint64_t tessControlShaderInvocations;
			uint64_t tessEvaluationShaderInvocations;
			uint64_t geometryShaderInvocations;
			uint64_t fragmentShaderInvocations;
			uint64_t tessEvaluationShaderPrimitives;
			uint64_t geometryShaderPrimitives;
			uint64_t clipperInputPrimitives;
			uint64_t clipperOutputPrimitives;
			uint64_t primitivesGenerated;
			uint64_t transformFeedbackPrimitivesWritten;
			uint32_t tilesProcessedByZcull;
			uint32_t pixelBlocksBehindPrimitivesAndCulled;
			uint32_t pixelBlocksInFrontOfPrimitivesCulled;
			uint32_t pixelBlocksFailedStencilTestAndCulled;
		} NVN;

		struct {
			char reserved[8];
		} EGL;

		struct {
			char reserved[8];
		} Vulkan;
	} PerfCounters;
} NX_PACKED;

static_assert(sizeof(NxFpsSharedBlock) == 310);

NxFpsSharedBlock* Shared = 0;

struct {
	uint8_t FPS = 0xFF;
	float FPSavg = 255;
	bool FPSmode = 0;
} Stats;

#if defined(SWITCH) || defined(SWITCH32)
	#define systemtickfrequency 19200000
#elif defined(OUNCE) || defined(OUNCE32)
	#define systemtickfrequency 31250000
#else
	uint64_t systemtickfrequency = 0;
#endif
static_assert(systemtickfrequency != 0);

bool changeFPS = false;
bool changedFPS = false;
uint64_t startFrameTick = 0;

size_t fileBytesRead = 0;

enum {
	ZeroSyncType_None,
	ZeroSyncType_Soft,
	ZeroSyncType_Semi
};

std::pair<uint32_t, uint32_t> last_viewport = {0, 0};

namespace nn {

	static void (*SetFocusHandlingMode_0)(AppletFocusHandlingMode mode);
	static u32 (*FileAccessorRead_0)(void* fileHandle, size_t* bytesRead, int64_t position, void* buffer, size_t readBytes, unsigned int* ReadOption);
	static u32 (*FileAccessorReadCache_0)(void* fileHandle, size_t* bytesRead, int64_t position, void* buffer, size_t readBytes, unsigned int* ReadOption, void* FileDataCacheAccessResult);
	static u32 (*roLookupSymbol_0)(uintptr_t* pOutAddress, const char* name);
	static u32 (*SetUserInactivityDetectionTimeExtended_0)(bool isTrue);

	Result FileAccessorRead(void* fileHandle, size_t* bytesRead, int64_t position, void* buffer, size_t readBytes, unsigned int* ReadOption) {
		size_t bytesRead_impl;
		if (!bytesRead)
			bytesRead = &bytesRead_impl;
		Result ret = FileAccessorRead_0(fileHandle, bytesRead, position, buffer, readBytes, ReadOption);
		if (R_SUCCEEDED(ret)) [[likely]] fileBytesRead += *bytesRead;
		return ret;
	}

	Result FileAccessorReadCache(void* fileHandle, size_t* bytesRead, int64_t position, void* buffer, size_t readBytes, unsigned int* ReadOption, void* FileDataCacheAccessResult) {
		size_t bytesRead_impl;
		if (!bytesRead)
			bytesRead = &bytesRead_impl;
		Result ret = FileAccessorReadCache_0(fileHandle, bytesRead, position, buffer, readBytes, ReadOption, FileDataCacheAccessResult);
		if (R_SUCCEEDED(ret)) [[likely]] fileBytesRead += *bytesRead;
		return ret;
	}

	Result SetUserInactivityDetectionTimeExtended(bool isTrue) {
		return SetUserInactivityDetectionTimeExtended_0(isTrue);
	}

	AppletFocusHandlingMode defaultFocusHandlingMode = AppletFocusHandlingMode_SuspendHomeSleep;
	bool focusHandlingOverwrite = false;

	void setFocusHandlingMode(AppletFocusHandlingMode mode) {
		static AppletFocusHandlingMode last_mode = AppletFocusHandlingMode_SuspendHomeSleep;
		static bool Initialized = false;
		if (!Initialized) {
			Initialized = true;
		}
		else if (last_mode == mode) return;
		last_mode = mode;
		if (!focusHandlingOverwrite) defaultFocusHandlingMode = mode;
		return SetFocusHandlingMode_0(mode);
	}
}

namespace Utils {

	inline uint64_t _getSystemTick() {
		return armGetSystemTick();
	}

	uint64_t _convertToTimeSpan(uint64_t tick) {
		#if defined(SWITCH) || defined(SWITCH32)
			return armTicksToNs(tick);
		#elif defined(OUNCE) || defined(OUNCE32)
			return tick << 5;
		#else
			return uint64_t((double)tick / ((double)(systemtickfrequency) / 1000000000.d));
		#endif
	}

	inline uintptr_t getMainAddress() {
		MemoryInfo memoryinfo = {0};
		u32 pageinfo = 0;

		uintptr_t base_address = SaltySDCore_getCodeStart() + 0x4000;
		for (size_t i = 0; i < 3; i++) {
			Result rc = svcQueryMemory(&memoryinfo, &pageinfo, base_address);
			if (R_FAILED(rc)) return 0;
			if ((memoryinfo.addr == base_address) && (memoryinfo.perm & Perm_X))
				return base_address;
			base_address = memoryinfo.addr+memoryinfo.size;
		}

		return 0;
	}
}

namespace NX_FPS_Math {
	uint8_t FPS_temp = 0;
	uint64_t starttick = 0;
	uint64_t starttick2 = 0;
	uint64_t frameend = 0;
	uint64_t frameavg = 0;
	uint8_t FPSlock = 0;
	int32_t FPStiming = 0;
	uint8_t FPStickItr = 0;
	uint8_t range = 0;
	
	bool FPSlock_delayed = false;
	bool old_force = false;
	uint32_t new_fpslock = 0;

	void PreFrame() {
		new_fpslock = (LOCK::patcher.refreshRateOverwrite() ? LOCK::patcher.refreshRateOverwrite() : ((*sharedOperationMode == 1) ? (Shared -> FPSlockedDocked) : (Shared -> FPSlocked)));
		if (old_force != (Shared -> forceOriginalRefreshRate)) {
			if (*sharedOperationMode == 1 && !(Shared -> dontForce60InDocked))
				svcSleepThread(LOCK::Patcher::DockedRefreshRateDelay);
			old_force = (Shared -> forceOriginalRefreshRate);
		}

		if ((FPStiming && !LOCK::patcher.fpsDelayBlocked() && (new_fpslock && new_fpslock < (Shared -> currentRefreshRate)))) {
			const int64_t FPSTiming_internal = FPStiming + (range * 32);
			if ((int64_t)(Utils::_getSystemTick() - frameend) < FPSTiming_internal) {
				FPSlock_delayed = true;
			}
			while ((int64_t)(Utils::_getSystemTick() - frameend) < FPSTiming_internal) {
				svcSleepThread(-2);
				svcSleepThread(10000);
			}
		}
	}

	void PostFrame() {
		last_viewport = {0, 0};
		Shared->frameNumber++;
		const uint64_t endtick = Utils::_getSystemTick();
		const uint64_t framedelta = endtick - frameend;

		Shared -> FPSticks[FPStickItr++] = framedelta;
		if (FPStickItr >= 10) FPStickItr = 0;
		
		frameavg = ((9*frameavg) + framedelta) / 10;
		float FPSavg = systemtickfrequency / (float)frameavg;
		Stats.FPSavg = FPSavg;

		if (FPSlock_delayed && FPStiming) {
			if (FPSavg > ((float)new_fpslock)) {
				if (range < 200) {
					range++;
				}
			}
			else if (((uint32_t)std::lroundf(FPSavg) == new_fpslock) && (FPSavg < (float)new_fpslock)) {
				if (range > 0) {
					range--;
				}
			}
		}

		frameend = endtick;
		FPS_temp++;
		const uint64_t deltatick = endtick - starttick;
		LOCK::patcher.clearRefreshRateOverwrite();
		if (!configRC && FPSlock) {
			LOCK::patcher.applyPatch(FPSlock, (Shared -> currentRefreshRate));
		}
		if (deltatick > systemtickfrequency) {
			nn::focusHandlingOverwrite = true;
			if (!Shared->forceSuspend) {
				nn::setFocusHandlingMode(nn::defaultFocusHandlingMode);
			}
			else {
				nn::setFocusHandlingMode(AppletFocusHandlingMode_SuspendHomeSleep);
			}
			nn::focusHandlingOverwrite = false;
			if (nn::FileAccessorRead_0) {
				const float seconds = (float)Utils::_convertToTimeSpan(deltatick) / 1000000000.f;
				float readSpeedPerSecond = (float)fileBytesRead / seconds;
				fileBytesRead = 0;
				if (readSpeedPerSecond == 0.f && Shared -> readSpeedPerSecond != 0.f) readSpeedPerSecond = 1;
				Shared -> readSpeedPerSecond = readSpeedPerSecond;
			}
			Stats.FPS = FPS_temp - 1;
			(Shared -> FPS) = Stats.FPS;
			if (deltatick > (systemtickfrequency * 2)) {
				starttick = Utils::_getSystemTick();
				FPS_temp = 0;
			}
			else {
				starttick += systemtickfrequency;
				FPS_temp = 1;
			}
			if (!configRC && FPSlock) {
				(Shared -> patchApplied) = 1;
			}
		}

		if (LOCK::patcher.refreshRateOverwrite() != 0) (Shared -> forceOriginalRefreshRate) = true;
		else (Shared -> forceOriginalRefreshRate) = false;

		(Shared -> FPSavg) = Stats.FPSavg;
		(Shared -> pluginActive) = true;

		if (!resolutionLookup && Shared -> renderCalls[0].calls == 0xFFFF) {
			resolutionLookup = true;
			Shared -> renderCalls[0].calls = 0;
		}
		if (resolutionLookup) {
			memcpy(Shared -> renderCalls, m_resolutionRenderCalls, sizeof(m_resolutionRenderCalls));
			memcpy(Shared -> viewportCalls, m_resolutionViewportCalls, sizeof(m_resolutionViewportCalls));
			memset(&m_resolutionRenderCalls, 0, sizeof(m_resolutionRenderCalls));
			memset(&m_resolutionViewportCalls, 0, sizeof(m_resolutionViewportCalls));
		}
	}

	template <typename T> void addResToViewports(T m_width, T m_height) {
		if ((m_height <= (T)160) || (m_height > (T)1440)) return;
		const T scaled = m_width * (T)10;
		if ((scaled >= ((T)6 * m_height)) && (scaled <= ((T)18 * m_height))) {
			struct {
				uint16_t width;
				uint16_t height;
			} value_to_compare;

			static_assert(sizeof(value_to_compare) == sizeof(m_resolutionViewportCalls[0].width) + sizeof(m_resolutionViewportCalls[0].height));

			value_to_compare = {(uint16_t)m_width, (uint16_t)m_height};

			for (size_t i = 0; i < 8; i++) {
				if (!m_resolutionViewportCalls[i].calls) {
					memcpy(&m_resolutionViewportCalls[i], &value_to_compare, sizeof(value_to_compare));
					m_resolutionViewportCalls[i].calls = 1;
					break;
				}
				if (!memcmp(&value_to_compare, &m_resolutionViewportCalls[i], sizeof(value_to_compare))) {
					m_resolutionViewportCalls[i].calls++;
					break;
				}
			}
		}		
	}

	template <typename T> void addResToRender(T m_width, T m_height) {
		if ((m_height <= (T)160) || (m_height > (T)1440)) return;
		const T scaled = m_width * (T)10;
		if ((scaled >= ((T)6 * m_height)) && (scaled <= ((T)18 * m_height))) {
			struct {
				uint16_t width;
				uint16_t height;
			} value_to_compare;

			static_assert(sizeof(value_to_compare) == sizeof(m_resolutionRenderCalls[0].width) + sizeof(m_resolutionRenderCalls[0].height));

			value_to_compare = {(uint16_t)m_width, (uint16_t)m_height};

			for (size_t i = 0; i < 8; i++) {
				if (!m_resolutionRenderCalls[i].calls) {
					memcpy(&m_resolutionRenderCalls[i], &value_to_compare, sizeof(value_to_compare));
					m_resolutionRenderCalls[i].calls = 1;
					break;
				}
				if (!memcmp(&value_to_compare, &m_resolutionRenderCalls[i], sizeof(value_to_compare))) {
					m_resolutionRenderCalls[i].calls++;
					break;
				}
			}
		}		
	}
}

namespace vk {

	struct VkViewport {
		float    x;
		float    y;
		float    width;
		float    height;
		float    minDepth;
		float    maxDepth;
	};

	struct VkRect2D {
		int32_t    x;
		int32_t    y;
		uint32_t   width;
		uint32_t   height;
	};

	typedef struct VkSwapchainCreateInfoKHR {
		int          sType;
		const void*  pNext;
		int          flags;
		void*        surface;
		uint32_t     minImageCount;
	} VkSwapchainCreateInfoKHR;

	struct VkDevice {
		char reserved[8];
	};
	struct VkInstance {
		char reserved[8];
	};
	struct VkCommandBuffer {
		char reserved[8];
	};
	struct VkAllocationCallbacks{
		char reserved[8];
	};
	struct VkSwapchainKHR {
		char reserved[8];
	};
	struct VkImage {
		char reserved[8];
	};
	struct VkQueue {
		char reserved[8];
	};
	struct VkPresentInfoKHR {
		char reserved[8];
	};

	static void* (*vkGetInstanceProcAddr_0)(VkInstance* instance, const char* vkFunction);
	static void* (*vkGetDeviceProcAddr_0)(VkDevice* device, const char* vkFunction);
	static void (*vkCmdSetViewport_0)(VkCommandBuffer* commandBuffer, uint32_t firstViewport, uint32_t viewportCount, const VkViewport* pViewports);
	static void (*vkCmdSetViewportWithCount_0)(VkCommandBuffer* commandBuffer, uint32_t viewportCount, const VkViewport* pViewports);
	static void (*vkCmdSetScissor_0)(VkCommandBuffer* commandBuffer, uint32_t firstScissor, uint32_t scissorCount, const VkRect2D* pScissors);
	static void (*vkCmdSetScissorWithCount_0)(VkCommandBuffer* commandBuffer, uint32_t scissorCount, const VkRect2D* pScissors);
	static int32_t (*vkCreateSwapchainKHR_0)(VkDevice* Device, const VkSwapchainCreateInfoKHR* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSwapchainKHR** pSwapchain);
	static int32_t (*vkGetSwapchainImagesKHR_0)(VkDevice* Device, VkSwapchainKHR* Swapchain, uint32_t* pSwapchainImageCount, VkImage* pSwapchainImages);
	static int32_t (*vkQueuePresentKHR_0)(VkQueue* vkQueue, VkPresentInfoKHR* VkPresentInfoKHR);
	static int32_t (*nvSwapchainQueuePresentKHR_0)(VkQueue* VkQueue_T, VkPresentInfoKHR* VkPresentInfoKHR);
	static int32_t (*nvSwapchainCreateSwapchainKHR_0)(VkDevice* Device, const VkSwapchainCreateInfoKHR* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSwapchainKHR** pSwapchain);
	static void* (*nvSwapchainGetDeviceProcAddr_0)(VkDevice* device, const char* vkFunction);
	static void* (*nvSwapchainGetInstanceProcAddr_0)(VkInstance* instance, const char* vkFunction);

	int32_t QueuePresent (VkQueue* VkQueue, VkPresentInfoKHR* PresentInfoKHR);
	int32_t CreateSwapchain(VkDevice* Device, const VkSwapchainCreateInfoKHR* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSwapchainKHR** pSwapchain);
	void* GetDeviceProcAddr(VkDevice* device, const char* vkFunction);
	void* GetInstanceProcAddr(VkInstance* instance, const char* vkFunction);

	void CmdSetViewport(VkCommandBuffer* commandBuffer, uint32_t firstViewport, uint32_t viewportCount, const VkViewport* pViewports) {
		if (resolutionLookup) for (uint i = firstViewport; i < firstViewport+viewportCount; i++) {
			if (pViewports[i].height > 1.f && pViewports[i].width > 1.f && pViewports[i].x == 0.f && pViewports[i].y == 0.f) {
				uint32_t width = (uint32_t)pViewports[i].width;
				uint32_t height = (uint32_t)pViewports[i].height;
				NX_FPS_Math::addResToViewports(width, height);
				last_viewport = {width, height};
			}
		}
		return vkCmdSetViewport_0(commandBuffer, firstViewport, viewportCount, pViewports);
	}

	void CmdSetViewportWithCount(VkCommandBuffer* commandBuffer, uint32_t viewportCount, const VkViewport* pViewports) {
		if (resolutionLookup) for (uint i = 0; i < viewportCount; i++) {
			if (pViewports[i].height > 1.f && pViewports[i].width > 1.f && pViewports[i].x == 0.f && pViewports[i].y == 0.f) {
				uint32_t width = (uint32_t)pViewports[i].width;
				uint32_t height = (uint32_t)pViewports[i].height;
				NX_FPS_Math::addResToViewports(width, height);
				last_viewport = {width, height};
			}
		}
		return vkCmdSetViewportWithCount_0(commandBuffer, viewportCount, pViewports);
	}

	void CmdSetScissor(VkCommandBuffer* commandBuffer, uint32_t firstScissor, uint32_t ScissorCount, const VkRect2D* pScissors) {
		if (resolutionLookup) for (uint i = firstScissor; i < firstScissor+ScissorCount; i++) {
			if (pScissors[i].height > 1 && pScissors[i].width > 1 && pScissors[i].x == 0 && pScissors[i].y == 0 && pScissors[i].width != last_viewport.first && pScissors[i].height != last_viewport.second) {
				NX_FPS_Math::addResToViewports(pScissors[i].width, pScissors[i].height);
			}
		}
		return vkCmdSetScissor_0(commandBuffer, firstScissor, ScissorCount, pScissors);
	}

	void CmdSetScissorWithCount(VkCommandBuffer* commandBuffer, uint32_t ScissorCount, const VkRect2D* pScissors) {
		if (resolutionLookup) for (uint i = 0; i < ScissorCount; i++) {
			if (pScissors[i].height > 1 && pScissors[i].width > 1 && pScissors[i].x == 0 && pScissors[i].y == 0 && pScissors[i].width != last_viewport.first && pScissors[i].height != last_viewport.second) {
				NX_FPS_Math::addResToViewports(pScissors[i].width, pScissors[i].height);
			}
		}
		return vkCmdSetScissorWithCount_0(commandBuffer, ScissorCount, pScissors);
	}

	namespace Common {
		NOINLINE int32_t QueuePresent(VkQueue* VkQueue_T, VkPresentInfoKHR* VkPresentInfoKHR_T, int32_t (*pointer)(VkQueue*, VkPresentInfoKHR*)) {

			static bool check_redirection = false;
			//Fix for games in which subsdk redirects internally vkQueuePresentKHR to nv::Swapchain
			if (check_redirection == true) {
				return pointer(VkQueue_T, VkPresentInfoKHR_T);
			}
			if (NX_FPS_Math::starttick == 0) [[unlikely]] {
				(Shared -> API) = 3;
				NX_FPS_Math::starttick = Utils::_getSystemTick();
				NX_FPS_Math::starttick2 = NX_FPS_Math::starttick;
			}
			
			NX_FPS_Math::PreFrame();
			check_redirection = true;
			const int32_t vulkanResult = pointer(VkQueue_T, VkPresentInfoKHR_T);
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

		NOINLINE int32_t CreateSwapchain(VkDevice* Device, const VkSwapchainCreateInfoKHR* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSwapchainKHR** pSwapchain, int32_t (*pointer)(VkDevice*, const VkSwapchainCreateInfoKHR*, const VkAllocationCallbacks*, VkSwapchainKHR**)) {
			if ((Shared -> SetBuffers) > 0) {
				VkSwapchainCreateInfoKHR* m_createInfo = (VkSwapchainCreateInfoKHR*)pCreateInfo;
				m_createInfo->minImageCount = (Shared -> SetBuffers);
			}
			int32_t vulkanResult = pointer(Device, pCreateInfo, pAllocator, pSwapchain);
			if (vulkanResult >= 0) {
				uint32_t numBuffers = 0;
				vkGetSwapchainImagesKHR_0(Device, pSwapchain[0], &numBuffers, nullptr);
				(Shared -> Buffers) = numBuffers;
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
			runtime_replace{"vkGetSwapchainImagesKHR", (uintptr_t*)&vkGetSwapchainImagesKHR_0}
		};

		NOINLINE void* GetDeviceProcAddr(VkDevice* device, const char* vkFunction, void* (*pointer)(VkDevice*, const char*)) {
			uintptr_t address = (uintptr_t)pointer(device, vkFunction);
			if (!strcmp("vkGetDeviceProcAddr", vkFunction)) {
				if (!vkGetDeviceProcAddr_0) {
					memcpy(&vkGetDeviceProcAddr_0, &address, sizeof(address));
				}
				return (void*)pointer;
			}

			for (const auto& replacement : vk_replacements) {
				if (!strcmp(replacement.name, vkFunction)) {
					if (replacement.orig_ptr && *replacement.orig_ptr == 0) *replacement.orig_ptr = address;
					if (replacement.hook_ptr) return replacement.hook_ptr;
					break;
				}
			}
			return (void*)address;
		}

		NOINLINE void* GetInstanceProcAddr(VkInstance* instance, const char* vkFunction, void* (*pointer)(VkInstance*, const char*)) {
			uintptr_t address = (uintptr_t)pointer(instance, vkFunction);
			
			for (const auto& replacement : vk_replacements) {
				if (!strcmp(replacement.name, vkFunction)) {
					if (replacement.orig_ptr && *replacement.orig_ptr == 0) *replacement.orig_ptr = address;
					if (replacement.hook_ptr) return replacement.hook_ptr;
					break;
				}
			}
			return (void*)address;
		}
	}

	namespace nvSwapchain { 
		int32_t QueuePresent (VkQueue* VkQueue_T, VkPresentInfoKHR* PresentInfoKHR) {
			return vk::Common::QueuePresent(VkQueue_T, PresentInfoKHR, nvSwapchainQueuePresentKHR_0);
		}

		int32_t CreateSwapchain(VkDevice* Device, const VkSwapchainCreateInfoKHR* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSwapchainKHR** pSwapchain) {
			return vk::Common::CreateSwapchain(Device, pCreateInfo, pAllocator, pSwapchain, nvSwapchainCreateSwapchainKHR_0);
		}

		void* GetDeviceProcAddr(VkDevice* device, const char* vkFunction) {
			return vk::Common::GetDeviceProcAddr(device, vkFunction, nvSwapchainGetDeviceProcAddr_0);
		}

		void* GetInstanceProcAddr(VkInstance* instance, const char* vkFunction) {
			return vk::Common::GetInstanceProcAddr(instance, vkFunction, nvSwapchainGetInstanceProcAddr_0);
		}
	}

	int32_t QueuePresent (VkQueue* VkQueue_T, VkPresentInfoKHR* PresentInfoKHR) {
		return vk::Common::QueuePresent(VkQueue_T, PresentInfoKHR, vkQueuePresentKHR_0);
	}

	int32_t CreateSwapchain(VkDevice* Device, const VkSwapchainCreateInfoKHR* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSwapchainKHR** pSwapchain) {
		return vk::Common::CreateSwapchain(Device, pCreateInfo, pAllocator, pSwapchain, vkCreateSwapchainKHR_0);
	}

	void* GetDeviceProcAddr(VkDevice* device, const char* vkFunction) {
		return vk::Common::GetDeviceProcAddr(device, vkFunction, vkGetDeviceProcAddr_0);
	}

	void* GetInstanceProcAddr(VkInstance* instance, const char* vkFunction) {
		return vk::Common::GetInstanceProcAddr(instance, vkFunction, vkGetInstanceProcAddr_0);
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

namespace EGL {

	#define EGL_MIN_SWAP_INTERVAL 0
	#define EGL_MAX_SWAP_INTERVAL 4

	struct glViewportArray {
		float x;
		float y;
		float width;
		float height;
	};

	struct EGLDisplay {
		char reserved[8];
	};
	struct EGLSurface {
		char reserved[8];
	};

	static bool (*eglSwapBuffers_0)(EGLDisplay*, EGLSurface*);
	static bool (*eglSwapInterval_0)(EGLDisplay*, int interval);
	static void (*glViewport_0)(int x, int y, uint width, uint height);
	static void (*glViewportArrayv_0)(uint firstViewport, uint viewportCount, const glViewportArray* pViewports);
	static void (*glViewportArrayvNV_0)(uint firstViewport, uint viewportCount, const glViewportArray* pViewports);
	static void (*glViewportArrayvOES_0)(uint firstViewport, uint viewportCount, const glViewportArray* pViewports);
	static void (*glViewportIndexedf_0)(uint index, float x, float y, float width, float height);
	static void (*glViewportIndexedfNV_0)(uint index, float x, float y, float width, float height);
	static void (*glViewportIndexedfOES_0)(uint index, float x, float y, float width, float height);
	static void (*glViewportIndexedfv_0)(uint index, const glViewportArray* pViewports);
	static void (*glViewportIndexedfvNV_0)(uint index, const glViewportArray* pViewports);
	static void (*glViewportIndexedfvOES_0)(uint index, const glViewportArray* pViewports);
	static u64 (*eglGetProcAddress_0)(const char* eglName);

	bool Interval(EGLDisplay* display, int interval) {
		bool result = false;
		if (!changeFPS) {
			result = eglSwapInterval_0(display, interval);
			changedFPS = false;
			if (result == true) {
				(Shared -> FPSmode) = std::clamp(interval, EGL_MIN_SWAP_INTERVAL, EGL_MAX_SWAP_INTERVAL);
			}
		}
		else if (interval < 0) {
			interval *= -1;
			if ((Shared -> FPSmode) != interval) {
				result = eglSwapInterval_0(display, interval);
				if (result == true)
					(Shared -> FPSmode) = interval;
			}
			changedFPS = true;
		}
		return result;
	}

	bool Swap (EGLDisplay* display, EGLSurface* surface) {

		if (NX_FPS_Math::starttick == 0) [[unlikely]] {
			(Shared -> API) = 2;
			NX_FPS_Math::starttick = Utils::_getSystemTick();
			NX_FPS_Math::starttick2 = NX_FPS_Math::starttick;
		}

		NX_FPS_Math::PreFrame();
		
		const bool result = eglSwapBuffers_0(display, surface);
		if (result == true)
			 NX_FPS_Math::PostFrame();
		
		const auto new_fpslock = NX_FPS_Math::new_fpslock;

		if (!new_fpslock) {
			NX_FPS_Math::FPStiming = 0;
			NX_FPS_Math::FPSlock = 0;
			changeFPS = false;
		}
		else {
			changeFPS = true;
			auto currentRefreshRate = (Shared -> currentRefreshRate);
			NX_FPS_Math::FPSlock = ((*sharedOperationMode == 1) ? (Shared -> FPSlockedDocked) : (Shared -> FPSlocked));
			auto FPSmode = (Shared -> FPSmode);
			const uint32_t rr = currentRefreshRate ? currentRefreshRate : 60;
			uint32_t threshold;
			int interval;
			if      (new_fpslock <= rr / 4) {interval = 4; threshold = rr / 4;}
			else if (new_fpslock <= rr / 3) {interval = 3; threshold = rr / 3;}
			else if (new_fpslock <= rr / 2) {interval = 2; threshold = rr / 2;}
			else                            {interval = 1; threshold = rr;    }

			if (FPSmode != interval) {
				EGL::Interval(display, interval * -1);
			}
			if (new_fpslock != threshold) {
				NX_FPS_Math::FPStiming = (systemtickfrequency/new_fpslock) - 6000;
			}
			else NX_FPS_Math::FPStiming = 0;
		}

		return result;
	}

	namespace Common {
		NOINLINE void ViewportArrayv(uint firstViewport, uint viewportCount, const glViewportArray* pViewports, void (*pointer)(uint, uint, const glViewportArray*)) {
			if (resolutionLookup) for (uint i = firstViewport; i < firstViewport+viewportCount; i++) {
				if (pViewports[i].height > 1.f && pViewports[i].width > 1.f && pViewports[i].x == 0.f && pViewports[i].y == 0.f) {
					NX_FPS_Math::addResToViewports((uint32_t)pViewports[i].width, (uint32_t)pViewports[i].height);
				}
			}
			return pointer(firstViewport, viewportCount, pViewports);
		}

		NOINLINE void ViewportIndexedf(uint index, float x, float y, float width, float height, void (*pointer)(uint, float, float, float, float)) {
			if (resolutionLookup && height > 1.f && width > 1.f && !x && !y) {
				NX_FPS_Math::addResToViewports((uint32_t)width, (uint32_t)height);
			}
			return pointer(index, x, y, width, height);
		}

		NOINLINE void ViewportIndexedfv(uint i, const glViewportArray* pViewports, void (*pointer)(uint, const glViewportArray*)) {
			if (resolutionLookup) {
				if (pViewports[i].height > 1.f && pViewports[i].width > 1.f && pViewports[i].x == 0.f && pViewports[i].y == 0.f) {
					NX_FPS_Math::addResToViewports((uint32_t)pViewports[i].width, (uint32_t)pViewports[i].height);
				}
			}
			return pointer(i, pViewports);
		}
	}

	void Viewport(int x, int y, uint width, uint height) {
		if (resolutionLookup && height > 1 && width > 1 && !x && !y) {
			NX_FPS_Math::addResToViewports(width, height);
		}
		return glViewport_0(x, y, width, height);
	}

	
	void ViewportArrayv(uint firstViewport, uint viewportCount, const glViewportArray* pViewports) {
		return EGL::Common::ViewportArrayv(firstViewport, viewportCount, pViewports, glViewportArrayv_0);
	}

	void ViewportArrayvNV(uint firstViewport, uint viewportCount, const glViewportArray* pViewports) {
		return EGL::Common::ViewportArrayv(firstViewport, viewportCount, pViewports, glViewportArrayvNV_0);
	}

	void ViewportArrayvOES(uint firstViewport, uint viewportCount, const glViewportArray* pViewports) {
		return EGL::Common::ViewportArrayv(firstViewport, viewportCount, pViewports, glViewportArrayvOES_0);
	}

	void ViewportIndexedf(uint index, float x, float y, float width, float height) {
		return EGL::Common::ViewportIndexedf(index, x, y, width, height, glViewportIndexedf_0);
	}

	void ViewportIndexedfNV(uint index, float x, float y, float width, float height) {
		return EGL::Common::ViewportIndexedf(index, x, y, width, height, glViewportIndexedfNV_0);
	}

	void ViewportIndexedfOES(uint index, float x, float y, float width, float height) {
		return EGL::Common::ViewportIndexedf(index, x, y, width, height, glViewportIndexedfOES_0);
	}

	void ViewportIndexedfv(uint i, const glViewportArray* pViewports) {
		return EGL::Common::ViewportIndexedfv(i, pViewports, glViewportIndexedfv_0);
	}

	void ViewportIndexedfvNV(uint i, const glViewportArray* pViewports) {
		return EGL::Common::ViewportIndexedfv(i, pViewports, glViewportIndexedfvNV_0);
	}

	void ViewportIndexedfvOES(uint i, const glViewportArray* pViewports) {
		return EGL::Common::ViewportIndexedfv(i, pViewports, glViewportIndexedfvOES_0);
	}

	uintptr_t GetProc(const char* eglName) {
		uintptr_t address = eglGetProcAddress_0(eglName);

		//They must be inside function otherwise 32-bit Core stucks at relocations
		std::array egl_replacements = {
			runtime_replace{"eglSwapInterval", (uintptr_t*)&eglSwapInterval_0, (void*)Interval},
			runtime_replace{"eglSwapBuffers", (uintptr_t*)&eglSwapBuffers_0, (void*)Swap},
			runtime_replace{"glViewport", (uintptr_t*)&glViewport_0, (void*)Viewport},
			runtime_replace{"glViewportArrayv", (uintptr_t*)&glViewportArrayv_0, (void*)ViewportArrayv},
			runtime_replace{"glViewportArrayvNV", (uintptr_t*)&glViewportArrayvNV_0, (void*)ViewportArrayvNV},
			runtime_replace{"glViewportArrayvOES", (uintptr_t*)&glViewportArrayvOES_0, (void*)ViewportArrayvOES},
			runtime_replace{"glViewportIndexedf", (uintptr_t*)&glViewportIndexedf_0, (void*)ViewportIndexedf},
			runtime_replace{"glViewportIndexedfNV", (uintptr_t*)&glViewportIndexedfNV_0, (void*)ViewportIndexedfNV},
			runtime_replace{"glViewportIndexedfOES", (uintptr_t*)&glViewportIndexedfOES_0, (void*)ViewportIndexedfOES},
			runtime_replace{"glViewportIndexedfv", (uintptr_t*)&glViewportIndexedfv_0, (void*)ViewportIndexedfv},
			runtime_replace{"glViewportIndexedfvNV", (uintptr_t*)&glViewportIndexedfvNV_0, (void*)ViewportIndexedfvNV},
			runtime_replace{"glViewportIndexedfvOES", (uintptr_t*)&glViewportIndexedfvOES_0, (void*)ViewportIndexedfvOES}
		};

		for (const auto& replacement : egl_replacements) {
			if (!strcmp(replacement.name, eglName)) {
				if (replacement.orig_ptr && *replacement.orig_ptr == 0) *replacement.orig_ptr = address;
				if (replacement.hook_ptr) return (uintptr_t)replacement.hook_ptr;
				break;
			}
		}
		return address;
	}
}

namespace NVN {

	struct Texture {
		char reserved[0xC0];
	};
	struct TextureView {
		char reserved[0x28];
	};
	struct TextureBuilder {
		char reserved[0x80];
	};
	struct Window {
		char reserved[0x180];
	};
	struct DeviceBuilder {
		char reserved[0x40];
	};
	struct Device {
		char reserved[0x3000];
	};
	struct CommandBuffer {
		char reserved[0xA0];
	};
	struct MemoryPool {
    	char reserved[0x100];
	};
	struct Sync {
		char reserved[0x40];
	};
	struct QueueBuilder {
    	char reserved[0x40];
	};
	struct Queue {
		char reserved[0x2000];
	};
	struct WindowBuilder {
		const char reserved[16];
		uint32_t numBufferedFrames;
		const char reserved2[28];
	};
	struct MemoryPoolBuilder {
		char reserved[0x40];
	};

	struct Viewport {
		float x;
		float y;
		float width;
		float height;
	};
	struct Scissor {
		int x;
		int y;
		int width;
		int height;
	};
	struct CopyRegion {
		int x;
		int y;
		int z;
		int width;
		int height;
		int depth;
	};

	Sync* WindowSync = 0;
	Device* mainDevice = 0;

	typedef int textureFlags;
	typedef int textureTarget;
	typedef int textureFormat;
	typedef int memoryPoolFlags;
	typedef uint64_t BufferAddress;
	typedef uint64_t CommandHandle;

	static uintptr_t (*nvnBootstrapLoader_0)(const char* nvnName);
	static u16 (*nvnTextureGetWidth_0)(const Texture* texture);
	static u16 (*nvnTextureGetHeight_0)(const Texture* texture);
	static u32 (*nvnTextureGetFormat_0)(const Texture* texture);
	static void* (*nvnCommandBufferSetRenderTargets_0)(CommandBuffer* cmdBuf, int numTextures, const Texture** texture, const TextureView** textureView, const Texture* depth, const TextureView* depthView);
	static void* (*nvnCommandBufferSetViewport_0)(CommandBuffer* cmdBuf, int x, int y, int width, int height);
	static void* (*nvnCommandBufferSetViewports_0)(CommandBuffer* cmdBuf, int start, int count, const Viewport* viewports);
	static void* (*nvnCommandBufferSetScissor_0)(CommandBuffer* cmdBuf, int x, int y, int width, int height);
	static void* (*nvnCommandBufferSetScissors_0)(CommandBuffer* cmdBuf, int start, int count, const Scissor* viewports);
	static void (*nvnQueuePresentTexture_0)(const Queue* queue, const Window* nvnWindow, int index);
	static uintptr_t (*nvnDeviceGetProcAddress_0)(const void* unk1_a, const char* nvnFunction_a);
	static void (*nvnWindowBuilderSetTextures_0)(const WindowBuilder* nvnWindowBuilder, int buffers, const Texture** texturesBuffer);
	static void (*nvnWindowSetNumActiveTextures_0)(const Window* nvnWindow, int buffers);
	static int (*nvnWindowGetNumActiveTextures_0)(const Window* nvnWindow);
	static bool (*nvnWindowInitialize_0)(const Window* nvnWindow, WindowBuilder* windowBuilder);
	static Result (*nvnWindowAcquireTexture_0)(const Window* nvnWindow, const Sync* nvnSync, const int* index);
	static void (*nvnWindowSetPresentInterval_0)(const Window* nvnWindow, int mode);
	static int (*nvnWindowGetPresentInterval_0)(const Window* nvnWindow);
	static int (*nvnSyncWait_0)(const Sync* _this, uint64_t timeout_ns);
	static void (*nvnQueueFinish_0)(const Queue* nvnQueue);
	static void (*nvnQueueFenceSync_0)(const Queue* nvnQueue, const Sync* nvnSync, int condition, int flags);
	static void (*nvnQueueSubmitCommands_0)(const Queue* nvnQueue, int numCommandBuffers, const CommandHandle* handles);
	static bool (*nvnCommandBufferInitialize_0)(const CommandBuffer* nvnCmdBuf, Device* nvnDevice);
	static void (*nvnCommandBufferAddCommandMemory_0)(const CommandBuffer* nvnCmdBuf, const MemoryPool* nvnMemPool, ptrdiff_t offset, size_t size);
	static void (*nvnCommandBufferAddControlMemory_0)(const CommandBuffer* nvnCmdBuf, void* buffer, size_t size);
	static void (*nvnCommandBufferBeginRecording_0)(const CommandBuffer* nvnCmdBuf);
	static CommandHandle (*nvnCommandBufferEndRecording_0)(const CommandBuffer* nvnCmdBuf);
	static void (*nvnCommandBufferReportCounter_0)(const CommandBuffer* nvnCmdBuf, int type, const BufferAddress address);
	static void (*nvnCommandBufferResetCounter_0)(const CommandBuffer* nvnCmdBuf, int type);
	static bool (*nvnSyncInitialize_0)(const Sync* nvnSync, Device* nvnDevice);
	static void (*nvnMemoryPoolBuilderSetDefaults_0)(const MemoryPoolBuilder* nvnMemPoolBuilder);
	static void (*nvnMemoryPoolBuilderSetDevice_0)(const MemoryPoolBuilder* nvnMemPoolBuilder, const Device* nvnDevice);
	static void (*nvnMemoryPoolBuilderSetFlags_0)(const MemoryPoolBuilder* nvnMemPoolBuilder, int flags);
	static void (*nvnMemoryPoolBuilderSetStorage_0)(const MemoryPoolBuilder* nvnMemPoolBuilder, void* buffer, size_t size);
	static bool (*nvnMemoryPoolInitialize_0)(const MemoryPool* nvnMemPool, const MemoryPoolBuilder* nvnMemPoolBuilder);
	static void* (*nvnMemoryPoolMap_0)(const MemoryPool* nvnMemPool);
	static BufferAddress (*nvnMemoryPoolGetBufferAddress_0)(const MemoryPool* nvnMemPool);
	static void (*nvnDeviceGetInteger_0)(const Device* nvnDevice, int info, int* out);
	static bool (*nvnDeviceInitialize_0)(Device* nvnDevice, const DeviceBuilder* nvnDeviceBuilder);
	static void (*nvnTextureBuilderSetDevice_0)(TextureBuilder* builder, Device* device);
	static void (*nvnTextureBuilderSetDefaults_0)(TextureBuilder* builder);
	static void (*nvnTextureBuilderSetFlags_0)(TextureBuilder* builder, int flags);
	static void (*nvnTextureBuilderSetTarget_0)(TextureBuilder* builder, int target);
	static void (*nvnTextureBuilderSetFormat_0)(TextureBuilder* builder, int format);
	static void (*nvnTextureBuilderSetSize2D_0)(TextureBuilder* builder, int width, int height);
	static size_t (*nvnTextureBuilderGetStorageSize_0)(const TextureBuilder* builder);
	static size_t (*nvnTextureBuilderGetStorageAlignment_0)(const TextureBuilder* builder);
	static void (*nvnTextureBuilderSetStorage_0)(TextureBuilder* builder, const MemoryPool* pool, ptrdiff_t offset);
	static bool (*nvnTextureInitialize_0)(Texture* texture, const TextureBuilder* builder);
	static int (*nvnTextureGetFlags_0)(const Texture* texture);
	static int (*nvnTextureGetTarget_0)(const Texture* texture);
	static void (*nvnCommandBufferFinalize_0)(const CommandBuffer* nvnCmdBuf);
	static void (*nvnQueueWaitSync_0)(const Queue* nvnQueue, const Sync* nvnSync);
	static void (*nvnCommandBufferBarrier_0)(const CommandBuffer* nvnCmdBuf, int barrier);
	static void (*nvnCommandBufferCopyTextureToTexture_0)(const CommandBuffer* nvnCmdBuf, const Texture* src, const TextureView* srcView, const CopyRegion* srcRegion, const Texture* dst, const TextureView* dstView, const CopyRegion* dstRegion, int flags);

	constexpr size_t COMMAND_MEMORY_PER_BUF = 0x1000; 
	constexpr size_t CONTROL_MEMORY_PER_BUF = 0x1000;
	int COUNTER_ALIGNMENT = 0x10;

	struct nvnCounterData {
		uint64_t empty;
		uint64_t timestamp;
		uint64_t samplesPassed;
		uint64_t timestamp1;
		uint64_t inputVertices;
		uint64_t timestamp2;
		uint64_t inputPrimitives;
		uint64_t timestamp3;
		uint64_t vertexShaderInvocations;
		uint64_t timestamp4;
		uint64_t tessControlShaderInvocations;
		uint64_t timestamp5;
		uint64_t tessEvaluationShaderInvocations;
		uint64_t timestamp6;
		uint64_t geometryShaderInvocations;
		uint64_t timestamp7;
		uint64_t fragmentShaderInvocations;
		uint64_t timestamp8;
		uint64_t tessEvaluationShaderPrimitives;
		uint64_t timestamp9;
		uint64_t geometryShaderPrimitives;
		uint64_t timestamp10;
		uint64_t clipperInputPrimitives;
		uint64_t timestamp11;
		uint64_t clipperOutputPrimitives;
		uint64_t timestamp12;
		uint64_t primitivesGenerated;
		uint64_t timestamp13;
		uint64_t transformFeedbackPrimitivesWritten;
		uint64_t timestamp14;
		uint32_t tilesProcessedByZcull;
		uint32_t pixelBlocksBehindPrimitivesAndCulled;
		uint32_t pixelBlocksInFrontOfPrimitivesCulled;
		uint32_t pixelBlocksFailedStencilTestAndCulled;
	};

	MemoryPool timestampDataPool{};
	nvnCounterData* timestampDataCPU = 0;
	MemoryPool profilingCmdMemoryPool{};
	alignas(0x1000) char controlMemoryStorage[0x1000]{};
	Sync timestampSync{};
	CommandBuffer tsCmdBuf{};
	alignas(0x1000) char dataPoolHostPtr[0x1000]{};
	alignas(0x1000) char cmdPoolHostPtr[0x1000]{};
	CommandHandle cmdHandles{};
	bool enableCounters = false;

	namespace TripleBuffer {
		// We assume that there won't be bigger than 1920x1080 RGBA8 texture.
		// Per swizzling requirements height must be aligned to 1152.
		// Size aligned to 0x1000;
		constexpr int WINDOW_TEXTURES = 3;
		constexpr int GAME_TEXTURES = 2;
		constexpr size_t TEXTURE_MEMORY_SIZE = (((1920 * 1152 * 4) * WINDOW_TEXTURES) + 0xFFF) & ~0xFFF;
		constexpr size_t COMMAND_MEMORY_SIZE = 0x1000;
		constexpr size_t CONTROL_MEMORY_SIZE = 0x1000;
		constexpr size_t RESERVED_MEMORY_SIZE = TEXTURE_MEMORY_SIZE + COMMAND_MEMORY_SIZE + CONTROL_MEMORY_SIZE;
		
		constexpr int MEMORY_POOL_FLAGS_CPU_NO_ACCESS = 0x1;
		constexpr int MEMORY_POOL_FLAGS_CPU_UNCACHED = 0x2;
		constexpr int MEMORY_POOL_FLAGS_GPU_CACHED = 0x20;
		constexpr int MEMORY_POOL_FLAGS_COMPRESSIBLE = 0x80;
		constexpr int BARRIER_ORDER_FRAGMENTS = 0x2;
		constexpr int BARRIER_INVALIDATE_TEXTURE = 0x10;
		constexpr int SYNC_CONDITION_ALL_GPU_COMMANDS_COMPLETE = 0;
		constexpr uint64_t WAIT_TIMEOUT_MAXIMUM = UINT64_MAX;

		// State
		bool requested = false;              // Flag detected and memory reserved at boot.
		bool poolInitialized = false;
		bool texturesInitialized = false;
		bool cmdBufInitialized = false;
		bool cmdPoolInitialized = false;

		uintptr_t memory = 0;
		size_t memorySize = 0;

		MemoryPool texturePool{};
		Texture textures[WINDOW_TEXTURES]{};
		const Texture* windowTextures[WINDOW_TEXTURES]{};
		const Texture* gameTextures[GAME_TEXTURES]{};
		int texWidth = 0, texHeight = 0, texFormat = 0, texFlags = 0, texTarget = 0;

		TextureBuilder textureBuilder{};
		MemoryPoolBuilder memoryPoolBuilder{};

		MemoryPool cmdPool{};
		CommandBuffer cmdBuf{};
		CommandHandle copyHandles[GAME_TEXTURES][WINDOW_TEXTURES]{};

		// Game with 2 textures has usually only 2 sets of per-frame resources indexed by texture index.
		// With 2 window textures, acquire sync guaranteed that frame which used the same index is finished on GPU.
		// With 3 window textures it doesn't, so we fence each frame and wait for it before giving its index back to game.
		Sync frameSyncs[GAME_TEXTURES]{};
		bool frameSyncPending[GAME_TEXTURES]{};
		bool frameSyncsInitialized = false;

		const WindowBuilder* activeBuilder = nullptr; // Builder that got our textures.
		const Window* activeWindow = nullptr;         // Window created from that builder, emulation is active for it.
		int gameIndex = 0;                            // Index returned to the game (0 or 1).
		int windowIndex = 0;                          // Index returned by real nvnWindowAcquireTexture.
		const Sync* windowSync = nullptr;             // Sync signaled when window texture at windowIndex is free.
		bool acquired = false;

		// Prepares a texture builder for our window texture.
		void setupBuilder(TextureBuilder* builder) {
			nvnTextureBuilderSetDefaults_0(builder);
			nvnTextureBuilderSetDevice_0(builder, mainDevice);
			nvnTextureBuilderSetFlags_0(builder, texFlags);
			nvnTextureBuilderSetTarget_0(builder, texTarget);
			nvnTextureBuilderSetFormat_0(builder, texFormat);
			nvnTextureBuilderSetSize2D_0(builder, texWidth, texHeight);
		}

		// Memory pool over whole reserved memory. It's created independently of textures,
		// so it's always compressible to be able to hold both compressible and non-compressible textures.
		bool createPool() {
			if (poolInitialized) return true;
			MemoryPoolBuilder* poolBuilder = &memoryPoolBuilder;
			nvnMemoryPoolBuilderSetDefaults_0(poolBuilder);
			nvnMemoryPoolBuilderSetDevice_0(poolBuilder, mainDevice);
			nvnMemoryPoolBuilderSetFlags_0(poolBuilder, MEMORY_POOL_FLAGS_CPU_NO_ACCESS | MEMORY_POOL_FLAGS_GPU_CACHED | MEMORY_POOL_FLAGS_COMPRESSIBLE);
			nvnMemoryPoolBuilderSetStorage_0(poolBuilder, (void*)memory, TEXTURE_MEMORY_SIZE);
			poolInitialized = nvnMemoryPoolInitialize_0(&texturePool, poolBuilder);
			if (!poolInitialized) requested = false;
			return poolInitialized;
		}

		// Creates 3 textures with the same properties as game's texture in reserved memory.
		bool createTextures(const Texture* reference) {
			const int width = nvnTextureGetWidth_0(reference);
			const int height = nvnTextureGetHeight_0(reference);
			const int format = nvnTextureGetFormat_0(reference);
			const int flags = nvnTextureGetFlags_0(reference);
			const int target = nvnTextureGetTarget_0(reference);

			if (texturesInitialized)
				return (width == texWidth && height == texHeight && format == texFormat && flags == texFlags && target == texTarget);

			texWidth = width; texHeight = height; texFormat = format; texFlags = flags; texTarget = target;

			TextureBuilder* builder = &textureBuilder;
			setupBuilder(builder);
			const size_t storageSize = nvnTextureBuilderGetStorageSize_0(builder);
			size_t alignment = nvnTextureBuilderGetStorageAlignment_0(builder);
			if (!alignment) alignment = 0x1000;
			const size_t stride = (storageSize + alignment - 1) & ~(alignment - 1);
			if (!storageSize || stride * WINDOW_TEXTURES > TEXTURE_MEMORY_SIZE) {
				return false;
			}

			for (int i = 0; i < WINDOW_TEXTURES; i++) {
				setupBuilder(builder);
				nvnTextureBuilderSetStorage_0(builder, &texturePool, (ptrdiff_t)(stride * i));
				if (!nvnTextureInitialize_0(&textures[i], builder)) {
					requested = false;
					return false;
				}
				windowTextures[i] = &textures[i];
			}
			texturesInitialized = true;
			return true;
		}

		// Records all 6 possible copies (game texture -> window texture).
		bool recordCopies() {
			if (cmdBufInitialized) {
				nvnCommandBufferFinalize_0(&cmdBuf);
				cmdBufInitialized = false;
			}
			if (!cmdPoolInitialized) {
				MemoryPoolBuilder* poolBuilder = &memoryPoolBuilder;
				nvnMemoryPoolBuilderSetDefaults_0(poolBuilder);
				nvnMemoryPoolBuilderSetDevice_0(poolBuilder, mainDevice);
				nvnMemoryPoolBuilderSetFlags_0(poolBuilder, MEMORY_POOL_FLAGS_CPU_UNCACHED | MEMORY_POOL_FLAGS_GPU_CACHED);
				nvnMemoryPoolBuilderSetStorage_0(poolBuilder, (void*)(memory + TEXTURE_MEMORY_SIZE), COMMAND_MEMORY_SIZE);
				if (!nvnMemoryPoolInitialize_0(&cmdPool, poolBuilder)) {
					return false;
				}
				cmdPoolInitialized = true;
			}
			if (!nvnCommandBufferInitialize_0(&cmdBuf, mainDevice)) {
				return false;
			}
			cmdBufInitialized = true;
			nvnCommandBufferAddCommandMemory_0(&cmdBuf, &cmdPool, 0, COMMAND_MEMORY_SIZE);
			nvnCommandBufferAddControlMemory_0(&cmdBuf, (void*)(memory + TEXTURE_MEMORY_SIZE + COMMAND_MEMORY_SIZE), CONTROL_MEMORY_SIZE);

			CopyRegion region{};
			region.x = 0;
			region.y = 0;
			region.z = 0;
			region.width = texWidth;
			region.height = texHeight;
			region.depth = 1;
			for (int src = 0; src < GAME_TEXTURES; src++) {
				for (int dst = 0; dst < WINDOW_TEXTURES; dst++) {
					nvnCommandBufferBeginRecording_0(&cmdBuf);
					// Game's rendering into its texture must be finished before it's read by copy.
					nvnCommandBufferBarrier_0(&cmdBuf, BARRIER_ORDER_FRAGMENTS | BARRIER_INVALIDATE_TEXTURE);
					nvnCommandBufferCopyTextureToTexture_0(&cmdBuf, gameTextures[src], nullptr, &region, &textures[dst], nullptr, &region, 0);
					copyHandles[src][dst] = nvnCommandBufferEndRecording_0(&cmdBuf);
				}
			}
			return true;
		}

		// Returns textures that should be passed to window or nullptr if emulation can't be used.
		const Texture** setup(const WindowBuilder* builder, int numTextures, const Texture** textures) {
			if (!requested || numTextures != GAME_TEXTURES || !textures || !textures[0] || !textures[1]) return nullptr;
			if (!memory) {
				memory = SaltySDCore_GetReservedMemory(&memorySize);
				if (!memory || memorySize < RESERVED_MEMORY_SIZE) {
					requested = false;
					return nullptr;
				}
			}
			if (!frameSyncsInitialized) {
				for (int i = 0; i < GAME_TEXTURES; i++) {
					if (!nvnSyncInitialize_0(&frameSyncs[i], mainDevice)) {
						requested = false;
						return nullptr;
					}
				}
				frameSyncsInitialized = true;
			}
			if (!createPool() || !createTextures(textures[0])) return nullptr;
			// Copies are recorded again only for new game textures. Otherwise command memory
			// that GPU may still be reading would be overwritten.
			if (!cmdBufInitialized || gameTextures[0] != textures[0] || gameTextures[1] != textures[1]) {
				gameTextures[0] = textures[0];
				gameTextures[1] = textures[1];
				if (!recordCopies()) return nullptr;
			}
			activeBuilder = builder;
			activeWindow = nullptr;
			gameIndex = 0;
			acquired = false;
			frameSyncPending[0] = false;
			frameSyncPending[1] = false;
			return windowTextures;
		}
	}

	bool WindowInitialize(const Window* nvnWindow, WindowBuilder* windowBuilder) {
		if (TripleBuffer::activeBuilder && windowBuilder == TripleBuffer::activeBuilder) {
			bool ret = nvnWindowInitialize_0(nvnWindow, windowBuilder);
			if (ret) TripleBuffer::activeWindow = nvnWindow;
			return ret;
		}
		// Window initialized without our textures (f.e. object reused after finalize), don't emulate it.
		if (nvnWindow == TripleBuffer::activeWindow) TripleBuffer::activeWindow = nullptr;
		if (Shared->Buffers == 0) {
			(Shared -> Buffers) = windowBuilder -> numBufferedFrames;
			if ((Shared -> SetBuffers) >= 2 && (Shared -> SetBuffers) <= windowBuilder -> numBufferedFrames) {
				windowBuilder -> numBufferedFrames = (Shared -> SetBuffers);
			}
			(Shared -> ActiveBuffers) = windowBuilder -> numBufferedFrames;	
		}
		return nvnWindowInitialize_0(nvnWindow, windowBuilder);
	}

	void WindowBuilderSetTextures(const WindowBuilder* nvnWindowBuilder, int numBufferedFrames, const Texture** nvnTextures) {
		if (const Texture** emulated = TripleBuffer::setup(nvnWindowBuilder, numBufferedFrames, nvnTextures)) {
			(Shared -> Buffers) = TripleBuffer::WINDOW_TEXTURES;
			(Shared -> ActiveBuffers) = TripleBuffer::WINDOW_TEXTURES;
			amountOfAvailableBuffers = TripleBuffer::WINDOW_TEXTURES;
			return nvnWindowBuilderSetTextures_0(nvnWindowBuilder, TripleBuffer::WINDOW_TEXTURES, emulated);
		}
		if (TripleBuffer::activeBuilder == nvnWindowBuilder) TripleBuffer::activeBuilder = nullptr;
		(Shared -> Buffers) = numBufferedFrames;
		amountOfAvailableBuffers = numBufferedFrames;
		if ((Shared -> SetBuffers) >= 2 && (Shared -> SetBuffers) <= numBufferedFrames) {
			if (!setNumActiveTexturesDetected) numBufferedFrames = (Shared -> SetBuffers);
			else Shared->expectedSetBuffers = (Shared -> SetBuffers);
		}
		(Shared -> ActiveBuffers) = numBufferedFrames;
		return nvnWindowBuilderSetTextures_0(nvnWindowBuilder, numBufferedFrames, nvnTextures);
	}

	void WindowSetNumActiveTextures(const Window* nvnWindow, int numBufferedFrames) {
		if (numBufferedFrames < 0) {
			numBufferedFrames *= -1;
			nvnWindowSetNumActiveTextures_0(nvnWindow, numBufferedFrames);
			(Shared -> ActiveBuffers) = nvnWindowGetNumActiveTextures_0(nvnWindow);
		}
		else {
			(Shared -> SetActiveBuffers) = numBufferedFrames;
			if (Shared->expectedSetBuffers > 0) return;
			if ((Shared -> SetBuffers) >= 2 && (Shared -> SetBuffers) <= (Shared -> Buffers)) {
				numBufferedFrames = (Shared -> SetBuffers);
			}
			(Shared -> ActiveBuffers) = numBufferedFrames;
		}
		return nvnWindowSetNumActiveTextures_0(nvnWindow, numBufferedFrames);
	}

	void SetPresentInterval(const Window* nvnWindow, int mode) {
		if (mode < 0) {
			mode *= -1;
			if ((Shared -> FPSmode) != mode) {
				nvnWindowSetPresentInterval_0(nvnWindow, mode);
				(Shared -> FPSmode) = mode;
			}
			changedFPS = true;
		}
		else if (!changeFPS) {
			nvnWindowSetPresentInterval_0(nvnWindow, mode);
			changedFPS = false;
			(Shared -> FPSmode) = mode;
		}
		return;
	}

	int SyncWait0(const Sync* _this, uint64_t timeout_ns) {
		if (_this == WindowSync && (Shared -> ActiveBuffers) == 2) {
			if ((Shared -> ZeroSync) == ZeroSyncType_Semi) {
				const uint64_t endFrameTick = Utils::_getSystemTick();
				u64 FrameTarget = (systemtickfrequency/60) - 8000;
				s64 new_timeout = (FrameTarget - (endFrameTick - startFrameTick)) - (systemtickfrequency / 1000);
				if (((*sharedOperationMode == 1) ? (Shared -> FPSlockedDocked) : (Shared -> FPSlocked)) == 60) {
					new_timeout = (systemtickfrequency/101) - (endFrameTick - startFrameTick);
				}
				if (new_timeout > 0) {
					timeout_ns = Utils::_convertToTimeSpan(new_timeout);
				}
				else timeout_ns = 0;
			}
			else if ((Shared -> ZeroSync) == ZeroSyncType_Soft) 
				timeout_ns = 0;
		}
		return nvnSyncWait_0(_this, timeout_ns);
	}

	bool DeviceInitialize(Device* nvnDevice, const DeviceBuilder* nvnDeviceBuilder) {
		bool ret = nvnDeviceInitialize_0(nvnDevice, nvnDeviceBuilder);
		if (mainDevice == 0) mainDevice = nvnDevice;
		return ret;
	}

	void PresentTexture(const Queue* queue, const Window* nvnWindow, int index) {

		//Initialize time calculation;
		bool m_enableCounters = enableCounters;
		if (NX_FPS_Math::starttick == 0) [[unlikely]] {
			NX_FPS_Math::starttick = Utils::_getSystemTick();
			NX_FPS_Math::starttick2 = NX_FPS_Math::starttick;

			if (m_enableCounters) {
				MemoryPoolBuilder dataPoolBuilder{};
				nvnMemoryPoolBuilderSetDefaults_0(&dataPoolBuilder);
				nvnMemoryPoolBuilderSetDevice_0(&dataPoolBuilder, mainDevice);
				nvnMemoryPoolBuilderSetFlags_0(&dataPoolBuilder, BIT(1) | BIT(5));
				nvnMemoryPoolBuilderSetStorage_0(&dataPoolBuilder, (void*)&dataPoolHostPtr, sizeof(dataPoolHostPtr));
				nvnMemoryPoolInitialize_0(&timestampDataPool, &dataPoolBuilder);
				timestampDataCPU = (nvnCounterData*)nvnMemoryPoolMap_0(&timestampDataPool);
				BufferAddress dataGpuAddress = nvnMemoryPoolGetBufferAddress_0(&timestampDataPool);

				MemoryPoolBuilder cmdPoolBuilder{};
				nvnMemoryPoolBuilderSetDefaults_0(&cmdPoolBuilder);
				nvnMemoryPoolBuilderSetDevice_0(&cmdPoolBuilder, mainDevice);
				nvnMemoryPoolBuilderSetFlags_0(&cmdPoolBuilder, BIT(1) | BIT(5));
				nvnMemoryPoolBuilderSetStorage_0(&cmdPoolBuilder, (void*)&cmdPoolHostPtr, sizeof(cmdPoolHostPtr));
				nvnMemoryPoolInitialize_0(&profilingCmdMemoryPool, &cmdPoolBuilder);

				//nvnDeviceGetInteger_0(mainDevice, 10, &COUNTER_ALIGNMENT);

				nvnCommandBufferInitialize_0(&tsCmdBuf, mainDevice);
				
				nvnCommandBufferAddCommandMemory_0(&tsCmdBuf, &profilingCmdMemoryPool, 0, COMMAND_MEMORY_PER_BUF);
				nvnCommandBufferAddControlMemory_0(&tsCmdBuf, controlMemoryStorage, CONTROL_MEMORY_PER_BUF);

				nvnCommandBufferBeginRecording_0(&tsCmdBuf);

				for (size_t counter = 0; counter < 0x10; counter++) {
					BufferAddress currentFrameOffset = dataGpuAddress + (counter * COUNTER_ALIGNMENT);
					nvnCommandBufferReportCounter_0(&tsCmdBuf, counter, currentFrameOffset);
					nvnCommandBufferResetCounter_0(&tsCmdBuf, counter);			
				}
				
				cmdHandles = nvnCommandBufferEndRecording_0(&tsCmdBuf);
			}
		}
		NX_FPS_Math::PreFrame();
		if (TripleBuffer::activeWindow && nvnWindow == TripleBuffer::activeWindow && TripleBuffer::acquired && (index == 0 || index == 1)) {
			// Window texture is written only by our copy, so it's enough that GPU waits for it to be released by display.
			// This allows ZeroSync to skip CPU wait for it in game.
			nvnQueueWaitSync_0(queue, TripleBuffer::windowSync);
			nvnQueueSubmitCommands_0(queue, 1, &TripleBuffer::copyHandles[index][TripleBuffer::windowIndex]);
			// Signaled when game's frame rendered to this index and our copy from it are finished.
			// No flags needed, present right after flushes queue.
			nvnQueueFenceSync_0(queue, &TripleBuffer::frameSyncs[index], TripleBuffer::SYNC_CONDITION_ALL_GPU_COMMANDS_COMPLETE, 0);
			TripleBuffer::frameSyncPending[index] = true;
			nvnQueuePresentTexture_0(queue, nvnWindow, TripleBuffer::windowIndex);
			TripleBuffer::gameIndex ^= 1;
			TripleBuffer::acquired = false;
		}
		else nvnQueuePresentTexture_0(queue, nvnWindow, index);
		if (m_enableCounters) {
			Shared->PerfCounters.NVN.timestamp = timestampDataCPU->timestamp;
			#if defined(SWITCH) || defined(OUNCE)
			uint64x2x4_t loaded_data1 = vld1q_u64_x4(&timestampDataCPU->samplesPassed);
			uint64x2x4_t loaded_data2 = vld1q_u64_x4(&timestampDataCPU->tessControlShaderInvocations);
			uint64x2x4_t loaded_data3 = vld1q_u64_x4(&timestampDataCPU->tessEvaluationShaderPrimitives);
			uint64x2x2_t loaded_data4 = vld1q_u64_x2(&timestampDataCPU->primitivesGenerated);
			uint32x4_t loaded_data5 = vld1q_u32(&timestampDataCPU->tilesProcessedByZcull);

			Shared->PerfCounters.NVN.samplesPassed = loaded_data1.val[0][0];
			Shared->PerfCounters.NVN.inputVertices = loaded_data1.val[1][0];
			Shared->PerfCounters.NVN.inputPrimitives = loaded_data1.val[2][0];
			Shared->PerfCounters.NVN.vertexShaderInvocations = loaded_data1.val[3][0];

			Shared->PerfCounters.NVN.tessControlShaderInvocations = loaded_data2.val[0][0];
			Shared->PerfCounters.NVN.tessEvaluationShaderInvocations = loaded_data2.val[1][0];
			Shared->PerfCounters.NVN.geometryShaderInvocations = loaded_data2.val[2][0];
			Shared->PerfCounters.NVN.fragmentShaderInvocations = loaded_data2.val[3][0];

			Shared->PerfCounters.NVN.tessEvaluationShaderPrimitives = loaded_data3.val[0][0];
			Shared->PerfCounters.NVN.geometryShaderPrimitives = loaded_data3.val[1][0];
			Shared->PerfCounters.NVN.clipperInputPrimitives = loaded_data3.val[2][0];
			Shared->PerfCounters.NVN.clipperOutputPrimitives = loaded_data3.val[3][0];

			Shared->PerfCounters.NVN.primitivesGenerated = loaded_data4.val[0][0];
			Shared->PerfCounters.NVN.transformFeedbackPrimitivesWritten = loaded_data4.val[1][0];

			Shared->PerfCounters.NVN.tilesProcessedByZcull = loaded_data5[0];
			Shared->PerfCounters.NVN.pixelBlocksBehindPrimitivesAndCulled = loaded_data5[1];
			Shared->PerfCounters.NVN.pixelBlocksInFrontOfPrimitivesCulled = loaded_data5[2];
			Shared->PerfCounters.NVN.pixelBlocksFailedStencilTestAndCulled = loaded_data5[3];
			#else
			Shared->PerfCounters.NVN.samplesPassed = timestampDataCPU->samplesPassed;
			Shared->PerfCounters.NVN.inputVertices = timestampDataCPU->inputVertices;
			Shared->PerfCounters.NVN.inputPrimitives = timestampDataCPU->inputPrimitives;
			Shared->PerfCounters.NVN.vertexShaderInvocations = timestampDataCPU->vertexShaderInvocations;
			Shared->PerfCounters.NVN.tessControlShaderInvocations = timestampDataCPU->tessControlShaderInvocations;
			Shared->PerfCounters.NVN.tessEvaluationShaderInvocations = timestampDataCPU->tessEvaluationShaderInvocations;
			Shared->PerfCounters.NVN.geometryShaderInvocations = timestampDataCPU->geometryShaderInvocations;
			Shared->PerfCounters.NVN.fragmentShaderInvocations = timestampDataCPU->fragmentShaderInvocations;
			Shared->PerfCounters.NVN.tessEvaluationShaderPrimitives = timestampDataCPU->tessEvaluationShaderPrimitives;
			Shared->PerfCounters.NVN.geometryShaderPrimitives = timestampDataCPU->geometryShaderPrimitives;
			Shared->PerfCounters.NVN.clipperInputPrimitives = timestampDataCPU->clipperInputPrimitives;
			Shared->PerfCounters.NVN.clipperOutputPrimitives = timestampDataCPU->clipperOutputPrimitives;
			Shared->PerfCounters.NVN.primitivesGenerated = timestampDataCPU->primitivesGenerated;
			Shared->PerfCounters.NVN.transformFeedbackPrimitivesWritten = timestampDataCPU->transformFeedbackPrimitivesWritten;
			Shared->PerfCounters.NVN.tilesProcessedByZcull = timestampDataCPU->tilesProcessedByZcull;
			Shared->PerfCounters.NVN.pixelBlocksBehindPrimitivesAndCulled = timestampDataCPU->pixelBlocksBehindPrimitivesAndCulled;
			Shared->PerfCounters.NVN.pixelBlocksInFrontOfPrimitivesCulled = timestampDataCPU->pixelBlocksInFrontOfPrimitivesCulled;
			Shared->PerfCounters.NVN.pixelBlocksFailedStencilTestAndCulled = timestampDataCPU->pixelBlocksFailedStencilTestAndCulled;
			#endif
			nvnQueueSubmitCommands_0(queue, 1, &cmdHandles);
		}
		NX_FPS_Math::PostFrame();

		if (setNumActiveTexturesDetected) {
			auto expectedBuffers = Shared->expectedSetBuffers;
			if ((expectedBuffers > 0) && (amountOfAvailableBuffers >= expectedBuffers) && (expectedBuffers != (Shared->ActiveBuffers))) {
				expectedBuffers *= -1;
				nvnQueueFinish_0(queue);
				WindowSetNumActiveTextures(nvnWindow, expectedBuffers);
			}
		}

		const auto nvnInterval = nvnWindowGetPresentInterval_0(nvnWindow);

		(Shared -> FPSmode) = (uint8_t)nvnInterval;

		const auto new_fpslock = NX_FPS_Math::new_fpslock;
		
		if (!new_fpslock) {
			NX_FPS_Math::FPStiming = 0;
			NX_FPS_Math::FPSlock = 0;
			changeFPS = false;
		}
		else {
			changeFPS = true;
			const auto currentRefreshRate = (Shared -> currentRefreshRate);
			NX_FPS_Math::FPSlock = ((*sharedOperationMode == 1) ? (Shared -> FPSlockedDocked) : (Shared -> FPSlocked));
			const uint32_t rr = currentRefreshRate ? currentRefreshRate : 60;
			uint32_t threshold;
			int interval;
			if      (new_fpslock <= rr / 4) {interval = 4; threshold = rr / 4;}
			else if (new_fpslock <= rr / 3) {interval = 3; threshold = rr / 3;}
			else if (new_fpslock <= rr / 2) {interval = 2; threshold = rr / 2;}
			else                            {interval = 1; threshold = rr;    }

			if (nvnInterval != interval) {
				if (interval == 1)
					NVN::SetPresentInterval(nvnWindow, -2); //This allows in game with glitched interval to unlock 60 FPS, f.e. WRC Generations
				NVN::SetPresentInterval(nvnWindow, interval * -1);
			}

			if (new_fpslock != threshold || Shared->ZeroSync) {
				NX_FPS_Math::FPStiming = (systemtickfrequency / new_fpslock) - 6000;
				if (new_fpslock == threshold)
					NX_FPS_Math::FPStiming -= 2000;
			} 
			else NX_FPS_Math::FPStiming = 0;
		}
		
		return;
	}

	Result AcquireTexture(const Window* nvnWindow, const Sync* nvnSync, const int* index) {
		if (WindowSync != nvnSync) {
			WindowSync = (Sync*)nvnSync;
		}
		Result ret = nvnWindowAcquireTexture_0(nvnWindow, nvnSync, index);
		if (R_SUCCEEDED(ret) && TripleBuffer::activeWindow && nvnWindow == TripleBuffer::activeWindow && index) {
			// Game gets only its own 2 textures, alternating with each present. Real window texture is used as copy target.
			const int realIndex = *index;
			TripleBuffer::acquired = (ret == 0 && realIndex >= 0 && realIndex < TripleBuffer::WINDOW_TEXTURES);
			if (TripleBuffer::acquired) {
				TripleBuffer::windowIndex = realIndex;
				TripleBuffer::windowSync = nvnSync;
			}
			const int gameIndex = TripleBuffer::gameIndex;
			if (TripleBuffer::frameSyncPending[gameIndex]) {
				nvnSyncWait_0(&TripleBuffer::frameSyncs[gameIndex], TripleBuffer::WAIT_TIMEOUT_MAXIMUM);
				TripleBuffer::frameSyncPending[gameIndex] = false;
			}
			*(int*)index = gameIndex;
		}
		
		startFrameTick = Utils::_getSystemTick();
		return ret;
	}

	void* CommandBufferSetViewports(CommandBuffer* cmdBuf, int start, int count, const Viewport* viewports) {
		if (resolutionLookup) for (int i = start; i < start+count; i++) {
			if (viewports[i].height > 1.f && viewports[i].width > 1.f && viewports[i].x == 0.f && viewports[i].y == 0.f) {
				uint32_t width = (uint32_t)viewports[i].width;
				uint32_t height = (uint32_t)viewports[i].height;
				NX_FPS_Math::addResToViewports(width, height);
				last_viewport = {width, height};
			}
		}
		return nvnCommandBufferSetViewports_0(cmdBuf, start, count, viewports);
	}

	void* CommandBufferSetViewport(CommandBuffer* cmdBuf, int x, int y, int width, int height) {
		if (!x && !y && height > 1 && width > 1 && resolutionLookup) {
			NX_FPS_Math::addResToViewports((uint32_t)width, (uint32_t)height);
				last_viewport = {(uint32_t)width, (uint32_t)height};
		}
		return nvnCommandBufferSetViewport_0(cmdBuf, x, y, width, height);
	}

	void* CommandBufferSetScissors(CommandBuffer* cmdBuf, int start, int count, const Scissor* viewports) {
		if (resolutionLookup) for (int i = start; i < start+count; i++) {
			if (viewports[i].height > 1 && viewports[i].width > 1 && viewports[i].x == 0 && viewports[i].y == 0 && (uint32_t)viewports[i].height != last_viewport.second && (uint32_t)viewports[i].width != last_viewport.first) {
				NX_FPS_Math::addResToViewports((uint32_t)viewports[i].width, (uint32_t)viewports[i].height);
			}
		}
		return nvnCommandBufferSetScissors_0(cmdBuf, start, count, viewports);
	}

	void* CommandBufferSetScissor(CommandBuffer* cmdBuf, int x, int y, int width, int height) {
		if (!x && !y && height > 1 && width > 1 && resolutionLookup && (uint32_t)width != last_viewport.first && (uint32_t)height != last_viewport.second) {
			NX_FPS_Math::addResToViewports((uint32_t)width, (uint32_t)height);
		}
		return nvnCommandBufferSetScissor_0(cmdBuf, x, y, width, height);
	}

	void* CommandBufferSetRenderTargets(CommandBuffer* cmdBuf, int numTextures, const Texture** texture, const TextureView** textureView, const Texture* depthTexture, const TextureView* depthView) {
		if (texture != NULL && depthTexture != NULL && resolutionLookup) {
			auto depth_width = nvnTextureGetWidth_0(depthTexture);
			auto depth_height = nvnTextureGetHeight_0(depthTexture);
			auto depth_format = nvnTextureGetFormat_0(depthTexture);
			if (depth_width > 1 && depth_height > 1 && (depth_format >= 51 && depth_format <= 54)) {
				NX_FPS_Math::addResToRender((uint32_t)depth_width, (uint32_t)depth_height);
			}
		}
		return nvnCommandBufferSetRenderTargets_0(cmdBuf, numTextures, texture, textureView, depthTexture, depthView);
	}

	void initWindowSetNumActiveTextures(bool* out) {
		setNumActiveTexturesDetected = true;
		Shared->expectedSetBuffers = 0;
		*out = true;
	}

	uintptr_t GetProcAddress0 (Device* nvnDevice, const char* nvnFunction);

	uintptr_t GetProcAddress0 (Device* nvnDevice, const char* nvnFunction) {
		if (nvnDevice) mainDevice = nvnDevice;
		uintptr_t address = nvnDeviceGetProcAddress_0(nvnDevice, nvnFunction);

		std::array nvn_replacements = {
			runtime_replace{"nvnDeviceGetProcAddress", nullptr, (void*)GetProcAddress0},
			runtime_replace{"nvnDeviceInitialize", (uintptr_t*)&nvnDeviceInitialize_0, (void*)DeviceInitialize},
			runtime_replace{"nvnDeviceGetInteger", (uintptr_t*)&nvnDeviceGetInteger_0},
			runtime_replace{"nvnQueuePresentTexture", (uintptr_t*)&nvnQueuePresentTexture_0, (void*)PresentTexture},
			runtime_replace{"nvnWindowAcquireTexture", (uintptr_t*)&nvnWindowAcquireTexture_0, (void*)AcquireTexture},
			runtime_replace{"nvnWindowSetPresentInterval", (uintptr_t*)&nvnWindowSetPresentInterval_0, (void*)SetPresentInterval},
			runtime_replace{"nvnWindowGetPresentInterval", (uintptr_t*)&nvnWindowGetPresentInterval_0},
			runtime_replace{"nvnWindowSetNumActiveTextures", (uintptr_t*)&nvnWindowSetNumActiveTextures_0, (void*)WindowSetNumActiveTextures, initWindowSetNumActiveTextures},
			runtime_replace{"nvnWindowBuilderSetTextures", (uintptr_t*)&nvnWindowBuilderSetTextures_0, (void*)WindowBuilderSetTextures},
			runtime_replace{"nvnWindowInitialize", (uintptr_t*)&nvnWindowInitialize_0, (void*)WindowInitialize},
			runtime_replace{"nvnSyncWait", (uintptr_t*)&nvnSyncWait_0, (void*)SyncWait0},
			runtime_replace{"nvnCommandBufferSetRenderTargets", (uintptr_t*)&nvnCommandBufferSetRenderTargets_0, (void*)CommandBufferSetRenderTargets},
			runtime_replace{"nvnCommandBufferSetViewport", (uintptr_t*)&nvnCommandBufferSetViewport_0, (void*)CommandBufferSetViewport},
			runtime_replace{"nvnCommandBufferSetViewports", (uintptr_t*)&nvnCommandBufferSetViewports_0, (void*)CommandBufferSetViewports},
			runtime_replace{"nvnCommandBufferSetScissor", (uintptr_t*)&nvnCommandBufferSetScissor_0, (void*)CommandBufferSetScissor},
			runtime_replace{"nvnCommandBufferSetScissors", (uintptr_t*)&nvnCommandBufferSetScissors_0, (void*)CommandBufferSetScissors},
			runtime_replace{"nvnTextureGetWidth", (uintptr_t*)&nvnTextureGetWidth_0},
			runtime_replace{"nvnTextureGetHeight", (uintptr_t*)&nvnTextureGetHeight_0},
			runtime_replace{"nvnTextureGetFormat", (uintptr_t*)&nvnTextureGetFormat_0},
			runtime_replace{"nvnQueueFinish", (uintptr_t*)&nvnQueueFinish_0},
			runtime_replace{"nvnWindowGetNumActiveTextures", (uintptr_t*)&nvnWindowGetNumActiveTextures_0},
			runtime_replace{"nvnMemoryPoolBuilderSetDefaults", (uintptr_t*)&nvnMemoryPoolBuilderSetDefaults_0},
			runtime_replace{"nvnMemoryPoolBuilderSetDevice", (uintptr_t*)&nvnMemoryPoolBuilderSetDevice_0},
			runtime_replace{"nvnMemoryPoolBuilderSetFlags", (uintptr_t*)&nvnMemoryPoolBuilderSetFlags_0},
			runtime_replace{"nvnMemoryPoolBuilderSetStorage", (uintptr_t*)&nvnMemoryPoolBuilderSetStorage_0},
			runtime_replace{"nvnMemoryPoolInitialize", (uintptr_t*)&nvnMemoryPoolInitialize_0},
			runtime_replace{"nvnMemoryPoolMap", (uintptr_t*)&nvnMemoryPoolMap_0},
			runtime_replace{"nvnMemoryPoolGetBufferAddress", (uintptr_t*)&nvnMemoryPoolGetBufferAddress_0},
			runtime_replace{"nvnSyncInitialize", (uintptr_t*)&nvnSyncInitialize_0},
			runtime_replace{"nvnCommandBufferInitialize", (uintptr_t*)&nvnCommandBufferInitialize_0},
			runtime_replace{"nvnCommandBufferAddCommandMemory", (uintptr_t*)&nvnCommandBufferAddCommandMemory_0},
			runtime_replace{"nvnCommandBufferAddControlMemory", (uintptr_t*)&nvnCommandBufferAddControlMemory_0},
			runtime_replace{"nvnCommandBufferBeginRecording", (uintptr_t*)&nvnCommandBufferBeginRecording_0},
			runtime_replace{"nvnCommandBufferReportCounter", (uintptr_t*)&nvnCommandBufferReportCounter_0},
			runtime_replace{"nvnCommandBufferEndRecording", (uintptr_t*)&nvnCommandBufferEndRecording_0},
			runtime_replace{"nvnQueueSubmitCommands", (uintptr_t*)&nvnQueueSubmitCommands_0},
			runtime_replace{"nvnQueueFenceSync", (uintptr_t*)&nvnQueueFenceSync_0},
			runtime_replace{"nvnCommandBufferResetCounter", (uintptr_t*)&nvnCommandBufferResetCounter_0},
			runtime_replace{"nvnTextureBuilderSetDevice", (uintptr_t*)&nvnTextureBuilderSetDevice_0},
			runtime_replace{"nvnTextureBuilderSetDefaults", (uintptr_t*)&nvnTextureBuilderSetDefaults_0},
			runtime_replace{"nvnTextureBuilderSetFlags", (uintptr_t*)&nvnTextureBuilderSetFlags_0},
			runtime_replace{"nvnTextureBuilderSetTarget", (uintptr_t*)&nvnTextureBuilderSetTarget_0},
			runtime_replace{"nvnTextureBuilderSetFormat", (uintptr_t*)&nvnTextureBuilderSetFormat_0},
			runtime_replace{"nvnTextureBuilderSetSize2D", (uintptr_t*)&nvnTextureBuilderSetSize2D_0},
			runtime_replace{"nvnTextureBuilderGetStorageSize", (uintptr_t*)&nvnTextureBuilderGetStorageSize_0},
			runtime_replace{"nvnTextureBuilderGetStorageAlignment", (uintptr_t*)&nvnTextureBuilderGetStorageAlignment_0},
			runtime_replace{"nvnTextureBuilderSetStorage", (uintptr_t*)&nvnTextureBuilderSetStorage_0},
			runtime_replace{"nvnTextureInitialize", (uintptr_t*)&nvnTextureInitialize_0},
			runtime_replace{"nvnTextureGetFlags", (uintptr_t*)&nvnTextureGetFlags_0},
			runtime_replace{"nvnTextureGetTarget", (uintptr_t*)&nvnTextureGetTarget_0},
			runtime_replace{"nvnCommandBufferFinalize", (uintptr_t*)&nvnCommandBufferFinalize_0},
			runtime_replace{"nvnQueueWaitSync", (uintptr_t*)&nvnQueueWaitSync_0},
			runtime_replace{"nvnCommandBufferBarrier", (uintptr_t*)&nvnCommandBufferBarrier_0},
			runtime_replace{"nvnCommandBufferCopyTextureToTexture", (uintptr_t*)&nvnCommandBufferCopyTextureToTexture_0}
		};

		for (const auto& replacement : nvn_replacements) {
			if (!strcmp(replacement.name, nvnFunction)) {
				if (replacement.orig_ptr && *replacement.orig_ptr == 0) *replacement.orig_ptr = address;
				if (replacement.hook_ptr) return (uintptr_t)replacement.hook_ptr;
				break;
			}
		}
		return address;
	}

	uintptr_t BootstrapLoader_1(const char* nvnName) {
		if (strcmp(nvnName, "nvnDeviceGetProcAddress") == 0) {
			Shared->API = 1;
			if (!NVN::nvnDeviceGetProcAddress_0) {
				auto temp = nvnBootstrapLoader_0("nvnDeviceGetProcAddress");
				memcpy(&NVN::nvnDeviceGetProcAddress_0, &temp, sizeof(temp));
				static_assert(sizeof(temp) == sizeof(void*));
			}
			return (uintptr_t)&GetProcAddress0;
		}
		return nvnBootstrapLoader_0(nvnName);
	}
}

void checkvkGetInstanceProcAddr(bool* out) {
	if (vk::vkGetInstanceProcAddr_0) *out = true;
	else *out = false;
}

void checkReadFlag(bool* out) {
	FILE* readFlag = SaltySDCore_fopen("sdmc:/SaltySD/flags/blockfilestats.flag", "rb");
	if (readFlag) {
		SaltySDCore_fclose(readFlag);
		*out = false;
	}
	else {
		*out = true;
	}
};

extern "C" {

	void NX_FPS(SharedMemory* _sharedmemory, uint32_t* _sharedOperationMode) {

		sharedOperationMode = _sharedOperationMode;
		SaltySDCore_printf("NX-FPS: alive\n");
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
