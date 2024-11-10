#include <switch_min.h>
#include "saltysd_ipc.h"
#include "saltysd_dynamic.h"
#include "saltysd_core.h"
#include <cstdlib>
#include <cmath>
#include "lock.hpp"

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

typedef int NVNtextureFlags;
typedef int NVNtextureTarget;
typedef int NVNformat;
typedef int NVNmemoryPoolFlags;

NVNWindow* m_nvnWindow = 0;
NVNDevice* m_nvnDevice = 0;
NVNTexture* framebufferTextures[4];

struct NVNViewport {
	float x;
	float y;
	float width;
	float height;
};

extern "C" {
	typedef u32 (*nvnBootstrapLoader_0)(const char * nvnName);
	typedef int (*eglSwapBuffers_0)(const void* EGLDisplay, const void* EGLSurface);
	typedef int (*eglSwapInterval_0)(const void* EGLDisplay, int interval);
	typedef u32 (*vkQueuePresentKHR_0)(const void* vkQueue, const void* VkPresentInfoKHR);
	typedef u32 (*_ZN11NvSwapchain15QueuePresentKHREP9VkQueue_TPK16VkPresentInfoKHR_0)(const void* VkQueue_T, const void* VkPresentInfoKHR);
	typedef u64 (*_ZN2nn2os17ConvertToTimeSpanENS0_4TickE_0)(u64 tick);
	typedef void (*_ZN2nn2os13GetSystemTickEv_0)(u64* output);
	typedef u32 (*eglGetProcAddress_0)(const char* eglName);
	typedef u8 (*_ZN2nn2oe16GetOperationModeEv)();
	typedef void* (*nvnCommandBufferSetRenderTargets_0)(void* cmdBuf, int numTextures, NVNTexture** texture, NVNTextureView** textureView, NVNTexture* depth, NVNTextureView* depthView);
	typedef void* (*nvnCommandBufferSetViewport_0)(void* cmdBuf, int x, int y, int width, int height);
	typedef void* (*nvnCommandBufferSetViewports_0)(void* cmdBuf, int start, int count, NVNViewport* viewports);
	typedef void* (*nvnCommandBufferSetDepthRange_0)(void* cmdBuf, float s0, float s1);
	typedef u16 (*nvnTextureGetWidth_0)(NVNTexture* texture);
	typedef u16 (*nvnTextureGetHeight_0)(NVNTexture* texture);
	typedef u32 (*nvnTextureGetFormat_0)(NVNTexture* texture);
	typedef void* (*_vkGetInstanceProcAddr_0)(void* instance, const char* vkFunction);
	typedef void* (*vkGetDeviceProcAddr_0)(void* device, const char* vkFunction);
}

struct {
	uintptr_t nvnBootstrapLoader;
	uintptr_t eglSwapBuffers;
	uintptr_t eglSwapInterval;
	uintptr_t vkQueuePresentKHR;
	uintptr_t nvSwapchainQueuePresentKHR;
	uintptr_t ConvertToTimeSpan;
	uintptr_t GetSystemTick;
	uintptr_t eglGetProcAddress;
	uintptr_t GetOperationMode;
	uintptr_t ReferSymbol;
	uintptr_t vkGetInstanceProcAddr;
} Address_weaks;

struct nvnWindowBuilder {
	const char reserved[16];
	uint8_t numBufferedFrames;
};

ptrdiff_t SharedMemoryOffset = 1234;
uint8_t* configBuffer = 0;
size_t configSize = 0;
Result configRC = 1;

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
	if (LOCK::gen == 2) {
		Result ret = LOCK::applyMasterWrite(patch_file, configSize, header_size - 4);
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
	uint8_t displaySync;
	resolutionCalls renderCalls[8];
	resolutionCalls viewportCalls[8];
	bool forceOriginalRefreshRate;
} PACKED;

NxFpsSharedBlock* Shared = 0;

struct {
	uintptr_t nvnDeviceGetProcAddress;
	uintptr_t nvnQueuePresentTexture;

	uintptr_t nvnWindowSetPresentInterval;
	uintptr_t nvnWindowGetPresentInterval;
	uintptr_t nvnWindowBuilderSetTextures;
	uintptr_t nvnWindowAcquireTexture;
	uintptr_t nvnSyncWait;

	uintptr_t nvnWindowSetNumActiveTextures;
	uintptr_t nvnWindowInitialize;
	uintptr_t nvnTextureGetWidth;
	uintptr_t nvnTextureGetHeight;
	uintptr_t nvnTextureGetFormat;
	uintptr_t nvnCommandBufferSetRenderTargets;
	uintptr_t nvnCommandBufferSetViewport;
	uintptr_t nvnCommandBufferSetViewports;
	uintptr_t nvnCommandBufferSetDepthRange;
	uintptr_t vkGetDeviceProcAddr;
} Ptrs;

struct {
	uintptr_t nvnWindowGetProcAddress;
	uintptr_t nvnQueuePresentTexture;
	uintptr_t nvnWindowSetPresentInterval;
	uintptr_t nvnWindowBuilderSetTextures;
	uintptr_t nvnWindowAcquireTexture;
	uintptr_t nvnSyncWait;
	uintptr_t nvnGetProcAddress;
	uintptr_t nvnWindowSetNumActiveTextures;
	uintptr_t nvnWindowInitialize;
	uintptr_t eglGetProcAddress;
	uintptr_t eglSwapBuffers;
	uintptr_t eglSwapInterval;
	uintptr_t nvnCommandBufferSetRenderTargets;
	uintptr_t nvnCommandBufferSetViewport;
	uintptr_t nvnCommandBufferSetViewports;
	uintptr_t nvnCommandBufferSetDepthRange;
} Address;

struct {
	uint8_t FPS = 0xFF;
	float FPSavg = 255;
	bool FPSmode = 0;
} Stats;

static uint32_t systemtickfrequency = 19200000;
typedef void (*nvnQueuePresentTexture_0)(const void* _this, const void* unk2_1, const void* unk3_1);
typedef uintptr_t (*GetProcAddress)(const void* unk1_a, const char * nvnFunction_a);

bool changeFPS = false;
bool changedFPS = false;
typedef void (*nvnBuilderSetTextures_0)(const nvnWindowBuilder* nvnWindowBuilder, int buffers, NVNTexture** texturesBuffer);
typedef void (*nvnWindowSetNumActiveTextures_0)(const NVNWindow* nvnWindow, int buffers);
typedef bool (*nvnWindowInitialize_0)(const NVNWindow* nvnWindow, struct nvnWindowBuilder* windowBuilder);
typedef void* (*nvnWindowAcquireTexture_0)(const NVNWindow* nvnWindow, const void* nvnSync, const void* index);
typedef void (*nvnSetPresentInterval_0)(const NVNWindow* nvnWindow, int mode);
typedef int (*nvnGetPresentInterval_0)(const NVNWindow* nvnWindow);
typedef void* (*nvnSyncWait_0)(const void* _this, uint64_t timeout_ns);
void* WindowSync = 0;
uint64_t startFrameTick = 0;

enum {
	ZeroSyncType_None,
	ZeroSyncType_Soft,
	ZeroSyncType_Semi
};

inline uint32_t getMainAddress() {
	MemoryInfo memoryinfo = {0};
	u32 pageinfo = 0;

	uint64_t base_address = SaltySDCore_getCodeStart() + 0x4000;
	for (size_t i = 0; i < 3; i++) {
		Result rc = svcQueryMemory(&memoryinfo, &pageinfo, base_address);
		if (R_FAILED(rc)) return 0;
		if ((memoryinfo.addr == base_address) && ((memoryinfo.perm & Perm_Rx) == Perm_Rx))
			return (uint32_t)base_address;
		base_address = memoryinfo.addr+memoryinfo.size;
	}

	return 0;
}

uint32_t vulkanSwap2 (const void* VkQueue_T, const void* VkPresentInfoKHR) {
	static uint8_t FPS_temp = 0;
	static uint64_t starttick = 0;
	static uint64_t endtick = 0;
	static uint64_t deltatick = 0;
	static uint64_t frameend = 0;
	static uint64_t framedelta = 0;
	static uint64_t frameavg = 0;
	static uint8_t FPSlock = 0;
	static uint32_t FPStiming = 0;
	static uint8_t FPStickItr = 0;
	static uint8_t range = 0;
	static uint8_t rangeoverride = 0;
	
	bool FPSlock_delayed = false;
	
	if (!starttick) {
		(Shared -> API) = 3;
		((_ZN2nn2os13GetSystemTickEv_0)(Address_weaks.GetSystemTick))(&starttick);
	}

	uint32_t FPStimingoverride = 0;
	if (LOCK::overwriteRefreshRate > 0) {
		if (LOCK::overwriteRefreshRate >= 60.0) {
			FPStimingoverride = (uint32_t)((double)systemtickfrequency / LOCK::overwriteRefreshRate) - 8000;
			FPStimingoverride += 20 * rangeoverride;
		}
		else {
			FPStimingoverride = (uint32_t)((double)systemtickfrequency / LOCK::overwriteRefreshRate) - 6000;		
			FPStimingoverride += 20 * rangeoverride;
		}
	}
	

	if ((FPStiming && !LOCK::blockDelayFPS && (!(Shared -> displaySync) || (Shared -> FPSlocked) < (Shared -> displaySync))) || FPStimingoverride) {
		uint64_t tick = 0;
		((_ZN2nn2os13GetSystemTickEv_0)(Address_weaks.GetSystemTick))(&tick);
		if ((tick - frameend) < (FPStimingoverride ? FPStimingoverride : FPStiming)) {
			FPSlock_delayed = true;
		}
		while ((tick - frameend) < (FPStimingoverride ? FPStimingoverride : FPStiming)) {
			svcSleepThread(-2);
			svcSleepThread(10000);
			((_ZN2nn2os13GetSystemTickEv_0)(Address_weaks.GetSystemTick))(&tick);
		}
	}

	uint32_t vulkanResult = ((_ZN11NvSwapchain15QueuePresentKHREP9VkQueue_TPK16VkPresentInfoKHR_0)(Address_weaks.nvSwapchainQueuePresentKHR))(VkQueue_T, VkPresentInfoKHR);
	((_ZN2nn2os13GetSystemTickEv_0)(Address_weaks.GetSystemTick))(&endtick);
	framedelta = endtick - frameend;
	frameavg = ((9*frameavg) + framedelta) / 10;
	Stats.FPSavg = systemtickfrequency / (float)frameavg;

	if ((FPSlock_delayed && FPStiming) || FPStimingoverride) {
		if (Stats.FPSavg > ((float)FPSlock)) {
			if (range < 200) {
				FPStiming += 20;
				range++;
			}
		}
		else if ((std::lround(Stats.FPSavg) == FPSlock) && (Stats.FPSavg < (float)FPSlock)) {
			if (range > 0) {
				FPStiming -= 20;
				range--;
			}
		}
		if (Stats.FPSavg > LOCK::overwriteRefreshRate) {
			if (rangeoverride < 200) {
				rangeoverride++;
			}
		}
		else if ((std::lround(Stats.FPSavg) == LOCK::overwriteRefreshRate) && (Stats.FPSavg <  LOCK::overwriteRefreshRate)) {
			if (rangeoverride > 0) {
				rangeoverride--;
			}
		}
	}

	frameend = endtick;
	
	FPS_temp++;
	deltatick = endtick - starttick;

	Shared -> FPSticks[FPStickItr++] = framedelta;
	FPStickItr %= 10;

	if (deltatick > systemtickfrequency) {
		((_ZN2nn2os13GetSystemTickEv_0)(Address_weaks.GetSystemTick))(&starttick);
		Stats.FPS = FPS_temp - 1;
		FPS_temp = 0;
		(Shared -> FPS) = Stats.FPS;
		if (changeFPS && !configRC && FPSlock) {
			LOCK::applyPatch(configBuffer, configSize, FPSlock, (Shared -> displaySync));
			(Shared -> patchApplied) = 1;
		}
		if (((_ZN2nn2oe16GetOperationModeEv)(Address_weaks.GetOperationMode))() == true && (Shared -> displaySync) != 0) {
			(Shared -> displaySync) = 0;
			FPSlock = 0;
		}
	}

	(Shared -> FPSavg) = Stats.FPSavg;
	(Shared -> pluginActive) = true;

	if (FPSlock != (Shared -> FPSlocked) || (FPSlock && !FPStiming)) {
		if (((Shared -> FPSlocked) < 60) && ((Shared -> FPSlocked) > 0)) {
			FPStiming = (systemtickfrequency/((Shared -> FPSlocked))) - 6000;
		}
		else FPStiming = 0;
		FPSlock = (Shared -> FPSlocked);
	}
	
	return vulkanResult;
}

uint32_t vulkanSwap (const void* VkQueue, const void* VkPresentInfoKHR) {
	static uint8_t FPS_temp = 0;
	static uint64_t starttick = 0;
	static uint64_t endtick = 0;
	static uint64_t deltatick = 0;
	static uint64_t frameend = 0;
	static uint64_t framedelta = 0;
	static uint64_t frameavg = 0;
	static uint8_t FPSlock = 0;
	static uint32_t FPStiming = 0;
	static uint8_t FPStickItr = 0;
	static uint8_t range = 0;
	static uint8_t rangeoverride = 0;
	
	bool FPSlock_delayed = false;
	
	if (!starttick) {
		(Shared -> API) = 3;
		((_ZN2nn2os13GetSystemTickEv_0)(Address_weaks.GetSystemTick))(&starttick);
	}

	uint32_t FPStimingoverride = 0;
	if (LOCK::overwriteRefreshRate > 0) {
		if (LOCK::overwriteRefreshRate >= 60.0) {
			FPStimingoverride = (uint32_t)((double)systemtickfrequency / LOCK::overwriteRefreshRate) - 8000;
			FPStimingoverride += 20 * rangeoverride;
		}
		else {
			FPStimingoverride = (uint32_t)((double)systemtickfrequency / LOCK::overwriteRefreshRate) - 6000;		
			FPStimingoverride += 20 * rangeoverride;
		}
	}
	

	if ((FPStiming && !LOCK::blockDelayFPS && (!(Shared -> displaySync) || (Shared -> FPSlocked) < (Shared -> displaySync))) || FPStimingoverride) {
		uint64_t tick = 0;
		((_ZN2nn2os13GetSystemTickEv_0)(Address_weaks.GetSystemTick))(&tick);
		if ((tick - frameend) < (FPStimingoverride ? FPStimingoverride : FPStiming)) {
			FPSlock_delayed = true;
		}
		while ((tick - frameend) < (FPStimingoverride ? FPStimingoverride : FPStiming)) {
			svcSleepThread(-2);
			svcSleepThread(10000);
			((_ZN2nn2os13GetSystemTickEv_0)(Address_weaks.GetSystemTick))(&tick);
		}
	}

	uint32_t vulkanResult = ((vkQueuePresentKHR_0)(Address_weaks.vkQueuePresentKHR))(VkQueue, VkPresentInfoKHR);
	((_ZN2nn2os13GetSystemTickEv_0)(Address_weaks.GetSystemTick))(&endtick);
	framedelta = endtick - frameend;
	frameavg = ((9*frameavg) + framedelta) / 10;
	Stats.FPSavg = systemtickfrequency / (float)frameavg;

	if ((FPSlock_delayed && FPStiming) || FPStimingoverride) {
		if (Stats.FPSavg > ((float)FPSlock)) {
			if (range < 200) {
				FPStiming += 20;
				range++;
			}
		}
		else if ((std::lround(Stats.FPSavg) == FPSlock) && (Stats.FPSavg < (float)FPSlock)) {
			if (range > 0) {
				FPStiming -= 20;
				range--;
			}
		}

		if (Stats.FPSavg > LOCK::overwriteRefreshRate) {
			if (rangeoverride < 200) {
				rangeoverride++;
			}
		}
		else if ((std::lround(Stats.FPSavg) == LOCK::overwriteRefreshRate) && (Stats.FPSavg <  LOCK::overwriteRefreshRate)) {
			if (rangeoverride > 0) {
				rangeoverride--;
			}
		}
	}

	frameend = endtick;
	
	FPS_temp++;
	deltatick = endtick - starttick;

	Shared -> FPSticks[FPStickItr++] = framedelta;
	FPStickItr %= 10;

	if (deltatick > systemtickfrequency) {
		((_ZN2nn2os13GetSystemTickEv_0)(Address_weaks.GetSystemTick))(&starttick);
		Stats.FPS = FPS_temp - 1;
		FPS_temp = 0;
		(Shared -> FPS) = Stats.FPS;
		if (changeFPS && !configRC && FPSlock) {
			LOCK::applyPatch(configBuffer, configSize, FPSlock, (Shared -> displaySync));
			(Shared -> patchApplied) = 1;
		}
		if (((_ZN2nn2oe16GetOperationModeEv)(Address_weaks.GetOperationMode))() == true && (Shared -> displaySync) != 0) {
			(Shared -> displaySync) = 0;
			FPSlock = 0;
		}
	}

	(Shared -> FPSavg) = Stats.FPSavg;
	(Shared -> pluginActive) = true;

	if (FPSlock != (Shared -> FPSlocked) || (FPSlock && !FPStiming)) {
		if (((Shared -> FPSlocked) < 60) && ((Shared -> FPSlocked) > 0)) {
			FPStiming = (systemtickfrequency/((Shared -> FPSlocked))) - 6000;
		}
		else FPStiming = 0;
		FPSlock = (Shared -> FPSlocked);
	}
	
	return vulkanResult;
}

void* vkGetDeviceProcAddr(void* device, const char* vkFunction) {
	if (!strcmp("vkQueuePresentKHR", vkFunction)) {
		Address_weaks.vkQueuePresentKHR = (uintptr_t)((vkGetDeviceProcAddr_0)(Ptrs.vkGetDeviceProcAddr))(device, vkFunction);
		return (void*)&vulkanSwap;
	}
	if (!strcmp("vkGetDeviceProcAddr", vkFunction)) {
		Ptrs.vkGetDeviceProcAddr = (uintptr_t)((vkGetDeviceProcAddr_0)(Ptrs.vkGetDeviceProcAddr))(device, vkFunction);
		return (void*)&vkGetDeviceProcAddr;
	}
	return ((vkGetDeviceProcAddr_0)(Ptrs.vkGetDeviceProcAddr))(device, vkFunction);
}

void* vkGetInstanceProcAddr(void* instance, const char* vkFunction) {
	if (!strcmp("vkQueuePresentKHR", vkFunction)) {
		Address_weaks.vkQueuePresentKHR = (uintptr_t)((_vkGetInstanceProcAddr_0)(Address_weaks.vkGetInstanceProcAddr))(instance, vkFunction);
		return (void*)&vulkanSwap;
	}
	if (!strcmp("vkGetDeviceProcAddr", vkFunction)) {
		Ptrs.vkGetDeviceProcAddr = (uintptr_t)((_vkGetInstanceProcAddr_0)(Address_weaks.vkGetInstanceProcAddr))(instance, vkFunction);
		return (void*)&vkGetDeviceProcAddr;
	}
	return ((_vkGetInstanceProcAddr_0)(Address_weaks.vkGetInstanceProcAddr))(instance, vkFunction);
}

int eglInterval(const void* EGLDisplay, int interval) {
	int result = false;
	if (!changeFPS) {
		result = ((eglSwapInterval_0)(Address_weaks.eglSwapInterval))(EGLDisplay, interval);
		changedFPS = false;
		(Shared -> FPSmode) = interval;
	}
	else if (interval < 0) {
		interval *= -1;
		if ((Shared -> FPSmode) != interval) {
			result = ((eglSwapInterval_0)(Address_weaks.eglSwapInterval))(EGLDisplay, interval);
			(Shared -> FPSmode) = interval;
		}
		changedFPS = true;
	}
	return result;
}

int eglSwap (const void* EGLDisplay, const void* EGLSurface) {
	static uint8_t FPS_temp = 0;
	static uint64_t starttick = 0;
	static uint64_t endtick = 0;
	static uint64_t deltatick = 0;
	static uint64_t frameend = 0;
	static uint64_t framedelta = 0;
	static uint64_t frameavg = 0;
	static uint8_t FPSlock = 0;
	static uint32_t FPStiming = 0;
	static uint8_t FPStickItr = 0;
	static uint8_t range = 0;
	static uint8_t rangeoverride = 0;
	
	bool FPSlock_delayed = false;

	if (!starttick) {
		(Shared -> API) = 2;
		((_ZN2nn2os13GetSystemTickEv_0)(Address_weaks.GetSystemTick))(&starttick);
	}

	uint32_t FPStimingoverride = 0;
	(Shared -> forceOriginalRefreshRate) = false;
	if (LOCK::overwriteRefreshRate > 0) {
		if ((LOCK::overwriteRefreshRate == 30) || (LOCK::overwriteRefreshRate == 60)) {
			(Shared -> forceOriginalRefreshRate) = true;
			FPStimingoverride = 1;
			eglInterval(EGLDisplay, (LOCK::overwriteRefreshRate / -30));
			
		}
		else {
			eglInterval(EGLDisplay, -1);
			if (LOCK::overwriteRefreshRate >= 60.0) {
				FPStimingoverride = (uint32_t)((double)systemtickfrequency / LOCK::overwriteRefreshRate) - 8000;
				FPStimingoverride += 20 * rangeoverride;
			}
			else {
				FPStimingoverride = (uint32_t)((double)systemtickfrequency / LOCK::overwriteRefreshRate) - 6000;		
				FPStimingoverride += 20 * rangeoverride;
			}
		}
	}

	if ((FPStiming && !LOCK::blockDelayFPS && (!(Shared -> displaySync) || (Shared -> FPSlocked) < (Shared -> displaySync))) || FPStimingoverride) {
		uint64_t tick = 0;
		((_ZN2nn2os13GetSystemTickEv_0)(Address_weaks.GetSystemTick))(&tick);
		if ((tick - frameend) < (FPStimingoverride ? FPStimingoverride : FPStiming)) {
			FPSlock_delayed = true;
		}
		while ((tick - frameend) < (FPStimingoverride ? FPStimingoverride : FPStiming)) {
			svcSleepThread(-2);
			svcSleepThread(10000);
			((_ZN2nn2os13GetSystemTickEv_0)(Address_weaks.GetSystemTick))(&tick);
		}
	}
	
	int result = ((eglSwapBuffers_0)(Address_weaks.eglSwapBuffers))(EGLDisplay, EGLSurface);
	((_ZN2nn2os13GetSystemTickEv_0)(Address_weaks.GetSystemTick))(&endtick);
	framedelta = endtick - frameend;
	frameavg = ((9*frameavg) + framedelta) / 10;
	Stats.FPSavg = systemtickfrequency / (float)frameavg;

	if ((FPSlock_delayed && FPStiming) || FPStimingoverride) {
		if (Stats.FPSavg > ((float)FPSlock)) {
			if (range < 200) {
				FPStiming += 20;
				range++;
			}
		}
		else if ((std::lround(Stats.FPSavg) == FPSlock) && (Stats.FPSavg < (float)FPSlock)) {
			if (range > 0) {
				FPStiming -= 20;
				range--;
			}
		}
		if (Stats.FPSavg > LOCK::overwriteRefreshRate) {
			if (rangeoverride < 200) {
				rangeoverride++;
			}
		}
		else if ((std::lround(Stats.FPSavg) == LOCK::overwriteRefreshRate) && (Stats.FPSavg <  LOCK::overwriteRefreshRate)) {
			if (rangeoverride > 0) {
				rangeoverride--;
			}
		}
	}

	frameend = endtick;
	
	FPS_temp++;
	deltatick = endtick - starttick;

	Shared -> FPSticks[FPStickItr++] = framedelta;
	FPStickItr %= 10;

	if (deltatick > systemtickfrequency) {
		((_ZN2nn2os13GetSystemTickEv_0)(Address_weaks.GetSystemTick))(&starttick);
		Stats.FPS = FPS_temp - 1;
		FPS_temp = 0;
		(Shared -> FPS) = Stats.FPS;
		if (changeFPS && !configRC && FPSlock) {
			LOCK::applyPatch(configBuffer, configSize, FPSlock, (Shared -> displaySync));
			(Shared -> patchApplied) = 1;
		}
		if (((_ZN2nn2oe16GetOperationModeEv)(Address_weaks.GetOperationMode))() == true && (Shared -> displaySync) != 0) {
			(Shared -> displaySync) = 0;
			FPSlock = 0;
		}
	}
	
	(Shared -> FPSavg) = Stats.FPSavg;
	(Shared -> pluginActive) = true;

	if ((FPSlock != (Shared -> FPSlocked)) || (FPSlock && !FPStiming) || ((Shared -> FPSlocked) > 30 && (Shared -> FPSmode) > 1)) {
		changeFPS = true;
		changedFPS = false;
		if ((Shared -> FPSlocked) == 0) {
			FPStiming = 0;
			changeFPS = false;
			FPSlock = (Shared -> FPSlocked);
		}
		else if ((Shared -> FPSlocked) <= 30) {
			eglInterval(EGLDisplay, -2);
			if ((Shared -> FPSlocked) != 30) {
				FPStiming = (systemtickfrequency/((Shared -> FPSlocked))) - 6000;
			}
			else FPStiming = 0;
		}
		else {
			eglInterval(EGLDisplay, -1);
			if ((Shared -> FPSlocked) != 60) {
				FPStiming = (systemtickfrequency/((Shared -> FPSlocked))) - 6000;
			}
			else FPStiming = 0;
		}
		if (changedFPS) {
			FPSlock = (Shared -> FPSlocked);
		}
	}

	return result;
}

uintptr_t eglGetProc(const char* eglName) {
	if (!strcmp(eglName, "eglSwapInterval")) {
		return Address.eglSwapInterval;
	}
	else if (!strcmp(eglName, "eglSwapBuffers")) {
		return Address.eglSwapBuffers;
	}
	return ((eglGetProcAddress_0)(Address_weaks.eglGetProcAddress))(eglName);
}

bool nvnWindowInitialize(const NVNWindow* nvnWindow, struct nvnWindowBuilder* windowBuilder) {
	m_nvnWindow = (NVNWindow*)nvnWindow;
	if (!(Shared -> Buffers)) {
		(Shared -> Buffers) = windowBuilder -> numBufferedFrames;
		if ((Shared -> SetBuffers) >= 2 && (Shared -> SetBuffers) <= windowBuilder -> numBufferedFrames) {
			windowBuilder -> numBufferedFrames = (Shared -> SetBuffers);
		}
		(Shared -> ActiveBuffers) = windowBuilder -> numBufferedFrames;	
	}
	return ((nvnWindowInitialize_0)(Ptrs.nvnWindowInitialize))(nvnWindow, windowBuilder);
}

void nvnWindowBuilderSetTextures(const nvnWindowBuilder* nvnWindowBuilder, int numBufferedFrames, NVNTexture** nvnTextures) {
	(Shared -> Buffers) = numBufferedFrames;
	for (int i = 0; i < numBufferedFrames; i++) {
		framebufferTextures[i] = nvnTextures[i];
	}
	if ((Shared -> SetBuffers) >= 2 && (Shared -> SetBuffers) <= numBufferedFrames) {
		numBufferedFrames = (Shared -> SetBuffers);
	}
	(Shared -> ActiveBuffers) = numBufferedFrames;
	return ((nvnBuilderSetTextures_0)(Ptrs.nvnWindowBuilderSetTextures))(nvnWindowBuilder, numBufferedFrames, nvnTextures);
}

void nvnWindowSetNumActiveTextures(const NVNWindow* nvnWindow, int numBufferedFrames) {
	(Shared -> SetActiveBuffers) = numBufferedFrames;
	if ((Shared -> SetBuffers) >= 2 && (Shared -> SetBuffers) <= (Shared -> Buffers)) {
		numBufferedFrames = (Shared -> SetBuffers);
	}
	(Shared -> ActiveBuffers) = numBufferedFrames;
	return ((nvnWindowSetNumActiveTextures_0)(Ptrs.nvnWindowSetNumActiveTextures))(nvnWindow, numBufferedFrames);
}

void nvnSetPresentInterval(const NVNWindow* nvnWindow, int mode) {
	if (mode < 0) {
		mode *= -1;
		if ((Shared -> FPSmode) != mode) {
			((nvnSetPresentInterval_0)(Ptrs.nvnWindowSetPresentInterval))(nvnWindow, mode);
			(Shared -> FPSmode) = mode;
		}
		changedFPS = true;
	}
	else if (!changeFPS) {
		((nvnSetPresentInterval_0)(Ptrs.nvnWindowSetPresentInterval))(nvnWindow, mode);
		changedFPS = false;
		(Shared -> FPSmode) = mode;
	}
	return;
}

void* nvnSyncWait0(const void* _this, uint64_t timeout_ns) {
	uint64_t endFrameTick = 0;
	((_ZN2nn2os13GetSystemTickEv_0)(Address_weaks.GetSystemTick))(&endFrameTick);
	if (_this == WindowSync && (Shared -> ActiveBuffers) == 2) {
		if ((Shared -> ZeroSync) == ZeroSyncType_Semi) {
			u64 FrameTarget = (systemtickfrequency/60) - 8000;
			s64 new_timeout = (FrameTarget - (endFrameTick - startFrameTick)) - 19200;
			if ((Shared -> FPSlocked) == 60) {
				new_timeout = (systemtickfrequency/101) - (endFrameTick - startFrameTick);
			}
			if (new_timeout > 0) {
				timeout_ns = ((_ZN2nn2os17ConvertToTimeSpanENS0_4TickE_0)(Address_weaks.ConvertToTimeSpan))(new_timeout);
			}
			else timeout_ns = 0;
		}
		else if ((Shared -> ZeroSync) == ZeroSyncType_Soft) 
			timeout_ns = 0;
	}
	return ((nvnSyncWait_0)(Ptrs.nvnSyncWait))(_this, timeout_ns);
}

bool nvnPresentedTexture = false;

void nvnPresentTexture(const void* _this, const NVNWindow* nvnWindow, const void* unk3) {
	static uint8_t FPS_temp = 0;
	static uint64_t starttick = 0;
	static uint64_t endtick = 0;
	static uint64_t deltatick = 0;
	static uint64_t frameend = 0;
	static uint64_t framedelta = 0;
	static uint64_t frameavg = 0;
	static uint8_t FPSlock = 0;
	static uint32_t FPStiming = 0;
	static uint8_t FPStickItr = 0;
	static uint8_t range = 0;
	static uint8_t rangeoverride = 0;
	
	bool FPSlock_delayed = false;

	if (!starttick) {
		((_ZN2nn2os13GetSystemTickEv_0)(Address_weaks.GetSystemTick))(&starttick);
		(Shared -> FPSmode) = (uint8_t)((nvnGetPresentInterval_0)(Ptrs.nvnWindowGetPresentInterval))(nvnWindow);
	}

	if (FPSlock) {
		if (((Shared -> ZeroSync) == ZeroSyncType_None) && FPStiming && ((Shared -> displaySync) == FPSlock || ((Shared -> displaySync) == 0 && (FPSlock == 60 || FPSlock == 30)))) {
			FPStiming = 0;
		}
		else if (((Shared -> ZeroSync) != ZeroSyncType_None) && !FPStiming) {
			if (FPSlock == 60) {
				FPStiming = (systemtickfrequency/((Shared -> FPSlocked))) - 8000;
			}
			else FPStiming = (systemtickfrequency/((Shared -> FPSlocked))) - 6000;
		}
	}

	uint32_t FPStimingoverride = 0;
	(Shared -> forceOriginalRefreshRate) = false;
	if (LOCK::overwriteRefreshRate > 0) {
		if ((LOCK::overwriteRefreshRate == 30) || (LOCK::overwriteRefreshRate == 60)) {
			(Shared -> forceOriginalRefreshRate) = true;
			FPStimingoverride = 1;
			nvnSetPresentInterval(nvnWindow, (LOCK::overwriteRefreshRate / -30));
		}
		else {
			nvnSetPresentInterval(nvnWindow, -1);
			if (LOCK::overwriteRefreshRate >= 60.0) {
				FPStimingoverride = (uint32_t)((double)systemtickfrequency / LOCK::overwriteRefreshRate) - 8000;
				FPStimingoverride += 20 * rangeoverride;
			}
			else {
				FPStimingoverride = (uint32_t)((double)systemtickfrequency / LOCK::overwriteRefreshRate) - 6000;		
				FPStimingoverride += 20 * rangeoverride;
			}
		}
	}

	if ((FPStiming && !LOCK::blockDelayFPS && (!(Shared -> displaySync) || (Shared -> FPSlocked) < (Shared -> displaySync))) || FPStimingoverride) {
		uint64_t tick = 0;
		((_ZN2nn2os13GetSystemTickEv_0)(Address_weaks.GetSystemTick))(&tick);
		if ((tick - frameend) < (FPStimingoverride ? FPStimingoverride : FPStiming)) {
			FPSlock_delayed = true;
		}
		while ((tick - frameend) < (FPStimingoverride ? FPStimingoverride : FPStiming)) {
			svcSleepThread(-2);
			svcSleepThread(10000);
			((_ZN2nn2os13GetSystemTickEv_0)(Address_weaks.GetSystemTick))(&tick);
		}
	}
	
	((nvnQueuePresentTexture_0)(Ptrs.nvnQueuePresentTexture))(_this, nvnWindow, unk3);
	
	nvnPresentedTexture = true;
	((_ZN2nn2os13GetSystemTickEv_0)(Address_weaks.GetSystemTick))(&endtick);
	framedelta = endtick - frameend;

	Shared -> FPSticks[FPStickItr++] = framedelta;
	FPStickItr %= 10;
	
	frameavg = ((9*frameavg) + framedelta) / 10;
	Stats.FPSavg = systemtickfrequency / (float)frameavg;

	if ((FPSlock_delayed && FPStiming) || FPStimingoverride) {
		if (Stats.FPSavg > ((float)FPSlock)) {
			if (range < 200) {
				FPStiming += 20;
				range++;
			}
		}
		else if ((std::lround(Stats.FPSavg) == FPSlock) && (Stats.FPSavg < (float)FPSlock)) {
			if (range > 0) {
				FPStiming -= 20;
				range--;
			}
		}

		if (Stats.FPSavg > LOCK::overwriteRefreshRate) {
			if (rangeoverride < 200) {
				rangeoverride++;
			}
		}
		else if ((std::lround(Stats.FPSavg) == LOCK::overwriteRefreshRate) && (Stats.FPSavg <  LOCK::overwriteRefreshRate)) {
			if (rangeoverride > 0) {
				rangeoverride--;
			}
		}
	}
	frameend = endtick;
	FPS_temp++;
	deltatick = endtick - starttick;
	if (deltatick > systemtickfrequency) {
		((_ZN2nn2os13GetSystemTickEv_0)(Address_weaks.GetSystemTick))(&starttick);
		Stats.FPS = FPS_temp - 1;
		FPS_temp = 0;
		(Shared -> FPS) = Stats.FPS;
		(Shared -> FPSmode) = (uint8_t)((nvnGetPresentInterval_0)(Ptrs.nvnWindowGetPresentInterval))(nvnWindow);
		if (changeFPS && !configRC && FPSlock) {
			LOCK::applyPatch(configBuffer, configSize, FPSlock, (Shared -> displaySync));
			(Shared -> patchApplied) = 1;
		}
		if (((_ZN2nn2oe16GetOperationModeEv)(Address_weaks.GetOperationMode))() == true && (Shared -> displaySync) != 0) {
			(Shared -> displaySync) = 0;
			FPSlock = 0;
		}
	}
	(Shared -> FPSavg) = Stats.FPSavg;
	
	
	(Shared -> pluginActive) = true;
	
	if ((FPSlock != (Shared -> FPSlocked)) || (FPSlock && !FPStiming) || ((Shared -> FPSlocked) > 30 && (Shared -> FPSmode) > 1)) {
		changeFPS = true;
		changedFPS = false;
		if ((Shared -> FPSlocked) == 0) {
			FPStiming = 0;
			changeFPS = false;
			FPSlock = (Shared -> FPSlocked);
		}
		else if ((Shared -> displaySync) == 0 && ((Shared -> FPSlocked) <= 30)) {
			nvnSetPresentInterval(nvnWindow, -2);
			if ((Shared -> FPSlocked) != 30 || (Shared -> ZeroSync)) {
				if ((Shared -> FPSlocked) == 30) {
					FPStiming = (systemtickfrequency/((Shared -> FPSlocked))) - 8000;
				}
				else FPStiming = (systemtickfrequency/((Shared -> FPSlocked))) - 6000;
			}
			else FPStiming = 0;
		}
		else {
			nvnSetPresentInterval(nvnWindow, -2); //This allows in game with glitched interval to unlock 60 FPS, f.e. WRC Generations
			nvnSetPresentInterval(nvnWindow, -1);
			if ((Shared -> FPSlocked) != 60 || (Shared -> ZeroSync)) {
				if ((Shared -> FPSlocked) == 60) {
					FPStiming = (systemtickfrequency/((Shared -> FPSlocked))) - 8000;
				}
				else FPStiming = (systemtickfrequency/((Shared -> FPSlocked))) - 6000;
			}
			else FPStiming = 0;
		}
		if (changedFPS) {
			FPSlock = (Shared -> FPSlocked);
		}
	}

	
	
	return;
}

void* nvnAcquireTexture(const NVNWindow* nvnWindow, const void* nvnSync, const void* index) {
	if (WindowSync != nvnSync) {
		WindowSync = (void*)nvnSync;
	}
	void* ret = ((nvnWindowAcquireTexture_0)(Ptrs.nvnWindowAcquireTexture))(nvnWindow, nvnSync, index);
	((_ZN2nn2os13GetSystemTickEv_0)(Address_weaks.GetSystemTick))(&startFrameTick);
	return ret;
}

struct nvnCommandBuffer {
	char reserved[0x80];
};

resolutionCalls m_resolutionRenderCalls[8] = {0};
resolutionCalls m_resolutionViewportCalls[8] = {0};

void* nvnCommandBufferSetViewports(nvnCommandBuffer* cmdBuf, int start, int count, NVNViewport* viewports) {
	if (resolutionLookup) for (int i = start; i < count; i++) {
		if (viewports[i].height > 1.f && viewports[i].width > 1.f && viewports[i].x == 0.f && viewports[i].y == 0.f) {
			uint16_t width = (uint16_t)(viewports[i].width);
			uint16_t height = (uint16_t)(viewports[i].height);
			int ratio = (width * 10) / height;
			if (ratio >= 12 && ratio <= 18) {
				//Dynamic Resolution is always the second value passed
				for (size_t i = 0; i < 8; i++) {
					if (width == m_resolutionViewportCalls[i].width) {
						m_resolutionViewportCalls[i].calls++;
						break;
					}
					if (m_resolutionViewportCalls[i].width == 0) {
						m_resolutionViewportCalls[i].width = width;
						m_resolutionViewportCalls[i].height = height;
						m_resolutionViewportCalls[i].calls = 1;
						break;
					}
				}			
			}
		}
	}
	return ((nvnCommandBufferSetViewports_0)(Ptrs.nvnCommandBufferSetViewports))(cmdBuf, start, count, viewports);
}

void* nvnCommandBufferSetViewport(nvnCommandBuffer* cmdBuf, int x, int y, int width, int height) {
	if (resolutionLookup && height > 1 && width > 1 && !x && !y) {
		int ratio = (width * 10) / height;
		if (ratio >= 12 && ratio <= 18) {
			//Dynamic Resolution is always the second value passed
			for (size_t i = 0; i < 8; i++) {
				if (width == m_resolutionViewportCalls[i].width) {
					m_resolutionViewportCalls[i].calls++;
					break;
				}
				if (m_resolutionViewportCalls[i].width == 0) {
					m_resolutionViewportCalls[i].width = width;
					m_resolutionViewportCalls[i].height = height;
					m_resolutionViewportCalls[i].calls = 1;
					break;
				}
			}			
		}
	}
	return ((nvnCommandBufferSetViewport_0)(Ptrs.nvnCommandBufferSetViewport))(cmdBuf, x, y, width, height);
}

void* nvnCommandBufferSetRenderTargets(nvnCommandBuffer* cmdBuf, int numTextures, NVNTexture** texture, NVNTextureView** textureView, NVNTexture* depthTexture, NVNTextureView* depthView) {
	if (!resolutionLookup && Shared -> renderCalls[0].calls == 0xFFFF) {
		resolutionLookup = true;
		Shared -> renderCalls[0].calls = 0;
	}
	if (resolutionLookup && depthTexture != NULL && texture != NULL) {
		uint16_t depth_width = ((nvnTextureGetWidth_0)(Ptrs.nvnTextureGetWidth))(depthTexture);
		uint16_t depth_height = ((nvnTextureGetHeight_0)(Ptrs.nvnTextureGetHeight))(depthTexture);
		int depth_format = ((nvnTextureGetFormat_0)(Ptrs.nvnTextureGetFormat))(depthTexture);
		if (depth_width > 1 && depth_height > 1 && (depth_format >= 51 && depth_format <= 54)) {
			if (nvnPresentedTexture) {
				memcpy(Shared -> renderCalls, m_resolutionRenderCalls, sizeof(m_resolutionRenderCalls));
				memcpy(Shared -> viewportCalls, m_resolutionViewportCalls, sizeof(m_resolutionViewportCalls));
				memset(&m_resolutionRenderCalls, 0, sizeof(m_resolutionRenderCalls));
				memset(&m_resolutionViewportCalls, 0, sizeof(m_resolutionViewportCalls));
				nvnPresentedTexture = false;
			}
			bool found = false;
			int ratio = ((depth_width * 10) / (depth_height));
			if (ratio < 12 || ratio > 18) {
				found = true;
			}
			if (!found) {
				for (size_t i = 0; i < 8; i++) {
					if (depth_width == m_resolutionRenderCalls[i].width) {
						m_resolutionRenderCalls[i].calls++;
						break;
					}
					if (m_resolutionRenderCalls[i].width == 0) {
						m_resolutionRenderCalls[i].width = depth_width;
						m_resolutionRenderCalls[i].height = depth_height;
						m_resolutionRenderCalls[i].calls = 1;
						break;
					}
				}
			}
		}
	}
	return ((nvnCommandBufferSetRenderTargets_0)(Ptrs.nvnCommandBufferSetRenderTargets))(cmdBuf, numTextures, texture, textureView, depthTexture, depthView);
}

uintptr_t nvnGetProcAddress (NVNDevice* nvnDevice, const char* nvnFunction) {
	uintptr_t address = ((GetProcAddress)(Ptrs.nvnDeviceGetProcAddress))(nvnDevice, nvnFunction);
	m_nvnDevice = nvnDevice;
	if (!strcmp("nvnDeviceGetProcAddress", nvnFunction))
		return Address.nvnGetProcAddress;
	else if (!strcmp("nvnQueuePresentTexture", nvnFunction)) {
		Ptrs.nvnQueuePresentTexture = address;
		return Address.nvnQueuePresentTexture;
	}
	else if (!strcmp("nvnWindowAcquireTexture", nvnFunction)) {
		Ptrs.nvnWindowAcquireTexture = address;
		return Address.nvnWindowAcquireTexture;
	}
	else if (!strcmp("nvnWindowSetPresentInterval", nvnFunction)) {
		Ptrs.nvnWindowSetPresentInterval = address;
		return Address.nvnWindowSetPresentInterval;
	}
	else if (!strcmp("nvnWindowGetPresentInterval", nvnFunction)) {
		Ptrs.nvnWindowGetPresentInterval = address;
		return address;
	}
	else if (!strcmp("nvnWindowSetNumActiveTextures", nvnFunction)) {
		Ptrs.nvnWindowSetNumActiveTextures = address;
		return Address.nvnWindowSetNumActiveTextures;
	}
	else if (!strcmp("nvnWindowBuilderSetTextures", nvnFunction)) {
		Ptrs.nvnWindowBuilderSetTextures = address;
		return Address.nvnWindowBuilderSetTextures;
	}
	else if (!strcmp("nvnWindowInitialize", nvnFunction)) {
		Ptrs.nvnWindowInitialize = address;
		return Address.nvnWindowInitialize;
	}
	else if (!strcmp("nvnSyncWait", nvnFunction)) {
		Ptrs.nvnSyncWait = address;
		return Address.nvnSyncWait;
	}
	else if (!strcmp("nvnCommandBufferSetRenderTargets", nvnFunction)) {
		Ptrs.nvnCommandBufferSetRenderTargets = address;
		return Address.nvnCommandBufferSetRenderTargets;
	}
	else if (!strcmp("nvnCommandBufferSetViewport", nvnFunction)) {
		Ptrs.nvnCommandBufferSetViewport = address;
		return Address.nvnCommandBufferSetViewport;
	}
	else if (!strcmp("nvnCommandBufferSetViewports", nvnFunction)) {
		Ptrs.nvnCommandBufferSetViewports = address;
		return Address.nvnCommandBufferSetViewports;
	}
	else if (!strcmp("nvnTextureGetWidth", nvnFunction)) {
		Ptrs.nvnTextureGetWidth = address;
	}
	else if (!strcmp("nvnTextureGetHeight", nvnFunction)) {
		Ptrs.nvnTextureGetHeight = address;
	}
	else if (!strcmp("nvnTextureGetFormat", nvnFunction)) {
		Ptrs.nvnTextureGetFormat = address;
	}
	return address;
}

uintptr_t nvnBootstrapLoader_1(const char* nvnName) {
	if (strcmp(nvnName, "nvnDeviceGetProcAddress") == 0) {
		(Shared -> API) = 1;
		Ptrs.nvnDeviceGetProcAddress = ((nvnBootstrapLoader_0)(Address_weaks.nvnBootstrapLoader))("nvnDeviceGetProcAddress");
		return Address.nvnGetProcAddress;
	}
	uintptr_t ptrret = ((nvnBootstrapLoader_0)(Address_weaks.nvnBootstrapLoader))(nvnName);
	return ptrret;
}

extern "C" {

	void NX_FPS(SharedMemory* _sharedmemory) {
		SaltySDCore_printf("NX-FPS: alive\n");
		LOCK::mappings.main_start = getMainAddress();
		SaltySDCore_printf("NX-FPS: found main at: 0x%lX\n", LOCK::mappings.main_start);
		Result ret = SaltySD_CheckIfSharedMemoryAvailable(&SharedMemoryOffset, sizeof(NxFpsSharedBlock));
		SaltySDCore_printf("NX-FPS: ret: 0x%X\n", ret);
		if (!ret) {
			SaltySDCore_printf("NX-FPS: MemoryOffset: %d\n", SharedMemoryOffset);

			Shared = (NxFpsSharedBlock*)((uintptr_t)shmemGetAddr(_sharedmemory) + SharedMemoryOffset);
			Shared -> MAGIC = 0x465053;
			
			Address.nvnGetProcAddress = (uintptr_t)&nvnGetProcAddress;
			Address.nvnQueuePresentTexture = (uintptr_t)&nvnPresentTexture;
			Address.nvnWindowAcquireTexture = (uintptr_t)&nvnAcquireTexture;
			Address.nvnWindowInitialize = (uintptr_t)&nvnWindowInitialize;
			Address_weaks.nvnBootstrapLoader = SaltySDCore_FindSymbolBuiltin("nvnBootstrapLoader");
			Address_weaks.eglSwapBuffers = SaltySDCore_FindSymbolBuiltin("eglSwapBuffers");
			Address_weaks.eglSwapInterval = SaltySDCore_FindSymbolBuiltin("eglSwapInterval");
			Address_weaks.vkQueuePresentKHR = SaltySDCore_FindSymbolBuiltin("vkQueuePresentKHR");
			Address_weaks.nvSwapchainQueuePresentKHR = SaltySDCore_FindSymbolBuiltin("_ZN11NvSwapchain15QueuePresentKHREP9VkQueue_TPK16VkPresentInfoKHR");
			Address_weaks.ConvertToTimeSpan = SaltySDCore_FindSymbolBuiltin("_ZN2nn2os17ConvertToTimeSpanENS0_4TickE");
			Address_weaks.GetSystemTick = SaltySDCore_FindSymbolBuiltin("_ZN2nn2os13GetSystemTickEv");
			Address_weaks.eglGetProcAddress = SaltySDCore_FindSymbolBuiltin("eglGetProcAddress");
			Address_weaks.GetOperationMode = SaltySDCore_FindSymbolBuiltin("_ZN2nn2oe16GetOperationModeEv");
			Address_weaks.vkGetInstanceProcAddr = SaltySDCore_FindSymbolBuiltin("vkGetInstanceProcAddr");
			SaltySDCore_ReplaceImport("nvnBootstrapLoader", (void*)nvnBootstrapLoader_1);
			SaltySDCore_ReplaceImport("eglSwapBuffers", (void*)eglSwap);
			SaltySDCore_ReplaceImport("eglSwapInterval", (void*)eglInterval);
			SaltySDCore_ReplaceImport("vkQueuePresentKHR", (void*)vulkanSwap);
			SaltySDCore_ReplaceImport("_ZN11NvSwapchain15QueuePresentKHREP9VkQueue_TPK16VkPresentInfoKHR", (void*)vulkanSwap2);
			SaltySDCore_ReplaceImport("eglGetProcAddress", (void*)eglGetProc);
			SaltySDCore_ReplaceImport("vkGetInstanceProcAddr", (void*)vkGetInstanceProcAddr);
			Address.nvnWindowSetPresentInterval = (uintptr_t)&nvnSetPresentInterval;
			Address.nvnSyncWait = (uintptr_t)&nvnSyncWait0;
			Address.nvnWindowBuilderSetTextures = (uintptr_t)&nvnWindowBuilderSetTextures;
			Address.nvnWindowSetNumActiveTextures = (uintptr_t)&nvnWindowSetNumActiveTextures;
			Address.eglGetProcAddress = (uintptr_t)&eglGetProc;
			Address.eglSwapBuffers = (uintptr_t)&eglSwap;
			Address.eglSwapInterval = (uintptr_t)&eglInterval;
			Address.nvnCommandBufferSetRenderTargets = (uintptr_t)&nvnCommandBufferSetRenderTargets;
			Address.nvnCommandBufferSetViewport = (uintptr_t)&nvnCommandBufferSetViewport;
			Address.nvnCommandBufferSetViewports = (uintptr_t)&nvnCommandBufferSetViewports;

			uint64_t titleid = 0;
			svcGetInfo(&titleid, InfoType_TitleId, CUR_PROCESS_HANDLE, 0);	
			char path[128];
			sprintf(path, "sdmc:/SaltySD/plugins/FPSLocker/%016llX.dat", titleid);
			FILE* file_dat = SaltySDCore_fopen(path, "rb");
			if (file_dat) {
				uint8_t temp = 0;
				SaltySDCore_fread(&temp, 1, 1, file_dat);
				(Shared -> FPSlocked) = temp;
				if (temp >= 40 && temp <= 75) {
					FILE* sync_file = SaltySDCore_fopen("sdmc:/SaltySD/flags/displaysync.flag", "rb");
					if  (sync_file) {
						SaltySDCore_fclose(sync_file);
						SaltySD_SetDisplayRefreshRate(temp);
						(Shared -> displaySync) = temp;
					}
				}
				SaltySDCore_fread(&temp, 1, 1, file_dat);
				(Shared -> ZeroSync) = temp;
				if (SaltySDCore_fread(&temp, 1, 1, file_dat))
					(Shared -> SetBuffers) = temp;
				SaltySDCore_fclose(file_dat);
			}

			u64 buildid = SaltySD_GetBID();
			if (!buildid) {
				SaltySDCore_printf("NX-FPS: getBID failed! Err: 0x%x\n", ret);
			}
			else {
				SaltySDCore_printf("NX-FPS: BID: %016llX\n", buildid);
				sprintf(path, "sdmc:/SaltySD/plugins/FPSLocker/patches/%016llX/%016llX.bin", titleid, buildid);
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
