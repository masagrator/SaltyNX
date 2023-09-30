#include <switch_min.h>

#include "saltysd_core.h"
#include "saltysd_ipc.h"
#include "saltysd_dynamic.h"

struct SystemEvent {
	const char reserved[16];
	bool flag;
};

extern "C" {
	typedef u32 (*_ZN2nn2oe18GetPerformanceModeEv)();
	typedef u8 (*_ZN2nn2oe16GetOperationModeEv)();
	typedef bool (*_ZN2nn2oe25TryPopNotificationMessageEPj)(int &Message);
	typedef int (*_ZN2nn2oe22PopNotificationMessageEv)();
	typedef void (*_ZN2nn2oe27GetDefaultDisplayResolutionEPiS1_)(int* width, int* height);
	typedef void (*_ZN2nn2oe38GetDefaultDisplayResolutionChangeEventEPNS_2os11SystemEventE)(SystemEvent* systemEvent);
	typedef bool (*nnosTryWaitSystemEvent)(SystemEvent* systemEvent);
	typedef SystemEvent* (*_ZN2nn2oe27GetNotificationMessageEventEv)();
	typedef void (*nnosInitializeMultiWaitHolderForSystemEvent)(void* MultiWaitHolderType, SystemEvent* systemEvent);
	typedef void (*nnosLinkMultiWaitHolder)(void* MultiWaitType, void* MultiWaitHolderType);
	typedef void* (*nnosWaitAny)(void* MultiWaitType);
	typedef void* (*nnosTimedWaitAny)(void* MultiWaitType, u64 TimeSpan);
}

struct {
	uintptr_t GetPerformanceMode;
	uintptr_t GetOperationMode;
	uintptr_t TryPopNotificationMessage;
	uintptr_t PopNotificationMessage;
	uintptr_t GetDefaultDisplayResolution;
	uintptr_t GetDefaultDisplayResolutionChangeEvent;
	uintptr_t TryWaitSystemEvent;
	uintptr_t GetNotificationMessageEvent;
	uintptr_t InitializeMultiWaitHolderForSystemEvent;
	uintptr_t LinkMultiWaitHolder;
	uintptr_t WaitAny;
	uintptr_t TimedWaitAny;	
} Address_weaks;

bool* def_shared = 0;
bool* isDocked_shared = 0;
bool* pluginActive_shared = 0;
const char* ver = "2.0.0";

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

	*pluginActive_shared = true;

	if (*def_shared) {
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
	if (compare2 != *isDocked_shared) {
		compare2 = *isDocked_shared;
		msg = 0x1f;
		return true;
	}
	if (compare != *isDocked_shared) {
		compare = *isDocked_shared;
		msg = 0x1e;
		return true;
	}
	
	return ((_ZN2nn2oe25TryPopNotificationMessageEPj)(Address_weaks.TryPopNotificationMessage))(msg);
}

int PopNotificationMessage() {
	
	static bool check1 = true;
	static bool check2 = true;
	static bool compare = false;
	static bool compare2 = false;
	
	*pluginActive_shared = true;
	
	if (*def_shared) {
		if (!check1) {
			check1 = true;
			return 0x1e;
		}
		else if (!check2) {
			check2 = true;
			return 0x1f;
		}
		else return ((_ZN2nn2oe22PopNotificationMessageEv)(Address_weaks.PopNotificationMessage))();
	}
	
	check1 = false;
	check2 = false;

	if (compare2 != *isDocked_shared) {
		compare2 = *isDocked_shared;
		return 0x1e;
	}
	else if (compare != *isDocked_shared) {
		compare = *isDocked_shared;
		return 0x1f;
	}
	
	return ((_ZN2nn2oe22PopNotificationMessageEv)(Address_weaks.PopNotificationMessage))();
}

uint32_t GetPerformanceMode() {
	if (*def_shared) *isDocked_shared = ((_ZN2nn2oe18GetPerformanceModeEv)(Address_weaks.GetPerformanceMode))();
	
	return *isDocked_shared;
}

uint8_t GetOperationMode() {
	if (*def_shared) *isDocked_shared = ((_ZN2nn2oe16GetOperationModeEv)(Address_weaks.GetOperationMode))();
	
	return *isDocked_shared;
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
	if (*def_shared) {
		((_ZN2nn2oe27GetDefaultDisplayResolutionEPiS1_)(Address_weaks.GetDefaultDisplayResolution))(width, height);
		if (*width == 1920) *isDocked_shared = true;
		else *isDocked_shared = false;
	}
	else if (*isDocked_shared) {
		*width = 1920;
		*height = 1080;
	}
	else {
		*width = 1280;
		*height = 720;
	}
}

void GetDefaultDisplayResolutionChangeEvent(SystemEvent* systemEvent) {
	((_ZN2nn2oe38GetDefaultDisplayResolutionChangeEventEPNS_2os11SystemEventE)(Address_weaks.GetDefaultDisplayResolutionChangeEvent))(systemEvent);
	defaultDisplayResolutionChangeEventCopy = systemEvent;
}

bool TryWaitSystemEvent(SystemEvent* systemEvent) {
	static bool check = true;
	static bool compare = false;

	if (systemEvent != defaultDisplayResolutionChangeEventCopy || *def_shared) {
		bool ret = ((nnosTryWaitSystemEvent)(Address_weaks.TryWaitSystemEvent))(systemEvent);
		compare = *isDocked_shared;
		if (systemEvent == defaultDisplayResolutionChangeEventCopy && !check) {
			check = true;
			return true;
		}
		return ret;
	}
	check = false;
	if (systemEvent == defaultDisplayResolutionChangeEventCopy) {
		if (compare != *isDocked_shared) {
			compare = *isDocked_shared;
			return true;
		}
		return false;
	}
	return ((nnosTryWaitSystemEvent)(Address_weaks.TryWaitSystemEvent))(systemEvent);
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
	void ReverseNX(SharedMemory* _sharedmemory) {
		SaltySDCore_printf("ReverseNX: alive\n");
		Result ret = SaltySD_CheckIfSharedMemoryAvailable(&SharedMemoryOffset2, 7);
		SaltySDCore_printf("ReverseNX: SharedMemory ret: 0x%X\n", ret);
		if (!ret) {
			SaltySDCore_printf("ReverseNX: SharedMemory MemoryOffset: %d\n", SharedMemoryOffset2);

			uintptr_t base = (uintptr_t)shmemGetAddr(_sharedmemory) + SharedMemoryOffset2;
			uint32_t* MAGIC = (uint32_t*)base;
			*MAGIC = 0x5452584E;
			isDocked_shared = (bool*)(base + 4);
			def_shared = (bool*)(base + 5);
			pluginActive_shared = (bool*)(base + 6);
			*isDocked_shared = false;
			*def_shared = true;
			*pluginActive_shared = false;
			Address_weaks.GetPerformanceMode = SaltySDCore_FindSymbolBuiltin("_ZN2nn2oe18GetPerformanceModeEv");
			Address_weaks.GetOperationMode = SaltySDCore_FindSymbolBuiltin("_ZN2nn2oe16GetOperationModeEv");
			Address_weaks.TryPopNotificationMessage = SaltySDCore_FindSymbolBuiltin("_ZN2nn2oe25TryPopNotificationMessageEPj");
			Address_weaks.PopNotificationMessage = SaltySDCore_FindSymbolBuiltin("_ZN2nn2oe22PopNotificationMessageEv");
			Address_weaks.GetDefaultDisplayResolution = SaltySDCore_FindSymbolBuiltin("_ZN2nn2oe27GetDefaultDisplayResolutionEPiS1_");
			Address_weaks.GetDefaultDisplayResolutionChangeEvent = SaltySDCore_FindSymbolBuiltin("_ZN2nn2oe38GetDefaultDisplayResolutionChangeEventEPNS_2os11SystemEventE");
			Address_weaks.TryWaitSystemEvent = SaltySDCore_FindSymbolBuiltin("_ZN2nn2os18TryWaitSystemEventEPNS0_15SystemEventTypeE");
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
			SaltySDCore_ReplaceImport("_ZN2nn2oe27GetNotificationMessageEventEv", (void*)GetNotificationMessageEvent);
			SaltySDCore_ReplaceImport("_ZN2nn2os25InitializeMultiWaitHolderEPNS0_19MultiWaitHolderTypeEPNS0_15SystemEventTypeE", (void*)InitializeMultiWaitHolder);
			SaltySDCore_ReplaceImport("_ZN2nn2os19LinkMultiWaitHolderEPNS0_13MultiWaitTypeEPNS0_19MultiWaitHolderTypeE", (void*)LinkMultiWaitHolder);
			SaltySDCore_ReplaceImport("_ZN2nn2os7WaitAnyEPNS0_13MultiWaitTypeE", (void*)WaitAny);
		}
		
		SaltySDCore_printf("ReverseNX: injection finished\n");
	}
}