#include <switch_min.h>

#include "saltysd_core.h"
#include "saltysd_ipc.h"
#include "saltysd_dynamic.h"
#include <cerrno>
#include <utility>

enum ReverseNX_state {
	ReverseNX_Switch_Invalid = -1,
	ReverseNX_Switch_Handheld = 0,
	ReverseNX_Switch_Docked = 1
};

struct ReverseNX_save {
	char MAGIC[4];
	uint8_t version;
	uint8_t state;
} NX_PACKED;

struct SystemEvent {
	const char reserved[16];
	bool flag;
};

typedef u32 (*_ZN2nn2oe18GetPerformanceModeEv)();
typedef u8 (*_ZN2nn2oe16GetOperationModeEv)();
typedef bool (*_ZN2nn2oe25TryPopNotificationMessageEPj)(int &Message);
typedef int (*_ZN2nn2oe22PopNotificationMessageEv)();
typedef void (*_ZN2nn2oe27GetDefaultDisplayResolutionEPiS1_)(int* width, int* height);
typedef void (*_ZN2nn2oe38GetDefaultDisplayResolutionChangeEventEPNS_2os11SystemEventE)(SystemEvent* systemEvent);
typedef bool (*nnosTryWaitSystemEvent)(SystemEvent* systemEvent);
typedef void (*nnosWaitSystemEvent)(SystemEvent* systemEvent);
typedef SystemEvent* (*_ZN2nn2oe27GetNotificationMessageEventEv)();
typedef void (*nnosInitializeMultiWaitHolderForSystemEvent)(void* MultiWaitHolderType, SystemEvent* systemEvent);
typedef void (*nnosLinkMultiWaitHolder)(void* MultiWaitType, void* MultiWaitHolderType);
typedef void* (*nnosWaitAny)(void* MultiWaitType);
typedef void* (*nnosTimedWaitAny)(void* MultiWaitType, u64 TimeSpan);

struct {
	uintptr_t GetPerformanceMode;
	uintptr_t GetOperationMode;
	uintptr_t TryPopNotificationMessage;
	uintptr_t PopNotificationMessage;
	uintptr_t GetDefaultDisplayResolution;
	uintptr_t GetDefaultDisplayResolutionChangeEvent;
	uintptr_t TryWaitSystemEvent;
	uintptr_t WaitSystemEvent;
	uintptr_t GetNotificationMessageEvent;
	uintptr_t InitializeMultiWaitHolderForSystemEvent;
	uintptr_t LinkMultiWaitHolder;
	uintptr_t WaitAny;
	uintptr_t TimedWaitAny;	
} Address_weaks;

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
	} PACKED res;
} PACKED;

Shared* ReverseNX_RT;

static uint32_t *sharedOperationMode = 0;

ReverseNX_state loadSave() {
	char path[128];
    uint64_t titid = 0;
    svcGetInfo(&titid, InfoType_TitleId, CUR_PROCESS_HANDLE, 0);	
	snprintf(path, sizeof(path), "sdmc:/SaltySD/plugins/ReverseNX-RT/%016llX.dat", titid);
	errno = 0;
	FILE* save_file = SaltySDCore_fopen(path, "rb");
	if (save_file) {
		uint32_t MAGIC = 0;
		SaltySDCore_fread(&MAGIC, 4, 1, save_file);
		if (MAGIC != 0x5452584E) {
			SaltySDCore_fclose(save_file);
			SaltySDCore_printf("ReverseNX: Save had wrong magic!\n", path);
			return ReverseNX_Switch_Invalid;
		}
		uint8_t version = 0;
		SaltySDCore_fread(&version, 1, 1, save_file);
		if (version != 1 && version != 2) {
			SaltySDCore_fclose(save_file);
			SaltySDCore_printf("ReverseNX: Save had wrong version!\n", path);
			return ReverseNX_Switch_Invalid;
		}
		uint8_t state = ReverseNX_Switch_Invalid;
		SaltySDCore_fread(&state, 1, 1, save_file);
		if (version == 2) {
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
		if (state > ReverseNX_Switch_Docked) {
			SaltySDCore_printf("ReverseNX: Save had wrong state!\n", path);
			return ReverseNX_Switch_Invalid;
		}
		SaltySDCore_printf("ReverseNX: Save loaded successfully!\n", path);
		return (ReverseNX_state)state;
	}
	else {
		if (errno == -2)
			SaltySDCore_printf("ReverseNX: Couldn't load save from %s! Using default settings.\n", path);
		else SaltySDCore_printf("ReverseNX: Couldn't load save from %s! Errno: %d. Using default settings.\n", path, errno);
		return ReverseNX_Switch_Invalid;
	}
}

ptrdiff_t SharedMemoryOffset2 = -1;

SystemEvent* defaultDisplayResolutionChangeEventCopy = 0;
SystemEvent* notificationMessageEventCopy = 0;
void* multiWaitHolderCopy = 0;
void* multiWaitCopy = 0;

bool TryPopNotificationMessage(int &msg) {

	static bool check1 = true;
	static bool check2 = true;
	static bool compare = false;
	static bool compare2 = false;

	(ReverseNX_RT->pluginActive) = true;

	if ((ReverseNX_RT->def)) {
		if (!check1) {
			check1 = true;
			msg = 0x1f;
			return true;
		}
		else if (!check2) {
			check2 = true;
			msg = 0x1e;
			return true;
		}
		else return ((_ZN2nn2oe25TryPopNotificationMessageEPj)(Address_weaks.TryPopNotificationMessage))(msg);
	}
	
	check1 = false;
	check2 = false;
	if (compare2 != (ReverseNX_RT->isDocked)) {
		compare2 = (ReverseNX_RT->isDocked);
		msg = 0x1f;
		return true;
	}
	if (compare != (ReverseNX_RT->isDocked)) {
		compare = (ReverseNX_RT->isDocked);
		msg = 0x1e;
		return true;
	}
	
	return ((_ZN2nn2oe25TryPopNotificationMessageEPj)(Address_weaks.TryPopNotificationMessage))(msg);
}

int PopNotificationMessage() {
	while (true) {
		int msg = 0;
		if (TryPopNotificationMessage(msg)) {
			return msg;
		}
		svcSleepThread(1000000);
	}
}

uint32_t GetPerformanceMode() {
	*sharedOperationMode = ((_ZN2nn2oe18GetPerformanceModeEv)(Address_weaks.GetPerformanceMode))();
	if ((ReverseNX_RT->def)) (ReverseNX_RT->isDocked) = *sharedOperationMode;
	
	return (ReverseNX_RT->isDocked);
}

uint8_t GetOperationMode() {
	*sharedOperationMode = ((_ZN2nn2oe16GetOperationModeEv)(Address_weaks.GetOperationMode))();
	if ((ReverseNX_RT->def)) (ReverseNX_RT->isDocked) = *sharedOperationMode;
	
	return (ReverseNX_RT->isDocked);
}

/* 
	Used by Red Dead Redemption.

	Without using functions above, mode is detected by checking what is
	default display resolution of currently running mode.
	Those are:
	Handheld - 1280x720
	Docked - 1920x1080
	
	Game is waiting for DefaultDisplayResolutionChange event to check again
	which mode is currently in use. And to do that nn::os::TryWaitSystemEvent is used
	that is always returning flag without waiting for it to change.
	
	So solution is to replace flag returned by nn::os::TryWaitSystemEvent
	when DefaultDisplayResolutionChange event is passed as argument,
	and replace values written by nn::oe::GetDefaultDisplayResolution.

*/
void GetDefaultDisplayResolution(int* width, int* height) {
	*sharedOperationMode = ((_ZN2nn2oe18GetPerformanceModeEv)(Address_weaks.GetPerformanceMode))();
	if (ReverseNX_RT->def) {
		((_ZN2nn2oe27GetDefaultDisplayResolutionEPiS1_)(Address_weaks.GetDefaultDisplayResolution))(width, height);
		ReverseNX_RT->isDocked = *sharedOperationMode;
	}
	else {
		if (ReverseNX_RT->isDocked && ReverseNX_RT->res.docked_res == res_mode_default) {
			if (!*sharedOperationMode) {
				*width = 1920;
				*height = 1080;
			}
			else {
				*width = 1280;
				*height = 720;
			}
		}
		else if (*sharedOperationMode && ReverseNX_RT->isDocked && ReverseNX_RT->res.docked_res == res_mode_default) {
			((_ZN2nn2oe27GetDefaultDisplayResolutionEPiS1_)(Address_weaks.GetDefaultDisplayResolution))(width, height);
		}
		else {
			*width = resolutions[ReverseNX_RT->isDocked ? ReverseNX_RT->res.docked_res : ReverseNX_RT->res.handheld_res].first;
			*height = resolutions[ReverseNX_RT->isDocked ? ReverseNX_RT->res.docked_res : ReverseNX_RT->res.handheld_res].second;
		}
	}
}

void GetDefaultDisplayResolutionChangeEvent(SystemEvent* systemEvent) {
	((_ZN2nn2oe38GetDefaultDisplayResolutionChangeEventEPNS_2os11SystemEventE)(Address_weaks.GetDefaultDisplayResolutionChangeEvent))(systemEvent);
	defaultDisplayResolutionChangeEventCopy = systemEvent;
}

bool TryWaitSystemEvent(SystemEvent* systemEvent) {
	static bool check = true;
	static bool compare = false;
	static uint8_t compare_res_mode_h = 0;
	static uint8_t compare_res_mode_d = 0;

	if (systemEvent != defaultDisplayResolutionChangeEventCopy) 
		return ((nnosTryWaitSystemEvent)(Address_weaks.TryWaitSystemEvent))(systemEvent);

	if (ReverseNX_RT->def) {
		bool ret = ((nnosTryWaitSystemEvent)(Address_weaks.TryWaitSystemEvent))(systemEvent);
		compare = ReverseNX_RT->isDocked;
		if (systemEvent == defaultDisplayResolutionChangeEventCopy && !check) {
			check = true;
			return true;
		}
		return ret;
	}
	check = false;
	bool ret = ((nnosTryWaitSystemEvent)(Address_weaks.TryWaitSystemEvent))(systemEvent);
	if (ret || compare != ReverseNX_RT->isDocked || compare_res_mode_d != ReverseNX_RT->res.docked_res || compare_res_mode_h != ReverseNX_RT->res.handheld_res) {
		compare = ReverseNX_RT->isDocked;
		compare_res_mode_d = ReverseNX_RT->res.docked_res;
		compare_res_mode_h = ReverseNX_RT->res.handheld_res;
		return true;
	}
	return false;
}

void WaitSystemEvent(SystemEvent* systemEvent) {
	if (systemEvent == defaultDisplayResolutionChangeEventCopy) {
		(ReverseNX_RT->pluginActive) = true;
		while(true) {
			bool return_now = TryWaitSystemEvent(systemEvent);
			if (return_now)
				return;
			svcSleepThread(20'000'000);
		}
	}
	return ((nnosWaitSystemEvent)(Address_weaks.WaitSystemEvent))(systemEvent);
}

/* 
	Used by Monster Hunter Rise.

	Game won't check if mode was changed until NotificationMessage event will be flagged.
	Functions below are detecting which MultiWait includes NotificationMessage event,
	and for that MultiWait passed as argument to nn::os::WaitAny it is redirected to nn::os::TimedWaitAny
	with timeout set to 1ms so we can force game to check NotificationMessage every 1ms.

	Almost all games are checking NotificationMessage in loops instead of waiting for event,
	so even though this is not a clean solution, it works and performance impact is negligible.
*/

SystemEvent* GetNotificationMessageEvent() {
	notificationMessageEventCopy = ((_ZN2nn2oe27GetNotificationMessageEventEv)(Address_weaks.GetNotificationMessageEvent))();
	return notificationMessageEventCopy;
}

void InitializeMultiWaitHolder(void* MultiWaitHolderType, SystemEvent* systemEvent) {
	((nnosInitializeMultiWaitHolderForSystemEvent)(Address_weaks.InitializeMultiWaitHolderForSystemEvent))(MultiWaitHolderType, systemEvent);
	if (systemEvent == notificationMessageEventCopy) 
		multiWaitHolderCopy = MultiWaitHolderType;
}

void LinkMultiWaitHolder(void* MultiWaitType, void* MultiWaitHolderType) {
	((nnosLinkMultiWaitHolder)(Address_weaks.LinkMultiWaitHolder))(MultiWaitType, MultiWaitHolderType);
	if (MultiWaitHolderType == multiWaitHolderCopy)
		multiWaitCopy = MultiWaitType;
}

void* WaitAny(void* MultiWaitType) {
	if (multiWaitCopy != MultiWaitType)
		return ((nnosWaitAny)(Address_weaks.WaitAny))(MultiWaitType);
	return ((nnosTimedWaitAny)(Address_weaks.TimedWaitAny))(MultiWaitType, 1000000);
}

extern "C" {
	void ReverseNX(SharedMemory* _sharedmemory, uint32_t* _sharedOperationMode) {
		sharedOperationMode = _sharedOperationMode;
		SaltySDCore_printf("ReverseNX: alive\n");
		Result ret = SaltySD_CheckIfSharedMemoryAvailable(&SharedMemoryOffset2, 7);
		SaltySDCore_printf("ReverseNX: SharedMemory ret: 0x%X\n", ret);
		if (!ret) {
			SaltySDCore_printf("ReverseNX: SharedMemory MemoryOffset: %d\n", SharedMemoryOffset2);

			ReverseNX_RT = (Shared*)((uintptr_t)shmemGetAddr(_sharedmemory) + SharedMemoryOffset2);
			ReverseNX_RT->MAGIC = 0x5452584E;
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
			Address_weaks.GetPerformanceMode = SaltySDCore_FindSymbolBuiltin("_ZN2nn2oe18GetPerformanceModeEv");
			Address_weaks.GetOperationMode = SaltySDCore_FindSymbolBuiltin("_ZN2nn2oe16GetOperationModeEv");
			Address_weaks.TryPopNotificationMessage = SaltySDCore_FindSymbolBuiltin("_ZN2nn2oe25TryPopNotificationMessageEPj");
			Address_weaks.PopNotificationMessage = SaltySDCore_FindSymbolBuiltin("_ZN2nn2oe22PopNotificationMessageEv");
			Address_weaks.GetDefaultDisplayResolution = SaltySDCore_FindSymbolBuiltin("_ZN2nn2oe27GetDefaultDisplayResolutionEPiS1_");
			Address_weaks.GetDefaultDisplayResolutionChangeEvent = SaltySDCore_FindSymbolBuiltin("_ZN2nn2oe38GetDefaultDisplayResolutionChangeEventEPNS_2os11SystemEventE");
			Address_weaks.TryWaitSystemEvent = SaltySDCore_FindSymbolBuiltin("_ZN2nn2os18TryWaitSystemEventEPNS0_15SystemEventTypeE");
			Address_weaks.WaitSystemEvent = SaltySDCore_FindSymbolBuiltin("_ZN2nn2os15WaitSystemEventEPNS0_15SystemEventTypeE");
			Address_weaks.GetNotificationMessageEvent = SaltySDCore_FindSymbolBuiltin("_ZN2nn2oe27GetNotificationMessageEventEv");
			Address_weaks.InitializeMultiWaitHolderForSystemEvent = SaltySDCore_FindSymbolBuiltin("_ZN2nn2os25InitializeMultiWaitHolderEPNS0_19MultiWaitHolderTypeEPNS0_15SystemEventTypeE");
			Address_weaks.LinkMultiWaitHolder = SaltySDCore_FindSymbolBuiltin("_ZN2nn2os19LinkMultiWaitHolderEPNS0_13MultiWaitTypeEPNS0_19MultiWaitHolderTypeE");
			Address_weaks.WaitAny = SaltySDCore_FindSymbolBuiltin("_ZN2nn2os7WaitAnyEPNS0_13MultiWaitTypeE");
			Address_weaks.TimedWaitAny = SaltySDCore_FindSymbolBuiltin("_ZN2nn2os12TimedWaitAnyEPNS0_13MultiWaitTypeENS_8TimeSpanE");
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