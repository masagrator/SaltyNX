#include <switch.h>
#include "ipc.h"
#include "legacy_libnx.h"
#include "fs_dev.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <dirent.h>
#include "svc_extra.h"

#include "spawner_ipc.h"

#include "loadelf.h"
#include "useful.h"
#include "dmntcht.h"
#include <math.h>

#define MODULE_SALTYSD 420
#define	NVDISP_GET_MODE 0x80380204
#define	NVDISP_SET_MODE 0x40380205
#define NVDISP_VALIDATE_MODE 0xC038020A
#define DSI_CLOCK_HZ 234000000llu

#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))

struct resolutionCalls {
	uint16_t width;
	uint16_t height;
	uint16_t calls;
};

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
	struct resolutionCalls renderCalls[8];
	struct resolutionCalls viewportCalls[8];
	bool forceOriginalRefreshRate;
} NX_PACKED;

struct NxFpsSharedBlock* nx_fps = 0;

u32 __nx_applet_type = AppletType_None;

void serviceThread(void* buf);

Handle saltyport, sdcard, injectserv;
static char g_heap[0x80000];
bool should_terminate = false;
bool already_hijacking = false;
DebugEventInfo eventinfo;
bool check = false;
u64 exception = 0x0;
SharedMemory _sharedMemory = {0};
size_t reservedSharedMemory = 0;
uint64_t clkVirtAddr = 0;
bool displaySync = false;
uint8_t refreshRate = 0;
s64 lastAppPID = -1;
bool isOLED = false;
bool isLite = false;
bool cheatCheck = false;
bool isDocked = false;
bool dontForce60InDocked = false;
bool matchLowestDocked = false;

/// Edid2
typedef struct {
	SetSysEdid edid;
	char reserved[0x100];
} SetSysEdid2;

Result setsysGetEdid2(Service* g_setsysSrv, SetSysEdid2 *out) {
    return serviceDispatch(g_setsysSrv, 41,
        .buffer_attrs = { SfBufferAttr_FixedSize | SfBufferAttr_HipcPointer | SfBufferAttr_Out },
        .buffers = { { out, sizeof(*out) } },
    );
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
    fsdevUnmountAll_old();
    smExit();
    setsysExit();
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

bool DockedModeRefreshRateAllowed[]         = { false,  //40Hz
                                                false,  //45Hz
                                                true,   //50Hz
                                                false,  //55Hz
                                                true};  //60Hz

uint8_t DockedModeRefreshRateAllowedValues[] = { 40,
                                                 45,
                                                 50,
                                                 55,
                                                 60};

static_assert(sizeof(DockedModeRefreshRateAllowedValues) == sizeof(DockedModeRefreshRateAllowed));

struct MinMax {
    u8 min;
    u8 max;
};

struct MinMax HandheldModeRefreshRateAllowed = {40, 60};

struct PLLD_BASE {
    unsigned int PLLD_DIVM: 8;
    unsigned int reserved_1: 3;
    unsigned int PLLD_DIVN: 8;
    unsigned int reserved_2: 1;
    unsigned int PLLD_DIVP: 3;
    unsigned int CSI_CLK_SRC: 1;
    unsigned int reserved_3: 1;
    unsigned int PLL_D: 1;
    unsigned int reserved_4: 1;
    unsigned int PLLD_LOCK: 1; //Read Only
    unsigned int reserved_5: 1;
    unsigned int PLLD_REF_DIS: 1;
    unsigned int PLLD_ENABLE: 1;
    unsigned int PLLD_BYPASS: 1;
};

struct PLLD_MISC {
    signed int PLLD_SDM_DIN: 16;
    unsigned int PLLD_EN_SDM: 1;
    unsigned int PLLD_LOCK_OVERRIDE: 1;
    unsigned int PLLD_EN_LCKDET: 1;
    unsigned int PLLD_FREQLOCK: 1; //Read Only
    unsigned int PLLD_IDDQ: 1; //X
    unsigned int PLLD_ENABLE_CLK: 1;
    unsigned int PLLD_KVCO: 1;
    unsigned int PLLD_KCP: 2;
    unsigned int PLLD_PTS: 2;
    unsigned int PLLD_LDPULSE_ADJ: 3;
    unsigned int reserved: 2;
};

struct nvdcMode {
    unsigned int hActive;
    unsigned int vActive;
    unsigned int hSyncWidth;
    unsigned int vSyncWidth;
    unsigned int hFrontPorch;
    unsigned int vFrontPorch;
    unsigned int hBackPorch;
    unsigned int vBackPorch;
    unsigned int hRefToSync; //always 1
    unsigned int vRefToSync; //always 1
    unsigned int pclkKHz;
    unsigned int bitsPerPixel; //always 0
    unsigned int vmode; //always 0
};

void setDefaultDockedSettings() {
    for (size_t i = 0; i < sizeof(DockedModeRefreshRateAllowed); i++) {
        if (DockedModeRefreshRateAllowedValues[i] == 50 || DockedModeRefreshRateAllowedValues[i] == 60) DockedModeRefreshRateAllowed[i] = true;
        else DockedModeRefreshRateAllowed[i] = false;
    }
    dontForce60InDocked = false;
    matchLowestDocked = false;
}

void LoadDockedModeAllowedSave() {
    SetSysEdid2 edid2 = {0};
    setDefaultDockedSettings();
    if (R_FAILED(setsysGetEdid2(setsysGetServiceSession(), &edid2))) {
        SaltySD_printf("SaltySD: Couldn't retrieve display EDID! Locking allowed refresh rates in docked mode to 50 and 60 Hz.\n");
        return;
    }
    char path[128] = "";
    snprintf(path, sizeof(path), "sdmc:/SaltySD/plugins/FPSLocker/ExtDisplays/%08X.dat", crc32Calculate(&edid2.edid, sizeof(edid2.edid)));
    FILE* file = fopen(path, "rb");
    if (file) {
        u64 MAGIC = 0x00FFFFFFFFFFFF00;
        u64 checkMAGIC = 0;
        fread(&checkMAGIC, 8, 1, file);
        if (checkMAGIC != MAGIC) {
            fclose(file);
            SaltySD_printf("SaltySD: File \"%s\" is invalid! Locking allowed refresh rates in docked mode to 50 and 60 Hz.\n", path);
            return;
        }

        SaltySD_printf("SaltySD: File \"%s\" was loaded! Allowed refresh rates: 60", path);
        fseek(file, 0x100, 0);
        for (size_t i = 0; i < sizeof(DockedModeRefreshRateAllowed); i++) {
            bool temp = false;
            if (!fread(&temp, 1, 1, file))
                break;
            if (DockedModeRefreshRateAllowedValues[i] == 60)
                continue;
            DockedModeRefreshRateAllowed[i] = temp;
            if (temp == true) {
                SaltySD_printf(", %d", DockedModeRefreshRateAllowedValues[i]);
            }
        }
        SaltySD_printf(".\n");
        fseek(file, 0x180, 0);
        uint8_t temp = 2;
        fread(&temp, 1, 1, file);
        if (temp > 1) SaltySD_printf("SaltySD: File \"%s\" has invalid dontForce60InDocked value. Setting it to false.\n", path);
        else dontForce60InDocked = (bool)temp;
        temp = 2;
        fread(&temp, 1, 1, file);
        if (temp > 1) SaltySD_printf("SaltySD: File \"%s\" has invalid matchLowestDocked value. Setting it to false.\n", path);
        else matchLowestDocked = (bool)temp;
        fclose(file);
    }
    else {
        SaltySD_printf("SaltySD: File \"%s\" not found! Locking allowed refresh rates in docked mode to 50 and 60 Hz.\n", path);
    }
}

bool canChangeRefreshRateDocked = false;

bool SetDisplayRefreshRate(uint32_t new_refreshRate) {
    if (!clkVirtAddr || !new_refreshRate)
        return false;
    struct PLLD_BASE base = {0};
    struct PLLD_MISC misc = {0};
    memcpy(&base, (void*)(clkVirtAddr + 0xD0), 4);
    memcpy(&misc, (void*)(clkVirtAddr + 0xDC), 4);
    uint32_t value = ((base.PLLD_DIVN / base.PLLD_DIVM) * 10) / 4;
    if (value == 0 || value == 80) { //We are in docked mode
        if (isLite || !canChangeRefreshRateDocked)
            return false;
        uint32_t fd = 0;
        if (R_FAILED(nvOpen(&fd, "/dev/nvdisp-disp1"))) {
            return false;
        }
        struct nvdcMode DISPLAY_B = {0};
        Result nvrc = nvIoctl(fd, NVDISP_GET_MODE, &DISPLAY_B);
        if (R_FAILED(nvrc)) {
            SaltySD_printf("SaltySD: NVDISP_GET_MODE failed! rc: 0x%x\n", nvrc);
            nvClose(fd);
            return false;
        }
        if (!DISPLAY_B.pclkKHz) {
            nvClose(fd);
            return false;
        }
        uint32_t h_total = DISPLAY_B.hActive + DISPLAY_B.hFrontPorch + DISPLAY_B.hSyncWidth + DISPLAY_B.hBackPorch;
        uint32_t v_total = DISPLAY_B.vActive + DISPLAY_B.vFrontPorch + DISPLAY_B.vSyncWidth + DISPLAY_B.vBackPorch;
        uint32_t refreshRateNow = ((DISPLAY_B.pclkKHz) * 1000 + 999) / (h_total * v_total);
        int8_t itr = -1;
        if ((60 == new_refreshRate) || (60 == (new_refreshRate * 2)) || (60 == (new_refreshRate * 3)) || (60 == (new_refreshRate * 4))) {
            itr = 4;
        }
        if (itr == -1) for (size_t i = 0; i < sizeof(DockedModeRefreshRateAllowed); i++) {
            if (DockedModeRefreshRateAllowed[i] != true)
                continue;
            uint8_t val = DockedModeRefreshRateAllowedValues[i];
            if ((val == new_refreshRate) || (val == (new_refreshRate * 2)) || (val == (new_refreshRate * 3)) || (val == (new_refreshRate * 4))) {
                itr = i;
                break;
            }
        }
        if (itr == -1) {
            if (!matchLowestDocked)
                itr = 4;
            else for (size_t i = 0; i < sizeof(DockedModeRefreshRateAllowed); i++) {
                if ((DockedModeRefreshRateAllowed[i] == true) && (new_refreshRate < DockedModeRefreshRateAllowedValues[i])) {
                    itr = i;
                    break;
                }
            }
        }
        bool increase = refreshRateNow < DockedModeRefreshRateAllowedValues[itr];
        while(itr >= 0 && itr < sizeof(DockedModeRefreshRateAllowed) && DockedModeRefreshRateAllowed[itr] != true) {
            if (!displaySync) {
                if (increase) itr++;
                else itr--;
            }
            else itr++;
        }
        if (refreshRateNow == DockedModeRefreshRateAllowedValues[itr]) {
            if (nx_fps) nx_fps->displaySync = DockedModeRefreshRateAllowedValues[itr];
            nvClose(fd);
            return true;
        }
        
        if (itr >= 0 && itr < sizeof(DockedModeRefreshRateAllowed)) {
            uint32_t clock = ((h_total * v_total) * DockedModeRefreshRateAllowedValues[itr]) / 1000;
            DISPLAY_B.pclkKHz = clock;
            nvrc = nvIoctl(fd, NVDISP_VALIDATE_MODE, &DISPLAY_B);
            if (R_SUCCEEDED(nvrc)) {
                nvrc = nvIoctl(fd, NVDISP_SET_MODE, &DISPLAY_B);
                if (R_FAILED(nvrc)) SaltySD_printf("SaltySD: NVDISP_SET_MODE failed! rc: 0x%x\n", nvrc);
                else if (nx_fps) nx_fps->displaySync = DockedModeRefreshRateAllowedValues[itr];
            }
            else SaltySD_printf("SaltySD: NVDISP_VALIDATE_MODE failed! rc: 0x%x, pclkKHz: %d, Hz: %d\n", nvrc, clock, DockedModeRefreshRateAllowedValues[itr]);
        }
        nvClose(fd);
        return true;

    }
    //We are in handheld mode
    
    if (new_refreshRate > HandheldModeRefreshRateAllowed.max) {
        new_refreshRate = HandheldModeRefreshRateAllowed.max;
    }
    else if (new_refreshRate < HandheldModeRefreshRateAllowed.min) {
        bool skip = false;
        for (size_t i = 2; i <= 4; i++) {
            if (new_refreshRate * i == 60) {
                skip = true;
                new_refreshRate = 60;
                break;
            }
        }
        if (!skip) for (size_t i = 2; i <= 4; i++) {
            if (((new_refreshRate * i) >= HandheldModeRefreshRateAllowed.min) && ((new_refreshRate * i) <= HandheldModeRefreshRateAllowed.max)) {
                skip = true;
                new_refreshRate *= i;
                break;
            }
        }
        if (!skip) new_refreshRate = 60;
    }
    uint32_t pixelClock = (9375 * ((4096 * ((2 * base.PLLD_DIVN) + 1)) + misc.PLLD_SDM_DIN)) / (8 * base.PLLD_DIVM);
    uint16_t refreshRateNow = pixelClock / (DSI_CLOCK_HZ / 60);

    if (refreshRateNow == new_refreshRate) {
        if (nx_fps) nx_fps->displaySync = new_refreshRate;
        return true;
    }

    uint8_t base_refreshRate = new_refreshRate - (new_refreshRate % 5);

    base.PLLD_DIVN = (4 * base_refreshRate) / 10;
    base.PLLD_DIVM = 1;

    uint64_t expected_pixel_clock = (DSI_CLOCK_HZ * new_refreshRate) / 60;

    misc.PLLD_SDM_DIN = ((8 * base.PLLD_DIVM * expected_pixel_clock) / 9375) - (4096 * ((2 * base.PLLD_DIVN)+1));

    memcpy((void*)(clkVirtAddr + 0xD0), &base, 4);
    memcpy((void*)(clkVirtAddr + 0xDC), &misc, 4);
    if (nx_fps) nx_fps->displaySync = new_refreshRate;
    return true;
}

bool GetDisplayRefreshRate(uint32_t* out_refreshRate, bool internal) {
    if (!clkVirtAddr)
        return false;
    uintptr_t sh_addr = (uintptr_t)shmemGetAddr(&_sharedMemory);
    if (!internal) {
        // We are using this trick because using nvOpen severes connection 
        // with whatever is actually connected to this sysmodule
        if (sh_addr) {
            *out_refreshRate = *(uint8_t*)(sh_addr + 1);
            return true;
        }
        else return false;
    }
    struct PLLD_BASE temp = {0};
    struct PLLD_MISC misc = {0};
    memcpy(&temp, (void*)(clkVirtAddr + 0xD0), 4);
    memcpy(&misc, (void*)(clkVirtAddr + 0xDC), 4);
    uint32_t value = ((temp.PLLD_DIVN / temp.PLLD_DIVM) * 10) / 4;
    static uint64_t tick = 0;
    if (value == 0 || value == 80) { //We are in docked mode
        if (isLite)
            return false;
        isDocked = true;
        //We must add delay for changing refresh rate when it was just put into dock to avoid doing calculation on default values instead of adjusted ones
        //From my tests 1 second is enough
        if (!canChangeRefreshRateDocked) {
            if (!tick) {
                tick = svcGetSystemTick();
                return false;
            }
            if (svcGetSystemTick() - tick < 19200000) {
                return false;
            }
            else {
                tick = 0;
                LoadDockedModeAllowedSave();
                canChangeRefreshRateDocked = true;
            }
        }
        uint32_t fd = 0;
        if (R_SUCCEEDED(nvOpen(&fd, "/dev/nvdisp-disp1"))) {
            struct nvdcMode DISPLAY_B = {0};
            if (R_SUCCEEDED(nvIoctl(fd, NVDISP_GET_MODE, &DISPLAY_B))) {
                if (!DISPLAY_B.pclkKHz) {
                    nvClose(fd);
                    return false;
                }
                uint32_t h_total = DISPLAY_B.hActive + DISPLAY_B.hFrontPorch + DISPLAY_B.hSyncWidth + DISPLAY_B.hBackPorch;
                uint32_t v_total = DISPLAY_B.vActive + DISPLAY_B.vFrontPorch + DISPLAY_B.vSyncWidth + DISPLAY_B.vBackPorch;
                uint32_t pixelClock = DISPLAY_B.pclkKHz * 1000 + 999;
                value = pixelClock / (h_total * v_total);
            }
            else value = 60;
            nvClose(fd);
        }
        else value = 60;
    }
    else {
        tick = 0;
        isDocked = false;
        canChangeRefreshRateDocked = false;
        //We are in handheld mode
        /*
            Official formula:
            Fvco = Fref / DIVM * (DIVN + 0.5 + (SDM_DIN / 8192))
            Fref = CNTFRQ_EL0 / 2
            Defaults: DIVM = 1, DIVN = 24, SDM_DIN = -1024

            My math formula allows avoiding decimals whenever possible
        */
        uint32_t pixelClock = (9375 * ((4096 * ((2 * temp.PLLD_DIVN) + 1)) + misc.PLLD_SDM_DIN)) / (8 * temp.PLLD_DIVM);
        value = pixelClock / (DSI_CLOCK_HZ / 60);
    }
    *out_refreshRate = value;
    if (sh_addr) 
        *(uint8_t*)(sh_addr + 1) = (uint8_t)value;
    return true;
}

bool isServiceRunning(const char *serviceName) {	
	Handle handle;	
	if (R_FAILED(smRegisterService(&handle, smEncodeName(serviceName), false, 1))) 
		return true;
	else {
		svcCloseHandle(handle);	
		smUnregisterService(smEncodeName(serviceName));
		return false;
	}
}

bool isCheatsFolderInstalled() {
    char romfspath[0x40] = "";
    bool flag = false;

    snprintf(romfspath, 0x40, "sdmc:/atmosphere/contents/%016lx/cheats", TIDnow);

    DIR* dir = opendir(romfspath);
    if (dir) {
        if (readdir(dir))
            flag = true;
        closedir(dir);
    }

    return flag;
}

void renameCheatsFolder() {
    char cheatspath[0x3C] = "";
    char cheatspathtemp[0x40] = "";

    snprintf(cheatspath, 0x3C, "sdmc:/atmosphere/contents/%016lx/cheats", TIDnow);
    snprintf(cheatspathtemp, 0x40, "%stemp", cheatspath);
    if (!check) {
        rename(cheatspath, cheatspathtemp);
        check = true;
    }
    else {
        rename(cheatspathtemp, cheatspath);
        check = false;
    }
    return;
}

bool isModInstalled() {
    char romfspath[0x40] = "";
    bool flag = false;

    snprintf(romfspath, 0x40, "sdmc:/atmosphere/contents/%016lx/romfs", TIDnow);

    DIR* dir = opendir(romfspath);
    if (dir) {
        if (readdir(dir))
            flag = true;
        closedir(dir);
    }

    return flag;
}

ptrdiff_t searchNxFpsSharedMemoryBlock(uintptr_t base) {
	ptrdiff_t search_offset = 0;
	while(search_offset < 0x1000) {
		uint32_t* MAGIC_shared = (uint32_t*)(base + search_offset);
		if (*MAGIC_shared == 0x465053) {
			return search_offset;
		}
		else search_offset += 4;
	}
	return -1;
}

bool hijack_bootstrap(Handle* debug, u64 pid, u64 tid, bool isA64)
{
    ThreadContext context;
    Result ret;

    reservedSharedMemory = 0;
    
    ret = svcGetDebugThreadContext(&context, *debug, tid, RegisterGroup_All);
    if (ret)
    {
        SaltySD_printf("SaltySD: svcGetDebugThreadContext returned %x, aborting...\n", ret);
        
        svcCloseHandle(*debug);
        return false;
    }
    
    // Load in the ELF
    //svcReadDebugProcessMemory(backup, debug, context.pc.x, 0x1000);
    uint64_t new_start;
    if (isA64) {
        FILE* file = 0;
        file = fopen("sdmc:/SaltySD/saltysd_bootstrap.elf", "rb");
        if (!file) {
            SaltySD_printf("SaltySD: SaltySD/saltysd_bootstrap.elf not found, aborting...\n", ret);
            svcCloseHandle(*debug);
            return false;
        }
        fseek(file, 0, 2);
        size_t saltysd_bootstrap_elf_size = ftell(file);
        fseek(file, 0, 0);
        u8* elf = malloc(saltysd_bootstrap_elf_size);
        fread(elf, saltysd_bootstrap_elf_size, 1, file);
        fclose(file);
        load_elf_debug(*debug, &new_start, elf, saltysd_bootstrap_elf_size);
        free(elf);
    }
    else load_elf32_debug(*debug, &new_start);

    // Set new PC
    context.pc.x = new_start;
    ret = svcSetDebugThreadContext(*debug, tid, &context, RegisterGroup_All);
    if (ret)
    {
        SaltySD_printf("SaltySD: svcSetDebugThreadContext returned %x!\n", ret);
    }
     
    svcCloseHandle(*debug);
    if (ret) return false;
    else return true;
}

void hijack_pid(u64 pid)
{
    Result ret = -1;
    s32 threads = 0;
    Handle debug;
    
    FILE* disabled = fopen("sdmc:/SaltySD/flags/disable.flag", "r");
    
    if (disabled) {
        fclose(disabled);
        SaltySD_printf("SaltySD: Detected disable.flag, aborting bootstrap...\n");
        return;
    }
    
    if (already_hijacking)
    {
        SaltySD_printf("SaltySD: PID %llx spawned before last hijack finished bootstrapping! Ignoring...\n", pid);
        return;
    }
    
    already_hijacking = true;
    svcDebugActiveProcess(&debug, pid);

    bool isA64 = true;

    while (1)
    {
        ret = svcGetDebugEventInfo(&eventinfo, debug);

        switch(ret) {
            case 0:
                break;
            case 0xE401:
                SaltySD_printf("SaltySD: PID %d is not allowing debugging, aborting...\n", pid);
                goto abort_bootstrap;
            case 0x8C01:
                SaltySD_printf("SaltySD: PID %d svcGetDebugEventInfo: end of events...\n", pid);
                break;
            default:
                SaltySD_printf("SaltySD: PID %d svcGetDebugEventInfo returned %x, breaking...\n", pid, ret);
                break;
        }
        if (ret)
            break;

        if (!check) {
            TIDnow = eventinfo.tid;
            exception = 0;
            renameCheatsFolder();
        }

        if (eventinfo.type == DebugEvent_AttachProcess)
        {

            if (eventinfo.tid <= 0x010000000000FFFF)
            {
                SaltySD_printf("SaltySD: %s TID %016lx is a system application, aborting bootstrap...\n", eventinfo.name, eventinfo.tid);
                goto abort_bootstrap;
            }
            if (eventinfo.tid > 0x01FFFFFFFFFFFFFF || (eventinfo.tid & 0x1F00) != 0)
            {
                SaltySD_printf("SaltySD: %s TID %016lx is a homebrew application, aborting bootstrap...\n", eventinfo.name, eventinfo.tid);
                goto abort_bootstrap;
            }
            if (shmemGetAddr(&_sharedMemory)) {
                memset(shmemGetAddr(&_sharedMemory), 0, 0x1000);
            }
            char* hbloader = "hbloader";
            if (strcasecmp(eventinfo.name, hbloader) == 0)
            {
                SaltySD_printf("SaltySD: Detected title replacement mode, aborting bootstrap...\n");
                goto abort_bootstrap;
            }
            
            FILE* except = fopen("sdmc:/SaltySD/exceptions.txt", "r");
            if (except) {
                char exceptions[20];
                char titleidnumX[20];

                snprintf(titleidnumX, sizeof titleidnumX, "X%016lx", eventinfo.tid);
                while (fgets(exceptions, sizeof(exceptions), except)) {
                    titleidnumX[0] = 'X';
                    if (!strncasecmp(exceptions, titleidnumX, 17)) {
                        SaltySD_printf("SaltySD: %s TID %016lx is forced in exceptions.txt, aborting bootstrap...\n", eventinfo.name, eventinfo.tid);
                        fclose(except);
                        goto abort_bootstrap;
                    }
                    else {
                        titleidnumX[0] = 'R';
                        if (!strncasecmp(exceptions, titleidnumX, 17)) {
                            if (isModInstalled()) {
                                SaltySD_printf("SaltySD: %s TID %016lx is in exceptions.txt as romfs excluded, aborting bootstrap...\n", eventinfo.name, eventinfo.tid);
                                fclose(except);
                                goto abort_bootstrap;
                            }
                            else SaltySD_printf("SaltySD: %s TID %016lx is in exceptions.txt as romfs excluded, but no romfs mod was detected...\n", eventinfo.name, eventinfo.tid);
                        }
                        else if (!strncasecmp(exceptions, &titleidnumX[1], 16)) {
                            SaltySD_printf("SaltySD: %s TID %016lx is in exceptions.txt, aborting loading plugins...\n", eventinfo.name, eventinfo.tid);
                            exception = 0x1;
                        }
                    }
                }
                fclose(except);
            }
            SaltySD_printf("SaltySD: found valid AttachProcess event:\n");
            SaltySD_printf("		 tid %016lx pid %016lx\n", eventinfo.tid, eventinfo.pid);
            SaltySD_printf("		 name %s\n", eventinfo.name);
            SaltySD_printf("		 isA64 %01x addrSpace %01x enableDebug %01x\n", eventinfo.isA64, eventinfo.addrSpace, eventinfo.enableDebug);
            SaltySD_printf("		 enableAslr %01x useSysMemBlocks %01x poolPartition %01x\n", eventinfo.enableAslr, eventinfo.useSysMemBlocks, eventinfo.poolPartition);
            SaltySD_printf("		 exception %016lx\n", eventinfo.userExceptionContextAddr);
            isA64 = eventinfo.isA64;
        }
        else
        {
            SaltySD_printf("SaltySD: debug event %x, passing...\n", eventinfo.type);
            continue;
        }
    }

    u64 threadid = 0;

    uint64_t tick_start = svcGetSystemTick();
    do {
        if (svcGetSystemTick() - tick_start > 19200000 * 30) {
            SaltySD_printf("SaltySD: Waiting for main thread timeout! Aborting...\n");
            goto abort_bootstrap;
        }
        ret = svcGetThreadList(&threads, &threadid, 1, debug);
        svcSleepThread(10000);
    } while (!threads);

    uint64_t passed_time_in_ticks = svcGetSystemTick() - tick_start;
    
    renameCheatsFolder();

    if (passed_time_in_ticks > 19200000 * 10) {
        SaltySD_printf("SaltySD: Waiting for main thread: %d ms, longer than normal!\n", passed_time_in_ticks / 19200);
    }
    
    if (hijack_bootstrap(&debug, pid, threadid, isA64)) {
        lastAppPID = pid;
        
        LoaderModuleInfo module_infos[2] = {0};
        s32 module_infos_count = 0;
        ret = ldrDmntGetProcessModuleInfo(pid, module_infos, 2, &module_infos_count);
        if (R_SUCCEEDED(ret)) {
            BIDnow = __builtin_bswap64(*(uint64_t*)&module_infos[1].build_id[0]);
            SaltySD_printf("SaltySD: BID: %016lX\n", BIDnow);
            ret = 0;
        }
        else SaltySD_printf("SaltySD: cmd 8 ldrDmntGetProcessModuleInfo failed! RC: 0x%X\n", ret);
    }
    else {
        already_hijacking = false;
    }

    return;

abort_bootstrap:
    if (check) renameCheatsFolder();
                
    already_hijacking = false;
    svcCloseHandle(debug);
}

Result handleServiceCmd(int cmd)
{
    Result ret = 0;

    // Send reply
    IpcCommand c;
    ipcInitialize(&c);
    ipcSendPid(&c);

    if (cmd == 0) // EndSession
    {
        ret = 0;
        should_terminate = true;
        //SaltySD_printf("SaltySD: cmd 0, terminating\n");
    }
    else if (cmd == 1) // LoadELF
    {
        IpcParsedCommand r = {0};
        ipcParse(&r);

        struct {
            u64 magic;
            u64 command;
            u64 heap;
            char name[64];
            u32 reserved[2];
        } *resp = r.Raw;

        Handle proc = r.Handles[0];
        u64 heap = resp->heap;
        char name[64];
        
        memcpy(name, resp->name, 64);
        
        SaltySD_printf("SaltySD: cmd 1 handler, proc handle %x, heap %llx, path %s\n", proc, heap, name);
        
        char* path = malloc(96);
        uint8_t* elf_data = NULL;
        u32 elf_size = 0;
        bool arm32 = false;
        if (!strncmp(name, "saltysd_core32.elf", 18)) arm32 = true;

        snprintf(path, 96, "sdmc:/SaltySD/plugins/%s", name);
        FILE* f = fopen(path, "rb");
        if (!f)
        {
            snprintf(path, 96, "sdmc:/SaltySD/%s", name);
            f = fopen(path, "rb");
        }

        if (!f)
        {
            SaltySD_printf("SaltySD: failed to load plugin `%s'!\n", name);
            elf_data = NULL;
            elf_size = 0;
        }
        else if (f)
        {
            fseek(f, 0, SEEK_END);
            elf_size = ftell(f);
            fseek(f, 0, SEEK_SET);
            
            SaltySD_printf("SaltySD: loading %s, size 0x%x\n", path, elf_size);
            
            elf_data = malloc(elf_size);
            if (elf_data) {
                fread(elf_data, elf_size, 1, f);
            }
            else SaltySD_printf("SaltySD: Not enough memory to load elf file! Aborting...\n");
        }
        free(path);
        
        u64 new_start = 0, new_size = 0;
        if (elf_data && elf_size) {
            if (!arm32)
                ret = load_elf_proc(proc, r.Pid, heap, &new_start, &new_size, elf_data, elf_size);
            else ret = load_elf32_proc(proc, r.Pid, (u32)heap, (u32*)&new_start, (u32*)&new_size, elf_data, elf_size);
            if (ret) SaltySD_printf("Load_elf arm32: %d, ret: 0x%x\n", arm32, ret);
        }
        else
            ret = MAKERESULT(MODULE_SALTYSD, 1);

        svcCloseHandle(proc);
        
        if (f)
        {
            if (elf_data)
                free(elf_data);
            fclose(f);
        }
        
        // Ship off results
        struct {
            u64 magic;
            u64 result;
            u64 new_addr;
            u64 new_size;
        } *raw;

        raw = ipcPrepareHeader(&c, sizeof(*raw));

        raw->magic = SFCO_MAGIC;
        raw->result = ret;
        raw->new_addr = new_start;
        raw->new_size = new_size;
        
        if (R_SUCCEEDED(ret)) debug_log("SaltySD: new_addr to %lx, %x\n", new_start, ret);

        return 0;
    }
    else if (cmd == 2) // RestoreBootstrapCode
    {
        IpcParsedCommand r = {0};
        ipcParse(&r);

        SaltySD_printf("SaltySD: cmd 2 handler\n");
        
        Handle debug;
        ret = svcDebugActiveProcess(&debug, r.Pid);
        if (!ret)
        {
            ret = restore_elf_debug(debug);
        }
        
        // Bootstrapping is done, we can handle another process now.
        already_hijacking = false;
        svcCloseHandle(debug);
    }
    else if (cmd == 3) // Memcpy
    {
        IpcParsedCommand r = {0};
        ipcParse(&r);

        struct {
            u64 magic;
            u64 command;
            u64 to;
            u64 from;
            u64 size;
        } *resp = r.Raw;
        
        u64 to, from, size;
        to = resp->to;
        from = resp->from;
        size = resp->size;
        
        Handle debug;
        ret = svcDebugActiveProcess(&debug, r.Pid);
        if (!ret)
        {
            u8* tmp = malloc(size);

            ret = svcReadDebugProcessMemory(tmp, debug, from, size);
            if (!ret)
                ret = svcWriteDebugProcessMemory(debug, tmp, to, size);

            free(tmp);
            
            svcCloseHandle(debug);
        }
        
        // Ship off results
        struct {
            u64 magic;
            u64 result;
            u64 reserved[2];
        } *raw;

        raw = ipcPrepareHeader(&c, sizeof(*raw));

        raw->magic = SFCO_MAGIC;
        raw->result = ret;

        SaltySD_printf("SaltySD: cmd 3 handler, memcpy(%llx, %llx, %llx)\n", to, from, size);

        return 0;
    }
    else if (cmd == 4) // GetSDCard
    {		
        ipcSendHandleCopy(&c, sdcard);

        SaltySD_printf("SaltySD: cmd 4 handler\n"); 
    }
    else if (cmd == 5) // Log
    {
        SaltySD_printf("SaltySD: cmd 5 handler\n");

        IpcParsedCommand r = {0};
        ipcParse(&r);

        struct {
            u64 magic;
            u64 command;
            char log[64];
            u32 reserved[2];
        } *resp = r.Raw;

        SaltySD_printf(resp->log);

        ret = 0;
    }
    else if (cmd == 6) // CheckIfSharedMemoryAvailable
    {		
        IpcParsedCommand r = {0};
        ipcParse(&r);

        struct {
            u64 magic;
            u64 cmd_id;
            u64 size;
            u64 reserved;
        } *resp = r.Raw;

        u64 new_size = resp->size;

        SaltySD_printf("SaltySD: cmd 6 handler, size: %d\n", new_size);

        struct {
            u64 magic;
            u64 result;
            u64 offset;
            u64 reserved;
        } *raw;

        raw = ipcPrepareHeader(&c, sizeof(*raw));

        raw->magic = SFCO_MAGIC;
        if (!new_size) {
            SaltySD_printf("SaltySD: cmd 6 failed. Wrong size.");
            raw->offset = 0;
            raw->result = 0xFFE;
        }
        else if (new_size < (_sharedMemory.size - reservedSharedMemory)) {
            if (shmemGetAddr(&_sharedMemory)) {
                if (!reservedSharedMemory) {
                    memset(shmemGetAddr(&_sharedMemory), 0, 0x1000);
                }
                raw->result = 0;
                raw->offset = reservedSharedMemory;
                reservedSharedMemory += new_size;
                if (reservedSharedMemory % 4 != 0) {
                    reservedSharedMemory += (4 - (reservedSharedMemory % 4));
                }
            }
            else {
                SaltySD_printf("SaltySD: cmd 6 failed. shmemMap error.");
                raw->offset = -1;
                raw->result = 0xFFE;
            }
        }
        else {
            SaltySD_printf("SaltySD: cmd 6 failed. Not enough free space. Left: %d\n", (_sharedMemory.size - reservedSharedMemory));
            raw->offset = -1;
            raw->result = 0xFFE;
        }

        return 0;
    }
    else if (cmd == 7) // GetSharedMemoryHandle
    {
        SaltySD_printf("SaltySD: cmd 7 handler\n");

        ipcSendHandleCopy(&c, _sharedMemory.handle);
    }
    else if (cmd == 8) { // Get BID

        IpcParsedCommand r = {0};
        ipcParse(&r);

        SaltySD_printf("SaltySD: cmd 8 handler PID: %ld\n", PIDnow);

        struct {
            u64 magic;
            u64 result;
        } *raw;

        raw = ipcPrepareHeader(&c, sizeof(*raw));
        raw->magic = SFCO_MAGIC;
        raw->result = BIDnow;

        return 0;
    }
    else if (cmd == 9) // Exception
    {
        IpcParsedCommand r = {0};
        ipcParse(&r);
        
        // Ship off results
        struct {
            u64 magic;
            u64 result;
            u64 reserved[2];
        } *raw;

        raw = ipcPrepareHeader(&c, sizeof(*raw));

        raw->magic = SFCO_MAGIC;
        raw->result = exception;

        return 0;
    }
    else if (cmd == 10) // GetDisplayRefreshRate
    {
        IpcParsedCommand r = {0};
        ipcParse(&r);

        SaltySD_printf("SaltySD: cmd 10 handler\n");
        
        // Ship off results
        struct {
            u64 magic;
            u64 result;
            u64 refreshRate;
            u64 reserved[2];
        } *raw;

        raw = ipcPrepareHeader(&c, sizeof(*raw));

        raw->magic = SFCO_MAGIC;
        uint32_t temp_refreshRate = 0;
        raw->result = !GetDisplayRefreshRate(&temp_refreshRate, false);
        raw->refreshRate = temp_refreshRate;

        return 0;
    }
    else if (cmd == 11) // SetDisplayRefreshRate
    {
        IpcParsedCommand r = {0};
        ipcParse(&r);

        struct {
            u64 magic;
            u64 cmd_id;
            u64 refreshRate;
            u64 reserved;
        } *resp = r.Raw;

        u64 refreshRate_temp = resp -> refreshRate;

        if (SetDisplayRefreshRate(refreshRate_temp)) {
            refreshRate = refreshRate_temp;
            ret = 0;
        }
        else ret = 0x1234;
        SaltySD_printf("SaltySD: cmd 11 handler -> %d\n", refreshRate_temp);
    }
    else if (cmd == 12) // SetDisplaySync
    {
        IpcParsedCommand r = {0};
        ipcParse(&r);

        struct {
            u64 magic;
            u64 cmd_id;
            u64 value;
            u64 reserved;
        } *resp = r.Raw;

        displaySync = (bool)(resp -> value);
        if (displaySync) {
            FILE* file = fopen("sdmc:/SaltySD/flags/displaysync.flag", "wb");
            fclose(file);
            SaltySD_printf("SaltySD: cmd 12 handler -> %d\n", displaySync);
        }
        else {
            remove("sdmc:/SaltySD/flags/displaysync.flag");
            SaltySD_printf("SaltySD: cmd 12 handler -> %d\n", displaySync);
        }

        ret = 0;
    }
    else if (cmd == 13) // SetAllowedDockedRefreshRates
    {
        IpcParsedCommand r = {0};
        ipcParse(&r);

        struct {
            u64 magic;
            u64 cmd_id;
            u32 refreshRate;
            u32 reserved[3];
        } *resp = r.Raw;

        struct {
            unsigned int Hz_40: 1;
            unsigned int Hz_45: 1;
            unsigned int Hz_50: 1;
            unsigned int Hz_55: 1;
            unsigned int Hz_60: 1;
            unsigned int reserved: 27;
        } DockedRefreshRates;

        memcpy(&DockedRefreshRates, &(resp -> refreshRate), 4);
        DockedModeRefreshRateAllowed[0] = DockedRefreshRates.Hz_40;
        DockedModeRefreshRateAllowed[1] = DockedRefreshRates.Hz_45;
        DockedModeRefreshRateAllowed[2] = DockedRefreshRates.Hz_50;
        DockedModeRefreshRateAllowed[3] = DockedRefreshRates.Hz_55;
        DockedModeRefreshRateAllowed[4] = true;
        SaltySD_printf("SaltySD: cmd 13 handler\n");

        ret = 0;
    }
    else if (cmd == 14) // SetDontForce60InDocked
    {
        IpcParsedCommand r = {0};
        ipcParse(&r);

        struct {
            u64 magic;
            u64 cmd_id;
            u64 force;
            u64 reserved;
        } *resp = r.Raw;

        dontForce60InDocked = (bool)(resp -> force);
        SaltySD_printf("SaltySD: cmd 14 handler\n");

        ret = 0;
    }
    else if (cmd == 15) // SetMatchLowestRR
    {
        IpcParsedCommand r = {0};
        ipcParse(&r);

        struct {
            u64 magic;
            u64 cmd_id;
            u64 force;
            u64 reserved;
        } *resp = r.Raw;

        matchLowestDocked = (bool)(resp -> force);
        SaltySD_printf("SaltySD: cmd 15 handler\n");

        ret = 0;
    }
    else
    {
        ret = 0xEE01;
    }
    
    struct {
        u64 magic;
        u64 result;
        u64 reserved[2];
    } *raw;

    raw = ipcPrepareHeader(&c, sizeof(*raw));

    raw->magic = SFCO_MAGIC;
    raw->result = ret;
    
    return ret;
}

void serviceThread(void* buf)
{
    Result ret;
    SaltySD_printf("SaltySD: accepting service calls\n");
    should_terminate = false;

    while (1)
    {
        Handle session;
        ret = svcAcceptSession(&session, saltyport);
        if (ret && ret != 0xf201)
        {
            SaltySD_printf("SaltySD: svcAcceptSession returned %x\n", ret);
        }
        else if (!ret)
        {
            SaltySD_printf("SaltySD: session %x being handled\n", session);

            int handle_index;
            Handle replySession = 0;
            while (1)
            {
                ret = svcReplyAndReceive(&handle_index, &session, 1, replySession, UINT64_MAX);
                
                if (should_terminate) break;
                
                if (ret) break;
                
                IpcParsedCommand r;
                ipcParse(&r);

                struct {
                    u64 magic;
                    u64 command;
                    u64 reserved[2];
                } *resp = r.Raw;

                u64 command = resp->command;

                handleServiceCmd(command);
                
                if (should_terminate) break;

                replySession = session;
                svcSleepThread(1000*1000);
            }
            
            svcCloseHandle(session);
        }
        else should_terminate = true;

        if (should_terminate) break;
        
        svcSleepThread(1000*1000*100);
    }
    
    SaltySD_printf("SaltySD: done accepting service calls\n");
}

int main(int argc, char *argv[])
{
    ABORT_IF_FAILED(smInitialize_old(), 0);
    Service_old toget;
    ABORT_IF_FAILED(smGetService_old(&toget, "fsp-srv"), 1);
    ABORT_IF_FAILED(fsp_init(toget), 2);
    ABORT_IF_FAILED(fsp_getSdCard(toget, &sdcard), 3);
    FsFileSystem_old sdcardfs;
    sdcardfs.s.handle = sdcard;
    if (fsdevMountDevice_old("sdmc", sdcardfs) == -1) {
        ABORT_IF_FAILED(0xDEADBEEF, 4);
    }
    serviceClose_old(&toget);
    smExit_old();
    SaltySD_printf("SaltySD: got SD card.\n");

    ABORT_IF_FAILED(smInitialize(), 5);
    ABORT_IF_FAILED(setsysInitialize(), 10);

    SetSysProductModel model;
    if (R_SUCCEEDED(setsysGetProductModel(&model))) {
        if (model == SetSysProductModel_Aula) {
            SaltySD_printf("SaltySD: Detected OLED model. Display Sync is not available in handheld mode.\n");
            isOLED = true;
        }
        else if (model == SetSysProductModel_Hoag) {
            isLite = true;
            SaltySD_printf("SaltySD: Detected Lite model. Docked refresh rate will be blocked.\n");
        }
    }

    if (!isLite) {
        ABORT_IF_FAILED(nvInitialize(), 6);
    }
    
    ABORT_IF_FAILED(ldrDmntInitialize(), 7);
    Service* ldrDmntSrv = ldrDmntGetServiceSession();
    Service ldrDmntClone;
    serviceClone(ldrDmntSrv, &ldrDmntClone);
    serviceClose(ldrDmntSrv);
    memcpy(ldrDmntSrv, &ldrDmntClone, sizeof(Service));


    FILE* file = fopen("sdmc:/SaltySD/flags/displaysync.flag", "rb");
    if (file) {
        fclose(file);
        displaySync = true;
    }
    // Start our port
    // For some reason, we only have one session maximum (0 reslimit handle related?)	
    svcManageNamedPort(&saltyport, "SaltySD", 1);
    svcManageNamedPort(&injectserv, "InjectServ", 1);

    uint64_t dummy = 0;
    Result rc = svcQueryMemoryMapping(&clkVirtAddr, &dummy, 0x60006000, 0x1000);
    if (R_FAILED(rc)) {
        SaltySD_printf("SaltySD: Retrieving virtual address for 0x60006000 failed. RC: 0x%x.\n", rc);
        clkVirtAddr = 0;
    }
    shmemCreate(&_sharedMemory, 0x1000, Perm_Rw, Perm_Rw);
    shmemMap(&_sharedMemory);
    // Main service loop
    u64* pids = malloc(0x200 * sizeof(u64));
    u64 max = 0;
    while (1)
    {
        s32 num;
        svcGetProcessList(&num, pids, 0x200);

        u64 old_max = max;
        for (int i = 0; i < num; i++)
        {
            if (pids[i] > max)
            {
                max = pids[i];
            }
        }
        
        if (lastAppPID != -1) {
            if (!nx_fps)  {
                uintptr_t sharedAddress = (uintptr_t)shmemGetAddr(&_sharedMemory);
                if (sharedAddress) {
                    ptrdiff_t offset = searchNxFpsSharedMemoryBlock(sharedAddress);
                    if (offset != -1) {
                        nx_fps = (struct NxFpsSharedBlock*)(sharedAddress + offset);
                    }
                }
            }
            if (!cheatCheck) {
                if (!isCheatsFolderInstalled() || !isServiceRunning("dmnt:cht"))
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
            bool found = false;
            for (int i = num - 1; lastAppPID <= pids[i]; i--)
            {
                if (pids[i] == lastAppPID)
                {	
                    found = true;
                    break;
                }
            }
            if (!found) {
                lastAppPID = -1;
                nx_fps = 0;
                cheatCheck = false;
                if (displaySync) {
                    uint32_t temp_refreshRate = 0;
                    if (GetDisplayRefreshRate(&temp_refreshRate, true) && temp_refreshRate != 60)
                        SetDisplayRefreshRate(60);
                    refreshRate = 0;
                }
            }
            else if (displaySync && (!isOLED || isDocked)) {
                uint32_t temp_refreshRate = 0;
                GetDisplayRefreshRate(&temp_refreshRate, true);
                uint32_t check_refresh_rate = refreshRate;
                if (nx_fps && nx_fps->forceOriginalRefreshRate && (!isDocked || (isDocked && !dontForce60InDocked))) {
                    check_refresh_rate = 60;
                }
                if (temp_refreshRate != check_refresh_rate)
                    SetDisplayRefreshRate(check_refresh_rate);
            }
        }
        uint32_t temp = 0;
        GetDisplayRefreshRate(&temp, true);

        // Detected new PID
        if (max != old_max && max > 0x80)
        {
            PIDnow = max;
            hijack_pid(max);
        }
        
        // If someone is waiting for us, handle them.
        if (!svcWaitSynchronizationSingle(saltyport, 1000))
        {
            serviceThread(NULL);
        }
        if (!svcWaitSynchronizationSingle(injectserv, 1000)) {
            Handle sesja;
            svcAcceptSession(&sesja, injectserv);
            svcCloseHandle(sesja);
        }

        svcSleepThread(10*1000*1000);
    }
    free(pids);

    return 0;
}

