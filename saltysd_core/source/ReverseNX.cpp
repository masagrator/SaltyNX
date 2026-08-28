#if defined(SWITCH32)
#include <switch_min.h>
#define NX_PACKED PACKED
#define InfoType_ProgramId InfoType_TitleId
#define AppletMessage_OperationModeChanged AppletNotificationMessage_OperationModeChanged
#define AppletMessage_PerformanceModeChanged AppletNotificationMessage_PerformanceModeChanged
#elif defined(SWITCH)
#include <switch.h>
#else
#error "Unsupported base architecture!"
#endif

#include "saltysd_core.h"
#include "saltysd_ipc.h"
#include "saltysd_dynamic.h"
#include <cerrno>
#include <utility>
#include "nanoprintf.h"
#include <array>

enum ReverseNX_state : int8_t {
	ReverseNX_Switch_Invalid = -1,
	ReverseNX_Switch_Handheld = 0,
	ReverseNX_Switch_Docked = 1
};

struct ReverseNX_save {
	uint32_t MAGIC;
	uint8_t version;
	ReverseNX_state state;
} NX_PACKED;

static_assert(sizeof(ReverseNX_save) == 6);

struct runtime_replace {
	const char* name;
	void** orig_ptr;
	void* hook_ptr;
	void (*cond_check)(bool* check);
};

namespace nn { 
	namespace os {
		struct SystemEvent {
			char reserved[8];
		};
		struct SystemEventType : public SystemEvent {
			SystemEventType() = default;

			SystemEventType(const SystemEvent& base) : SystemEvent(base) {}
		};

		struct MultiWaitHolderType {
			char reserved[8];
		};
		struct MultiWaitType {
			char reserved[8];
		};

		static bool (*TryWaitSystemEvent)(SystemEventType*);
		static void (*WaitSystemEvent)(SystemEventType*);
		static void (*InitializeMultiWaitHolder)(MultiWaitHolderType*, SystemEventType*);
		static void (*LinkMultiWaitHolder)(MultiWaitType*, MultiWaitHolderType*);
		static void* (*WaitAny)(MultiWaitType*);
		static void* (*TimedWaitAny)(MultiWaitType*, uint64_t);

	}
	
	namespace oe {
		static u32 (*GetPerformanceMode)();
		static u8 (*GetOperationMode)();
		static bool (*TryPopNotificationMessage)(uint32_t* Message);
		static uint32_t (*PopNotificationMessage)();
		static void (*GetDefaultDisplayResolution)(int* width, int* height);
		static void (*GetDefaultDisplayResolutionChangeEvent)(nn::os::SystemEvent* systemEvent);
		static nn::os::SystemEvent* (*GetNotificationMessageEvent)();
	}
}

enum res_mode {
	res_mode_default = 0,
	res_mode_480p = 1,
	res_mode_540p = 2,
	res_mode_630p = 3,
	res_mode_720p = 4,
	res_mode_810p = 5,
	res_mode_900p = 6,
	res_mode_1080p = 7,
	res_mode_amount = 8
};

std::pair<int, int> resolutions[] = {{0 ,0}, {854, 480}, {960, 540}, {1120, 630}, {1280, 720}, {1440, 810}, {1600, 900}, {1920, 1080}};

struct Shared {
	uint32_t MAGIC;
	bool isDocked;
	bool def;
	bool pluginActive;
	struct {
		res_mode handheld_res: 4;
		res_mode docked_res: 4;
	} NX_PACKED res;
	bool wasDDRused;
} NX_PACKED;

static_assert(sizeof(Shared) == 9);

Shared* ReverseNX_RT;

ptrdiff_t SharedMemoryOffset2 = -1;

nn::os::SystemEvent* defaultDisplayResolutionChangeEventCopy = 0;
nn::os::SystemEvent* notificationMessageEventCopy = 0;
nn::os::MultiWaitHolderType* multiWaitHolderCopy = 0;
nn::os::MultiWaitType* multiWaitCopy = 0;
bool multiWaitHack = false;

static uint32_t* sharedOperationMode = 0;

ReverseNX_state loadSave() {
	char path[128];
    uint64_t titid = 0;
    svcGetInfo(&titid, InfoType_ProgramId, CUR_PROCESS_HANDLE, 0);
	#if defined(SWITCH32) || defined(OUNCE32)
	npf_snprintf(path, sizeof(path), "sdmc:/SaltySD/plugins/ReverseNX-RT/%016llX.dat", titid);
	#else
	npf_snprintf(path, sizeof(path), "sdmc:/SaltySD/plugins/ReverseNX-RT/%016lX.dat", titid);
	#endif
	errno = 0;
	FILE* save_file = SaltySDCore_fopen(path, "rb");
	if (save_file) {
		ReverseNX_save header;
		SaltySDCore_fread(&header, sizeof(header), 1, save_file);
		if (header.MAGIC != *(uint32_t*)&"NXRT") {
			SaltySDCore_fclose(save_file);
			SaltySDCore_printf("ReverseNX: Save had wrong magic!\n", path);
			return ReverseNX_Switch_Invalid;
		}
		if (header.version != 1 && header.version != 2) {
			SaltySDCore_fclose(save_file);
			SaltySDCore_printf("ReverseNX: Save had wrong version!\n", path);
			return ReverseNX_Switch_Invalid;
		}
		if (header.version == 2) {
			int8_t res_mode_h = -1;
			SaltySDCore_fread(&res_mode_h, 1, 1, save_file);
			int8_t res_mode_d = -1;
			SaltySDCore_fread(&res_mode_d, 1, 1, save_file);
			if (res_mode_h >= 0 && res_mode_h < res_mode_amount && res_mode_d >= 0 && res_mode_d < res_mode_amount) {
				ReverseNX_RT->res.handheld_res = (res_mode)res_mode_h;
				ReverseNX_RT->res.docked_res = (res_mode)res_mode_d;
			}
		}
		SaltySDCore_fclose(save_file);
		if (header.state > ReverseNX_Switch_Docked || header.state < ReverseNX_Switch_Handheld) {
			SaltySDCore_printf("ReverseNX: Save had wrong state!\n", path);
			return ReverseNX_Switch_Invalid;
		}
		SaltySDCore_printf("ReverseNX: Save loaded successfully!\n", path);
		return header.state;
	}
	else {
		if (errno == -2)
			SaltySDCore_printf("ReverseNX: Couldn't load save from %s! Using default settings.\n", path);
		else SaltySDCore_printf("ReverseNX: Couldn't load save from %s! Errno: %d. Using default settings.\n", path, errno);
		return ReverseNX_Switch_Invalid;
	}
}

bool TryPopNotificationMessage(uint32_t* msg) {

	static bool check1 = true;
	static bool check2 = true;
	static bool compare = false;
	static bool compare2 = false;

	if (!ReverseNX_RT->pluginActive) ReverseNX_RT->pluginActive = true;

	if (ReverseNX_RT->def) {
		if (!check1) {
			*msg = AppletMessage_OperationModeChanged;
			check1 = true;
			return true;
		}
		else if (!check2) {
			*msg = AppletMessage_PerformanceModeChanged;
			check2 = true;
			return true;
		}
		else if (multiWaitHack == true) {
			*msg = 0xFF;
			multiWaitHack = false;
			return true;
		}
		else return nn::oe::TryPopNotificationMessage(msg);
	}
	
	check1 = false;
	check2 = false;
	if (compare2 != ReverseNX_RT->isDocked) {
		*msg = AppletMessage_OperationModeChanged;
		compare2 = ReverseNX_RT->isDocked;
		return true;
	}
	if (compare != ReverseNX_RT->isDocked) {
		*msg = AppletMessage_PerformanceModeChanged;
		compare = ReverseNX_RT->isDocked;
		return true;
	}
	if (multiWaitHack == true) {
		*msg = 0xFF;
		multiWaitHack = false;
		return true;
	}
	return nn::oe::TryPopNotificationMessage(msg);
}

int PopNotificationMessage() {
	uint32_t msg = 0;
	while (true) {
		if (TryPopNotificationMessage(&msg)) {
			return msg;
		}
		svcSleepThread(1000000);
	}
}

uint32_t GetPerformanceMode() {
	*sharedOperationMode = nn::oe::GetPerformanceMode();
	if (ReverseNX_RT->def) ReverseNX_RT->isDocked = *sharedOperationMode;
	
	return ReverseNX_RT->isDocked;
}

uint8_t GetOperationMode() {
	//Fix for Unravel Two that calls this function constantly without checking notifications
	if (!ReverseNX_RT->pluginActive) ReverseNX_RT->pluginActive = true;
	*sharedOperationMode = nn::oe::GetOperationMode();
	if (ReverseNX_RT->def) ReverseNX_RT->isDocked = *sharedOperationMode;
	
	return ReverseNX_RT->isDocked;
}

/* 
	Used by Red Dead Redemption.

	Without using functions above, mode is detected by checking what is
	default display resolution of currently running mode.
	Those are:
	Handheld - 1280x720
	Docked - 1920x1080 only when true handheld mode is detected
	
	Game is waiting for DefaultDisplayResolutionChange event to check again
	which mode is currently in use. And to do that nn::os::TryWaitSystemEvent is used
	that is always returning flag without waiting for it to change.
	
	So solution is to replace flag returned by nn::os::TryWaitSystemEvent
	when DefaultDisplayResolutionChange event is passed as argument,
	and replace values written by nn::oe::GetDefaultDisplayResolution.

*/
void GetDefaultDisplayResolution(int* width, int* height) {
	if (ReverseNX_RT->wasDDRused == false) {
		ReverseNX_RT->wasDDRused = true;
		ReverseNX_RT->pluginActive = true;
	}
	*sharedOperationMode = nn::oe::GetPerformanceMode();
	if (ReverseNX_RT->def) {
		nn::oe::GetDefaultDisplayResolution(width, height);
		ReverseNX_RT->isDocked = *sharedOperationMode;
	}
	else {
		if (*sharedOperationMode && ReverseNX_RT->isDocked && ReverseNX_RT->res.docked_res == res_mode_default) {
			nn::oe::GetDefaultDisplayResolution(width, height);
			return;
		}
		if (*sharedOperationMode && !(ReverseNX_RT->isDocked) && ReverseNX_RT->res.handheld_res == res_mode_default) {
			*width = 1280;
			*height = 720;
			return;
		}
		if (!*sharedOperationMode && !(ReverseNX_RT->isDocked) && ReverseNX_RT->res.handheld_res == res_mode_default) {
			nn::oe::GetDefaultDisplayResolution(width, height);
			return;
		}
		if (!*sharedOperationMode && (ReverseNX_RT->isDocked) && ReverseNX_RT->res.docked_res == res_mode_default) {
			*width = 1920;
			*height = 1080;
			return;
		}
		res_mode res = ReverseNX_RT->isDocked ? ReverseNX_RT->res.docked_res : ReverseNX_RT->res.handheld_res;
		if (res == res_mode_default) {
			nn::oe::GetDefaultDisplayResolution(width, height);
			return;
		}
		*width = resolutions[res].first;
		*height = resolutions[res].second;
	}
}

void GetDefaultDisplayResolutionChangeEvent(nn::os::SystemEvent* systemEvent) {
	nn::oe::GetDefaultDisplayResolutionChangeEvent(systemEvent);
	defaultDisplayResolutionChangeEventCopy = systemEvent;
}

bool TryWaitSystemEvent(nn::os::SystemEventType* systemEvent) {
	static bool check = true;
	static bool compare = false;
	static uint8_t compare_res_mode_h = 0;
	static uint8_t compare_res_mode_d = 0;

	if (systemEvent != defaultDisplayResolutionChangeEventCopy && systemEvent != notificationMessageEventCopy) 
		return nn::os::TryWaitSystemEvent(systemEvent);

	if (ReverseNX_RT->def) {
		bool ret = nn::os::TryWaitSystemEvent(systemEvent);
		compare = ReverseNX_RT->isDocked;
		if (!check) {
			check = true;
			return true;
		}
		return ret;
	}
	bool last_check = check;
	check = false;
	bool ret = nn::os::TryWaitSystemEvent(systemEvent);
	if (last_check || ret || compare != ReverseNX_RT->isDocked || compare_res_mode_d != ReverseNX_RT->res.docked_res || compare_res_mode_h != ReverseNX_RT->res.handheld_res) {
		compare = ReverseNX_RT->isDocked;
		compare_res_mode_d = ReverseNX_RT->res.docked_res;
		compare_res_mode_h = ReverseNX_RT->res.handheld_res;
		return true;
	}
	return false;
}

void WaitSystemEvent(nn::os::SystemEventType* systemEvent) {
	if (systemEvent == defaultDisplayResolutionChangeEventCopy) {
		while(true) {
			bool return_now = TryWaitSystemEvent(systemEvent);
			if (return_now)
				return;
			svcSleepThread(20'000'000);
		}
	}
	return nn::os::WaitSystemEvent(systemEvent);
}

/* 
	Used by Monster Hunter Rise and The Legend of Zelda: Echoes of Wisdom

	Game won't check if mode was changed until NotificationMessage event will be flagged.
	Functions below are detecting which MultiWait includes NotificationMessage event,
	and for that MultiWait passed as argument to nn::os::WaitAny it is redirected to nn::os::TimedWaitAny
	with timeout set to 1ms so we can force game to check NotificationMessage every 1ms.

	Almost all games are checking NotificationMessage in loops instead of waiting for event,
	so even though this is not a clean solution, it works and performance impact is negligible.
*/

nn::os::SystemEvent* GetNotificationMessageEvent() {
	notificationMessageEventCopy = nn::oe::GetNotificationMessageEvent();
	return notificationMessageEventCopy;
}

void InitializeMultiWaitHolder(nn::os::MultiWaitHolderType* MultiWaitHolderType, nn::os::SystemEventType* systemEvent) {
	nn::os::InitializeMultiWaitHolder(MultiWaitHolderType, systemEvent);
	if (systemEvent == notificationMessageEventCopy) 
		multiWaitHolderCopy = MultiWaitHolderType;
}

void LinkMultiWaitHolder(nn::os::MultiWaitType* MultiWaitType, nn::os::MultiWaitHolderType* MultiWaitHolderType) {
	nn::os::LinkMultiWaitHolder(MultiWaitType, MultiWaitHolderType);
	if (MultiWaitHolderType == multiWaitHolderCopy)
		multiWaitCopy = MultiWaitType;
}

void* WaitAny(nn::os::MultiWaitType* MultiWaitType) {
	if (multiWaitCopy != MultiWaitType)
		return nn::os::WaitAny(MultiWaitType);
	if (!ReverseNX_RT->pluginActive) ReverseNX_RT->pluginActive = true;
	void* ret_value = nn::os::TimedWaitAny(MultiWaitType, 1000000);
	if (ret_value != NULL) return ret_value;
	multiWaitHack = true;
	return multiWaitHolderCopy;
}

std::array replacements = {
	runtime_replace{"_ZN2nn2oe18GetPerformanceModeEv", (void**)&nn::oe::GetPerformanceMode, (void*)GetPerformanceMode, nullptr},
	runtime_replace{"_ZN2nn2oe16GetOperationModeEv", (void**)&nn::oe::GetOperationMode, (void*)GetOperationMode, nullptr},
	runtime_replace{"_ZN2nn2oe25TryPopNotificationMessageEPj", (void**)&nn::oe::TryPopNotificationMessage, (void*)TryPopNotificationMessage, nullptr},
	runtime_replace{"_ZN2nn2oe22PopNotificationMessageEv", (void**)&nn::oe::PopNotificationMessage, (void*)PopNotificationMessage, nullptr},
	runtime_replace{"_ZN2nn2oe27GetDefaultDisplayResolutionEPiS1_", (void**)&nn::oe::GetDefaultDisplayResolution, (void*)GetDefaultDisplayResolution, nullptr},
	runtime_replace{"_ZN2nn2oe38GetDefaultDisplayResolutionChangeEventEPNS_2os11SystemEventE", (void**)&nn::oe::GetDefaultDisplayResolutionChangeEvent, (void*)GetDefaultDisplayResolutionChangeEvent, nullptr},
	runtime_replace{"_ZN2nn2os18TryWaitSystemEventEPNS0_15SystemEventTypeE", (void**)&nn::os::TryWaitSystemEvent, (void*)TryWaitSystemEvent, nullptr},
	runtime_replace{"_ZN2nn2os15WaitSystemEventEPNS0_15SystemEventTypeE", (void**)&nn::os::WaitSystemEvent, (void*)WaitSystemEvent, nullptr},
	runtime_replace{"_ZN2nn2os25InitializeMultiWaitHolderEPNS0_19MultiWaitHolderTypeEPNS0_15SystemEventTypeE", (void**)&nn::os::InitializeMultiWaitHolder, (void*)InitializeMultiWaitHolder, nullptr},
	runtime_replace{"_ZN2nn2os19LinkMultiWaitHolderEPNS0_13MultiWaitTypeEPNS0_19MultiWaitHolderTypeE", (void**)&nn::os::LinkMultiWaitHolder, (void*)LinkMultiWaitHolder, nullptr},
	runtime_replace{"_ZN2nn2os7WaitAnyEPNS0_13MultiWaitTypeE", (void**)&nn::os::WaitAny, (void*)WaitAny, nullptr},
	runtime_replace{"_ZN2nn2os12TimedWaitAnyEPNS0_13MultiWaitTypeENS_8TimeSpanE", (void**)&nn::os::TimedWaitAny, nullptr, nullptr},
	runtime_replace{"_ZN2nn2oe27GetNotificationMessageEventEv", (void**)&nn::oe::GetNotificationMessageEvent, (void*)GetNotificationMessageEvent, nullptr},
};

extern "C" {
	void ReverseNX(SharedMemory* _sharedmemory, uint32_t* _sharedOperationMode) {
		sharedOperationMode = _sharedOperationMode;
		SaltySDCore_printf("ReverseNX: alive\n");
		Result ret = SaltySD_CheckIfSharedMemoryAvailable(&SharedMemoryOffset2, 7);
		SaltySDCore_printf("ReverseNX: SharedMemory ret: 0x%X\n", ret);
		if (!ret) {
			SaltySDCore_printf("ReverseNX: SharedMemory MemoryOffset: %d\n", SharedMemoryOffset2);

			ReverseNX_RT = (Shared*)__builtin_assume_aligned((const void*)((uintptr_t)shmemGetAddr(_sharedmemory) + SharedMemoryOffset2), 4);
			ReverseNX_RT->MAGIC = *(uint32_t*)&"NXRT";
			ReverseNX_RT->pluginActive = false;
			ReverseNX_state state = loadSave();
			if (state == ReverseNX_Switch_Docked || state == ReverseNX_Switch_Handheld) {
				ReverseNX_RT->isDocked = state;
				ReverseNX_RT->def = false;
			}
			else {
				ReverseNX_RT->isDocked = false;
				ReverseNX_RT->def = true;
			}
			
			uintptr_t addr = SaltySDCore_FindSymbolBuiltin("_ZN2nn2oe18GetPerformanceModeEv");
			*(void**)&nn::oe::GetPerformanceMode = (void*)addr;
			addr = SaltySDCore_FindSymbolBuiltin("_ZN2nn2oe16GetOperationModeEv");
			*(void**)&nn::oe::GetOperationMode = (void*)addr;
			addr = SaltySDCore_FindSymbolBuiltin("_ZN2nn2oe25TryPopNotificationMessageEPj");
			*(void**)&nn::oe::TryPopNotificationMessage = (void*)addr;
			addr = SaltySDCore_FindSymbolBuiltin("_ZN2nn2oe22PopNotificationMessageEv");
			*(void**)&nn::oe::PopNotificationMessage = (void*)addr;
			addr = SaltySDCore_FindSymbolBuiltin("_ZN2nn2oe27GetDefaultDisplayResolutionEPiS1_");
			*(void**)&nn::oe::GetDefaultDisplayResolution = (void*)addr;
			addr = SaltySDCore_FindSymbolBuiltin("_ZN2nn2oe38GetDefaultDisplayResolutionChangeEventEPNS_2os11SystemEventE");
			*(void**)&nn::oe::GetDefaultDisplayResolutionChangeEvent = (void*)addr;
			addr = SaltySDCore_FindSymbolBuiltin("_ZN2nn2os18TryWaitSystemEventEPNS0_15SystemEventTypeE");
			*(void**)&nn::os::TryWaitSystemEvent = (void*)addr;
			addr = SaltySDCore_FindSymbolBuiltin("_ZN2nn2os15WaitSystemEventEPNS0_15SystemEventTypeE");
			*(void**)&nn::os::WaitSystemEvent = (void*)addr;
			addr = SaltySDCore_FindSymbolBuiltin("_ZN2nn2oe27GetNotificationMessageEventEv");
			*(void**)&nn::oe::GetNotificationMessageEvent = (void*)addr;
			addr = SaltySDCore_FindSymbolBuiltin("_ZN2nn2os25InitializeMultiWaitHolderEPNS0_19MultiWaitHolderTypeEPNS0_15SystemEventTypeE");
			*(void**)&nn::os::InitializeMultiWaitHolder = (void*)addr;
			addr = SaltySDCore_FindSymbolBuiltin("_ZN2nn2os19LinkMultiWaitHolderEPNS0_13MultiWaitTypeEPNS0_19MultiWaitHolderTypeE");
			*(void**)&nn::os::LinkMultiWaitHolder = (void*)addr;
			addr = SaltySDCore_FindSymbolBuiltin("_ZN2nn2os7WaitAnyEPNS0_13MultiWaitTypeE");
			*(void**)&nn::os::WaitAny = (void*)addr;
			addr = SaltySDCore_FindSymbolBuiltin("_ZN2nn2os12TimedWaitAnyEPNS0_13MultiWaitTypeENS_8TimeSpanE");
			*(void**)&nn::os::TimedWaitAny = (void*)addr;
			SaltySDCore_ReplaceImport("_ZN2nn2oe25TryPopNotificationMessageEPj", (void*)TryPopNotificationMessage);
			SaltySDCore_ReplaceImport("_ZN2nn2oe22PopNotificationMessageEv", (void*)PopNotificationMessage);
			SaltySDCore_ReplaceImport("_ZN2nn2oe18GetPerformanceModeEv", (void*)GetPerformanceMode);
			SaltySDCore_ReplaceImport("_ZN2nn2oe16GetOperationModeEv", (void*)GetOperationMode);
			SaltySDCore_ReplaceImport("_ZN2nn2oe27GetDefaultDisplayResolutionEPiS1_", (void*)GetDefaultDisplayResolution);
			SaltySDCore_ReplaceImport("_ZN2nn2oe38GetDefaultDisplayResolutionChangeEventEPNS_2os11SystemEventE", (void*)GetDefaultDisplayResolutionChangeEvent);
			SaltySDCore_ReplaceImport("_ZN2nn2os18TryWaitSystemEventEPNS0_15SystemEventTypeE", (void*)TryWaitSystemEvent);
			SaltySDCore_ReplaceImport("_ZN2nn2os15WaitSystemEventEPNS0_15SystemEventTypeE", (void*)WaitSystemEvent);
			SaltySDCore_ReplaceImport("_ZN2nn2oe27GetNotificationMessageEventEv", (void*)GetNotificationMessageEvent);
			SaltySDCore_ReplaceImport("_ZN2nn2os25InitializeMultiWaitHolderEPNS0_19MultiWaitHolderTypeEPNS0_15SystemEventTypeE", (void*)InitializeMultiWaitHolder);
			SaltySDCore_ReplaceImport("_ZN2nn2os19LinkMultiWaitHolderEPNS0_13MultiWaitTypeEPNS0_19MultiWaitHolderTypeE", (void*)LinkMultiWaitHolder);
			SaltySDCore_ReplaceImport("_ZN2nn2os7WaitAnyEPNS0_13MultiWaitTypeE", (void*)WaitAny);

		}
		
		SaltySDCore_printf("ReverseNX: injection finished\n");
	}
}
