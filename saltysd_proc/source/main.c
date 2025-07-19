#include <switch.h>
#include "display_refresh_rate.h"
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
#include <ctype.h>
#include <sys/stat.h>

#define MODULE_SALTYSD 420
#define NVDISP_PANEL_GET_VENDOR_ID 0xC003021A

#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))

struct NxFpsSharedBlock* nx_fps = 0;

u32 __nx_applet_type = AppletType_None;

void serviceThread(void* buf);

struct MinMax {
    u8 min;
    u8 max;
};

Handle saltyport, sdcard, injectserv;
static char g_heap[0x70000];
bool should_terminate = false;
bool already_hijacking = false;
DebugEventInfo eventinfo;
bool check = false;
u64 exception = 0x0;
SharedMemory _sharedMemory = {0};
size_t reservedSharedMemory = 0;
uint64_t clkVirtAddr = 0;
uint64_t dsiVirtAddr = 0;
bool displaySync = false;
bool displaySyncDocked = false;
uint8_t refreshRate = 0;
s64 lastAppPID = -1;
bool isOLED = false;
bool isLite = false;
bool cheatCheck = false;
bool isDocked = false;
bool dontForce60InDocked = false;
bool matchLowestDocked = false;
uint8_t dockedHighestRefreshRate = 60;
uint8_t dockedLinkRate = 10;
bool isRetroSUPER = false;
bool isPossiblySpoofedRetro = false;
bool wasRetroSuperTurnedOff = false;
uint64_t systemtickfrequency = 0;
extern bool DockedModeRefreshRateAllowed[];
extern struct MinMax HandheldModeRefreshRateAllowed;

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
    nvExit();
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
        
    if (file_or_directory_exists("sdmc:/SaltySD/flags/disable.flag") == true) {
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
        if (svcGetSystemTick() - tick_start > systemtickfrequency * 30) {
            SaltySD_printf("SaltySD: Waiting for main thread timeout! Aborting...\n");
            goto abort_bootstrap;
        }
        ret = svcGetThreadList(&threads, &threadid, 1, debug);
        svcSleepThread(10000);
    } while (!threads);

    uint64_t passed_time_in_ticks = svcGetSystemTick() - tick_start;
    
    renameCheatsFolder();

    if (passed_time_in_ticks > systemtickfrequency * 10) {
        SaltySD_printf("SaltySD: Waiting for main thread: %d ms, longer than normal!\n", passed_time_in_ticks / (systemtickfrequency / 1000));
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
            unsigned int Hz_70: 1;
            unsigned int Hz_72: 1;
            unsigned int Hz_75: 1;
            unsigned int Hz_80: 1;
            unsigned int Hz_90: 1;
            unsigned int Hz_95: 1;
            unsigned int Hz_100: 1;
            unsigned int Hz_110: 1;
            unsigned int Hz_120: 1;
            unsigned int reserved: 18;
        } DockedRefreshRates;

        memcpy(&DockedRefreshRates, &(resp -> refreshRate), 4);
        DockedModeRefreshRateAllowed[0] = DockedRefreshRates.Hz_40;
        DockedModeRefreshRateAllowed[1] = DockedRefreshRates.Hz_45;
        DockedModeRefreshRateAllowed[2] = DockedRefreshRates.Hz_50;
        DockedModeRefreshRateAllowed[3] = DockedRefreshRates.Hz_55;
        DockedModeRefreshRateAllowed[4] = true;
        DockedModeRefreshRateAllowed[5] = DockedRefreshRates.Hz_70;
        DockedModeRefreshRateAllowed[6] = DockedRefreshRates.Hz_72;
        DockedModeRefreshRateAllowed[7] = DockedRefreshRates.Hz_75;
        DockedModeRefreshRateAllowed[8] = DockedRefreshRates.Hz_80;
        DockedModeRefreshRateAllowed[9] = DockedRefreshRates.Hz_90;
        DockedModeRefreshRateAllowed[10] = DockedRefreshRates.Hz_95;
        DockedModeRefreshRateAllowed[11] = DockedRefreshRates.Hz_100;
        DockedModeRefreshRateAllowed[12] = DockedRefreshRates.Hz_110;
        DockedModeRefreshRateAllowed[13] = DockedRefreshRates.Hz_120;
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
    else if (cmd == 16) // GetDockedHighestRefreshRate
    {
        IpcParsedCommand r = {0};
        ipcParse(&r);

        SaltySD_printf("SaltySD: cmd 16 handler\n");
        
        // Ship off results
        struct {
            u64 magic;
            u64 result;
            u32 refreshRate;
            u32 linkRate;
            u64 reserved;
        } *raw;

        raw = ipcPrepareHeader(&c, sizeof(*raw));

        raw->magic = SFCO_MAGIC;
        raw->result = 0;
        raw->refreshRate = dockedHighestRefreshRate;
        raw->linkRate = dockedLinkRate;

        return 0;
    }
    else if (cmd == 17) // IsPossiblyRetroRemake
    {
        IpcParsedCommand r = {0};
        ipcParse(&r);

        SaltySD_printf("SaltySD: cmd 17 handler\n");
        
        // Ship off results
        struct {
            u64 magic;
            u64 result;
            u64 value;
            u64 reserved;
        } *raw;

        raw = ipcPrepareHeader(&c, sizeof(*raw));

        raw->magic = SFCO_MAGIC;
        raw->result = 0;
        raw->value = isPossiblySpoofedRetro;

        return 0;
    }
    else if (cmd == 18) // SetDisplaySyncDocked
    {
        IpcParsedCommand r = {0};
        ipcParse(&r);

        struct {
            u64 magic;
            u64 cmd_id;
            u64 value;
            u64 reserved;
        } *resp = r.Raw;

        displaySyncDocked = (bool)(resp -> value);
        if (displaySyncDocked) {
            FILE* file = fopen("sdmc:/SaltySD/flags/displaysyncdocked.flag", "wb");
            fclose(file);
            SaltySD_printf("SaltySD: cmd 18 handler -> %d\n", displaySyncDocked);
        }
        else {
            remove("sdmc:/SaltySD/flags/displaysyncdocked.flag");
            SaltySD_printf("SaltySD: cmd 18 handler -> %d\n", displaySyncDocked);
        }

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
	systemtickfrequency = armGetSystemTickFreq();
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

    if (file_or_directory_exists("sdmc:/SaltySD/flags/displaysync.flag")) {
        displaySync = true;
    }
    if (file_or_directory_exists("sdmc:/SaltySD/flags/displaysyncdocked.flag")) {
        displaySyncDocked = true;
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
    if (isOLED) {
        Result rc = svcQueryMemoryMapping(&dsiVirtAddr, &dummy, 0x54300000, 0x40000);
        if (R_FAILED(rc)) {
            SaltySD_printf("SaltySD: Retrieving virtual address for 0x54300000 failed. RC: 0x%x.\n", rc);
            dsiVirtAddr = 0;
        }
    }
    shmemCreate(&_sharedMemory, 0x1000, Perm_Rw, Perm_Rw);
    shmemMap(&_sharedMemory);
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
                if (shmemGetAddr(&_sharedMemory)) {
                    memset(shmemGetAddr(&_sharedMemory), 0, 0x1000);
                }
                if ((!isDocked && displaySync) || (isDocked && displaySyncDocked)) {
                    uint32_t temp_refreshRate = 0;
                    if (GetDisplayRefreshRate(&temp_refreshRate, true) && temp_refreshRate != 60)
                        SetDisplayRefreshRate(60);
                    refreshRate = 0;
                }
            }
            else {
                if ((!isDocked && displaySync) || (isDocked && displaySyncDocked)) {
                    uint32_t temp_refreshRate = 0;
                    GetDisplayRefreshRate(&temp_refreshRate, true);
                    uint32_t check_refresh_rate = refreshRate;
                    if (nx_fps && (isDocked ? nx_fps->FPSlockedDocked : nx_fps->FPSlocked)) check_refresh_rate = (isDocked ? nx_fps->FPSlockedDocked : nx_fps->FPSlocked);
                    if (nx_fps && nx_fps->forceOriginalRefreshRate && (!isDocked || (isDocked && !dontForce60InDocked))) {
                        check_refresh_rate = 60;
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
        }
        uint32_t crr = 0;
        GetDisplayRefreshRate(&crr, true);
        if (isOLED) correctOledGamma(crr);

        if (nx_fps) nx_fps -> dontForce60InDocked = dontForce60InDocked;

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

