#include <switch_min.h>
#include "saltysd_ipc.h"
#include "saltysd_dynamic.h"
#include "saltysd_core.h"
#include <cstdlib>
#include <cmath>
#include "lock.hpp"
#include <algorithm>

struct NVNTexture {
	char reserved[0x80];
};

struct NVNTextureView {
	char reserved[0x40];
};

struct NVNTextureBuilder {
	char reserved[0x80];
};

struct NVNWindow {
	char reserved[0x180];
};

struct NVNDevice {
	char reserved[0x3000];
};

struct nvnWindowBuilder {
	const char reserved[16];
	uint8_t numBufferedFrames;
};

typedef int NVNtextureFlags;
typedef int NVNtextureTarget;
typedef int NVNformat;
typedef int NVNmemoryPoolFlags;

struct NVNViewport {
	float x;
	float y;
	float width;
	float height;
};

struct glViewportArray {
	float x;
	float y;
	float width;
	float height;
};

typedef u32 (*nvnBootstrapLoader_0)(const char * nvnName);
typedef void* (*nvnCommandBufferSetRenderTargets_0)(void* cmdBuf, int numTextures, NVNTexture** texture, NVNTextureView** textureView, NVNTexture* depth, NVNTextureView* depthView);
typedef void* (*nvnCommandBufferSetViewport_0)(void* cmdBuf, int x, int y, int width, int height);
typedef void* (*nvnCommandBufferSetViewports_0)(void* cmdBuf, int start, int count, NVNViewport* viewports);
typedef void* (*nvnCommandBufferSetDepthRange_0)(void* cmdBuf, float s0, float s1);
typedef u16 (*nvnTextureGetWidth_0)(NVNTexture* texture);
typedef u16 (*nvnTextureGetHeight_0)(NVNTexture* texture);
typedef u32 (*nvnTextureGetFormat_0)(NVNTexture* texture);
typedef void (*nvnQueuePresentTexture_0)(const void* _this, const void* unk2_1, const int index);
typedef uintptr_t (*GetProcAddress)(const void* unk1_a, const char * nvnFunction_a);
typedef void (*nvnBuilderSetTextures_0)(const nvnWindowBuilder* nvnWindowBuilder, int buffers, const NVNTexture** texturesBuffer);
typedef void (*nvnWindowSetNumActiveTextures_0)(const NVNWindow* nvnWindow, int buffers);
typedef int (*nvnWindowGetNumActiveTextures_0)(const NVNWindow* nvnWindow);
typedef bool (*nvnWindowInitialize_0)(const NVNWindow* nvnWindow, struct nvnWindowBuilder* windowBuilder);
typedef void* (*nvnWindowAcquireTexture_0)(const NVNWindow* nvnWindow, const void* nvnSync, const void* index);
typedef void (*nvnSetPresentInterval_0)(const NVNWindow* nvnWindow, int mode);
typedef int (*nvnGetPresentInterval_0)(const NVNWindow* nvnWindow);
typedef void* (*nvnSyncWait_0)(const void* _this, uint64_t timeout_ns);
typedef void (*nvnQueueFinish_0)(const void* nvnQueue);

typedef u32 (*eglGetProcAddress_0)(const char* eglName);
typedef bool (*eglSwapBuffers_0)(const void* EGLDisplay, const void* EGLSurface);
typedef bool (*eglSwapInterval_0)(const void* EGLDisplay, int interval);
typedef void (*glViewport_0)(int x, int y, uint width, uint height);
typedef void (*glViewportArrayvNV_0)(uint firstViewport, uint viewportCount, const glViewportArray* pViewports);
typedef void (*glViewportArrayvOES_0)(uint firstViewport, uint viewportCount, const glViewportArray* pViewports);
typedef void (*glViewportIndexedfNV_0)(uint index, float x, float y, float width, float height);
typedef void (*glViewportIndexedfvNV_0)(uint index, const glViewportArray* pViewports);
typedef void (*glViewportIndexedfOES_0)(uint index, float x, float y, float width, float height);
typedef void (*glViewportIndexedfvOES_0)(uint index, const glViewportArray* pViewports);
typedef void (*glViewportArrayv_0)(uint firstViewport, uint viewportCount, const glViewportArray* pViewports);
typedef void (*glViewportIndexedf_0)(uint index, float x, float y, float width, float height);
typedef void (*glViewportIndexedfv_0)(uint index, const glViewportArray* pViewports);

typedef s32 (*vkQueuePresentKHR_0)(const void* vkQueue, const void* VkPresentInfoKHR);
typedef s32 (*_ZN11NvSwapchain15QueuePresentKHREP9VkQueue_TPK16VkPresentInfoKHR_0)(const void* VkQueue_T, const void* VkPresentInfoKHR);
typedef void* (*_vkGetInstanceProcAddr_0)(void* instance, const char* vkFunction);
typedef void* (*vkGetDeviceProcAddr_0)(void* device, const char* vkFunction);

typedef u64 (*_ZN2nn2os17ConvertToTimeSpanENS0_4TickE_0)(u64 tick);
typedef u64 (*_ZN2nn2os13GetSystemTickEv_0)();
typedef void (*_ZN2nn2os13GetSystemTickEv_1)(u64* tick);
typedef u64 (*_ZN2nn2os22GetSystemTickFrequencyEv_0)();
typedef void (*_ZN2nn2os22GetSystemTickFrequencyEv_1)(u64* tickfrequency);
typedef AppletFocusState (*GetCurrentFocusState_0)();
typedef void (*SetFocusHandlingMode_0)(AppletFocusHandlingMode mode);
typedef u32 (*FileAccessorRead_0)(void* fileHandle, size_t* bytesRead, int64_t position, void* buffer, size_t readBytes, unsigned int* ReadOption);

struct {
	uintptr_t nvnBootstrapLoader;
	uintptr_t nvnDeviceGetProcAddress;
	uintptr_t nvnQueuePresentTexture;
	uintptr_t nvnWindowSetPresentInterval;
	uintptr_t nvnWindowGetPresentInterval;
	uintptr_t nvnWindowBuilderSetTextures;
	uintptr_t nvnWindowAcquireTexture;
	uintptr_t nvnSyncWait;
	uintptr_t nvnWindowSetNumActiveTextures;
	uintptr_t nvnWindowGetNumActiveTextures;
	uintptr_t nvnWindowInitialize;
	uintptr_t nvnTextureGetWidth;
	uintptr_t nvnTextureGetHeight;
	uintptr_t nvnTextureGetFormat;
	uintptr_t nvnCommandBufferSetRenderTargets;
	uintptr_t nvnCommandBufferSetViewport;
	uintptr_t nvnCommandBufferSetViewports;
	uintptr_t nvnCommandBufferSetDepthRange;
	uintptr_t nvnQueueFinish;

	uintptr_t eglGetProcAddress;
	uintptr_t eglSwapBuffers;
	uintptr_t eglSwapInterval;
	uintptr_t glViewport;
	uintptr_t glViewportArrayvNV;
	uintptr_t glViewportArrayvOES;
	uintptr_t glViewportIndexedfNV;
	uintptr_t glViewportIndexedfvNV;
	uintptr_t glViewportIndexedfOES;
	uintptr_t glViewportIndexedfvOES;
	uintptr_t glViewportArrayv;
	uintptr_t glViewportIndexedf;
	uintptr_t glViewportIndexedfv;

	uintptr_t vkQueuePresentKHR;
	uintptr_t nvSwapchainQueuePresentKHR;
	uintptr_t vkGetInstanceProcAddr;
	uintptr_t vkGetDeviceProcAddr;

	uintptr_t ConvertToTimeSpan;
	uintptr_t GetSystemTick;
	uintptr_t GetSystemTickFrequency;
	uintptr_t ReferSymbol;
	uintptr_t GetCurrentFocusState;
	uintptr_t SetFocusHandlingMode;
	uintptr_t FileAccessorRead;
} Address_weaks;

ptrdiff_t SharedMemoryOffset = 1234;
uint8_t* configBuffer = 0;
size_t configSize = 0;
Result configRC = 1;

static uint32_t* sharedOperationMode = 0;

Result readConfig(const char* path, uint8_t** output_buffer) {
	FILE* patch_file = SaltySDCore_fopen(path, "rb");
	SaltySDCore_fseek(patch_file, 0, 2);
	configSize = SaltySDCore_ftell(patch_file);
	SaltySDCore_fseek(patch_file, 8, 0);
	uint32_t header_size = 0;
	SaltySDCore_fread(&header_size, 0x4, 1, patch_file);
	uint8_t* buffer = (uint8_t*)calloc(1, header_size);
	SaltySDCore_fseek(patch_file, 0, 0);
	SaltySDCore_fread(buffer, header_size, 1, patch_file);
	if (SaltySDCore_ftell(patch_file) != header_size || !LOCK::isValid(buffer, header_size)) {
		SaltySDCore_fclose(patch_file);
		free(buffer);
		return 1;
	}
	if (LOCK::masterWrite) {
		Result ret = LOCK::applyMasterWrite(patch_file, header_size - 4);
		if (R_FAILED(ret))  {
			SaltySDCore_fclose(patch_file);
			return ret;
		}
		configSize = *(uint32_t*)(&(buffer[header_size - 4]));
	}
	free(buffer);
	buffer = (uint8_t*)calloc(1, configSize);
	SaltySDCore_fseek(patch_file, 0, 0);
	SaltySDCore_fread(buffer, configSize, 1, patch_file);
	SaltySDCore_fclose(patch_file);
	*output_buffer = buffer;
	return 0;
}

struct resolutionCalls {
	uint16_t width;
	uint16_t height;
	uint16_t calls;
};

bool resolutionLookup = false;
bool setNumActiveTexturesDetected = false;
uint8_t amountOfAvailableBuffers = 0;

struct NxFpsSharedBlock {
	uint32_t MAGIC;
	uint8_t FPS;
	float FPSavg;
	bool pluginActive;
	uint8_t FPSlocked;
	uint8_t FPSmode;
	uint8_t ZeroSync;
	uint8_t patchApplied;
	uint8_t API;
	uint32_t FPSticks[10];
	uint8_t Buffers;
	uint8_t SetBuffers;
	uint8_t ActiveBuffers;
	uint8_t SetActiveBuffers;
	union {
		struct {
			bool handheld: 1;
			bool docked: 1;
			unsigned int reserved: 6;
		} PACKED ds;
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
	uint8_t currentFocusState;
} PACKED;

static_assert(sizeof(NxFpsSharedBlock) == 175);

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

resolutionCalls m_resolutionRenderCalls[8] = {0};
resolutionCalls m_resolutionViewportCalls[8] = {0};

inline uintptr_t getMainAddress() {
	MemoryInfo memoryinfo = {0};
	u32 pageinfo = 0;

	uintptr_t base_address = SaltySDCore_getCodeStart() + 0x4000;
	for (size_t i = 0; i < 3; i++) {
		Result rc = svcQueryMemory(&memoryinfo, &pageinfo, base_address);
		if (R_FAILED(rc)) return 0;
		if ((memoryinfo.addr == base_address) && ((memoryinfo.perm & Perm_Rx) == Perm_Rx))
			return base_address;
		base_address = memoryinfo.addr+memoryinfo.size;
	}

	return 0;
}

namespace nn {
	Result FileAccessorRead(void* fileHandle, size_t* bytesRead, int64_t position, void* buffer, size_t readBytes, unsigned int* ReadOption) {
		size_t bytesRead_impl = 0;
		if (!bytesRead)
			bytesRead = &bytesRead_impl;
		Result ret = ((FileAccessorRead_0)(Address_weaks.FileAccessorRead))(fileHandle, bytesRead, position, buffer, readBytes, ReadOption);
		fileBytesRead += *bytesRead;
		return ret;
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
		return ((SetFocusHandlingMode_0)(Address_weaks.SetFocusHandlingMode))(mode);
	}

	AppletFocusState getCurrentFocusState() {
		AppletFocusState state = ((GetCurrentFocusState_0)(Address_weaks.GetCurrentFocusState))();
		if (Shared->currentFocusState != state) Shared->currentFocusState = state;
		return state;
	}
}

namespace Utils {
	inline uint64_t _getSystemTick() {
		#if SWITCH
			return armGetSystemTick();
		#elif defined(SWITCH32) || defined(OUNCE32)
			u64 tick = 0;
			((_ZN2nn2os13GetSystemTickEv_1)(Address_weaks.GetSystemTick))(&tick);
			return tick;
		#else
			return ((_ZN2nn2os13GetSystemTickEv_0)(Address_weaks.GetSystemTick))();
		#endif
	}

	inline uint64_t _convertToTimeSpan(uint64_t tick) {
		#if defined(SWITCH) || defined(SWITCH32)
			return armTicksToNs(tick);
		#elif defined(OUNCE) || defined(OUNCE32)
			return tick << 5;
		#else
			return uint64_t((double)tick / ((double)(systemtickfrequency) / 1000000000.d));
		#endif
	}

	inline AppletFocusState _getCurrentFocusState() {
		return nn::getCurrentFocusState();
	}
}

namespace NX_FPS_Math {
	uint8_t FPS_temp = 0;
	uint64_t starttick = 0;
	uint64_t starttick2 = 0;
	uint64_t endtick = 0;
	uint64_t frameend = 0;
	uint64_t frameavg = 0;
	uint8_t FPSlock = 0;
	int32_t FPStiming = 0;
	uint8_t FPStickItr = 0;
	uint8_t range = 0;
	bool FPSlock_delayed = false;
	bool old_force = false;
	int32_t new_fpslock = 0;

	void PreFrame() {
		new_fpslock = (LOCK::overwriteRefreshRate ? LOCK::overwriteRefreshRate : ((*sharedOperationMode == 1) ? (Shared -> FPSlockedDocked) : (Shared -> FPSlocked)));
		if (old_force != (Shared -> forceOriginalRefreshRate)) {
			if (*sharedOperationMode == 1 && !(Shared -> dontForce60InDocked))
				svcSleepThread(LOCK::DockedRefreshRateDelay);
			old_force = (Shared -> forceOriginalRefreshRate);
		}

		if ((FPStiming && !LOCK::blockDelayFPS && (new_fpslock && new_fpslock < (Shared -> currentRefreshRate)))) {
			uint64_t tick = Utils::_getSystemTick();
			if ((int64_t)(tick - frameend) < (FPStiming + (range * 20))) {
				FPSlock_delayed = true;
			}
			while ((int64_t)(tick - frameend) < FPStiming + (range * 20)) {
				svcSleepThread(-2);
				svcSleepThread(10000);
				tick = Utils::_getSystemTick();
			}
		}
	}

	void PostFrame() {
		Shared->frameNumber++;
		endtick = Utils::_getSystemTick();
		uint64_t framedelta = endtick - frameend;
		frameavg = ((9*frameavg) + framedelta) / 10;
		Stats.FPSavg = systemtickfrequency / (float)frameavg;

		if (FPSlock_delayed && FPStiming) {
			if (Stats.FPSavg > ((float)new_fpslock)) {
				if (range < 200) {
					range++;
				}
			}
			else if ((std::lround(Stats.FPSavg) == new_fpslock) && (Stats.FPSavg < (float)new_fpslock)) {
				if (range > 0) {
					range--;
				}
			}
		}

		frameend = endtick;
		
		FPS_temp++;
		uint64_t deltatick = endtick - starttick;

		Shared -> FPSticks[FPStickItr++] = framedelta;
		FPStickItr %= 10;

		nn::focusHandlingOverwrite = true;
		AppletFocusState state = Utils::_getCurrentFocusState();
		if (state == AppletFocusState_NotFocusedHomeSleep) {
			if (!Shared->forceSuspend) {
				nn::setFocusHandlingMode(nn::defaultFocusHandlingMode);
			}
			else {
				nn::setFocusHandlingMode(AppletFocusHandlingMode_SuspendHomeSleep);
			}
			svcSleepThread(1);
		}
		else if (state == AppletFocusState_NotFocusedLibraryApplet) {
			nn::setFocusHandlingMode(nn::defaultFocusHandlingMode);
			svcSleepThread(1);
		}
		else {
			nn::setFocusHandlingMode(AppletFocusHandlingMode_NoSuspend);
		}
		nn::focusHandlingOverwrite = false;

		if (!configRC && FPSlock) {
			LOCK::applyPatch(configBuffer, FPSlock, (Shared -> currentRefreshRate));
		}

		if (deltatick > systemtickfrequency) {
			if (Address_weaks.FileAccessorRead) {
				float seconds = (float)(Utils::_convertToTimeSpan(deltatick)) / 1000000000.f;
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
		T ratio = (m_width * 10) / m_height;
		if (ratio >= (T)6 && ratio <= (T)18) {
			union {
				struct {
					uint16_t width;
					uint16_t height;
				} values;
				uint32_t value;
			} value_to_compare;

			static_assert(sizeof(value_to_compare) == sizeof(m_resolutionViewportCalls[0].width) + sizeof(m_resolutionViewportCalls[0].height));

			value_to_compare.values = {(uint16_t)m_width, (uint16_t)m_height};

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
		T ratio = (m_width * 10) / m_height;
		if (ratio >= (T)6 && ratio <= (T)18) {
			union {
				struct {
					uint16_t width;
					uint16_t height;
				} values;
				uint32_t value;
			} value_to_compare;

			static_assert(sizeof(value_to_compare) == sizeof(m_resolutionRenderCalls[0].width) + sizeof(m_resolutionRenderCalls[0].height));

			value_to_compare.values = {(uint16_t)m_width, (uint16_t)m_height};

			for (size_t i = 0; i < 8; i++) {
				if (!m_resolutionRenderCalls[i].calls) {
					memcpy(&m_resolutionRenderCalls[i], &value_to_compare, sizeof(value_to_compare));
					m_resolutionRenderCalls[i].calls = 1;
					break;
				}
				if (!memcmp(&value_to_compare.value, &m_resolutionRenderCalls[i], sizeof(value_to_compare))) {
					m_resolutionRenderCalls[i].calls++;
					break;
				}
			}
		}		
	}
}

namespace vk {
	namespace nvSwapchain { 
	int32_t QueuePresent (const void* VkQueue_T, const void* VkPresentInfoKHR) {
		
		if (!NX_FPS_Math::starttick) {
			(Shared -> API) = 3;
			NX_FPS_Math::starttick = Utils::_getSystemTick();
			NX_FPS_Math::starttick2 = NX_FPS_Math::starttick;
		}
		
		NX_FPS_Math::PreFrame();
		int32_t vulkanResult = ((_ZN11NvSwapchain15QueuePresentKHREP9VkQueue_TPK16VkPresentInfoKHR_0)(Address_weaks.nvSwapchainQueuePresentKHR))(VkQueue_T, VkPresentInfoKHR);
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
	}}

	int32_t QueuePresent (const void* VkQueue, const void* VkPresentInfoKHR) {

		if (!NX_FPS_Math::starttick) {
			(Shared -> API) = 3;
			NX_FPS_Math::starttick = Utils::_getSystemTick();
			NX_FPS_Math::starttick2 = NX_FPS_Math::starttick;
		}
		
		NX_FPS_Math::PreFrame();

		int32_t vulkanResult = ((vkQueuePresentKHR_0)(Address_weaks.vkQueuePresentKHR))(VkQueue, VkPresentInfoKHR);
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

	void* GetDeviceProcAddr(void* device, const char* vkFunction) {
		if (!strcmp("vkQueuePresentKHR", vkFunction)) {
			Address_weaks.vkQueuePresentKHR = (uintptr_t)((vkGetDeviceProcAddr_0)(Address_weaks.vkGetDeviceProcAddr))(device, vkFunction);
			return (void*)&vk::QueuePresent;
		}
		if (!strcmp("vkGetDeviceProcAddr", vkFunction)) {
			Address_weaks.vkGetDeviceProcAddr = (uintptr_t)((vkGetDeviceProcAddr_0)(Address_weaks.vkGetDeviceProcAddr))(device, vkFunction);
			return (void*)&vk::GetDeviceProcAddr;
		}
		return ((vkGetDeviceProcAddr_0)(Address_weaks.vkGetDeviceProcAddr))(device, vkFunction);
	}

	void* GetInstanceProcAddr(void* instance, const char* vkFunction) {
		if (!strcmp("vkQueuePresentKHR", vkFunction)) {
			Address_weaks.vkQueuePresentKHR = (uintptr_t)((_vkGetInstanceProcAddr_0)(Address_weaks.vkGetInstanceProcAddr))(instance, vkFunction);
			return (void*)&vk::QueuePresent;
		}
		if (!strcmp("vkGetDeviceProcAddr", vkFunction)) {
			Address_weaks.vkGetDeviceProcAddr = (uintptr_t)((_vkGetInstanceProcAddr_0)(Address_weaks.vkGetInstanceProcAddr))(instance, vkFunction);
			return (void*)&vk::GetDeviceProcAddr;
		}
		return ((_vkGetInstanceProcAddr_0)(Address_weaks.vkGetInstanceProcAddr))(instance, vkFunction);
	}
}

namespace EGL {

	#define EGL_MIN_SWAP_INTERVAL 0
	#define EGL_MAX_SWAP_INTERVAL 4

	bool Interval(const void* EGLDisplay, int interval) {
		bool result = false;
		if (!changeFPS) {
			result = ((eglSwapInterval_0)(Address_weaks.eglSwapInterval))(EGLDisplay, interval);
			changedFPS = false;
			if (result == true) {
				(Shared -> FPSmode) = std::clamp(interval, EGL_MIN_SWAP_INTERVAL, EGL_MAX_SWAP_INTERVAL);
			}
		}
		else if (interval < 0) {
			interval *= -1;
			if ((Shared -> FPSmode) != interval) {
				result = ((eglSwapInterval_0)(Address_weaks.eglSwapInterval))(EGLDisplay, interval);
				if (result == true)
					(Shared -> FPSmode) = interval;
			}
			changedFPS = true;
		}
		return result;
	}

	bool Swap (const void* EGLDisplay, const void* EGLSurface) {

		if (!NX_FPS_Math::starttick) {
			(Shared -> API) = 2;
			NX_FPS_Math::starttick = Utils::_getSystemTick();
			NX_FPS_Math::starttick2 = NX_FPS_Math::starttick;
		}
		
		NX_FPS_Math::PreFrame();
		
		bool result = ((eglSwapBuffers_0)(Address_weaks.eglSwapBuffers))(EGLDisplay, EGLSurface);
		if (result == true) NX_FPS_Math::PostFrame();

		if (!NX_FPS_Math::new_fpslock) {
			NX_FPS_Math::FPStiming = 0;
			NX_FPS_Math::FPSlock = 0;
			changeFPS = false;
		}
		else {
			changeFPS = true;
			auto currentRefreshRate = (Shared -> currentRefreshRate);
			NX_FPS_Math::FPSlock = ((*sharedOperationMode == 1) ? (Shared -> FPSlockedDocked) : (Shared -> FPSlocked));
			if (NX_FPS_Math::new_fpslock <= (currentRefreshRate ? (currentRefreshRate / 4) : 15)) {
				if ((Shared -> FPSmode) != 4)
					EGL::Interval(EGLDisplay, -4);
				if (NX_FPS_Math::new_fpslock != (currentRefreshRate ? (currentRefreshRate / 4) : 15)) {
					NX_FPS_Math::FPStiming = (systemtickfrequency/NX_FPS_Math::new_fpslock) - 6000;
				}
				else NX_FPS_Math::FPStiming = 0;			
			}
			else if (NX_FPS_Math::new_fpslock <= (currentRefreshRate ? (currentRefreshRate / 3) : 20)) {
				if ((Shared -> FPSmode) != 3)
					EGL::Interval(EGLDisplay, -3);
				if (NX_FPS_Math::new_fpslock != (currentRefreshRate ? (currentRefreshRate / 3) : 20)) {
					NX_FPS_Math::FPStiming = (systemtickfrequency/NX_FPS_Math::new_fpslock) - 6000;
				}
				else NX_FPS_Math::FPStiming = 0;			
			}
			else if (NX_FPS_Math::new_fpslock <= (currentRefreshRate ? (currentRefreshRate / 2) : 30)) {
				if ((Shared -> FPSmode) != 2)
					EGL::Interval(EGLDisplay, -2);
				if (NX_FPS_Math::new_fpslock != (currentRefreshRate ? (currentRefreshRate / 2) : 30)) {
					NX_FPS_Math::FPStiming = (systemtickfrequency/NX_FPS_Math::new_fpslock) - 6000;
				}
				else NX_FPS_Math::FPStiming = 0;			
			}
			else if (NX_FPS_Math::new_fpslock > (currentRefreshRate ? (currentRefreshRate / 2) : 30)) {
				if ((Shared -> FPSmode) != 1)
					EGL::Interval(EGLDisplay, -1);
				if (NX_FPS_Math::new_fpslock != (currentRefreshRate ? currentRefreshRate : 60)) {
					NX_FPS_Math::FPStiming = (systemtickfrequency/NX_FPS_Math::new_fpslock) - 6000;
				}
				else NX_FPS_Math::FPStiming = 0;
			}
		}

		return result;
	}

	namespace Common {
		void ViewportArrayv(uint firstViewport, uint viewportCount, const glViewportArray* pViewports, uintptr_t pointer) {
			if (resolutionLookup) for (uint i = firstViewport; i < firstViewport+viewportCount; i++) {
				if (pViewports[i].height > 1.f && pViewports[i].width > 1.f && pViewports[i].x == 0.f && pViewports[i].y == 0.f) {
					NX_FPS_Math::addResToViewports(pViewports[i].width, pViewports[i].height);
				}
			}
			return ((glViewportArrayv_0)(pointer))(firstViewport, viewportCount, pViewports);
		}

		void ViewportIndexedf(uint index, float x, float y, float width, float height, uintptr_t pointer) {
			if (resolutionLookup && height > 1.f && width > 1.f && !x && !y) {
				NX_FPS_Math::addResToViewports(width, height);
			}
			return ((glViewportIndexedf_0)(pointer))(index, x, y, width, height);
		}

		void ViewportIndexedfv(uint i, const glViewportArray* pViewports, uintptr_t pointer) {
			if (resolutionLookup) {
				if (pViewports[i].height > 1.f && pViewports[i].width > 1.f && pViewports[i].x == 0.f && pViewports[i].y == 0.f) {
					NX_FPS_Math::addResToViewports(pViewports[i].width, pViewports[i].height);
				}
			}
			return ((glViewportIndexedfv_0)(pointer))(i, pViewports);
		}
	}

	void Viewport(int x, int y, uint width, uint height) {
		if (resolutionLookup && height > 1 && width > 1 && !x && !y) {
			NX_FPS_Math::addResToViewports(width, height);
		}
		return ((glViewport_0)(Address_weaks.glViewport))(x, y, width, height);
	}

	
	void ViewportArrayv(uint firstViewport, uint viewportCount, const glViewportArray* pViewports) {
		return EGL::Common::ViewportArrayv(firstViewport, viewportCount, pViewports, Address_weaks.glViewportArrayv);
	}

	void ViewportArrayvNV(uint firstViewport, uint viewportCount, const glViewportArray* pViewports) {
		return EGL::Common::ViewportArrayv(firstViewport, viewportCount, pViewports, Address_weaks.glViewportArrayvNV);
	}

	void ViewportArrayvOES(uint firstViewport, uint viewportCount, const glViewportArray* pViewports) {
		return EGL::Common::ViewportArrayv(firstViewport, viewportCount, pViewports, Address_weaks.glViewportArrayvOES);
	}

	void ViewportIndexedf(uint index, float x, float y, float width, float height) {
		return EGL::Common::ViewportIndexedf(index, x, y, width, height, Address_weaks.glViewportIndexedf);
	}

	void ViewportIndexedfNV(uint index, float x, float y, float width, float height) {
		return EGL::Common::ViewportIndexedf(index, x, y, width, height, Address_weaks.glViewportIndexedfNV);
	}

	void ViewportIndexedfOES(uint index, float x, float y, float width, float height) {
		return EGL::Common::ViewportIndexedf(index, x, y, width, height, Address_weaks.glViewportIndexedfOES);
	}

	void ViewportIndexedfv(uint i, const glViewportArray* pViewports) {
		return EGL::Common::ViewportIndexedfv(i, pViewports, Address_weaks.glViewportIndexedfv);
	}

	void ViewportIndexedfvNV(uint i, const glViewportArray* pViewports) {
		return EGL::Common::ViewportIndexedfv(i, pViewports, Address_weaks.glViewportIndexedfvNV);
	}

	void ViewportIndexedfvOES(uint i, const glViewportArray* pViewports) {
		return EGL::Common::ViewportIndexedfv(i, pViewports, Address_weaks.glViewportIndexedfvOES);
	}

	uintptr_t GetProc(const char* eglName) {
		if (!strcmp(eglName, "eglSwapInterval")) {
			if (!Address_weaks.eglSwapInterval) Address_weaks.eglSwapInterval = ((eglGetProcAddress_0)(Address_weaks.eglGetProcAddress))(eglName);
			return (uintptr_t)&Interval;
		}
		else if (!strcmp(eglName, "eglSwapBuffers")) {
			if (!Address_weaks.eglSwapBuffers) Address_weaks.eglSwapBuffers = ((eglGetProcAddress_0)(Address_weaks.eglGetProcAddress))(eglName);
			return (uintptr_t)&Swap;
		}
		else if (!strcmp(eglName, "glViewport")) {
			if (!Address_weaks.glViewport) Address_weaks.glViewport = ((eglGetProcAddress_0)(Address_weaks.eglGetProcAddress))(eglName);
			return (uintptr_t)&Viewport;
		}
		else if (!strcmp(eglName, "glViewportArrayv")) {
			if (!Address_weaks.glViewportArrayv) Address_weaks.glViewportArrayv = ((eglGetProcAddress_0)(Address_weaks.eglGetProcAddress))(eglName);
			return (uintptr_t)&ViewportArrayv;
		}
		else if (!strcmp(eglName, "glViewportArrayvNV")) {
			if (!Address_weaks.glViewportArrayvNV) Address_weaks.glViewportArrayvNV = ((eglGetProcAddress_0)(Address_weaks.eglGetProcAddress))(eglName);
			return (uintptr_t)&ViewportArrayvNV;
		}
		else if (!strcmp(eglName, "glViewportArrayvOES")) {
			if (!Address_weaks.glViewportArrayvOES) Address_weaks.glViewportArrayvOES = ((eglGetProcAddress_0)(Address_weaks.eglGetProcAddress))(eglName);
			return (uintptr_t)&ViewportArrayvOES;
		}
		else if (!strcmp(eglName, "glViewportIndexedf")) {
			if (!Address_weaks.glViewportIndexedf) Address_weaks.glViewportIndexedfNV = ((eglGetProcAddress_0)(Address_weaks.eglGetProcAddress))(eglName);
			return (uintptr_t)&ViewportIndexedf;
		}
		else if (!strcmp(eglName, "glViewportIndexedfNV")) {
			if (!Address_weaks.glViewportIndexedfNV) Address_weaks.glViewportIndexedfNV = ((eglGetProcAddress_0)(Address_weaks.eglGetProcAddress))(eglName);
			return (uintptr_t)&ViewportIndexedfNV;
		}
		else if (!strcmp(eglName, "glViewportIndexedfOES")) {
			if (!Address_weaks.glViewportIndexedfOES) Address_weaks.glViewportIndexedfOES = ((eglGetProcAddress_0)(Address_weaks.eglGetProcAddress))(eglName);
			return (uintptr_t)&ViewportIndexedfOES;
		}
		else if (!strcmp(eglName, "glViewportIndexedfv")) {
			if (!Address_weaks.glViewportIndexedfv) Address_weaks.glViewportIndexedfvNV = ((eglGetProcAddress_0)(Address_weaks.eglGetProcAddress))(eglName);
			return (uintptr_t)&ViewportIndexedfv;
		}
		else if (!strcmp(eglName, "glViewportIndexedfvNV")) {
			if (!Address_weaks.glViewportIndexedfvNV) Address_weaks.glViewportIndexedfvNV = ((eglGetProcAddress_0)(Address_weaks.eglGetProcAddress))(eglName);
			return (uintptr_t)&ViewportIndexedfvNV;
		}
		else if (!strcmp(eglName, "glViewportIndexedfvOES")) {
			if (!Address_weaks.glViewportIndexedfvOES) Address_weaks.glViewportIndexedfvOES = ((eglGetProcAddress_0)(Address_weaks.eglGetProcAddress))(eglName);
			return (uintptr_t)&ViewportIndexedfvOES;
		}
		return ((eglGetProcAddress_0)(Address_weaks.eglGetProcAddress))(eglName);
	}
}

namespace NVN {
	void* WindowSync = 0;

	bool WindowInitialize(const NVNWindow* nvnWindow, struct nvnWindowBuilder* windowBuilder) {
		if (!(Shared -> Buffers)) {
			(Shared -> Buffers) = windowBuilder -> numBufferedFrames;
			if ((Shared -> SetBuffers) >= 2 && (Shared -> SetBuffers) <= windowBuilder -> numBufferedFrames) {
				windowBuilder -> numBufferedFrames = (Shared -> SetBuffers);
			}
			(Shared -> ActiveBuffers) = windowBuilder -> numBufferedFrames;	
		}
		return ((nvnWindowInitialize_0)(Address_weaks.nvnWindowInitialize))(nvnWindow, windowBuilder);
	}

	//This function accepts pointer and how much frames is passed to framebuffer
	void WindowBuilderSetTextures(const nvnWindowBuilder* nvnWindowBuilder, int numBufferedFrames, const NVNTexture** nvnTextures) {
		(Shared -> Buffers) = numBufferedFrames;
		amountOfAvailableBuffers = numBufferedFrames;
		if ((Shared -> SetBuffers) >= 2 && (Shared -> SetBuffers) <= numBufferedFrames) {
			if (!setNumActiveTexturesDetected) numBufferedFrames = (Shared -> SetBuffers);
			else Shared->expectedSetBuffers = (Shared -> SetBuffers);
		}
		(Shared -> ActiveBuffers) = numBufferedFrames;
		return ((nvnBuilderSetTextures_0)(Address_weaks.nvnWindowBuilderSetTextures))(nvnWindowBuilder, numBufferedFrames, nvnTextures);
	}

	//This function can change on the fly how much frames from framebuffer game can use, it cannot be more than amount passed to nvnWindowBuilderSetTextures
	void WindowSetNumActiveTextures(const NVNWindow* nvnWindow, int numBufferedFrames) {
		if (numBufferedFrames < 0) {
			numBufferedFrames *= -1;
			((nvnWindowSetNumActiveTextures_0)(Address_weaks.nvnWindowSetNumActiveTextures))(nvnWindow, numBufferedFrames);
			(Shared -> ActiveBuffers) = ((nvnWindowGetNumActiveTextures_0)(Address_weaks.nvnWindowGetNumActiveTextures))(nvnWindow);
		}
		else {
			(Shared -> SetActiveBuffers) = numBufferedFrames;
			if (Shared->expectedSetBuffers > 0) return;
			if ((Shared -> SetBuffers) >= 2 && (Shared -> SetBuffers) <= (Shared -> Buffers)) {
				numBufferedFrames = (Shared -> SetBuffers);
			}
			(Shared -> ActiveBuffers) = numBufferedFrames;
		}
		return ((nvnWindowSetNumActiveTextures_0)(Address_weaks.nvnWindowSetNumActiveTextures))(nvnWindow, numBufferedFrames);
	}

	//This function changes amount of vsync events it must wait before frame is passed to display.
	//In case of 60 Hz, if mode is 2 game is blocked to 30 FPS
	void SetPresentInterval(const NVNWindow* nvnWindow, int mode) {
		if (mode < 0) {
			mode *= -1;
			if ((Shared -> FPSmode) != mode) {
				((nvnSetPresentInterval_0)(Address_weaks.nvnWindowSetPresentInterval))(nvnWindow, mode);
				(Shared -> FPSmode) = mode;
			}
			changedFPS = true;
		}
		else if (!changeFPS) {
			((nvnSetPresentInterval_0)(Address_weaks.nvnWindowSetPresentInterval))(nvnWindow, mode);
			changedFPS = false;
			(Shared -> FPSmode) = mode;
		}
		return;
	}

	//This function is used to wait until all events passed to nvnSync are signaled.
	//Some games are using it to make sure that double buffer vsync is maintained.
	void* SyncWait0(const void* _this, uint64_t timeout_ns) {
		uint64_t endFrameTick = 0;
		endFrameTick = Utils::_getSystemTick();
		if (_this == WindowSync && (Shared -> ActiveBuffers) == 2) {
			if ((Shared -> ZeroSync) == ZeroSyncType_Semi) {
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
		return ((nvnSyncWait_0)(Address_weaks.nvnSyncWait))(_this, timeout_ns);
	}

	//This function accepts which frame pushed to nvnWindowBuilderSetTexture should be shown on screen.
	//It pushes that frame into queue
	void PresentTexture(const void* _this, const NVNWindow* nvnWindow, const int index) {

		if (!NX_FPS_Math::starttick) {
			NX_FPS_Math::starttick = Utils::_getSystemTick();
			NX_FPS_Math::starttick2 = NX_FPS_Math::starttick;
			(Shared -> FPSmode) = (uint8_t)((nvnGetPresentInterval_0)(Address_weaks.nvnWindowGetPresentInterval))(nvnWindow);
		}
		
		NX_FPS_Math::PreFrame();
		((nvnQueuePresentTexture_0)(Address_weaks.nvnQueuePresentTexture))(_this, nvnWindow, index);
		NX_FPS_Math::PostFrame();

		if (setNumActiveTexturesDetected) {
			auto expectedBuffers = Shared->expectedSetBuffers;
			if ((expectedBuffers > 0) && (amountOfAvailableBuffers >= expectedBuffers) && (expectedBuffers != (Shared->ActiveBuffers))) {
				expectedBuffers *= -1;
				((nvnQueueFinish_0)(Address_weaks.nvnQueueFinish))(_this);
				WindowSetNumActiveTextures(nvnWindow, expectedBuffers);
			}
		}

		(Shared -> FPSmode) = (uint8_t)((nvnGetPresentInterval_0)(Address_weaks.nvnWindowGetPresentInterval))(nvnWindow);

		if (!NX_FPS_Math::new_fpslock) {
			NX_FPS_Math::FPStiming = 0;
			NX_FPS_Math::FPSlock = 0;
			changeFPS = false;
		}
		else {
			changeFPS = true;
			auto currentRefreshRate = (Shared -> currentRefreshRate);
			NX_FPS_Math::FPSlock = ((*sharedOperationMode == 1) ? (Shared -> FPSlockedDocked) : (Shared -> FPSlocked));
			if (NX_FPS_Math::new_fpslock <= (currentRefreshRate ? (currentRefreshRate / 4) : 15)) {
				if (((nvnGetPresentInterval_0)(Address_weaks.nvnWindowGetPresentInterval))(nvnWindow) != 4)
					NVN::SetPresentInterval(nvnWindow, -4);
				if ((NX_FPS_Math::new_fpslock != (currentRefreshRate ? (currentRefreshRate / 4) : 15))
				|| (Shared -> ZeroSync)) {
					NX_FPS_Math::FPStiming = (systemtickfrequency/NX_FPS_Math::new_fpslock) - 6000;
					if (NX_FPS_Math::new_fpslock == (currentRefreshRate ? (currentRefreshRate / 4) : 15)) {
						NX_FPS_Math::FPStiming -= 2000;
					}
				}
				else NX_FPS_Math::FPStiming = 0;			
			}
			else if (NX_FPS_Math::new_fpslock <= (currentRefreshRate ? (currentRefreshRate / 3) : 20)) {
				if (((nvnGetPresentInterval_0)(Address_weaks.nvnWindowGetPresentInterval))(nvnWindow) != 3)
					NVN::SetPresentInterval(nvnWindow, -3);
				if ((NX_FPS_Math::new_fpslock != (currentRefreshRate ? (currentRefreshRate / 3) : 20))
				|| (Shared -> ZeroSync)) {
					NX_FPS_Math::FPStiming = (systemtickfrequency/NX_FPS_Math::new_fpslock) - 6000;
					if (NX_FPS_Math::new_fpslock == (currentRefreshRate ? (currentRefreshRate / 3) : 20)) {
						NX_FPS_Math::FPStiming -= 2000;
					}
				}
				else NX_FPS_Math::FPStiming = 0;			
			}
			else if (NX_FPS_Math::new_fpslock <= (currentRefreshRate ? (currentRefreshRate / 2) : 30)) {
				if (((nvnGetPresentInterval_0)(Address_weaks.nvnWindowGetPresentInterval))(nvnWindow) != 2)
					NVN::SetPresentInterval(nvnWindow, -2);
				if (NX_FPS_Math::new_fpslock != (currentRefreshRate ? (currentRefreshRate / 2) : 30)
				|| (Shared -> ZeroSync)) {
					NX_FPS_Math::FPStiming = (systemtickfrequency/NX_FPS_Math::new_fpslock) - 6000;
					if (NX_FPS_Math::new_fpslock == (currentRefreshRate ? (currentRefreshRate / 2) : 30)) {
						NX_FPS_Math::FPStiming -= 2000;
					}
				}
				else NX_FPS_Math::FPStiming = 0;			
			}
			else if (NX_FPS_Math::new_fpslock > (currentRefreshRate ? (currentRefreshRate / 2) : 30)) {
				if (((nvnGetPresentInterval_0)(Address_weaks.nvnWindowGetPresentInterval))(nvnWindow) != 1) {
					NVN::SetPresentInterval(nvnWindow, -2); //This allows in game with glitched interval to unlock 60 FPS, f.e. WRC Generations
					NVN::SetPresentInterval(nvnWindow, -1);
				}
				if ((NX_FPS_Math::new_fpslock != (currentRefreshRate ? currentRefreshRate : 60))
				|| (Shared -> ZeroSync)) {
					NX_FPS_Math::FPStiming = (systemtickfrequency/NX_FPS_Math::new_fpslock) - 6000;
					if (NX_FPS_Math::new_fpslock == (currentRefreshRate ? currentRefreshRate : 60)) {
						NX_FPS_Math::FPStiming -= 2000;
					}
				}
				else NX_FPS_Math::FPStiming = 0;
			}
		}
		
		return;
	}

	//This function retrieves number of frame which is currently free to use.
	//Blocks execution if no frame is available at the moment
	void* AcquireTexture(const NVNWindow* nvnWindow, const void* nvnSync, const void* index) {
		if (WindowSync != nvnSync) {
			WindowSync = (void*)nvnSync;
		}
		void* ret = ((nvnWindowAcquireTexture_0)(Address_weaks.nvnWindowAcquireTexture))(nvnWindow, nvnSync, index);
		startFrameTick = Utils::_getSystemTick();
		return ret;
	}

	struct nvnCommandBuffer {
		char reserved[0x80];
	};

	//Sets resolutions and start point of passed to command buffer textures, they cannot be bigger than texture are originally
	void* CommandBufferSetViewports(nvnCommandBuffer* cmdBuf, int start, int count, NVNViewport* viewports) {
		if (resolutionLookup) for (int i = start; i < start+count; i++) {
			if (viewports[i].height > 1.f && viewports[i].width > 1.f && viewports[i].x == 0.f && viewports[i].y == 0.f) {
				NX_FPS_Math::addResToViewports(viewports[i].width, viewports[i].height);
			}
		}
		return ((nvnCommandBufferSetViewports_0)(Address_weaks.nvnCommandBufferSetViewports))(cmdBuf, start, count, viewports);
	}

	//Sets resolutions and start point of passed to command buffer textures, they cannot be bigger than texture are originally
	void* CommandBufferSetViewport(nvnCommandBuffer* cmdBuf, int x, int y, int width, int height) {
		if (resolutionLookup && height > 1 && width > 1 && !x && !y) {
			NX_FPS_Math::addResToViewports(width, height);
		}
		return ((nvnCommandBufferSetViewport_0)(Address_weaks.nvnCommandBufferSetViewport))(cmdBuf, x, y, width, height);
	}

	void* CommandBufferSetRenderTargets(nvnCommandBuffer* cmdBuf, int numTextures, NVNTexture** texture, NVNTextureView** textureView, NVNTexture* depthTexture, NVNTextureView* depthView) {
		if (resolutionLookup && depthTexture != NULL && texture != NULL) {
			uint16_t depth_width = ((nvnTextureGetWidth_0)(Address_weaks.nvnTextureGetWidth))(depthTexture);
			uint16_t depth_height = ((nvnTextureGetHeight_0)(Address_weaks.nvnTextureGetHeight))(depthTexture);
			int depth_format = ((nvnTextureGetFormat_0)(Address_weaks.nvnTextureGetFormat))(depthTexture);
			if (depth_width > 1 && depth_height > 1 && (depth_format >= 51 && depth_format <= 54)) {
				NX_FPS_Math::addResToRender(depth_width, depth_height);
			}
		}
		return ((nvnCommandBufferSetRenderTargets_0)(Address_weaks.nvnCommandBufferSetRenderTargets))(cmdBuf, numTextures, texture, textureView, depthTexture, depthView);
	}

	//It's used to retrieve pointer to function asked in second argument
	uintptr_t GetProcAddress0 (NVNDevice* nvnDevice, const char* nvnFunction) {
		uintptr_t address = ((GetProcAddress)(Address_weaks.nvnDeviceGetProcAddress))(nvnDevice, nvnFunction);
		if (!strcmp("nvnDeviceGetProcAddress", nvnFunction))
			return (uintptr_t)&NVN::GetProcAddress0;
		else if (!strcmp("nvnQueuePresentTexture", nvnFunction)) {
			if (!Address_weaks.nvnQueuePresentTexture) Address_weaks.nvnQueuePresentTexture = address;
			return (uintptr_t)&NVN::PresentTexture;
		}
		else if (!strcmp("nvnWindowAcquireTexture", nvnFunction)) {
			if (!Address_weaks.nvnWindowAcquireTexture) Address_weaks.nvnWindowAcquireTexture = address;
			return (uintptr_t)&NVN::AcquireTexture;
		}
		else if (!strcmp("nvnWindowSetPresentInterval", nvnFunction)) {
			if (!Address_weaks.nvnWindowSetPresentInterval) Address_weaks.nvnWindowSetPresentInterval = address;
			return (uintptr_t)&NVN::SetPresentInterval;
		}
		else if (!strcmp("nvnWindowGetPresentInterval", nvnFunction)) {
			if (!Address_weaks.nvnWindowGetPresentInterval) Address_weaks.nvnWindowGetPresentInterval = address;
		}
		else if (!strcmp("nvnWindowSetNumActiveTextures", nvnFunction)) {
			if (!Address_weaks.nvnWindowSetNumActiveTextures) Address_weaks.nvnWindowSetNumActiveTextures = address;
			setNumActiveTexturesDetected = true;
			Shared->expectedSetBuffers = 0;
			return (uintptr_t)&NVN::WindowSetNumActiveTextures;
		}
		else if (!strcmp("nvnWindowBuilderSetTextures", nvnFunction)) {
			if (!Address_weaks.nvnWindowBuilderSetTextures) Address_weaks.nvnWindowBuilderSetTextures = address;
			return (uintptr_t)&NVN::WindowBuilderSetTextures;
		}
		else if (!strcmp("nvnWindowInitialize", nvnFunction)) {
			if (!Address_weaks.nvnWindowInitialize) Address_weaks.nvnWindowInitialize = address;
			return (uintptr_t)&NVN::WindowInitialize;
		}
		else if (!strcmp("nvnSyncWait", nvnFunction)) {
			if (!Address_weaks.nvnSyncWait) Address_weaks.nvnSyncWait = address;
			return (uintptr_t)&NVN::SyncWait0;
		}
		else if (!strcmp("nvnCommandBufferSetRenderTargets", nvnFunction)) {
			if (!Address_weaks.nvnCommandBufferSetRenderTargets) Address_weaks.nvnCommandBufferSetRenderTargets = address;
			return (uintptr_t)&NVN::CommandBufferSetRenderTargets;
		}
		else if (!strcmp("nvnCommandBufferSetViewport", nvnFunction)) {
			if (!Address_weaks.nvnCommandBufferSetViewport) Address_weaks.nvnCommandBufferSetViewport = address;
			return (uintptr_t)&NVN::CommandBufferSetViewport;
		}
		else if (!strcmp("nvnCommandBufferSetViewports", nvnFunction)) {
			if (!Address_weaks.nvnCommandBufferSetViewports) Address_weaks.nvnCommandBufferSetViewports = address;
			return (uintptr_t)&NVN::CommandBufferSetViewports;
		}
		else if (!strcmp("nvnTextureGetWidth", nvnFunction)) {
			if (!Address_weaks.nvnTextureGetWidth) Address_weaks.nvnTextureGetWidth = address;
		}
		else if (!strcmp("nvnTextureGetHeight", nvnFunction)) {
			if (!Address_weaks.nvnTextureGetHeight) Address_weaks.nvnTextureGetHeight = address;
		}
		else if (!strcmp("nvnTextureGetFormat", nvnFunction)) {
			if (!Address_weaks.nvnTextureGetFormat) Address_weaks.nvnTextureGetFormat = address;
		}
		else if (!strcmp("nvnQueueFinish", nvnFunction)) {
			if (!Address_weaks.nvnQueueFinish) Address_weaks.nvnQueueFinish = address;
		}
		else if (!strcmp("nvnWindowGetNumActiveTextures", nvnFunction)) {
			if (!Address_weaks.nvnWindowGetNumActiveTextures) Address_weaks.nvnWindowGetNumActiveTextures = address;
		}
		return address;
	}

	//It's the only exposed nvn function, used to retrieve only nvnDeviceGetProcAddress
	uintptr_t BootstrapLoader_1(const char* nvnName) {
		if (strcmp(nvnName, "nvnDeviceGetProcAddress") == 0) {
			(Shared -> API) = 1;
			if (!Address_weaks.nvnDeviceGetProcAddress) Address_weaks.nvnDeviceGetProcAddress = ((nvnBootstrapLoader_0)(Address_weaks.nvnBootstrapLoader))("nvnDeviceGetProcAddress");
			return (uintptr_t)&NVN::GetProcAddress0;
		}
		uintptr_t ptrret = ((nvnBootstrapLoader_0)(Address_weaks.nvnBootstrapLoader))(nvnName);
		return ptrret;
	}
}

extern "C" {

	void NX_FPS(SharedMemory* _sharedmemory, uint32_t* _sharedOperationMode) {
		sharedOperationMode = _sharedOperationMode;
		SaltySDCore_printf("NX-FPS: alive\n");
		LOCK::mappings.main_start = getMainAddress();
		SaltySDCore_printf("NX-FPS: found main at: 0x%lX\n", LOCK::mappings.main_start);
		Result ret = SaltySD_CheckIfSharedMemoryAvailable(&SharedMemoryOffset, sizeof(NxFpsSharedBlock));
		SaltySDCore_printf("NX-FPS: ret: 0x%X\n", ret);
		if (!ret) {
			SaltySDCore_printf("NX-FPS: MemoryOffset: %d\n", SharedMemoryOffset);

			Shared = (NxFpsSharedBlock*)((uintptr_t)shmemGetAddr(_sharedmemory) + SharedMemoryOffset);
			Shared -> MAGIC = 0x465053;
			Shared->expectedSetBuffers = -1;
			
			Address_weaks.nvnBootstrapLoader = SaltySDCore_FindSymbolBuiltin("nvnBootstrapLoader");
			Address_weaks.eglSwapBuffers = SaltySDCore_FindSymbolBuiltin("eglSwapBuffers");
			Address_weaks.eglSwapInterval = SaltySDCore_FindSymbolBuiltin("eglSwapInterval");
			Address_weaks.vkQueuePresentKHR = SaltySDCore_FindSymbolBuiltin("vkQueuePresentKHR");
			Address_weaks.nvSwapchainQueuePresentKHR = SaltySDCore_FindSymbolBuiltin("_ZN11NvSwapchain15QueuePresentKHREP9VkQueue_TPK16VkPresentInfoKHR");
			Address_weaks.ConvertToTimeSpan = SaltySDCore_FindSymbolBuiltin("_ZN2nn2os17ConvertToTimeSpanENS0_4TickE");
			Address_weaks.GetSystemTick = SaltySDCore_FindSymbolBuiltin("_ZN2nn2os13GetSystemTickEv");
			Address_weaks.GetSystemTickFrequency = SaltySDCore_FindSymbolBuiltin("_ZN2nn2os22GetSystemTickFrequencyEv");
			Address_weaks.eglGetProcAddress = SaltySDCore_FindSymbolBuiltin("eglGetProcAddress");
			Address_weaks.vkGetInstanceProcAddr = SaltySDCore_FindSymbolBuiltin("vkGetInstanceProcAddr");
			Address_weaks.glViewport = SaltySDCore_FindSymbolBuiltin("glViewport");
			Address_weaks.glViewportArrayvNV = SaltySDCore_FindSymbolBuiltin("glViewportArrayvNV");
			Address_weaks.glViewportArrayvOES = SaltySDCore_FindSymbolBuiltin("glViewportArrayvOES");
			Address_weaks.glViewportIndexedfNV = SaltySDCore_FindSymbolBuiltin("glViewportIndexedfNV");
			Address_weaks.glViewportIndexedfOES = SaltySDCore_FindSymbolBuiltin("glViewportIndexedfOES");
			Address_weaks.glViewportIndexedfvNV = SaltySDCore_FindSymbolBuiltin("glViewportIndexedfvNV");
			Address_weaks.glViewportIndexedfvOES = SaltySDCore_FindSymbolBuiltin("glViewportIndexedfvOES");
			Address_weaks.glViewportArrayv = SaltySDCore_FindSymbolBuiltin("glViewportArrayv");
			Address_weaks.glViewportIndexedf = SaltySDCore_FindSymbolBuiltin("glViewportIndexedf");
			Address_weaks.glViewportIndexedfv = SaltySDCore_FindSymbolBuiltin("glViewportIndexedfv");
			Address_weaks.GetCurrentFocusState = SaltySDCore_FindSymbolBuiltin("_ZN2nn2oe20GetCurrentFocusStateEv");
			Address_weaks.SetFocusHandlingMode = SaltySDCore_FindSymbolBuiltin("_ZN2nn2oe20SetFocusHandlingModeENS0_17FocusHandlingModeE");
			SaltySDCore_ReplaceImport("nvnBootstrapLoader", (void*)NVN::BootstrapLoader_1);
			SaltySDCore_ReplaceImport("eglSwapBuffers", (void*)EGL::Swap);
			SaltySDCore_ReplaceImport("eglSwapInterval", (void*)EGL::Interval);
			SaltySDCore_ReplaceImport("eglGetProcAddress", (void*)EGL::GetProc);
			SaltySDCore_ReplaceImport("glViewport", (void*)EGL::Viewport);
			SaltySDCore_ReplaceImport("glViewportArrayvNV", (void*)EGL::ViewportArrayvNV);
			SaltySDCore_ReplaceImport("glViewportArrayvOES", (void*)EGL::ViewportArrayvOES);
			SaltySDCore_ReplaceImport("glViewportIndexedfNV", (void*)EGL::ViewportIndexedfNV);
			SaltySDCore_ReplaceImport("glViewportIndexedfOES", (void*)EGL::ViewportIndexedfOES);
			SaltySDCore_ReplaceImport("glViewportIndexedfvNV", (void*)EGL::ViewportIndexedfvNV);
			SaltySDCore_ReplaceImport("glViewportIndexedfvOES", (void*)EGL::ViewportIndexedfvOES);
			SaltySDCore_ReplaceImport("glViewportArrayv", (void*)EGL::ViewportArrayv);
			SaltySDCore_ReplaceImport("glViewportIndexedf", (void*)EGL::ViewportIndexedf);
			SaltySDCore_ReplaceImport("glViewportIndexedfv", (void*)EGL::ViewportIndexedfv);
			SaltySDCore_ReplaceImport("vkQueuePresentKHR", (void*)vk::QueuePresent);
			SaltySDCore_ReplaceImport("_ZN11NvSwapchain15QueuePresentKHREP9VkQueue_TPK16VkPresentInfoKHR", (void*)vk::nvSwapchain::QueuePresent);
			SaltySDCore_ReplaceImport("vkGetInstanceProcAddr", (void*)vk::GetInstanceProcAddr);
			SaltySDCore_ReplaceImport("_ZN2nn2oe20GetCurrentFocusStateEv", (void*)nn::getCurrentFocusState);
			SaltySDCore_ReplaceImport("_ZN2nn2oe20SetFocusHandlingModeENS0_17FocusHandlingModeE", (void*)nn::setFocusHandlingMode);
			FILE* readFlag = SaltySDCore_fopen("sdmc:/SaltySD/flags/blockfilestats.flag", "rb");
			if (!readFlag) {
				//For 32-bit it's different
				Address_weaks.FileAccessorRead = SaltySDCore_FindSymbolBuiltin("_ZN2nn2fs6detail12FileAccessor4ReadEPjxPvjRKNS0_10ReadOptionE");		
				SaltySDCore_ReplaceImport("_ZN2nn2fs6detail12FileAccessor4ReadEPjxPvjRKNS0_10ReadOptionE", (void*)nn::FileAccessorRead);
			}
			else SaltySDCore_fclose(readFlag);

			uint64_t titleid = 0;
			svcGetInfo(&titleid, InfoType_TitleId, CUR_PROCESS_HANDLE, 0);	
			char path[128];
			snprintf(path, sizeof(path), "sdmc:/SaltySD/plugins/FPSLocker/%016llX.dat", titleid);
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
				SaltySDCore_printf("NX-FPS: BID: %016llX\n", buildid);
				snprintf(path, sizeof(path), "sdmc:/SaltySD/plugins/FPSLocker/patches/%016llX/%016llX.bin", titleid, buildid);
				FILE* patch_file = SaltySDCore_fopen(path, "rb");
				if (patch_file) {
					SaltySDCore_fclose(patch_file);
					SaltySDCore_printf("NX-FPS: FPSLocker: successfully opened: %s\n", path);
					configRC = readConfig(path, &configBuffer);
					if (LOCK::MasterWriteApplied) {
						(Shared -> patchApplied) = 2;
					}
					SaltySDCore_printf("NX-FPS: FPSLocker: readConfig rc: 0x%x\n", configRC);
					uint64_t alias = 0;
					uint64_t heap = 0;

					svcGetInfo(&alias, InfoType_AliasRegionAddress, CUR_PROCESS_HANDLE, 0);
					svcGetInfo(&heap, InfoType_HeapRegionAddress, CUR_PROCESS_HANDLE, 0);
					LOCK::mappings.alias_start = (uint32_t)alias;
					LOCK::mappings.heap_start = (uint32_t)heap;
				}
				else SaltySDCore_printf("NX-FPS: FPSLocker: File not found: %s\n", path);
			}
		}
		SaltySDCore_printf("NX-FPS: injection finished\n");
	}
}
