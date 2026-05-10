#include <switch.h>
#include "ipc.h"
#include "display_refresh_rate.h"

#include <stdlib.h>
#include <dirent.h>

#define NANOPRINTF_IMPLEMENTATION
#include "useful.h"
#include "dmntcht.h"
#include <malloc.h>
#include "service.h"
#include "hijack.h"

#define MODULE_SALTYSD 420
#define NVDISP_PANEL_GET_VENDOR_ID 0xC003021A

const size_t shmem_size = 0x1000;

struct NxFpsSharedBlock* nx_fps = 0;

u32 __nx_applet_type = AppletType_None;

struct MinMax {
    u8 min;
    u8 max;
};

Handle saltyport, sdcard, injectserv;
static char g_heap[0x30000];
bool already_hijacking = false;
bool should_terminate = false;
u64 exception = 0x0;
SharedMemory _sharedMemory = {0};
size_t reservedSharedMemory = 0;
uint64_t clkVirtAddr = 0;
uint64_t dsiVirtAddr = 0;
bool displaySync = false;
bool displaySyncOutOfFocus60 = false;
bool displaySyncDocked = false;
bool displaySyncDockedOutOfFocus60 = false;
uint8_t refreshRate = 0;
s64 lastAppPID = -1;
bool isOLED = false;
bool isLite = false;
bool cheatCheck = false;
bool isDocked = false;
bool dontForce60InDocked = false;
bool matchLowestDocked = false;

#ifdef SWITCH
    #define systemtickfrequency 19200000
#elif OUNCE
    #define systemtickfrequency 31250000
#else 
    uint64_t systemtickfrequency = 0;
#endif
static_assert(systemtickfrequency != 0);

//This is done to save some space as they have no practical use in our case
void* __real___cxa_throw(void *thrown_exception, void *pvar, void (*dest)(void *));
void* __real__Unwind_Resume();
void* __real___gxx_personality_v0();

void __wrap___cxa_throw(void *thrown_exception, void *pvar, void (*dest)(void *)) {
    abort();
}

void __wrap__Unwind_Resume() {
    return;
}

void __wrap___gxx_personality_v0() {
    return;
}

void __libnx_initheap(void)
{
    extern char* fake_heap_start;
    extern char* fake_heap_end;

    fake_heap_start = &g_heap[0];
    fake_heap_end   = &g_heap[sizeof g_heap];
}

void __appInit(void)
{
    svcSleepThread(1*1000*1000*1000);
}

void __appExit(void)
{
    already_hijacking = false;
    fsdevUnmountAll();
    smExit();
    setsysExit();
    nvExit();
    pdmqryExit();
}

void ABORT_IF_FAILED(Result rc, uint8_t ID) {
    if (R_FAILED(rc)) {
        uint32_t* address = (uint32_t*)(0x7100000000 + ID);
        *address = rc;
    }
}

//Tweaks to nvInitialize so it will take less RAM
#define NVDRV_TMEM_SIZE (8 * 0x1000)
alignas(0x1000) char nvdrv_tmem_data[NVDRV_TMEM_SIZE];

Result __nx_nv_create_tmem(TransferMemory *t, u32 *out_size, Permission perm) {
    *out_size = NVDRV_TMEM_SIZE;
    return tmemCreateFromMemory(t, nvdrv_tmem_data, NVDRV_TMEM_SIZE, perm);
}

u64 TIDnow;
u64 PIDnow;
u64 BIDnow;

static bool isServiceRunning(const char *serviceName) {	
	Handle handle;	
	if (R_FAILED(smRegisterService(&handle, smEncodeName(serviceName), false, 1))) 
		return true;
	else {
		svcCloseHandle(handle);	
		smUnregisterService(smEncodeName(serviceName));
		return false;
	}
}

static bool isCheatsFolderInstalled() {
    char romfspath[0x40] = "";
    bool flag = false;

    npf_snprintf(romfspath, 0x40, "sdmc:/atmosphere/contents/%016lx/cheats", TIDnow);

    DIR* dir = opendir(romfspath);
    if (dir) {
        if (readdir(dir))
            flag = true;
        closedir(dir);
    }

    return flag;
}

static ptrdiff_t searchNxFpsSharedMemoryBlock(uintptr_t base) {
	ptrdiff_t search_offset = 0;
	while(search_offset < shmem_size) {
		uint32_t* MAGIC_shared = (uint32_t*)(base + search_offset);
		if (*MAGIC_shared == 0x465053) {
			return search_offset;
		}
		else search_offset += 4;
	}
	return -1;
}

__attribute__((noinline)) static Result isApplicationOutOfFocus(bool* outOfFocus) {
    static s32 last_total_entries = 0;
    static bool isOutOfFocus = false;
    s32 total_entries = 0;
    s32 start_entry_index = 0;
    s32 end_entry_index = 0;
    Result rc = pdmqryGetAvailablePlayEventRange(&total_entries, &start_entry_index, &end_entry_index);
    if (R_FAILED(rc)) return rc;
    if (total_entries == last_total_entries) {
        *outOfFocus = isOutOfFocus;
        return 0;
    }
    last_total_entries = total_entries;

    #define EVENT_COUNT 16
    PdmPlayEvent events[EVENT_COUNT];
    s32 out = 0;
    s32 start_entry = end_entry_index - (EVENT_COUNT-1);
    if (start_entry < 0) start_entry = 0;
    rc = pdmqryQueryPlayEvent(start_entry, events, EVENT_COUNT, &out);
    if (R_FAILED(rc)) return rc;
    if (out == 0) return 1;

    int itr = -1;
    for (int i = out-1; i >= 0; i--) {
        PdmPlayEvent* event = &events[i];
        if (event->play_event_type != PdmPlayEventType_Applet)
            continue;
        if (event->event_data.applet.applet_id != AppletId_application)
            continue;
        union {
            uint32_t parts[2];
            uint64_t full;
        } TID;
        TID.parts[0] = event->event_data.applet.program_id[1];
        TID.parts[1] = event->event_data.applet.program_id[0];

        if (TID.full != (TIDnow & ~0xFFF))
            continue;
        else {
            itr = i;
            break;
        }
    }
    if (itr == -1) return 1;

    bool isOut = events[itr].event_data.applet.event_type == PdmAppletEventType_OutOfFocus || events[itr].event_data.applet.event_type == PdmAppletEventType_OutOfFocus4;
    *outOfFocus = isOut;
    isOutOfFocus = isOut;
    return 0;
}

void updateNxFpsPointer() {
    uintptr_t sharedAddress = (uintptr_t)shmemGetAddr(&_sharedMemory);
    if (sharedAddress) {
        ptrdiff_t offset = searchNxFpsSharedMemoryBlock(sharedAddress);
        if (offset != -1) {
            nx_fps = (struct NxFpsSharedBlock*)(sharedAddress + offset);
        }
    }
}

void checkIfDmntChtNeedsToBeActive() {
    static bool dmntchtActive = false;
    if (!dmntchtActive) dmntchtActive = isServiceRunning("dmnt:cht");
    if (!dmntchtActive || !isCheatsFolderInstalled())
        cheatCheck = true;
    else {
        Handle debug_handle;
        if (R_SUCCEEDED(svcDebugActiveProcess(&debug_handle, lastAppPID))) {
            s32 thread_count;
            u64 threads[2];
            svcGetThreadList(&thread_count, threads, 2, debug_handle);
            svcCloseHandle(debug_handle);
            if (thread_count > 1) {
                cheatCheck = true;
                if (R_SUCCEEDED(dmntchtInitialize())) {
                    dmntchtForceOpenCheatProcess();
                    dmntchtExit();
                }
            }
        }
        else cheatCheck = true;
    }
}

void cleanAppData() {
    lastAppPID = -1;
    nx_fps = 0;
    cheatCheck = false;
    uintptr_t shmem = (uintptr_t)shmemGetAddr(&_sharedMemory);
    if (shmem) {
        memset((void*)(shmem+4), 0, shmem_size-4);
    }
    if ((!isDocked && displaySync) || (isDocked && displaySyncDocked)) {
        uint32_t temp_refreshRate = 0;
        if (GetDisplayRefreshRate(&temp_refreshRate, true) && temp_refreshRate != 60)
            SetDisplayRefreshRate(60);
        refreshRate = 0;
    }
    if (openedFilesAmount) {
        for (size_t i = 0; i < openedFilesAmount; i++) {
            fclose(openedFilesArray[i].file);
        }
        memset(openedFilesArray, 0, sizeof(openedFilesArray));
        openedFilesAmount = 0;
    }
    if (openedDirsAmount) {
        for (size_t i = 0; i < openedDirsAmount; i++) {
            closedir(openedDirsArray[i].dir);
        }
        memset(openedDirsArray, 0, sizeof(openedDirsArray));
        openedDirsAmount = 0;
    }
}

void updateRefreshRateForApp() {
    if ((!isDocked && displaySync) || (isDocked && displaySyncDocked)) {
        uint32_t temp_refreshRate = 0;
        GetDisplayRefreshRate(&temp_refreshRate, true);
        uint32_t check_refresh_rate = refreshRate;
        if (nx_fps && (isDocked ? nx_fps->FPSlockedDocked : nx_fps->FPSlocked)) check_refresh_rate = (isDocked ? nx_fps->FPSlockedDocked : nx_fps->FPSlocked);
        if (check_refresh_rate == 0)
            check_refresh_rate = 60;
        if (check_refresh_rate != 60 && nx_fps && nx_fps->forceOriginalRefreshRate && (!isDocked || (isDocked && !dontForce60InDocked))) {
            check_refresh_rate = 60;
        }
        if (check_refresh_rate != 60 && ((isDocked && displaySyncDockedOutOfFocus60) || (!isDocked && displaySyncOutOfFocus60))) {
            bool isOutOfFocus = true;
            if (R_SUCCEEDED(isApplicationOutOfFocus(&isOutOfFocus)) && isOutOfFocus) {
                check_refresh_rate = 60;
            }
        }
        if (temp_refreshRate != check_refresh_rate)
            SetDisplayRefreshRate(check_refresh_rate);
    }
    if (!isDocked && nx_fps && nx_fps->FPSlocked > HandheldModeRefreshRateAllowed.max) {
        nx_fps->FPSlocked = HandheldModeRefreshRateAllowed.max;
        refreshRate = HandheldModeRefreshRateAllowed.max;
    }
    else if (isDocked && nx_fps) {
        uint8_t highestrr = getDockedHighestRefreshRateAllowed();
        if (nx_fps->FPSlockedDocked > highestrr) {
            nx_fps->FPSlockedDocked = highestrr;
            refreshRate = highestrr;
        }
    }
}

void updateRefreshRate() {
    static bool wasLastDocked = false;
    if ((wasLastDocked && !isDocked && !displaySync) || (!wasLastDocked && isDocked && !displaySyncDocked)) {
        uint32_t temp_refreshRate = 0;
        if (GetDisplayRefreshRate(&temp_refreshRate, true) && temp_refreshRate != 60) {
            SetDisplayRefreshRate(60);
            refreshRate = 0;
        }
    }
    wasLastDocked = isDocked;

    if (isDocked && !displaySyncDocked && nx_fps && nx_fps->FPSlockedDocked > 60) {
        uint32_t temp_refreshRate = 0;
        GetDisplayRefreshRate(&temp_refreshRate, true);
        if (temp_refreshRate <= 60) {
            nx_fps->FPSlockedDocked = 60;
        }
    }
    
    uint32_t crr = 0;
    GetDisplayRefreshRate(&crr, true);
    if (isOLED) correctOledGamma(crr);

    if (nx_fps) nx_fps -> dontForce60InDocked = dontForce60InDocked;
}

void Initialize() {
    #if !defined(SWITCH) && !defined(OUNCE)
	    systemtickfrequency = armGetSystemTickFreq();
    #endif
    ABORT_IF_FAILED(smInitialize(), 1);
    ABORT_IF_FAILED(fsInitialize(), 2);
    ABORT_IF_FAILED(fsdevMountSdmc(), 3);
    SaltySD_printf("SaltySD " APP_VERSION ": got SD card.\n");

    
    ABORT_IF_FAILED(setsysInitialize(), 10);

    SetSysFirmwareVersion fw;
    Result rc = setsysGetFirmwareVersion(&fw);
    if (R_SUCCEEDED(rc)) {
        hosversionSet(MAKEHOSVERSION(fw.major, fw.minor, fw.micro));
    }
    else {
        SaltySD_printf("SaltySD: Couldn't retrieve Firmware Version! rc: 0x%x.\n", rc);
    }

    SetSysProductModel model;
    if (R_SUCCEEDED(setsysGetProductModel(&model))) {
        if (model == SetSysProductModel_Aula) {
            SaltySD_printf("SaltySD: Detected OLED model. Locking minimum refresh rate to 45 Hz.\n");
            isOLED = true;
            HandheldModeRefreshRateAllowed.min = 45;
        }
        else if (model == SetSysProductModel_Hoag) {
            isLite = true;
            SaltySD_printf("SaltySD: Detected Lite model. Docked refresh rate will be blocked.\n");
        }
    }
    
    ABORT_IF_FAILED(nvInitialize(), 6);
    u32 fd = 0;
    if (R_FAILED(nvOpen(&fd, "/dev/nvdisp-disp0"))) {
        SaltySD_printf("SaltySD: Couldn't open /dev/nvdisp-disp0! Can't check if using Retro Remake display.\n");
    }
    else {
        struct vendorID {
            u8 vendor;
            u8 model;
            u8 board;
        };
        struct vendorID _vendorid = {0};
        Result nvrc = nvIoctl(fd, NVDISP_PANEL_GET_VENDOR_ID, &_vendorid);
        nvClose(fd);
        if (R_SUCCEEDED(nvrc)) {
            if (_vendorid.vendor == 0xE0 || _vendorid.vendor == 0xE1) {
                isRetroSUPER = true;
            }
            if (_vendorid.vendor == 0x20 && _vendorid.model == 0x94 && _vendorid.board == 0x10) {
                isPossiblySpoofedRetro = true;
            }
        }
    }
    if (!isLite) {
        if (file_or_directory_exists("sdmc:/SaltySD/plugins/FPSLocker/ExtDisplays") == false) {
            mkdir("sdmc:/SaltySD/plugins", 69);
            mkdir("sdmc:/SaltySD/plugins/FPSLocker", 420);
            mkdir("sdmc:/SaltySD/plugins/FPSLocker/ExtDisplays", 2137);
        }
    }
    ABORT_IF_FAILED(ldrDmntInitialize(), 7);
    Service* ldrDmntSrv = ldrDmntGetServiceSession();
    Service ldrDmntClone;
    serviceClone(ldrDmntSrv, &ldrDmntClone);
    serviceClose(ldrDmntSrv);
    memcpy(ldrDmntSrv, &ldrDmntClone, sizeof(Service));

    ABORT_IF_FAILED(pdmqryInitialize(), 8);
    Service* pdmqrySrv = pdmqryGetServiceSession();
    Service pdmqryClone;
    serviceClone(pdmqrySrv, &pdmqryClone);
    serviceClose(pdmqrySrv);
    memcpy(pdmqrySrv, &pdmqryClone, sizeof(Service));

    if (file_or_directory_exists("sdmc:/SaltySD/flags/displaysync.flag")) {
        displaySync = true;
    }
    if (file_or_directory_exists("sdmc:/SaltySD/flags/displaysyncdocked.flag")) {
        displaySyncDocked = true;
    }
    if (file_or_directory_exists("sdmc:/SaltySD/flags/displaysync_outoffocus.flag")) {
        displaySyncOutOfFocus60 = true;
    }

    // Start our port
    // For some reason, we only have one session maximum (0 reslimit handle related?)	
    svcManageNamedPort(&saltyport, "SaltySD", 1);
    svcManageNamedPort(&injectserv, "InjectServ", 1);

    uint64_t dummy = 0;
    rc = svcQueryMemoryMapping(&clkVirtAddr, &dummy, 0x60006000, 0x1000);
    if (R_FAILED(rc)) {
        SaltySD_printf("SaltySD: Retrieving virtual address for 0x60006000 failed. RC: 0x%x.\n", rc);
        clkVirtAddr = 0;
    }
    if (isOLED) {
        Result rc = svcQueryMemoryMapping(&dsiVirtAddr, &dummy, 0x54300000, 0x40000);
        if (R_FAILED(rc)) {
            SaltySD_printf("SaltySD: Retrieving virtual address for 0x54300000 failed. RC: 0x%x.\n", rc);
            dsiVirtAddr = 0;
        }
    }
    shmemCreate(&_sharedMemory, shmem_size, Perm_Rw, Perm_Rw);
    shmemMap(&_sharedMemory);
    memset(shmemGetAddr(&_sharedMemory), 0, shmem_size);
}

int main(int argc, char *argv[])
{   

    Initialize();
    
    // Main service loop
    u64* pids = malloc(0x200 * sizeof(u64));
    u64 max = 0;
    while (1)
    {
        s32 num;
        static s32 init_num = 0;
        svcGetProcessList(&num, pids, 0x200);
        if (!init_num) init_num = num;
        u64 old_max = max;
        for (int i = init_num; i < num; i++)
        {
            if (pids[i] > max)
            {
                max = pids[i];
            }
        }
        
        if (lastAppPID != -1) {
            if (!nx_fps) updateNxFpsPointer();
            if (!cheatCheck) checkIfDmntChtNeedsToBeActive();
            bool found = false;
            for (int i = num - 1; lastAppPID <= pids[i]; i--)
            {
                if (pids[i] == lastAppPID)
                {	
                    found = true;
                    break;
                }
            }
            if (!found) cleanAppData();
            else updateRefreshRateForApp();
        }

        updateRefreshRate();

        // Detected new PID
        if (max != old_max && max > 0x80)
        {
            PIDnow = max;
            hijack_pid(max);
        }
        
        // If someone is waiting for us, handle them.
        if (R_SUCCEEDED(svcWaitSynchronizationSingle(saltyport, 9000000)))
        {
            serviceThread();
        }
        if (R_SUCCEEDED(svcWaitSynchronizationSingle(injectserv, 1000000))) {
            Handle sesja;
            svcAcceptSession(&sesja, injectserv);
            svcCloseHandle(sesja);
        }
    }
    free(pids);

    return 0;
}
