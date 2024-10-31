#include <switch_min.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <dirent.h>
#include <switch_min/kernel/svc_extra.h>
#include <switch_min/kernel/ipc.h>

#include "spawner_ipc.h"

#include "loadelf.h"
#include "useful.h"
#include "dmntcht.h"

#define MODULE_SALTYSD 420

u32 __nx_applet_type = AppletType_None;

void serviceThread(void* buf);

Handle saltyport, sdcard, injectserv;
static char g_heap[0x70000];
bool should_terminate = false;
bool already_hijacking = false;
DebugEventInfo eventinfo;
bool check = false;
u64 exception = 0x0;
SharedMemory _sharedMemory = {};
size_t reservedSharedMemory = 0;
uint64_t clkVirtAddr = 0;
bool displaySync = false;
uint8_t refreshRate = 0;
s64 lastAppPID = -1;
bool isOLED = false;
bool isErista = false;
bool cheatCheck = false;

void __libnx_initheap(void)
{
    extern char* fake_heap_start;
    extern char* fake_heap_end;

    fake_heap_start = &g_heap[0];
    fake_heap_end   = &g_heap[sizeof g_heap];
}

void __appInit(void)
{
    
}

void __appExit(void)
{
    already_hijacking = false;
    fsdevUnmountAll();
    fsExit();
    smExit();
}

u64 TIDnow;
u64 PIDnow;

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

bool SetDisplayRefreshRate(uint32_t refreshRate) {
    if (!clkVirtAddr)
        return false;
    if (refreshRate > 75 || refreshRate < 35 || refreshRate % 5 != 0)
        return false;
    struct PLLD_BASE base = {0};
    struct PLLD_MISC misc = {0};
    memcpy(&base, (void*)(clkVirtAddr + 0xD0), 4);
    memcpy(&misc, (void*)(clkVirtAddr + 0xDC), 4);
    
    base.PLLD_DIVN = (4 * refreshRate) / 10;
    base.PLLD_DIVM = 1;

    int16_t Step_5Hz = 256;
    int16_t default_SDM_DIN = -1024;
    int8_t steps = ((int32_t)refreshRate - 60) / 5;
    
    misc.PLLD_SDM_DIN = default_SDM_DIN + (Step_5Hz * steps);

    memcpy((void*)(clkVirtAddr + 0xD0), &base, 4);
    memcpy((void*)(clkVirtAddr + 0xDC), &misc, 4);
    return true;
}

bool GetDisplayRefreshRate(uint32_t* refreshRate) {
    if (!clkVirtAddr)
        return false;
    struct PLLD_BASE temp = {0};
    uint32_t value = *(uint32_t*)(clkVirtAddr + 0xD0);
    memcpy(&temp, &value, 4);
    value = ((temp.PLLD_DIVN / temp.PLLD_DIVM) * 10) / 4;
    if (value == 0)
        value = 60;
    *refreshRate = value;
    return true;
}

bool isServiceRunning(const char *serviceName) {	
	Handle handle;	
	if (R_FAILED(smRegisterService(&handle, serviceName, false, 1))) 
		return true;
	else {
		svcCloseHandle(handle);	
		smUnregisterService(serviceName);
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
    FILE* file = 0;
    if (isA64)
        file = fopen("sdmc:/SaltySD/saltysd_bootstrap.elf", "rb");
    else file = fopen("sdmc:/SaltySD/saltysd_bootstrap32.elf", "rb");
    if (!file) {
        if (isA64) SaltySD_printf("SaltySD: SaltySD/saltysd_bootstrap.elf not found, aborting...\n", ret);
        else SaltySD_printf("SaltySD: SaltySD/saltysd_bootstrap32.elf not found, aborting...\n", ret);
        svcCloseHandle(*debug);
        return false;
    }
    fseek(file, 0, 2);
    size_t saltysd_bootstrap_elf_size = ftell(file);
    fseek(file, 0, 0);
    u8* elf = malloc(saltysd_bootstrap_elf_size);
    fread(elf, saltysd_bootstrap_elf_size, 1, file);
    fclose(file);
    
    uint64_t new_start;
    if (isA64) load_elf_debug(*debug, &new_start, elf, saltysd_bootstrap_elf_size);
    else load_elf32_debug(*debug, &new_start, elf, saltysd_bootstrap_elf_size);
    free(elf);

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
    Result ret;
    u32 threads;
    Handle debug;
    
    FILE* disabled = fopen("sdmc:/SaltySD/flags/disable.flag", "r");
    u8 disable = 1;
    
    if (disabled == NULL) {
        disable = 0;
    }
    fclose(disabled);
    
    if (already_hijacking)
    {
        SaltySD_printf("SaltySD: PID %llx spawned before last hijack finished bootstrapping! Ignoring...\n", pid);
        return;
    }
    
    already_hijacking = true;
    svcDebugActiveProcess(&debug, pid);

    u64* tids = malloc(0x200 * sizeof(u64));

    do
    {
        ret = svcGetThreadList(&threads, tids, 0x200, debug);
        svcSleepThread(-1);
    }
    while (!tids);
    
    ThreadContext context;
    ret = svcGetDebugThreadContext(&context, debug, tids[0], RegisterGroup_All);

    SaltySD_printf("SaltySD: new max %lx, %x %016lx\n", pid, threads, context.pc.x);

    char exceptions[20];
    char line[20];
    char titleidnum[20];
    char titleidnumX[20];
    char titleidnumR[20];
    bool isA64 = true;

    while (1)
    {
        ret = svcGetDebugEventInfo(&eventinfo, debug);

        if (check == false) {
            TIDnow = eventinfo.tid;
            exception = 0;
            renameCheatsFolder();
        }

        if (ret)
        {
            SaltySD_printf("SaltySD: svcGetDebugEventInfo returned %x, breaking\n", ret);
            // Invalid Handle
            if (ret == 0xe401)
                goto abort_bootstrap;
            break;
        }

        if (eventinfo.type == DebugEvent_AttachProcess)
        {
            if (disable == 1) {
                SaltySD_printf("SaltySD: Detected disable.flag, aborting bootstrap...\n");
                goto abort_bootstrap;
            }

            if (eventinfo.tid <= 0x010000000000FFFF)
            {
                SaltySD_printf("SaltySD: TID %016lx is a system application, aborting bootstrap...\n", eventinfo.tid);
                goto abort_bootstrap;
            }
            if (eventinfo.tid > 0x01FFFFFFFFFFFFFF || (eventinfo.tid & 0x1F00) != 0)
            {
                SaltySD_printf("SaltySD: TID %016lx is a homebrew application, aborting bootstrap...\n", eventinfo.tid);
                goto abort_bootstrap;
            }
            if (!shmemMap(&_sharedMemory)) {
                memset(shmemGetAddr(&_sharedMemory), 0, 0x1000);
                shmemUnmap(&_sharedMemory);
            }
            char* hbloader = "hbloader";
            if (strcasecmp(eventinfo.name, hbloader) == 0)
            {
                SaltySD_printf("SaltySD: Detected title replacement mode, aborting bootstrap...\n");
                goto abort_bootstrap;
            }
            
            FILE* except = fopen("sdmc:/SaltySD/exceptions.txt", "r");
            if (except) {
                snprintf(titleidnum, sizeof titleidnum, "%016lx", eventinfo.tid);
                snprintf(titleidnumX, sizeof titleidnumX, "X%016lx", eventinfo.tid);
                snprintf(titleidnumR, sizeof titleidnumR, "R%016lx", eventinfo.tid);
                while (fgets(line, sizeof(line), except)) {
                    snprintf(exceptions, sizeof exceptions, "%s", line); 
                    if (!strncasecmp(exceptions, titleidnumX, 17)) {
                        SaltySD_printf("SaltySD: TID %016lx is forced in exceptions.txt, aborting bootstrap...\n", eventinfo.tid);
                        fclose(except);
                        goto abort_bootstrap;
                    }
                    else if (!strncasecmp(exceptions, titleidnumR, 17)) {
                        if (isModInstalled()) {
                            SaltySD_printf("SaltySD: TID %016lx is in exceptions.txt as romfs excluded, aborting bootstrap...\n", eventinfo.tid);
                            fclose(except);
                            goto abort_bootstrap;
                        }
                        else SaltySD_printf("SaltySD: TID %016lx is in exceptions.txt as romfs excluded, but no romfs mod was detected...\n", eventinfo.tid);
                    }
                    else if (!strncasecmp(exceptions, titleidnum, 16)) {
                        SaltySD_printf("SaltySD: TID %016lx is in exceptions.txt, aborting loading plugins...\n", eventinfo.tid);
                        exception = 0x1;
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
    // Poll for new threads (svcStartProcess) while stuck in debug
    
    uint64_t tick_start = svcGetSystemTick();
    do
    {
        if (svcGetSystemTick() - tick_start > 19200000 * 10) {
            goto abort_bootstrap;
        }
        ret = svcGetThreadList(&threads, tids, 0x200, debug);
        svcSleepThread(-1);
    }
    while (!threads);
    renameCheatsFolder();
    
    if (hijack_bootstrap(&debug, pid, tids[0], isA64)) {
        lastAppPID = pid;
    }
    else {
        already_hijacking = false;
        disable = 0;
    }
    
    free(tids);

    return;

abort_bootstrap:
    disable = 0;
    free(tids);
    renameCheatsFolder();
                
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
            
            fread(elf_data, elf_size, 1, f);
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
        
        debug_log("SaltySD: new_addr to %lx, %x\n", new_start, ret);

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
            if (!shmemMap(&_sharedMemory)) {
                if (!reservedSharedMemory) {
                    memset(shmemGetAddr(&_sharedMemory), 0, 0x1000);
                }
                raw->result = 0;
                raw->offset = reservedSharedMemory;
                reservedSharedMemory += new_size;
                if (reservedSharedMemory % 4 != 0) {
                    reservedSharedMemory += (4 - (reservedSharedMemory % 4));
                }
                shmemUnmap(&_sharedMemory);
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

        SaltySD_printf("SaltySD: cmd 8 handlerm PID: %ld\n", PIDnow);

        u64 BID = 0;
        
        ret = ldrDmntInitialize();
        if (R_SUCCEEDED(ret)) {
            LoaderModuleInfo module_infos[2] = {0};
            u32 module_infos_count = 0;
            ret = ldrDmntGetModuleInfos(PIDnow, module_infos, 2, &module_infos_count);
            ldrDmntExit();
            if (R_SUCCEEDED(ret)) {
                BID = __builtin_bswap64(*(uint64_t*)&module_infos[1].build_id[0]);
                SaltySD_printf("SaltySD: cmd 8 Main found. BID: %016lX\n", BID);
                ret = 0;
            }
            else SaltySD_printf("SaltySD: cmd 8 ldrDmntGetModuleInfos failed! RC: 0x%X\n", ret);
        }
        else SaltySD_printf("SaltySD: cmd 8 ldrDmntInitialize failed! RC: 0x%X\n", ret);

        struct {
            u64 magic;
            u64 result;
        } *raw;

        raw = ipcPrepareHeader(&c, sizeof(*raw));
        raw->magic = SFCO_MAGIC;
        if (!ret) {
            raw->result = BID;
        }
        else {
            raw->result = 0;
        }

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
        uint32_t refreshRate = 0;
        raw->result = !GetDisplayRefreshRate(&refreshRate);
        raw->refreshRate = refreshRate;

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

        if (refreshRate_temp > 79 || refreshRate_temp < 31) {
            SaltySD_printf("SaltySD: cmd 11 handler -> %d, invalid value. Setting 60...\n", refreshRate_temp);
            refreshRate = 60;
        }
        else {
            SaltySD_printf("SaltySD: cmd 11 handler -> %d\n", refreshRate_temp);
            refreshRate = refreshRate_temp;
        }
        if (SetDisplayRefreshRate(refreshRate)) 
            ret = 0;
        else ret = 0x1234;
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

        if (!isOLED) displaySync = (bool)(resp -> value);
        if (!isOLED && displaySync) {
            FILE* file = fopen("sdmc:/SaltySD/flags/displaysync.flag", "wb");
            fclose(file);
            SaltySD_printf("SaltySD: cmd 12 handler -> %d\n", displaySync);
        }
        else if (isOLED) {
            SaltySD_printf("SaltySD: cmd 12 handler -> %d. Detected OLED model, ignoring...\n", displaySync);
            remove("sdmc:/SaltySD/flags/displaysync.flag");
        }
        else {
            remove("sdmc:/SaltySD/flags/displaysync.flag");
            SaltySD_printf("SaltySD: cmd 12 handler -> %d\n", displaySync);
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
                ret = svcReplyAndReceive(&handle_index, &session, 1, replySession, U64_MAX);
                
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

Result fsp_init(Service fsp)
{
    Result rc;
    IpcCommand c;
    ipcInitialize(&c);
    ipcSendPid(&c);

    struct {
        u64 magic;
        u64 cmd_id;
        u64 unk;
    } *raw;

    raw = ipcPrepareHeader(&c, sizeof(*raw));

    raw->magic = SFCI_MAGIC;
    raw->cmd_id = 1;
    raw->unk = 0;

    rc = serviceIpcDispatch(&fsp);

    if (R_SUCCEEDED(rc)) {
        IpcParsedCommand r;
        ipcParse(&r);

        struct {
            u64 magic;
            u64 result;
        } *resp = r.Raw;

        rc = resp->result;
    }
    
    return rc;
}

Result fsp_getSdCard(Service fsp, Handle* out)
{
    Result rc;
    IpcCommand c;
    ipcInitialize(&c);

    struct {
        u64 magic;
        u64 cmd_id;
    } *raw;

    raw = ipcPrepareHeader(&c, sizeof(*raw));

    raw->magic = SFCI_MAGIC;
    raw->cmd_id = 18;

    rc = serviceIpcDispatch(&fsp);

    if (R_SUCCEEDED(rc)) {
        IpcParsedCommand r;
        ipcParse(&r);

        struct {
            u64 magic;
            u64 result;
        } *resp = r.Raw;

        *out = r.Handles[0];

        rc = resp->result;
    }
    
    return rc;
}

int main(int argc, char *argv[])
{
    
    svcSleepThread(1*1000*1000*1000);
    smInitialize();

    Service toget;
    smGetService(&toget, "fsp-srv");
    fsp_init(toget);
    fsp_getSdCard(toget, &sdcard);
    SaltySD_printf("SaltySD: got SD card dev.\n");
    FsFileSystem sdcardfs;
    sdcardfs.s.handle = sdcard;
    fsdevMountDevice("sdmc", sdcardfs);
    SaltySD_printf("SaltySD: got SD card.\n");

    setsysInitialize();
    SetSysProductModel model;
    if (R_SUCCEEDED(setsysGetProductModel(&model))) {
        if (model == SetSysProductModel_Aula) {
            SaltySD_printf("SaltySD: Detected OLED model. Display Sync is not available.\n");
            isOLED = true;
            remove("sdmc:/SaltySD/flags/displaysync.flag");
        }
        else if (model == SetSysProductModel_Nx) {
            isErista = true;
        }
    }
    setsysExit();
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
    // Main service loop
    u64* pids = malloc(0x200 * sizeof(u64));
    u64 max = 0;
    while (1)
    {
        u32 num;
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
            if (!cheatCheck) {
                if (!isCheatsFolderInstalled() || !isServiceRunning("dmnt:cht"))
                    cheatCheck = true;
                else {
                    Handle debug_handle;
                    if (R_SUCCEEDED(svcDebugActiveProcess(&debug_handle, lastAppPID))) {
                        u32 thread_count;
                        u64 threads[2];
                        svcGetThreadList(&thread_count, threads, 2, debug_handle);
                        svcCloseHandle(debug_handle);
                        if (thread_count > 1) {
                            cheatCheck = true;
                            dmntchtInitialize();
                            dmntchtForceOpenCheatProcess();
                            dmntchtExit();
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
                cheatCheck = false;
                if (displaySync && !isOLED) {
                    uint32_t temp_refreshRate = 0;
                    if (GetDisplayRefreshRate(&temp_refreshRate) && temp_refreshRate != 60)
                        SetDisplayRefreshRate(60);
                    refreshRate = 0;
                }
            }
            else if (displaySync && !isOLED) {
                uint32_t temp_refreshRate = 0;
                if (GetDisplayRefreshRate(&temp_refreshRate) && temp_refreshRate != refreshRate)
                    SetDisplayRefreshRate(refreshRate);
            }
        }

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

