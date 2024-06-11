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

typedef int NVNtextureFlags;
typedef int NVNtextureTarget;
typedef int NVNformat;
typedef int NVNmemoryPoolFlags;

NVNWindow* m_nvnWindow = 0;
NVNDevice* m_nvnDevice = 0;

extern "C" {
	typedef u64 (*nvnBootstrapLoader_0)(const char * nvnName);
	typedef int (*eglSwapBuffers_0)(const void* EGLDisplay, const void* EGLSurface);
	typedef int (*eglSwapInterval_0)(const void* EGLDisplay, int interval);
	typedef u32 (*vkQueuePresentKHR_0)(const void* vkQueue, const void* VkPresentInfoKHR);
	typedef u32 (*_ZN11NvSwapchain15QueuePresentKHREP9VkQueue_TPK16VkPresentInfoKHR_0)(const void* VkQueue_T, const void* VkPresentInfoKHR);
	typedef u64 (*_ZN2nn2os17ConvertToTimeSpanENS0_4TickE_0)(u64 tick);
	typedef u64 (*_ZN2nn2os13GetSystemTickEv_0)();
	typedef u64 (*eglGetProcAddress_0)(const char* eglName);
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
	uint8_t* displaySync = 0;
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
	uintptr_t nvnWindowInitialize;
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
	if (FPStiming && !LOCK::blockDelayFPS && *(Shared.displaySync) < *(Shared.FPSlocked)) {
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
	if (FPStiming && !LOCK::blockDelayFPS && *(Shared.displaySync) < *(Shared.FPSlocked)) {
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

bool nvnWindowInitialize(const NVNWindow* nvnWindow, struct nvnWindowBuilder* windowBuilder) {
	m_nvnWindow = (NVNWindow*)nvnWindow;
	if (!*(Shared.Buffers)) {
		*(Shared.Buffers) = windowBuilder -> numBufferedFrames;
		if (*(Shared.SetBuffers) >= 2 && *(Shared.SetBuffers) <= windowBuilder -> numBufferedFrames) {
			windowBuilder -> numBufferedFrames = *(Shared.SetBuffers);
		}
		*(Shared.ActiveBuffers) = windowBuilder -> numBufferedFrames;	
	}
	return ((nvnWindowInitialize_0)(Ptrs.nvnWindowInitialize))(nvnWindow, windowBuilder);
}

void nvnWindowBuilderSetTextures(const nvnWindowBuilder* nvnWindowBuilder, int numBufferedFrames, NVNTexture** nvnTextures) {
	*(Shared.Buffers) = numBufferedFrames;
	if (*(Shared.SetBuffers) >= 2 && *(Shared.SetBuffers) <= numBufferedFrames) {
		numBufferedFrames = *(Shared.SetBuffers);
	}
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

	if (FPStiming && !LOCK::blockDelayFPS && *(Shared.displaySync) < *(Shared.FPSlocked)) {
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

	if (FPSlock != *(Shared.FPSlocked) || (*(Shared.FPSlocked) > 30 && *(Shared.FPSmode) > 1)) {
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

void* nvnAcquireTexture(const NVNWindow* nvnWindow, const void* nvnSync, const void* index) {
	if (WindowSync != nvnSync) {
		WindowSync = (void*)nvnSync;
	}
	void* ret = ((nvnWindowAcquireTexture_0)(Ptrs.nvnWindowAcquireTexture))(nvnWindow, nvnSync, index);
	startFrameTick = ((_ZN2nn2os13GetSystemTickEv_0)(Address_weaks.GetSystemTick))();
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
	else if (!strcmp("nvnWindowInitialize", nvnFunction)) {
		Ptrs.nvnWindowInitialize = address;
		return Address.nvnWindowInitialize;
	}
	else if (!strcmp("nvnSyncWait", nvnFunction)) {
		Ptrs.nvnSyncWait = address;
		return Address.nvnSyncWait;
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
		Result ret = SaltySD_CheckIfSharedMemoryAvailable(&SharedMemoryOffset, 60);
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
			Address.nvnWindowInitialize = (uint64_t)&nvnWindowInitialize;
			Address_weaks.nvnBootstrapLoader = SaltySDCore_FindSymbolBuiltin("nvnBootstrapLoader");
			Address_weaks.eglSwapBuffers = SaltySDCore_FindSymbolBuiltin("eglSwapBuffers");
			Address_weaks.eglSwapInterval = SaltySDCore_FindSymbolBuiltin("eglSwapInterval");
			Address_weaks.vkQueuePresentKHR = SaltySDCore_FindSymbolBuiltin("vkQueuePresentKHR");
			Address_weaks.nvSwapchainQueuePresentKHR = SaltySDCore_FindSymbolBuiltin("_ZN11NvSwapchain15QueuePresentKHREP9VkQueue_TPK16VkPresentInfoKHR");
			Address_weaks.ConvertToTimeSpan = SaltySDCore_FindSymbolBuiltin("_ZN2nn2os17ConvertToTimeSpanENS0_4TickE");
			Address_weaks.GetSystemTick = SaltySDCore_FindSymbolBuiltin("_ZN2nn2os13GetSystemTickEv");
			Address_weaks.eglGetProcAddress = SaltySDCore_FindSymbolBuiltin("eglGetProcAddress");
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
			Shared.displaySync = (uint8_t*)(base + 59);
			Address.nvnWindowSetPresentInterval = (uint64_t)&nvnSetPresentInterval;
			Address.nvnSyncWait = (uint64_t)&nvnSyncWait0;
			Address.nvnWindowBuilderSetTextures = (uint64_t)&nvnWindowBuilderSetTextures;
			Address.nvnWindowSetNumActiveTextures = (uint64_t)&nvnWindowSetNumActiveTextures;
			Address.eglGetProcAddress = (uint64_t)&eglGetProc;
			Address.eglSwapBuffers = (uint64_t)&eglSwap;
			Address.eglSwapInterval = (uint64_t)&eglInterval;

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
				if (temp >= 40 && temp <= 75) {
					FILE* sync_file = SaltySDCore_fopen("sdmc:/SaltySD/flags/displaysync.flag", "rb");
					if  (sync_file) {
						SaltySDCore_fclose(sync_file);
						SaltySD_SetDisplayRefreshRate(temp);
						*(Shared.displaySync) = temp;
					}
				}
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
