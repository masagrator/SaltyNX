#include <switch.h>
#include <dirent.h>
#include "ipc.h"
#include "svc_extra.h"
#include "loadelf.h"
#include "useful.h"
#include <stdlib.h>
#include "display_refresh_rate.h"

#define shmem_size 0x1000
#define MODULE_SALTYSD 420

extern s64 lastAppPID;
extern bool already_hijacking;
extern u64 TIDnow;
extern u64 BIDnow;
extern u64 PIDnow;
extern SharedMemory _sharedMemory;
extern Handle sdcard;
extern Handle saltyport;
extern uint8_t refreshRate;
extern bool displaySync;
extern bool displaySyncOutOfFocus60;
extern bool displaySyncDocked;
extern bool displaySyncDockedOutOfFocus60;
extern bool dontForce60InDocked;

bool check = false;
u64 exception = 0x0;
size_t reservedSharedMemory = 0;
uintptr_t game_start_address = 0;
bool should_terminate = false;
bool matchLowestDocked = false;

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
    game_start_address = context.pc.x;
    uint64_t new_start;
    if (isA64) {
        FILE* file = 0;
        file = fopen("sdmc:/SaltySD/saltysd_bootstrap.elf", "rb");
        if (!file) {
            SaltySD_printf("SaltySD: SaltySD/saltysd_bootstrap.elf not found, aborting...\n");
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

static bool isModInstalled() {
    char romfspath[0x40] = "";
    bool flag = false;

    npf_snprintf(romfspath, 0x40, "sdmc:/atmosphere/contents/%016lx/romfs", TIDnow);

    DIR* dir = opendir(romfspath);
    if (dir) {
        if (readdir(dir))
            flag = true;
        closedir(dir);
    }

    return flag;
}

static void renameCheatsFolder() {
    char cheatspath[0x3C] = "";
    char cheatspathtemp[0x40] = "";

    npf_snprintf(cheatspath, 0x3C, "sdmc:/atmosphere/contents/%016lx/cheats", TIDnow);
    npf_snprintf(cheatspathtemp, 0x40, "%stemp", cheatspath);
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

void hijack_pid(u64 pid)
{
    Result ret = -1;
    s32 threads = 0;
    Handle debug;
        
    if (file_or_directory_exists("sdmc:/SaltySD/flags/disable.flag") == true) {
        SaltySD_printf("SaltySD: Detected disable.flag, aborting bootstrap...\n");
        return;
    }
    
    if (lastAppPID == -1) {
        already_hijacking = false;
    }

    if (already_hijacking)
    {
        SaltySD_printf("SaltySD: PID %d spawned before last hijack finished bootstrapping! Ignoring...\n", pid);
        return;
    }
    
    already_hijacking = true;
    Result rc = svcDebugActiveProcess(&debug, pid);
    if (R_FAILED(rc)) {
        SaltySD_printf("SaltySD: PID %d is not allowing debugging, error 0x%x, aborting...\n", pid, rc);
        goto abort_bootstrap;
    }

    bool isA64 = true;
    DebugEvent event;
    while (1)
    {
        ret = svcGetDebugEvent(&event, debug);

        switch(ret) {
            case 0:
                break;
            case 0xE401:
                SaltySD_printf("SaltySD: PID %d is not allowing debugging, aborting...\n", pid);
                goto abort_bootstrap;
            case 0x8C01:
                SaltySD_printf("SaltySD: PID %d svcGetDebugevent: end of events...\n", pid);
                break;
            default:
                SaltySD_printf("SaltySD: PID %d svcGetDebugevent returned %x, breaking...\n", pid, ret);
                break;
        }
        if (ret)
            break;

        if (!check) {
            TIDnow = event.info.create_process.program_id;
            exception = 0;
            renameCheatsFolder();
        }

        if (event.type == DebugEventType_CreateProcess)
        {

            if (event.info.create_process.program_id <= 0x010000000000FFFF)
            {
                SaltySD_printf("SaltySD: %s TID %016lx is a system application, aborting bootstrap...\n", event.info.create_process.name, event.info.create_process.program_id);
                goto abort_bootstrap;
            }
            if (event.info.create_process.program_id > 0x01FFFFFFFFFFFFFF || (event.info.create_process.program_id & 0x1F00) != 0)
            {
                SaltySD_printf("SaltySD: %s TID %016lx is a homebrew application, aborting bootstrap...\n", event.info.create_process.name, event.info.create_process.program_id);
                goto abort_bootstrap;
            }
            uintptr_t shmem = (uintptr_t)shmemGetAddr(&_sharedMemory);
            if (shmem) {
                memset((void*)(shmem+4), 0, shmem_size-4);
            }
            char* hbloader = "hbloader";
            if (strcasecmp(event.info.create_process.name, hbloader) == 0)
            {
                SaltySD_printf("SaltySD: Detected title replacement mode, aborting bootstrap...\n");
                goto abort_bootstrap;
            }
            
            FILE* except = fopen("sdmc:/SaltySD/exceptions.txt", "r");
            if (except) {
                char exceptions[20];
                char titleidnumX[20];

                npf_snprintf(titleidnumX, sizeof titleidnumX, "X%016lx", event.info.create_process.program_id);
                while (fgets(exceptions, sizeof(exceptions), except)) {
                    titleidnumX[0] = 'X';
                    if (!strncasecmp(exceptions, titleidnumX, 17)) {
                        SaltySD_printf("SaltySD: %s TID %016lx is forced in exceptions.txt, aborting bootstrap...\n", event.info.create_process.name, event.info.create_process.program_id);
                        fclose(except);
                        goto abort_bootstrap;
                    }
                    else {
                        titleidnumX[0] = 'R';
                        if (!strncasecmp(exceptions, titleidnumX, 17)) {
                            if (isModInstalled()) {
                                SaltySD_printf("SaltySD: %s TID %016lx is in exceptions.txt as romfs excluded, aborting bootstrap...\n", event.info.create_process.name, event.info.create_process.program_id);
                                fclose(except);
                                goto abort_bootstrap;
                            }
                            else SaltySD_printf("SaltySD: %s TID %016lx is in exceptions.txt as romfs excluded, but no romfs mod was detected...\n", event.info.create_process.name, event.info.create_process.program_id);
                        }
                        else if (!strncasecmp(exceptions, &titleidnumX[1], 16)) {
                            SaltySD_printf("SaltySD: %s TID %016lx is in exceptions.txt, aborting loading plugins...\n", event.info.create_process.name, event.info.create_process.program_id);
                            exception = 0x1;
                        }
                    }
                }
                fclose(except);
            }
            CreateProcessFlags ProcessFlags;
            memcpy(&ProcessFlags, &event.info.create_process.flags, 4);
            SaltySD_printf("SaltySD: found valid CreateProcess event:\n");
            SaltySD_printf("		 tid %016lx pid %lu\n", event.info.create_process.program_id, event.info.create_process.process_id);
            SaltySD_printf("		 name %s\n", event.info.create_process.name);
            SaltySD_printf("		 isA64 %01x addrSpace %01x enableDebug %01x\n", ProcessFlags.is_64bit, ProcessFlags.address_space, ProcessFlags.enable_debug);
            SaltySD_printf("		 enableAslr %01x poolPartition %01x\n", ProcessFlags.enable_aslr, ProcessFlags.pool_partition);
            SaltySD_printf("		 exception 0x%p\n", event.info.create_process.user_exception_context_address);
            isA64 = ProcessFlags.is_64bit;
        }
        else
        {
            SaltySD_printf("SaltySD: debug event %x, passing...\n", event.type);
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

static Result handleServiceCmd(int cmd)
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
        
        SaltySD_printf("SaltySD: cmd 1 handler, proc handle %x, heap %lx, path %s\n", proc, heap, name);
        
        char* path = malloc(96);
        u32 elf_size = 0;
        bool arm32 = false;
        if (!strncmp(name, "saltysd_core32.elf", 18)) arm32 = true;

        npf_snprintf(path, 96, "sdmc:/SaltySD/plugins/%s", name);
        FILE* f = fopen(path, "rb");
        if (!f)
        {
            npf_snprintf(path, 96, "sdmc:/SaltySD/%s", name);
            f = fopen(path, "rb");
        }

        if (!f)
        {
            SaltySD_printf("SaltySD: failed to load plugin `%s'!\n", name);
            elf_size = 0;
        }
        else
        {
            fseek(f, 0, SEEK_END);
            elf_size = ftell(f);
            fseek(f, 0, SEEK_SET);
            
            SaltySD_printf("SaltySD: loading %s, size 0x%x\n", path, elf_size);
        }
        free(path);
        
        u64 new_start = 0, new_size = 0;
        if (f && elf_size) {
            if (!arm32)
                ret = load_elf_proc(proc, r.Pid, heap, &new_start, &new_size, f, elf_size);
            else ret = load_elf32_proc(proc, r.Pid, (u32)heap, (u32*)&new_start, (u32*)&new_size, f, elf_size);
            if (ret) SaltySD_printf("Load_elf arm32: %d, ret: 0x%x\n", arm32, ret);
        }
        else
            ret = MAKERESULT(MODULE_SALTYSD, 1);

        svcCloseHandle(proc);
        
        if (f)
            fclose(f);
        
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

        SaltySD_printf("SaltySD: cmd 3 handler, memcpy(%lx, %lx, %lx)\n", to, from, size);

        return 0;
    }
    else if (cmd == 4) // GetSDCard
    {		
        ipcSendHandleCopy(&c, sdcard);

        SaltySD_printf("SaltySD: cmd 4 handler\n"); 
    }
    else if (cmd == 5) // Log
    {

        IpcParsedCommand r = {0};
        ipcParse(&r);

        struct {
            u64 magic;
            u64 command;
            char log[64];
            u32 reserved[2];
        } *resp = r.Raw;

        SaltySD_printf(resp->log);

        SaltySD_printf("SaltySD: cmd 5 handler\n");

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
        else if (new_size < (shmem_size - reservedSharedMemory)) {
            if (shmemGetAddr(&_sharedMemory)) {
                if (!reservedSharedMemory) {
                    uintptr_t shmem = (uintptr_t)shmemGetAddr(&_sharedMemory);
                    if (shmem) memset((void*)(shmem+4), 0, shmem_size-4);
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
            SaltySD_printf("SaltySD: cmd 6 failed. Not enough free space. Left: %d\n", (shmem_size - reservedSharedMemory));
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
            u32 is720p;
            u32 reserved[2];
        } *resp = r.Raw;

        setAllowedDockedRefreshRatesIPC(resp -> refreshRate, (bool)resp->is720p);
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
    else if (cmd == 19) // SetDisplaySyncRefreshRate60WhenOutOfFocus
    {
        IpcParsedCommand r = {0};
        ipcParse(&r);

        struct {
            u64 magic;
            u64 cmd_id;
            u32 value;
            u32 inDocked;
            u64 reserved;
        } *resp = r.Raw;

        bool inDocked = (bool)(resp -> inDocked);
        if (inDocked) {
            displaySyncDockedOutOfFocus60 = (bool)(resp -> value);
        }
        else {
            displaySyncOutOfFocus60 = (bool)(resp -> value);
            if (displaySyncOutOfFocus60) {
                FILE* file = fopen("sdmc:/SaltySD/flags/displaysync_outoffocus.flag", "wb");
                fclose(file);
                SaltySD_printf("SaltySD: cmd 19 handler -> %d\n", displaySyncOutOfFocus60);
            }
            else {
                remove("sdmc:/SaltySD/flags/displaysync_outoffocus.flag");
                SaltySD_printf("SaltySD: cmd 19 handler -> %d\n", displaySyncOutOfFocus60);
            }
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