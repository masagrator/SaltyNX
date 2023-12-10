#include <switch_min.h>
#include "saltysd_ipc.h"
#include "saltysd_dynamic.h"
#include "saltysd_core.h"
#include "ltoa.h"
#include <cstdlib>
#include <cmath>
#include "lock.hpp"

struct NVNTexture {
	char reserved[0xC0];
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

struct NVNDeviceBuilder {
	char reserved[0x40];
};

struct NVNMemoryPool {
	char reserved[0x100];
};

struct NVNMemoryPoolBuilder {
	char reserved[0x40];
};

struct NVNTextureView {
	char reserved[0x28];
};

typedef int NVNtextureFlags;
typedef int NVNtextureTarget;
typedef int NVNformat;
typedef int NVNmemoryPoolFlags;

NVNWindow* m_nvnWindow = 0;
NVNDevice* m_nvnDevice = 0;
NVNMemoryPool* m_nvnMemoryPool = 0;
NVNMemoryPool m_ThirdBufferPool = {0};
NVNTexture m_ThirdBuffer = {0};
NVNTextureView m_ThirdBufferView = {0};
NVNTextureBuilder m_nvnTextureBuilder = {0};
NVNMemoryPoolBuilder m_nvnMemoryPoolBuilder = {0};

NVNTexture* Frame_buffers[3] = {0};

extern "C" {
	typedef u64 (*nvnBootstrapLoader_0)(const char * nvnName);
	typedef int (*eglSwapBuffers_0)(const void* EGLDisplay, const void* EGLSurface);
	typedef int (*eglSwapInterval_0)(const void* EGLDisplay, int interval);
	typedef u32 (*vkQueuePresentKHR_0)(const void* vkQueue, const void* VkPresentInfoKHR);
	typedef u32 (*_ZN11NvSwapchain15QueuePresentKHREP9VkQueue_TPK16VkPresentInfoKHR_0)(const void* VkQueue_T, const void* VkPresentInfoKHR);
	typedef u64 (*_ZN2nn2os17ConvertToTimeSpanENS0_4TickE_0)(u64 tick);
	typedef u64 (*_ZN2nn2os13GetSystemTickEv_0)();
	typedef u64 (*eglGetProcAddress_0)(const char* eglName);
	typedef void* (*aligned_alloc_0)(size_t alignment, size_t size);
	typedef void (*free_0)(void* buffer);
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
	uintptr_t alignedAlloc;
	uintptr_t free;
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
	SaltySDCore_fseek(patch_file, 0, 0);
	uint8_t* buffer = (uint8_t*)calloc(1, 0x34);
	SaltySDCore_fread(buffer, 0x34, 1, patch_file);
	if (SaltySDCore_ftell(patch_file) != 0x34 || !LOCK::isValid(buffer, 0x34)) {
		SaltySDCore_fclose(patch_file);
		free(buffer);
		return 1;
	}
	if (LOCK::gen == 2) {
		Result ret = LOCK::applyMasterWrite(patch_file, configSize);
		if (R_FAILED(ret))  {
			SaltySDCore_fclose(patch_file);
			return ret;
		}
		configSize = *(uint32_t*)(&(buffer[0x30]));
	}
	free(buffer);
	buffer = (uint8_t*)calloc(1, configSize);
	SaltySDCore_fseek(patch_file, 0, 0);
	SaltySDCore_fread(buffer, configSize, 1, patch_file);
	SaltySDCore_fclose(patch_file);
	*output_buffer = buffer;
	return 0;
}

struct {
	uint8_t* FPS = 0;
	float* FPSavg = 0;
	bool* pluginActive = 0;
	uint8_t* FPSlocked = 0;
	uint8_t* FPSmode = 0;
	uint8_t* ZeroSync = 0;
	uint8_t* patchApplied = 0;
	uint8_t* API = 0;
	uint32_t* FPSticks = 0;
	uint8_t* Buffers = 0;
	uint8_t* SetBuffers = 0;
	uint8_t* ActiveBuffers = 0;
	uint8_t* SetActiveBuffers = 0;
} Shared;

struct {
	uintptr_t nvnDeviceGetProcAddress;
	uintptr_t nvnQueuePresentTexture;

	uintptr_t nvnWindowSetPresentInterval;
	uintptr_t nvnWindowGetPresentInterval;
	uintptr_t nvnWindowBuilderSetTextures;
	uintptr_t nvnWindowAcquireTexture;
	uintptr_t nvnSyncWait;

	uintptr_t nvnWindowSetNumActiveTextures;
	uintptr_t nvnTextureBuilderSetDevice;
	uintptr_t nvnTextureBuilderSetDefaults;
	uintptr_t nvnTextureBuilderSetFlags;
	uintptr_t nvnTextureBuilderSetSize2D;
	uintptr_t nvnTextureBuilderSetTarget;
	uintptr_t nvnTextureBuilderSetFormat;
	uintptr_t nvnTextureBuilderSetStorage;
	uintptr_t nvnTextureInitialize;
	uintptr_t nvnTextureFinalize;
	uintptr_t nvnTextureGetFlags;
	uintptr_t nvnTextureGetTarget;
	uintptr_t nvnTextureGetFormat;
	uintptr_t nvnTextureGetHeight;
	uintptr_t nvnTextureGetWidth;
	uintptr_t nvnTextureGetMemoryPool;
	uintptr_t nvnTextureGetMemoryOffset;
	uintptr_t nvnMemoryPoolBuilderSetDefaults;
	uintptr_t nvnMemoryPoolBuilderSetDevice;
	uintptr_t nvnMemoryPoolGetFlags;
	uintptr_t nvnMemoryPoolBuilderSetFlags;
	uintptr_t nvnMemoryPoolFinalize;
	uintptr_t nvnTextureBuilderGetStorageSize;
	uintptr_t nvnTextureBuilderGetStorageAlignment;
	uintptr_t nvnMemoryPoolBuilderSetStorage;
	uintptr_t nvnMemoryPoolInitialize;
	uintptr_t nvnCommandBufferSetRenderTargets;
	uintptr_t nvnCommandBufferCopyTextureToTexture;
	uintptr_t nvnTextureViewGetLevels;
	uintptr_t nvnTextureViewGetFormat;
	uintptr_t nvnTextureViewGetLayers;
	uintptr_t nvnTextureViewGetTarget;
	uintptr_t nvnTextureViewSetLevels;
	uintptr_t nvnTextureViewSetFormat;
	uintptr_t nvnTextureViewSetLayers;
	uintptr_t nvnTextureViewSetTarget;
	uintptr_t nvnTextureViewSetDefaults;
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
	uintptr_t eglGetProcAddress;
	uintptr_t eglSwapBuffers;
	uintptr_t eglSwapInterval;
	uintptr_t nvnCommandBufferSetRenderTargets;
	uintptr_t nvnCommandBufferCopyTextureToTexture;
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
typedef void* (*nvnWindowAcquireTexture_0)(const NVNWindow* nvnWindow, const void* nvnSync, int* index);
typedef void (*nvnSetPresentInterval_0)(const NVNWindow* nvnWindow, int mode);
typedef int (*nvnGetPresentInterval_0)(const NVNWindow* nvnWindow);
typedef void* (*nvnSyncWait_0)(const void* _this, uint64_t timeout_ns);
typedef void (*nvnTextureBuilderSetDefaults_0)(NVNTextureBuilder* _nvnTextureBuilder);
typedef void (*nvnTextureBuilderSetDevice_0)(NVNTextureBuilder* _nvnTextureBuilder, NVNDevice* _nvnDevice);
typedef NVNtextureFlags (*nvnTextureGetFlags_0)(NVNTexture* _nvnTexture);
typedef void (*nvnTextureBuilderSetFlags_0)(NVNTextureBuilder* _nvnTextureBuilder, NVNtextureFlags _nvnTextureFlags);
typedef NVNtextureTarget (*nvnTextureGetTarget_0)(NVNTexture* _nvnTexture);
typedef void (*nvnTextureBuilderSetTarget_0)(NVNTextureBuilder* _nvnTextureBuilder, NVNtextureTarget _nvnTextureTarget);
typedef NVNformat (*nvnTextureGetFormat_0)(NVNTexture* _nvnTexture);
typedef void (*nvnTextureBuilderSetFormat_0)(NVNTextureBuilder* _nvnTextureBuilder, NVNformat _nvnFormat);
typedef int (*nvnTextureGetHeight_0)(NVNTexture* _nvnTexture);
typedef int (*nvnTextureGetWidth_0)(NVNTexture* _nvnTexture);
typedef void (*nvnTextureBuilderSetSize2D_0)(NVNTextureBuilder* _nvnTextureBuilder, int width, int height);
typedef NVNMemoryPool* (*nvnTextureGetMemoryPool_0)(NVNTexture* _nvnTexture);
typedef ptrdiff_t (*nvnTextureGetMemoryOffset_0)(NVNTexture* _nvnTexture);
typedef void (*nvnTextureBuilderSetStorage_0)(NVNTextureBuilder* _nvnTextureBuilder, const NVNMemoryPool* _nvnMemoryPool, ptrdiff_t offset);
typedef bool (*nvnTextureInitialize_0)(NVNTexture* _nvnTexture, const NVNTextureBuilder* _nvnTextureBuilder);
typedef void (*nvnTextureFinalize_0)(NVNTexture* _nvnTexture);
typedef void (*nvnMemoryPoolBuilderSetDefaults_0)(NVNMemoryPoolBuilder* _nvnMemoryPoolBuilder);
typedef void (*nvnMemoryPoolBuilderSetDevice_0)(NVNMemoryPoolBuilder* _nvnMemoryPoolBuilder, NVNDevice* _nvnDevice);
typedef NVNmemoryPoolFlags (*nvnMemoryPoolGetFlags_0)(NVNMemoryPool* _nvnMemoryPool);
typedef void (*nvnMemoryPoolBuilderSetFlags_0)(NVNMemoryPoolBuilder* _nvnMemoryPoolBuilder, NVNmemoryPoolFlags _nvnMemoryPoolFlags);
typedef size_t (*nvnTextureBuilderGetStorageSize_0)(NVNTextureBuilder* _nvnTextureBuilder);
typedef size_t (*nvnTextureBuilderGetStorageAlignment_0)(NVNTextureBuilder* _nvnTextureBuilder);
typedef bool (*nvnMemoryPoolInitialize_0)(NVNMemoryPool* _nvnMemoryPool, NVNMemoryPoolBuilder* _nvnMemoryPoolBuilder);
typedef void (*nvnMemoryPoolFinalize_0)(NVNMemoryPool* _nvnMemoryPool);
typedef void (*nvnMemoryPoolBuilderSetStorage_0)(NVNMemoryPoolBuilder* _nvnMemoryPoolBuilder, void* buffer, size_t size);
typedef void (*nvnCommandBufferSetRenderTargets_0)(const void* nvnCommandBuffer, int numBufferedFrames, NVNTexture** nvnTextures, NVNTextureView** nvnTextureViews, NVNTexture* nvnDepthTexture, NVNTextureView* nvnDepthTextureView);
typedef void (*nvnCommandBufferCopyTextureToTexture_0)(const void* nvnCommandBuffer, const NVNTexture* nvnTextureSrc, NVNTextureView* nvnTextureSrcView, void* unk2, const NVNTexture* nvnTextureDst, NVNTextureView* nvnTextureDstView, void* unk4);
typedef bool (*nvnTextureViewGetFormat_0)(const NVNTextureView *nvnTextureView, NVNformat *nvnFormat);
typedef bool (*nvnTextureViewGetTarget_0)(const NVNTextureView *nvnTextureView, NVNtextureTarget *target);
typedef bool (*nvnTextureViewGetLevels_0)(const NVNtextureView *nvnTextureView, int *nvnTextureLevel, int *numLevels);
typedef bool (*nvnTextureViewGetLayers_0)(const NVNtextureView *nvnTextureView, int *numLayer, int *numLayers);
typedef bool (*nvnTextureViewSetFormat_0)(const NVNTextureView *nvnTextureView, NVNformat nvnFormat);
typedef bool (*nvnTextureViewSetTarget_0)(const NVNTextureView *nvnTextureView, NVNtextureTarget target);
typedef bool (*nvnTextureViewSetLevels_0)(const NVNtextureView *nvnTextureView, int nvnTextureLevel, int numLevels);
typedef bool (*nvnTextureViewSetLayers_0)(const NVNtextureView *nvnTextureView, int numLayer, int numLayers);
typedef void (*nvnTextureViewSetDefaults_0)(const NVNtextureView *nvnTextureView);

void* WindowSync = 0;
uint64_t startFrameTick = 0;

enum {
	ZeroSyncType_None,
	ZeroSyncType_Soft,
	ZeroSyncType_Semi
};

inline void createBuildidPath(const uint64_t buildid, char* titleid, char* buffer) {
	strcpy(buffer, "sdmc:/SaltySD/plugins/FPSLocker/patches/0");
	strcat(buffer, &titleid[0]);
	strcat(buffer, "/");
	ltoa(buildid, &titleid[0], 16);
	int zero_count = 16 - strlen(&titleid[0]);
	for (int i = 0; i < zero_count; i++) {
		strcat(buffer, "0");
	}
	strcat(buffer, &titleid[0]);
	strcat(buffer, ".bin");	
}

inline void CheckTitleID(char* buffer) {
    uint64_t titid = 0;
    svcGetInfo(&titid, 18, CUR_PROCESS_HANDLE, 0);	
    ltoa(titid, buffer, 16);
}

inline uint64_t getMainAddress() {
	MemoryInfo memoryinfo = {0};
	u32 pageinfo = 0;

	uint64_t base_address = SaltySDCore_getCodeStart() + 0x4000;
	for (size_t i = 0; i < 3; i++) {
		Result rc = svcQueryMemory(&memoryinfo, &pageinfo, base_address);
		if (R_FAILED(rc)) return 0;
		if ((memoryinfo.addr == base_address) && (memoryinfo.perm & Perm_X))
			return base_address;
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
	
	bool FPSlock_delayed = false;
	
	if (!starttick) {
		*(Shared.API) = 3;
		starttick = ((_ZN2nn2os13GetSystemTickEv_0)(Address_weaks.GetSystemTick))();
	}
	if (FPStiming && !LOCK::blockDelayFPS) {
		if ((((_ZN2nn2os13GetSystemTickEv_0)(Address_weaks.GetSystemTick))() - frameend) < FPStiming) {
			FPSlock_delayed = true;
		}
		while ((((_ZN2nn2os13GetSystemTickEv_0)(Address_weaks.GetSystemTick))() - frameend) < FPStiming) {
			svcSleepThread(-2);
		}
	}

	uint32_t vulkanResult = ((_ZN11NvSwapchain15QueuePresentKHREP9VkQueue_TPK16VkPresentInfoKHR_0)(Address_weaks.nvSwapchainQueuePresentKHR))(VkQueue_T, VkPresentInfoKHR);
	endtick = ((_ZN2nn2os13GetSystemTickEv_0)(Address_weaks.GetSystemTick))();
	framedelta = endtick - frameend;
	frameavg = ((9*frameavg) + framedelta) / 10;
	Stats.FPSavg = systemtickfrequency / (float)frameavg;

	if (FPSlock_delayed && FPStiming) {
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
	}

	frameend = endtick;
	
	FPS_temp++;
	deltatick = endtick - starttick;

	Shared.FPSticks[FPStickItr++] = framedelta;
	FPStickItr %= 10;

	if (deltatick > systemtickfrequency) {
		starttick = ((_ZN2nn2os13GetSystemTickEv_0)(Address_weaks.GetSystemTick))();
		Stats.FPS = FPS_temp - 1;
		FPS_temp = 0;
		*(Shared.FPS) = Stats.FPS;
		if (changeFPS && !configRC && FPSlock) {
			LOCK::applyPatch(configBuffer, configSize, FPSlock);
			*(Shared.patchApplied) = 1;
		}
	}

	*(Shared.FPSavg) = Stats.FPSavg;
	*(Shared.pluginActive) = true;

	if (FPSlock != *(Shared.FPSlocked)) {
		if ((*(Shared.FPSlocked) < 60) && (*(Shared.FPSlocked) > 0)) {
			FPStiming = (systemtickfrequency/(*(Shared.FPSlocked))) - 6000;
		}
		else FPStiming = 0;
		FPSlock = *(Shared.FPSlocked);
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
	
	bool FPSlock_delayed = false;
	
	if (!starttick) {
		*(Shared.API) = 3;
		starttick = ((_ZN2nn2os13GetSystemTickEv_0)(Address_weaks.GetSystemTick))();
	}
	if (FPStiming && !LOCK::blockDelayFPS) {
		if ((((_ZN2nn2os13GetSystemTickEv_0)(Address_weaks.GetSystemTick))() - frameend) < FPStiming) {
			FPSlock_delayed = true;
		}
		while ((((_ZN2nn2os13GetSystemTickEv_0)(Address_weaks.GetSystemTick))() - frameend) < FPStiming) {
			svcSleepThread(-2);
		}
	}

	uint32_t vulkanResult = ((vkQueuePresentKHR_0)(Address_weaks.vkQueuePresentKHR))(VkQueue, VkPresentInfoKHR);
	endtick = ((_ZN2nn2os13GetSystemTickEv_0)(Address_weaks.GetSystemTick))();
	framedelta = endtick - frameend;
	frameavg = ((9*frameavg) + framedelta) / 10;
	Stats.FPSavg = systemtickfrequency / (float)frameavg;

	if (FPSlock_delayed && FPStiming) {
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
	}

	frameend = endtick;
	
	FPS_temp++;
	deltatick = endtick - starttick;

	Shared.FPSticks[FPStickItr++] = framedelta;
	FPStickItr %= 10;

	if (deltatick > systemtickfrequency) {
		starttick = ((_ZN2nn2os13GetSystemTickEv_0)(Address_weaks.GetSystemTick))();
		Stats.FPS = FPS_temp - 1;
		FPS_temp = 0;
		*(Shared.FPS) = Stats.FPS;
		if (changeFPS && !configRC && FPSlock) {
			LOCK::applyPatch(configBuffer, configSize, FPSlock);
			*(Shared.patchApplied) = 1;
		}
	}

	*(Shared.FPSavg) = Stats.FPSavg;
	*(Shared.pluginActive) = true;

	if (FPSlock != *(Shared.FPSlocked)) {
		if ((*(Shared.FPSlocked) < 60) && (*(Shared.FPSlocked) > 0)) {
			FPStiming = (systemtickfrequency/(*(Shared.FPSlocked))) - 6000;
		}
		else FPStiming = 0;
		FPSlock = *(Shared.FPSlocked);
	}
	
	return vulkanResult;
}

int eglInterval(const void* EGLDisplay, int interval) {
	int result = false;
	if (!changeFPS) {
		result = ((eglSwapInterval_0)(Address_weaks.eglSwapInterval))(EGLDisplay, interval);
		changedFPS = false;
		*(Shared.FPSmode) = interval;
	}
	else if (interval < 0) {
		interval *= -1;
		if (*(Shared.FPSmode) != interval) {
			result = ((eglSwapInterval_0)(Address_weaks.eglSwapInterval))(EGLDisplay, interval);
			*(Shared.FPSmode) = interval;
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
	
	bool FPSlock_delayed = false;

	if (!starttick) {
		*(Shared.API) = 2;
		starttick = ((_ZN2nn2os13GetSystemTickEv_0)(Address_weaks.GetSystemTick))();
	}
	if (FPStiming && !LOCK::blockDelayFPS) {
		if ((((_ZN2nn2os13GetSystemTickEv_0)(Address_weaks.GetSystemTick))() - frameend) < FPStiming) {
			FPSlock_delayed = true;
		}
		while ((((_ZN2nn2os13GetSystemTickEv_0)(Address_weaks.GetSystemTick))() - frameend) < FPStiming) {
			svcSleepThread(-2);
		}
	}
	
	int result = ((eglSwapBuffers_0)(Address_weaks.eglSwapBuffers))(EGLDisplay, EGLSurface);
	endtick = ((_ZN2nn2os13GetSystemTickEv_0)(Address_weaks.GetSystemTick))();
	framedelta = endtick - frameend;
	frameavg = ((9*frameavg) + framedelta) / 10;
	Stats.FPSavg = systemtickfrequency / (float)frameavg;

	if (FPSlock_delayed && FPStiming) {
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
	}

	frameend = endtick;
	
	FPS_temp++;
	deltatick = endtick - starttick;

	Shared.FPSticks[FPStickItr++] = framedelta;
	FPStickItr %= 10;

	if (deltatick > systemtickfrequency) {
		starttick = ((_ZN2nn2os13GetSystemTickEv_0)(Address_weaks.GetSystemTick))();
		Stats.FPS = FPS_temp - 1;
		FPS_temp = 0;
		*(Shared.FPS) = Stats.FPS;
		if (changeFPS && !configRC && FPSlock) {
			LOCK::applyPatch(configBuffer, configSize, FPSlock);
			*(Shared.patchApplied) = 1;
		}
	}
	
	*(Shared.FPSavg) = Stats.FPSavg;
	*(Shared.pluginActive) = true;

	if (FPSlock != *(Shared.FPSlocked)) {
		changeFPS = true;
		changedFPS = false;
		if (*(Shared.FPSlocked) == 0) {
			FPStiming = 0;
			changeFPS = false;
			FPSlock = *(Shared.FPSlocked);
		}
		else if (*(Shared.FPSlocked) <= 30) {
			eglInterval(EGLDisplay, -2);
			if (*(Shared.FPSlocked) != 30) {
				FPStiming = (systemtickfrequency/(*(Shared.FPSlocked))) - 6000;
			}
			else FPStiming = 0;
		}
		else {
			eglInterval(EGLDisplay, -1);
			if (*(Shared.FPSlocked) != 60) {
				FPStiming = (systemtickfrequency/(*(Shared.FPSlocked))) - 6000;
			}
			else FPStiming = 0;
		}
		if (changedFPS) {
			FPSlock = *(Shared.FPSlocked);
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

NVNTexture* orig_nvnTextures[2] = {0};
bool isDoubleBuffer = false;
int texture_index = -1;
void nvnCommandBufferCopyTextureToTexture(const void* nvnCommandBuffer, const NVNTexture* nvnTextureSrc, NVNTextureView* nvnTextureSrcView, void* unk2, NVNTexture* nvnTextureDst, NVNTextureView* nvnTextureDstView, void* unk4) {
	if (isDoubleBuffer && texture_index == 2) {
		nvnTextureDst = Frame_buffers[texture_index];
	}
	return ((nvnCommandBufferCopyTextureToTexture_0)(Ptrs.nvnCommandBufferCopyTextureToTexture))(nvnCommandBuffer, nvnTextureSrc, unk1, unk2, nvnTextureDst, unk3, unk4);
}

void nvnCommandBufferSetRenderTargets(const void* nvnCommandBuffer, int numBufferedFrames, NVNTexture** nvnTextures, NVNTextureView** nvnTexturesView, NVNTexture* nvnDepthTexture, NVNTextureView* nvnDepthTextureView) {
	if (isDoubleBuffer) {
		static bool initialized = false;
		if (nvnTextures[0] == orig_nvnTextures[0]) {
			initialized = true;
			((nvnCommandBufferSetRenderTargets_0)(Ptrs.nvnCommandBufferSetRenderTargets))(nvnCommandBuffer, numBufferedFrames, nvnTextures, unk1, nvnDepthTexture, unk2);
			NVNformat m_nvnformat = 0;
			((nvnTextureViewGetFormat_0)(Ptrs.nvnTextureViewGetFormat))(nvnTexturesView[0], &m_nvnformat);
			int m_nvnlevel = 0;
			int m_nvnlevels = 0;
			((nvnTextureViewGetLevels_0)(Ptrs.nvnTextureViewGetLevels))(nvnTexturesView[0], &m_nvnlevel, &m_nvnlevels);
			NVNtextureTarget m_nvnTextureTarget = 0;
			((nvnTextureViewGetTarget_0)(Ptrs.nvnTextureViewGetTarget))(nvnTexturesView[0], &m_nvnTextureTarget);
			int m_nvnlayer = 0;
			int m_nvnlayers = 0;
			((nvnTextureViewGetLayers_0)(Ptrs.nvnTextureViewGetLayers))(nvnTexturesView[0], &m_nvnlayer, &m_nvnlayers);
			((nvnTextureViewSetDefaults_0)(Ptrs.nvnTextureViewSetDefaults))(&m_ThirdBufferView);
			((nvnTextureViewSetFormat_0)(Ptrs.nvnTextureViewSetFormat))(&m_ThirdBufferView, m_nvnformat);
			((nvnTextureViewSetLevels_0)(Ptrs.nvnTextureViewSetLevels))(&m_ThirdBufferView, m_nvnlevel, m_nvnlevels);
			((nvnTextureViewSetTarget_0)(Ptrs.nvnTextureViewSetTarget))(&m_ThirdBufferView, m_nvnTextureTarget);
			((nvnTextureViewSetLayers_0)(Ptrs.nvnTextureViewSetTarget))(&m_ThirdBufferView, m_nvnlayer, m_nvnlayers);
		}
	}
	return ((nvnCommandBufferSetRenderTargets_0)(Ptrs.nvnCommandBufferSetRenderTargets))(nvnCommandBuffer, numBufferedFrames, nvnTextures, unk1, nvnDepthTexture, unk2);
}

void nvnWindowBuilderSetTextures(const nvnWindowBuilder* nvnWindowBuilder, int numBufferedFrames, NVNTexture** nvnTextures) {
	if (numBufferedFrames == 2 /*&& *(Shared.SetBuffers) == 3*/) {
		isDoubleBuffer = true;
		orig_nvnTextures[0] = nvnTextures[0];
		orig_nvnTextures[1] = nvnTextures[1];
		static bool initialized = false;
		static void* buffer = 0;

		((nvnTextureBuilderSetDevice_0)(Ptrs.nvnTextureBuilderSetDevice))(&m_nvnTextureBuilder, m_nvnDevice);
		((nvnTextureBuilderSetDefaults_0)(Ptrs.nvnTextureBuilderSetDefaults))(&m_nvnTextureBuilder);
		NVNtextureFlags m_nvnTextureFlags = ((nvnTextureGetFlags_0)(Ptrs.nvnTextureGetFlags))(nvnTextures[0]);
		((nvnTextureBuilderSetFlags_0)(Ptrs.nvnTextureBuilderSetFlags))(&m_nvnTextureBuilder, m_nvnTextureFlags);
		NVNtextureTarget m_nvnTextureTarget = ((nvnTextureGetTarget_0)(Ptrs.nvnTextureGetTarget))(nvnTextures[0]);
		((nvnTextureBuilderSetTarget_0)(Ptrs.nvnTextureBuilderSetTarget))(&m_nvnTextureBuilder, m_nvnTextureTarget);
		NVNformat m_nvnFormat = ((nvnTextureGetFormat_0)(Ptrs.nvnTextureGetFormat))(nvnTextures[0]);
		((nvnTextureBuilderSetFormat_0)(Ptrs.nvnTextureBuilderSetFormat))(&m_nvnTextureBuilder, m_nvnFormat);
		int width = ((nvnTextureGetWidth_0)(Ptrs.nvnTextureGetWidth))(nvnTextures[0]);
		int height = ((nvnTextureGetHeight_0)(Ptrs.nvnTextureGetHeight))(nvnTextures[0]);
		((nvnTextureBuilderSetSize2D_0)(Ptrs.nvnTextureBuilderSetSize2D))(&m_nvnTextureBuilder, width, height);
		NVNMemoryPool* m_nvnMemoryPool = ((nvnTextureGetMemoryPool_0)(Ptrs.nvnTextureGetMemoryPool))(nvnTextures[0]);

		((nvnMemoryPoolBuilderSetDefaults_0)(Ptrs.nvnMemoryPoolBuilderSetDefaults))(&m_nvnMemoryPoolBuilder);
		((nvnMemoryPoolBuilderSetDevice_0)(Ptrs.nvnMemoryPoolBuilderSetDevice))(&m_nvnMemoryPoolBuilder, m_nvnDevice);
		NVNmemoryPoolFlags m_memoryPoolFlags = ((nvnMemoryPoolGetFlags_0)(Ptrs.nvnMemoryPoolGetFlags))(m_nvnMemoryPool);
		((nvnMemoryPoolBuilderSetFlags_0)(Ptrs.nvnMemoryPoolBuilderSetFlags))(&m_nvnMemoryPoolBuilder, m_memoryPoolFlags);
		if (initialized) {
			((nvnTextureFinalize_0)(Ptrs.nvnTextureFinalize))(&m_ThirdBuffer);
			((nvnMemoryPoolFinalize_0)(Ptrs.nvnMemoryPoolFinalize))(&m_ThirdBufferPool);
			((free_0)(Address_weaks.free))(buffer);
		}
		
		size_t buffer_size = ((nvnTextureBuilderGetStorageSize_0)(Ptrs.nvnTextureBuilderGetStorageSize))(&m_nvnTextureBuilder);
		size_t alignment = ((nvnTextureBuilderGetStorageAlignment_0)(Ptrs.nvnTextureBuilderGetStorageAlignment))(&m_nvnTextureBuilder);
		size_t aligned_size = buffer_size;
		if (aligned_size % alignment != 0) {
			aligned_size += alignment - (aligned_size % alignment);
		}
		buffer = ((aligned_alloc_0)(Address_weaks.alignedAlloc))(alignment, buffer_size);
		((nvnMemoryPoolBuilderSetStorage_0)(Ptrs.nvnMemoryPoolBuilderSetStorage))(&m_nvnMemoryPoolBuilder, buffer, aligned_size);
		((nvnMemoryPoolInitialize_0)(Ptrs.nvnMemoryPoolInitialize))(&m_ThirdBufferPool, &m_nvnMemoryPoolBuilder);
		((nvnTextureBuilderSetStorage_0)(Ptrs.nvnTextureBuilderSetStorage))(&m_nvnTextureBuilder, &m_ThirdBufferPool, 0);

		initialized = true;
		((nvnTextureInitialize_0)(Ptrs.nvnTextureInitialize))(&m_ThirdBuffer, &m_nvnTextureBuilder);
		Frame_buffers[0] = nvnTextures[0];
		Frame_buffers[1] = nvnTextures[1];
		Frame_buffers[2] = &m_ThirdBuffer;
		nvnTextures = Frame_buffers;
		numBufferedFrames = 3;
		*(Shared.Buffers) = 3;
		*(Shared.ActiveBuffers) = 3;
		*(Shared.SetActiveBuffers) = 2;

	}
	else if (*(Shared.SetBuffers) >= 2 && *(Shared.SetBuffers) <= numBufferedFrames) {
		numBufferedFrames = *(Shared.SetBuffers);
	}
	*(Shared.Buffers) = numBufferedFrames;
	*(Shared.ActiveBuffers) = numBufferedFrames;
	return ((nvnBuilderSetTextures_0)(Ptrs.nvnWindowBuilderSetTextures))(nvnWindowBuilder, numBufferedFrames, nvnTextures);
}

void nvnWindowSetNumActiveTextures(const NVNWindow* nvnWindow, int numBufferedFrames) {
	*(Shared.SetActiveBuffers) = numBufferedFrames;
	if (*(Shared.SetBuffers) >= 2 && *(Shared.SetBuffers) <= *(Shared.Buffers)) {
		numBufferedFrames = *(Shared.SetBuffers);
	}
	*(Shared.ActiveBuffers) = numBufferedFrames;
	return ((nvnWindowSetNumActiveTextures_0)(Ptrs.nvnWindowSetNumActiveTextures))(nvnWindow, numBufferedFrames);
}

void nvnSetPresentInterval(const NVNWindow* nvnWindow, int mode) {
	if (!changeFPS) {
		((nvnSetPresentInterval_0)(Ptrs.nvnWindowSetPresentInterval))(nvnWindow, mode);
		changedFPS = false;
		*(Shared.FPSmode) = mode;
	}
	else if (mode < 0) {
		mode *= -1;
		if (*(Shared.FPSmode) != mode) {
			((nvnSetPresentInterval_0)(Ptrs.nvnWindowSetPresentInterval))(nvnWindow, mode);
			*(Shared.FPSmode) = mode;
		}
		changedFPS = true;
	}
	return;
}

void* nvnSyncWait0(const void* _this, uint64_t timeout_ns) {
	uint64_t endFrameTick = ((_ZN2nn2os13GetSystemTickEv_0)(Address_weaks.GetSystemTick))();
	if (_this == WindowSync && *(Shared.ActiveBuffers) == 2) {
		if (*(Shared.ZeroSync) == ZeroSyncType_Semi) {
			u64 FrameTarget = (systemtickfrequency/60) - 8000;
			s64 new_timeout = (FrameTarget - (endFrameTick - startFrameTick)) - 19200;
			if (*(Shared.FPSlocked) == 60) {
				new_timeout = (systemtickfrequency/101) - (endFrameTick - startFrameTick);
			}
			if (new_timeout > 0) {
				timeout_ns = ((_ZN2nn2os17ConvertToTimeSpanENS0_4TickE_0)(Address_weaks.ConvertToTimeSpan))(new_timeout);
			}
			else timeout_ns = 0;
		}
		else if (*(Shared.ZeroSync) == ZeroSyncType_Soft) 
			timeout_ns = 0;
	}
	return ((nvnSyncWait_0)(Ptrs.nvnSyncWait))(_this, timeout_ns);
}

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
	
	bool FPSlock_delayed = false;

	if (!starttick) {
		starttick = ((_ZN2nn2os13GetSystemTickEv_0)(Address_weaks.GetSystemTick))();
		*(Shared.FPSmode) = (uint8_t)((nvnGetPresentInterval_0)(Ptrs.nvnWindowGetPresentInterval))(nvnWindow);
	}
	
	if (FPSlock) {
		if ((*(Shared.ZeroSync) == ZeroSyncType_None) && FPStiming && (FPSlock == 60 || FPSlock == 30)) {
			FPStiming = 0;
		}
		else if ((*(Shared.ZeroSync) != ZeroSyncType_None) && !FPStiming) {
			if (FPSlock == 60) {
				FPStiming = (systemtickfrequency/(*(Shared.FPSlocked))) - 8000;
			}
			else FPStiming = (systemtickfrequency/(*(Shared.FPSlocked))) - 6000;
		}
	}

	if (FPStiming && !LOCK::blockDelayFPS) {
		if ((((_ZN2nn2os13GetSystemTickEv_0)(Address_weaks.GetSystemTick))() - frameend) < FPStiming) {
			FPSlock_delayed = true;
		}
		while ((((_ZN2nn2os13GetSystemTickEv_0)(Address_weaks.GetSystemTick))() - frameend) < FPStiming) {
			svcSleepThread(-2);
		}
	}
	
	((nvnQueuePresentTexture_0)(Ptrs.nvnQueuePresentTexture))(_this, nvnWindow, unk3);
	endtick = ((_ZN2nn2os13GetSystemTickEv_0)(Address_weaks.GetSystemTick))();
	framedelta = endtick - frameend;

	Shared.FPSticks[FPStickItr++] = framedelta;
	FPStickItr %= 10;
	
	frameavg = ((9*frameavg) + framedelta) / 10;
	Stats.FPSavg = systemtickfrequency / (float)frameavg;

	if (FPSlock_delayed && FPStiming) {
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
	}

	frameend = endtick;
	FPS_temp++;
	deltatick = endtick - starttick;
	if (deltatick > systemtickfrequency) {
		starttick = ((_ZN2nn2os13GetSystemTickEv_0)(Address_weaks.GetSystemTick))();
		Stats.FPS = FPS_temp - 1;
		FPS_temp = 0;
		*(Shared.FPS) = Stats.FPS;
		*(Shared.FPSmode) = (uint8_t)((nvnGetPresentInterval_0)(Ptrs.nvnWindowGetPresentInterval))(nvnWindow);
		if (changeFPS && !configRC && FPSlock) {
			LOCK::applyPatch(configBuffer, configSize, FPSlock);
			*(Shared.patchApplied) = 1;
		}
	}

	*(Shared.FPSavg) = Stats.FPSavg;
	*(Shared.pluginActive) = true;

	if (FPSlock != *(Shared.FPSlocked)) {
		changeFPS = true;
		changedFPS = false;
		if (*(Shared.FPSlocked) == 0) {
			FPStiming = 0;
			changeFPS = false;
			FPSlock = *(Shared.FPSlocked);
		}
		else if (*(Shared.FPSlocked) <= 30) {
			nvnSetPresentInterval(nvnWindow, -2);
			if (*(Shared.FPSlocked) != 30 || *(Shared.ZeroSync)) {
				if (*(Shared.FPSlocked) == 30) {
					FPStiming = (systemtickfrequency/(*(Shared.FPSlocked))) - 8000;
				}
				else FPStiming = (systemtickfrequency/(*(Shared.FPSlocked))) - 6000;
			}
			else FPStiming = 0;
		}
		else {
			nvnSetPresentInterval(nvnWindow, -2); //This allows in game with glitched interval to unlock 60 FPS, f.e. WRC Generations
			nvnSetPresentInterval(nvnWindow, -1);
			if (*(Shared.FPSlocked) != 60 || *(Shared.ZeroSync)) {
				if (*(Shared.FPSlocked) == 60) {
					FPStiming = (systemtickfrequency/(*(Shared.FPSlocked))) - 8000;
				}
				else FPStiming = (systemtickfrequency/(*(Shared.FPSlocked))) - 6000;
			}
			else FPStiming = 0;
		}
		if (changedFPS) {
			FPSlock = *(Shared.FPSlocked);
		}
	}
	
	return;
}

void* nvnAcquireTexture(const NVNWindow* nvnWindow, const void* nvnSync, int* index) {
	if (WindowSync != nvnSync) {
		WindowSync = (void*)nvnSync;
	}
	void* ret = ((nvnWindowAcquireTexture_0)(Ptrs.nvnWindowAcquireTexture))(nvnWindow, nvnSync, index);
	startFrameTick = ((_ZN2nn2os13GetSystemTickEv_0)(Address_weaks.GetSystemTick))();
	texture_index = *index;
	return ret;
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
	else if (!strcmp("nvnSyncWait", nvnFunction)) {
		Ptrs.nvnSyncWait = address;
		return Address.nvnSyncWait;
	}
	else if (!strcmp("nvnTextureBuilderSetDevice", nvnFunction)) {
		Ptrs.nvnTextureBuilderSetDevice = address;
	}
	else if (!strcmp("nvnTextureBuilderSetDefaults", nvnFunction)) {
		Ptrs.nvnTextureBuilderSetDefaults = address;
	}
	else if (!strcmp("nvnTextureBuilderSetFlags", nvnFunction)) {
		Ptrs.nvnTextureBuilderSetFlags = address;
	}
	else if (!strcmp("nvnTextureBuilderSetSize2D", nvnFunction)) {
		Ptrs.nvnTextureBuilderSetSize2D = address;
	}
	else if (!strcmp("nvnTextureBuilderSetTarget", nvnFunction)) {
		Ptrs.nvnTextureBuilderSetTarget = address;
	}
	else if (!strcmp("nvnTextureBuilderSetFormat", nvnFunction)) {
		Ptrs.nvnTextureBuilderSetFormat = address;
	}
	else if (!strcmp("nvnTextureBuilderSetStorage", nvnFunction)) {
		Ptrs.nvnTextureBuilderSetStorage = address;
	}
	else if (!strcmp("nvnTextureInitialize", nvnFunction)) {
		Ptrs.nvnTextureInitialize = address;
	}
	else if (!strcmp("nvnTextureFinalize", nvnFunction)) {
		Ptrs.nvnTextureFinalize = address;
	}
	else if (!strcmp("nvnTextureGetFlags", nvnFunction)) {
		Ptrs.nvnTextureGetFlags = address;
	}
	else if (!strcmp("nvnTextureGetTarget", nvnFunction)) {
		Ptrs.nvnTextureGetTarget = address;
	}
	else if (!strcmp("nvnTextureGetFormat", nvnFunction)) {
		Ptrs.nvnTextureGetFormat = address;
	}
	else if (!strcmp("nvnTextureGetHeight", nvnFunction)) {
		Ptrs.nvnTextureGetHeight = address;
	}
	else if (!strcmp("nvnTextureGetWidth", nvnFunction)) {
		Ptrs.nvnTextureGetWidth = address;
	}
	else if (!strcmp("nvnTextureGetMemoryPool", nvnFunction)) {
		Ptrs.nvnTextureGetMemoryPool = address;
	}
	else if (!strcmp("nvnTextureGetMemoryOffset", nvnFunction)) {
		Ptrs.nvnTextureGetMemoryOffset = address;
	}
	else if (!strcmp("nvnMemoryPoolBuilderSetDefaults", nvnFunction)) {
		Ptrs.nvnMemoryPoolBuilderSetDefaults = address;
	}
	else if (!strcmp("nvnMemoryPoolBuilderSetDevice", nvnFunction)) {
		Ptrs.nvnMemoryPoolBuilderSetDevice = address;
	}
	else if (!strcmp("nvnMemoryPoolGetFlags", nvnFunction)) {
		Ptrs.nvnMemoryPoolGetFlags = address;
	}
	else if (!strcmp("nvnMemoryPoolBuilderSetFlags", nvnFunction)) {
		Ptrs.nvnMemoryPoolBuilderSetFlags = address;
	}
	else if (!strcmp("nvnMemoryPoolFinalize", nvnFunction)) {
		Ptrs.nvnMemoryPoolFinalize = address;
	}
	else if (!strcmp("nvnTextureBuilderGetStorageSize", nvnFunction)) {
		Ptrs.nvnTextureBuilderGetStorageSize = address;
	}
	else if (!strcmp("nvnTextureBuilderGetStorageAlignment", nvnFunction)) {
		Ptrs.nvnTextureBuilderGetStorageAlignment = address;
	}
	else if (!strcmp("nvnMemoryPoolBuilderSetStorage", nvnFunction)) {
		Ptrs.nvnMemoryPoolBuilderSetStorage = address;
	}
	else if (!strcmp("nvnMemoryPoolInitialize", nvnFunction)) {
		Ptrs.nvnMemoryPoolInitialize = address;
	}
	else if (!strcmp("nvnCommandBufferSetRenderTargets", nvnFunction)) {
		Ptrs.nvnCommandBufferSetRenderTargets = address;
		return Address.nvnCommandBufferSetRenderTargets;
	}
	else if (!strcmp("nvnCommandBufferCopyTextureToTexture", nvnFunction)) {
		Ptrs.nvnCommandBufferCopyTextureToTexture = address;
		return Address.nvnCommandBufferCopyTextureToTexture;		
	}
	else if (!strcmp("nvnTextureViewGetLevels", nvnFunction)) {
		Ptrs.nvnTextureViewGetLevels = address;
	}
	else if (!strcmp("nvnTextureViewGetTarget", nvnFunction)) {
		Ptrs.nvnTextureViewGetTarget = address;
	}
	else if (!strcmp("nvnTextureViewGetLayers", nvnFunction)) {
		Ptrs.nvnTextureViewGetLayers = address;
	}
	else if (!strcmp("nvnTextureViewGetFormat", nvnFunction)) {
		Ptrs.nvnTextureViewGetFormat = address;
	}
	else if (!strcmp("nvnTextureViewSetLevels", nvnFunction)) {
		Ptrs.nvnTextureViewSetLevels = address;
	}
	else if (!strcmp("nvnTextureViewSetTarget", nvnFunction)) {
		Ptrs.nvnTextureViewSetTarget = address;
	}
	else if (!strcmp("nvnTextureViewSetLayers", nvnFunction)) {
		Ptrs.nvnTextureViewSetLayers = address;
	}
	else if (!strcmp("nvnTextureViewSetFormat", nvnFunction)) {
		Ptrs.nvnTextureViewSetFormat = address;
	}
	else if (!strcmp("nvnTextureViewSetDefaults", nvnFunction)) {
		Ptrs.nvnTextureViewSetDefaults = address;
	}
	return address;
}

uintptr_t nvnBootstrapLoader_1(const char* nvnName) {
	if (strcmp(nvnName, "nvnDeviceGetProcAddress") == 0) {
		*(Shared.API) = 1;
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
		Result ret = SaltySD_CheckIfSharedMemoryAvailable(&SharedMemoryOffset, 59);
		SaltySDCore_printf("NX-FPS: ret: 0x%X\n", ret);
		if (!ret) {
			SaltySDCore_printf("NX-FPS: MemoryOffset: %d\n", SharedMemoryOffset);

			uintptr_t base = (uintptr_t)shmemGetAddr(_sharedmemory) + SharedMemoryOffset;
			uint32_t* MAGIC = (uint32_t*)base;
			*MAGIC = 0x465053;
			Shared.FPS = (uint8_t*)(base + 4);
			Shared.FPSavg = (float*)(base + 5);
			Shared.pluginActive = (bool*)(base + 9);
			
			Address.nvnGetProcAddress = (uint64_t)&nvnGetProcAddress;
			Address.nvnQueuePresentTexture = (uint64_t)&nvnPresentTexture;
			Address.nvnWindowAcquireTexture = (uint64_t)&nvnAcquireTexture;
			Address_weaks.nvnBootstrapLoader = SaltySDCore_FindSymbolBuiltin("nvnBootstrapLoader");
			Address_weaks.eglSwapBuffers = SaltySDCore_FindSymbolBuiltin("eglSwapBuffers");
			Address_weaks.eglSwapInterval = SaltySDCore_FindSymbolBuiltin("eglSwapInterval");
			Address_weaks.vkQueuePresentKHR = SaltySDCore_FindSymbolBuiltin("vkQueuePresentKHR");
			Address_weaks.nvSwapchainQueuePresentKHR = SaltySDCore_FindSymbolBuiltin("_ZN11NvSwapchain15QueuePresentKHREP9VkQueue_TPK16VkPresentInfoKHR");
			Address_weaks.ConvertToTimeSpan = SaltySDCore_FindSymbolBuiltin("_ZN2nn2os17ConvertToTimeSpanENS0_4TickE");
			Address_weaks.GetSystemTick = SaltySDCore_FindSymbolBuiltin("_ZN2nn2os13GetSystemTickEv");
			Address_weaks.eglGetProcAddress = SaltySDCore_FindSymbolBuiltin("eglGetProcAddress");
			Address_weaks.alignedAlloc = SaltySDCore_FindSymbolBuiltin("aligned_alloc");
			Address_weaks.free = SaltySDCore_FindSymbolBuiltin("free");
			SaltySDCore_ReplaceImport("nvnBootstrapLoader", (void*)nvnBootstrapLoader_1);
			SaltySDCore_ReplaceImport("eglSwapBuffers", (void*)eglSwap);
			SaltySDCore_ReplaceImport("eglSwapInterval", (void*)eglInterval);
			SaltySDCore_ReplaceImport("vkQueuePresentKHR", (void*)vulkanSwap);
			SaltySDCore_ReplaceImport("_ZN11NvSwapchain15QueuePresentKHREP9VkQueue_TPK16VkPresentInfoKHR", (void*)vulkanSwap2);
			SaltySDCore_ReplaceImport("eglGetProcAddress", (void*)eglGetProc);

			Shared.FPSlocked = (uint8_t*)(base + 10);
			Shared.FPSmode = (uint8_t*)(base + 11);
			Shared.ZeroSync = (uint8_t*)(base + 12);
			Shared.patchApplied = (uint8_t*)(base + 13);
			Shared.API = (uint8_t*)(base + 14);
			Shared.FPSticks = (uint32_t*)(base + 15);
			Shared.Buffers = (uint8_t*)(base + 55);
			Shared.SetBuffers = (uint8_t*)(base + 56);
			Shared.ActiveBuffers = (uint8_t*)(base + 57);
			Shared.SetActiveBuffers = (uint8_t*)(base + 58);
			Address.nvnWindowSetPresentInterval = (uint64_t)&nvnSetPresentInterval;
			Address.nvnSyncWait = (uint64_t)&nvnSyncWait0;
			Address.nvnWindowBuilderSetTextures = (uint64_t)&nvnWindowBuilderSetTextures;
			Address.nvnWindowSetNumActiveTextures = (uint64_t)&nvnWindowSetNumActiveTextures;
			Address.eglGetProcAddress = (uint64_t)&eglGetProc;
			Address.eglSwapBuffers = (uint64_t)&eglSwap;
			Address.eglSwapInterval = (uint64_t)&eglInterval;
			Address.nvnCommandBufferSetRenderTargets = (uint64_t)&nvnCommandBufferSetRenderTargets;
			Address.nvnCommandBufferCopyTextureToTexture = (uint64_t)&nvnCommandBufferCopyTextureToTexture;

			char titleid[17];
			CheckTitleID(&titleid[0]);
			char path[128];
			strcpy(&path[0], "sdmc:/SaltySD/plugins/FPSLocker/0");
			strcat(&path[0], &titleid[0]);
			strcat(&path[0], ".dat");
			FILE* file_dat = SaltySDCore_fopen(path, "rb");
			if (file_dat) {
				uint8_t temp = 0;
				SaltySDCore_fread(&temp, 1, 1, file_dat);
				*(Shared.FPSlocked) = temp;
				SaltySDCore_fread(&temp, 1, 1, file_dat);
				*(Shared.ZeroSync) = temp;
				if (SaltySDCore_fread(&temp, 1, 1, file_dat))
					*(Shared.SetBuffers) = temp;
				SaltySDCore_fclose(file_dat);
			}

			u64 buildid = SaltySD_GetBID();
			if (!buildid) {
				SaltySDCore_printf("NX-FPS: getBID failed! Err: 0x%x\n", ret);
			}
			else {
				SaltySDCore_printf("NX-FPS: BID: %016lX\n", buildid);
				createBuildidPath(buildid, &titleid[0], &path[0]);
				FILE* patch_file = SaltySDCore_fopen(path, "rb");
				if (patch_file) {
					SaltySDCore_fclose(patch_file);
					SaltySDCore_printf("NX-FPS: FPSLocker: successfully opened: %s\n", path);
					configRC = readConfig(path, &configBuffer);
					if (LOCK::MasterWriteApplied) {
						*(Shared.patchApplied) = 2;
					}
					SaltySDCore_printf("NX-FPS: FPSLocker: readConfig rc: 0x%x\n", configRC);
					svcGetInfo(&LOCK::mappings.alias_start, InfoType_AliasRegionAddress, CUR_PROCESS_HANDLE, 0);
					svcGetInfo(&LOCK::mappings.heap_start, InfoType_HeapRegionAddress, CUR_PROCESS_HANDLE, 0);
				}
				else SaltySDCore_printf("NX-FPS: FPSLocker: File not found: %s\n", path);
			}
		}
		SaltySDCore_printf("NX-FPS: injection finished\n");
	}
}