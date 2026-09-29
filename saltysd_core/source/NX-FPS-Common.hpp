#pragma once

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
#include <cstring>

namespace LOCK {
	inline constinit LOCK::Patcher patcher;
}

struct runtime_replace {
	const char* name;
	uintptr_t* orig_ptr;
	void* hook_ptr;
	void (*cond_check)(bool* check);
};

inline Result configRC = 1;

inline uint32_t* sharedOperationMode = 0;

struct resolutionCalls {
	uint16_t width;
	uint16_t height;
	uint16_t calls;
};

inline bool resolutionLookup = false;

inline resolutionCalls m_resolutionRenderCalls[8] = {0};
inline resolutionCalls m_resolutionViewportCalls[8] = {0};

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

inline NxFpsSharedBlock* Shared = 0;

struct StatsData {
	uint8_t FPS = 0xFF;
	float FPSavg = 255;
	bool FPSmode = 0;
};
inline StatsData Stats;

#if defined(SWITCH) || defined(SWITCH32)
	#define systemtickfrequency 19200000
#elif defined(OUNCE) || defined(OUNCE32)
	#define systemtickfrequency 31250000
#else
	inline uint64_t systemtickfrequency = 0;
#endif
static_assert(systemtickfrequency != 0);

inline bool changeFPS = false;
inline bool changedFPS = false;

inline size_t fileBytesRead = 0;

inline std::pair<uint32_t, uint32_t> last_viewport = {0, 0};

namespace nn {

	inline void (*SetFocusHandlingMode_0)(AppletFocusHandlingMode mode);
	inline u32 (*FileAccessorRead_0)(void* fileHandle, size_t* bytesRead, int64_t position, void* buffer, size_t readBytes, unsigned int* ReadOption);
	inline u32 (*FileAccessorReadCache_0)(void* fileHandle, size_t* bytesRead, int64_t position, void* buffer, size_t readBytes, unsigned int* ReadOption, void* FileDataCacheAccessResult);
	inline u32 (*roLookupSymbol_0)(uintptr_t* pOutAddress, const char* name);
	inline u32 (*SetUserInactivityDetectionTimeExtended_0)(bool isTrue);

	inline Result FileAccessorRead(void* fileHandle, size_t* bytesRead, int64_t position, void* buffer, size_t readBytes, unsigned int* ReadOption) {
		size_t bytesRead_impl;
		if (!bytesRead)
			bytesRead = &bytesRead_impl;
		Result ret = FileAccessorRead_0(fileHandle, bytesRead, position, buffer, readBytes, ReadOption);
		if (R_SUCCEEDED(ret)) [[likely]] fileBytesRead += *bytesRead;
		return ret;
	}

	inline Result FileAccessorReadCache(void* fileHandle, size_t* bytesRead, int64_t position, void* buffer, size_t readBytes, unsigned int* ReadOption, void* FileDataCacheAccessResult) {
		size_t bytesRead_impl;
		if (!bytesRead)
			bytesRead = &bytesRead_impl;
		Result ret = FileAccessorReadCache_0(fileHandle, bytesRead, position, buffer, readBytes, ReadOption, FileDataCacheAccessResult);
		if (R_SUCCEEDED(ret)) [[likely]] fileBytesRead += *bytesRead;
		return ret;
	}

	Result SetUserInactivityDetectionTimeExtended(bool isTrue);

	inline AppletFocusHandlingMode defaultFocusHandlingMode = AppletFocusHandlingMode_SuspendHomeSleep;
	inline bool focusHandlingOverwrite = false;

	inline void setFocusHandlingMode(AppletFocusHandlingMode mode) {
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

	uint64_t _convertToTimeSpan(uint64_t tick);

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
	inline uint8_t FPS_temp = 0;
	inline uint64_t starttick = 0;
	inline uint64_t starttick2 = 0;
	inline uint64_t frameend = 0;
	inline uint64_t frameavg = 0;
	inline uint8_t FPSlock = 0;
	inline int32_t FPStiming = 0;
	inline uint8_t FPStickItr = 0;
	inline uint8_t range = 0;
	
	inline bool FPSlock_delayed = false;
	inline bool old_force = false;
	inline uint32_t new_fpslock = 0;

	inline void PreFrame() {
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

	inline void PostFrame() {
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

inline void checkReadFlag(bool* out) {
	FILE* readFlag = SaltySDCore_fopen("sdmc:/SaltySD/flags/blockfilestats.flag", "rb");
	if (readFlag) {
		SaltySDCore_fclose(readFlag);
		*out = false;
	}
	else {
		*out = true;
	}
}

