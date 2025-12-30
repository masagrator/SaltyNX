#include <switch.h>
#include <dirent.h>
#include "ipc.h"
#include "svc_extra.h"
#include "loadelf.h"
#include "useful.h"
#include <stdlib.h>
extern "C" {
    #include "display_refresh_rate.h"
}
#include <charconv>

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

static bool hijack_bootstrap(Handle* debug, u64 pid, u64 tid, bool isA64)
{
    ThreadContext context;
    Result ret;

    reservedSharedMemory = 0;
    
    ret = svcGetDebugThreadContext(&context, *debug, tid, RegisterGroup_All);
    if (ret)
    {
        SaltySD_printf("SaltyNX: svcGetDebugThreadContext returned %x, aborting...\n", ret);
        
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
            SaltySD_printf("SaltyNX: SaltySD/saltysd_bootstrap.elf not found, aborting...\n");
            svcCloseHandle(*debug);
            return false;
        }
        fseek(file, 0, 2);
        size_t saltysd_bootstrap_elf_size = ftell(file);
        fseek(file, 0, 0);
        u8* elf = (u8*)malloc(saltysd_bootstrap_elf_size);
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
        SaltySD_printf("SaltyNX: svcSetDebugThreadContext returned %x!\n", ret);
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

void abort_bootstrap(Handle debug) {
    if (check) renameCheatsFolder();
                
    already_hijacking = false;
    svcCloseHandle(debug);
}

extern "C" void hijack_pid(u64 pid)
{
    Result ret = -1;
    s32 threads = 0;
    Handle debug;
        
    if (file_or_directory_exists("sdmc:/SaltySD/flags/disable.flag") == true) {
        SaltySD_printf("SaltyNX: Detected disable.flag, aborting bootstrap...\n");
        return;
    }
    
    if (lastAppPID == -1) {
        already_hijacking = false;
    }

    if (already_hijacking)
    {
        SaltySD_printf("SaltyNX: PID %d spawned before last hijack finished bootstrapping! Ignoring...\n", pid);
        return;
    }
    
    already_hijacking = true;
    Result rc = svcDebugActiveProcess(&debug, pid);
    if (R_FAILED(rc)) {
        SaltySD_printf("SaltyNX: PID %d is not allowing debugging, error 0x%x, aborting...\n", pid, rc);
        return abort_bootstrap(debug);
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
                SaltySD_printf("SaltyNX: PID %d is not allowing debugging, aborting...\n", pid);
                return abort_bootstrap(debug);
            case 0x8C01:
                SaltySD_printf("SaltyNX: PID %d svcGetDebugevent: end of events...\n", pid);
                break;
            default:
                SaltySD_printf("SaltyNX: PID %d svcGetDebugevent returned %x, breaking...\n", pid, ret);
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
                SaltySD_printf("SaltyNX: %s TID %016lx is a system application, aborting bootstrap...\n", event.info.create_process.name, event.info.create_process.program_id);
                return abort_bootstrap(debug);
            }
            if (event.info.create_process.program_id > 0x01FFFFFFFFFFFFFF || (event.info.create_process.program_id & 0x1F00) != 0)
            {
                SaltySD_printf("SaltyNX: %s TID %016lx is a homebrew application, aborting bootstrap...\n", event.info.create_process.name, event.info.create_process.program_id);
                return abort_bootstrap(debug);
            }
            uintptr_t shmem = (uintptr_t)shmemGetAddr(&_sharedMemory);
            if (shmem) {
                memset((void*)(shmem+4), 0, shmem_size-4);
            }
            if (strcasecmp(event.info.create_process.name, "hbloader") == 0)
            {
                SaltySD_printf("SaltyNX: Detected title replacement mode, aborting bootstrap...\n");
                return abort_bootstrap(debug);
            }
            
            FILE* except = fopen("sdmc:/SaltySD/exceptions.txt", "r");
            if (except) {
                char exceptions[20];
                char titleidnum[17];
                npf_snprintf(titleidnum, sizeof titleidnum, "%016lx", event.info.create_process.program_id);

                while (fgets(exceptions, sizeof(exceptions), except)) {
                    char firstChara = exceptions[0];
                    bool ForcedAbort = firstChara == 'X';
                    bool romfsExcluded = firstChara == 'R';
                    if (ForcedAbort || romfsExcluded) {
                        if (!strncasecmp(&exceptions[1], titleidnum, 16)) {
                            if (ForcedAbort) {
                                SaltySD_printf("SaltyNX: %s TID %016lx is forced in exceptions.txt, aborting bootstrap...\n", event.info.create_process.name, event.info.create_process.program_id);
                                fclose(except);
                                return abort_bootstrap(debug);
                            }
                            else if (isModInstalled()) {
                                SaltySD_printf("SaltyNX: %s TID %016lx is in exceptions.txt as romfs excluded, aborting bootstrap...\n", event.info.create_process.name, event.info.create_process.program_id);
                                fclose(except);
                                return abort_bootstrap(debug);
                            }
                            else SaltySD_printf("SaltyNX: %s TID %016lx is in exceptions.txt as romfs excluded, but no romfs mod was detected...\n", event.info.create_process.name, event.info.create_process.program_id);
                        }
                    }
                    else if (!strncasecmp(exceptions, titleidnum, 16)) {
                        SaltySD_printf("SaltyNX: %s TID %016lx is in exceptions.txt, aborting loading plugins...\n", event.info.create_process.name, event.info.create_process.program_id);
                        exception = 0x1;    
                    }
                }
                fclose(except);
            }
            CreateProcessFlags ProcessFlags;
            memcpy(&ProcessFlags, &event.info.create_process.flags, 4);
            SaltySD_printf("SaltyNX: found valid CreateProcess event:\n");
            SaltySD_printf("		 tid %016lx pid %lu\n", event.info.create_process.program_id, event.info.create_process.process_id);
            SaltySD_printf("		 name %s\n", event.info.create_process.name);
            SaltySD_printf("		 isA64 %01x addrSpace %01x enableDebug %01x\n", ProcessFlags.is_64bit, ProcessFlags.address_space, ProcessFlags.enable_debug);
            SaltySD_printf("		 enableAslr %01x poolPartition %01x\n", ProcessFlags.enable_aslr, ProcessFlags.pool_partition);
            SaltySD_printf("		 exception 0x%p\n", event.info.create_process.user_exception_context_address);
            isA64 = ProcessFlags.is_64bit;
        }
        else
        {
            SaltySD_printf("SaltyNX: debug event %x, passing...\n", event.type);
            continue;
        }
    }

    u64 threadid = 0;

    uint64_t tick_start = svcGetSystemTick();
    do {
        if (svcGetSystemTick() - tick_start > systemtickfrequency * 30) {
            SaltySD_printf("SaltyNX: Waiting for main thread timeout! Aborting...\n");
            return abort_bootstrap(debug);
        }
        ret = svcGetThreadList(&threads, &threadid, 1, debug);
        svcSleepThread(10000);
    } while (!threads);

    uint64_t passed_time_in_ticks = svcGetSystemTick() - tick_start;
    
    renameCheatsFolder();

    if (passed_time_in_ticks > systemtickfrequency * 10) {
        SaltySD_printf("SaltyNX: Waiting for main thread: %d ms, longer than normal!\n", passed_time_in_ticks / (systemtickfrequency / 1000));
    }
    
    if (hijack_bootstrap(&debug, pid, threadid, isA64)) {
        lastAppPID = pid;
        
        LoaderModuleInfo module_infos[2] = {0};
        s32 module_infos_count = 0;
        ret = ldrDmntGetProcessModuleInfo(pid, module_infos, 2, &module_infos_count);
        if (R_SUCCEEDED(ret)) {
            BIDnow = __builtin_bswap64(*(uint64_t*)&module_infos[1].build_id[0]);
            SaltySD_printf("SaltyNX: BID: %016lX\n", BIDnow);
            ret = 0;
        }
        else SaltySD_printf("SaltyNX: cmd 8 ldrDmntGetProcessModuleInfo failed! RC: 0x%X\n", ret);
    }
    else {
        already_hijacking = false;
    }

    return;
}

static void saltynxLoadELF(IpcCommand* c, void* arg, Handle proc, u64 Pid) {

    Result ret = 0;

    struct ipc {
        u64 heap;
        char name[64];
    } *resp = (ipc*)arg;

    u64 heap = resp->heap;
    char name[65];
    name[64] = 0;
    memcpy(name, resp->name, 64);
    
    SaltySD_printf("SaltyNX: cmd LoadELF, proc handle %x, heap 0x%lx, path %s\n", proc, heap, name);
    
    char* path = (char*)malloc(96);
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
        SaltySD_printf("SaltyNX: failed to load plugin `%s'!\n", name);
        elf_size = 0;
    }
    else
    {
        fseek(f, 0, SEEK_END);
        elf_size = ftell(f);
        fseek(f, 0, SEEK_SET);
        
        SaltySD_printf("SaltyNX: loading %s, size 0x%x\n", path, elf_size);
    }
    free(path);
    
    u64 new_start = 0, new_size = 0;
    if (f && elf_size) {
        if (!arm32)
            ret = load_elf_proc(proc, Pid, heap, &new_start, &new_size, f, elf_size);
        else ret = load_elf32_proc(proc, Pid, (u32)heap, (u32*)&new_start, (u32*)&new_size, f, elf_size);
        if (ret) SaltySD_printf("SaltyNX: Load_elf arm32: %d, ret: 0x%x\n", arm32, ret);
    }
    else
        ret = MAKERESULT(MODULE_SALTYSD, 1);

    svcCloseHandle(proc);
    
    if (f)
        fclose(f);

    struct opc {
        u64 magic;
        u64 result;
        u64 new_addr;
        u64 new_size;
    } *raw;
    raw = (opc*)ipcPrepareHeader(c, sizeof(*raw));
    raw->magic = SFCO_MAGIC;

    raw->new_addr = new_start;
    raw->new_size = new_size;
    raw->result = ret;

    return;
}

static void saltynxRestoreBootstrapCode(u64 Pid) {
    Handle debug;
    Result ret = svcDebugActiveProcess(&debug, Pid);
    if (!ret)
    {
        ret = restore_elf_debug(debug);
    }
    
    // Bootstrapping is done, we can handle another process now.
    already_hijacking = false;
    svcCloseHandle(debug);
    if (R_FAILED(ret)) SaltySD_printf("SaltyNX: cmd RestoreBootstrapCode failed: 0x%x\n", ret);
}

static void saltynxMemcpy(IpcCommand* c, void* arg, u64 Pid) {

    struct opc {
        u64 magic;
        u64 result;
        u64 reserved[2];
    } *raw;
    raw = (opc*)ipcPrepareHeader(c, sizeof(*raw));
    raw->magic = SFCO_MAGIC;

    Result ret = 0;
    struct ipc {
        u64 to;
        u64 from;
        u64 size;
    } *resp = (ipc*)arg;
    
    u64 to = resp->to;
    u64 from = resp->from;
    u64 size = resp->size;
    
    Handle debug;
    ret = svcDebugActiveProcess(&debug, Pid);
    if (!ret)
    {
        u8* tmp = (u8*)malloc(size);

        ret = svcReadDebugProcessMemory(tmp, debug, from, size);
        if (!ret)
            ret = svcWriteDebugProcessMemory(debug, tmp, to, size);

        free(tmp);
        
        svcCloseHandle(debug);
    }
    raw->result = ret;
    SaltySD_printf("SaltyNX: cmd Memcpy(%lx, %lx, %lx)\n", to, from, size);
}

static void saltynxCheckIfSharedMemoryAvailable(IpcCommand* c, void* arg) {

    struct opc {
        u64 magic;
        u64 result;
        u64 offset;
        u64 reserved;
    } *raw;
    raw = (opc*)ipcPrepareHeader(c, sizeof(*raw));
    raw->magic = SFCO_MAGIC;

    struct ipc {
        u64 size;
    } *resp = (ipc*)arg;
    u64 new_size = resp->size;
    
    if (!new_size) {
        SaltySD_printf("SaltyNX: cmd CheckIfSharedMemoryAvailable failed. Wrong size.");
        raw->result = 0xFFE;
    }
    else if (new_size < (shmem_size - reservedSharedMemory)) {
        if (shmemGetAddr(&_sharedMemory)) {
            if (!reservedSharedMemory) {
                uintptr_t shmem = (uintptr_t)shmemGetAddr(&_sharedMemory);
                if (shmem) memset((void*)(shmem+4), 0, shmem_size-4);
            }
            raw->offset = reservedSharedMemory;
            reservedSharedMemory += new_size;
            if (reservedSharedMemory % 4 != 0) {
                reservedSharedMemory += (4 - (reservedSharedMemory % 4));
            }
            raw->result = 0;
        }
        else {
            SaltySD_printf("SaltyNX: cmd CheckIfSharedMemoryAvailable failed. shmemMap error.");
            raw->result = 0xFFE;
        }
    }
    else {
        SaltySD_printf("SaltyNX: cmd CheckIfSharedMemoryAvailable failed. Not enough free space. Left: %d\n", (shmem_size - reservedSharedMemory));
        raw->result = 0xFFE;
    }
    return;
}

static Result saltynxSetDisplayRefreshRate(void* arg) {

    struct ipc {
        u64 refreshRate;
    } *resp = (ipc*)arg;

    u64 refreshRate_temp = resp -> refreshRate;

    SaltySD_printf("SaltyNX: cmd SetDisplayRefreshRate -> %d\n", refreshRate_temp);

    if (SetDisplayRefreshRate(refreshRate_temp)) {
        refreshRate = refreshRate_temp;
        return 0;
    }
    return 0x1234;
}

static void saltynxSetDisplaySync(void* arg) {

    struct ipc {
        u64 value;
    } *resp = (ipc*)arg;

    displaySync = (bool)(resp -> value);
    if (displaySync) {
        FILE* file = fopen("sdmc:/SaltySD/flags/displaysync.flag", "wb");
        fclose(file);
    }
    else {
        remove("sdmc:/SaltySD/flags/displaysync.flag");
    }
    SaltySD_printf("SaltyNX: cmd SetDisplaySync -> %d\n", displaySync);
    return;
}

static void saltynxGetBID(IpcCommand* c) {
    struct opc {
        u64 magic;
        u64 BID;
    } *raw;
    raw = (opc*)ipcPrepareHeader(c, sizeof(*raw));
    raw->magic = SFCO_MAGIC;
    raw->BID = BIDnow;
    return;
}

static void saltynxException(IpcCommand* c) {
    struct opc {
        u64 magic;
        u64 result;
        u64 reserved[2];
    } *raw;

    raw = (opc*)ipcPrepareHeader(c, sizeof(*raw));

    raw->magic = SFCO_MAGIC;
    raw->result = exception;

    return;
}

static void saltynxGetDisplayRefreshRate(IpcCommand* c) {
    struct opc {
        u64 magic;
        u64 result;
        u64 refreshRate;
        u64 reserved[2];
    } *raw;

    raw = (opc*)ipcPrepareHeader(c, sizeof(*raw));

    raw->magic = SFCO_MAGIC;
    uint32_t temp_refreshRate = 0;
    raw->result = !GetDisplayRefreshRate(&temp_refreshRate, false);
    raw->refreshRate = temp_refreshRate;
    return;
}

static void saltynxSetAllowedDockedRefreshRates(void* arg) {
    struct ipc {
        u32 refreshRate;
        u32 is720p;
    } *resp = (ipc*)arg;

    setAllowedDockedRefreshRatesIPC(resp -> refreshRate, (bool)resp->is720p);
    SaltySD_printf("SaltyNX: cmd SetAllowedDockedRefreshRates -> 0x%x, is720p: %d\n", resp -> refreshRate, resp->is720p);
    return;
}

static void saltynxSetDontForce60InDocked(void* arg) {
    struct ipc {
        u64 force;
    } *resp = (ipc*)arg;

    dontForce60InDocked = (bool)(resp -> force);
    SaltySD_printf("SaltyNX: cmd SetDontForce60InDocked -> %d\n", dontForce60InDocked);

    return;
}

static void saltynxSetMatchLowestRR(void* arg) {
    struct ipc {
        u64 force;
    } *resp = (ipc*)arg;

    matchLowestDocked = (bool)(resp -> force);
    SaltySD_printf("SaltyNX: cmd SetMatchLowestRR -> %d\n", matchLowestDocked);

    return;
}

static void saltynxGetDockedHighestRefreshRate(IpcCommand* c) {    
    struct opc {
        u64 magic;
        u64 result;
        u32 refreshRate;
        u32 linkRate;
        u64 reserved;
    } *raw;

    raw = (opc*)ipcPrepareHeader(c, sizeof(*raw));

    raw->magic = SFCO_MAGIC;
    raw->result = 0;
    raw->refreshRate = dockedHighestRefreshRate;
    raw->linkRate = dockedLinkRate;

    return;
}

static void saltynxIsPossiblyRetroRemake(IpcCommand* c) {
    struct opc {
        u64 magic;
        u64 result;
        u64 value;
        u64 reserved;
    } *raw;

    raw = (opc*)ipcPrepareHeader(c, sizeof(*raw));

    raw->magic = SFCO_MAGIC;
    raw->result = 0;
    raw->value = isPossiblySpoofedRetro;

    return;
}

static void saltynxSetDisplaySyncDocked(void* arg) {
    struct ipc {
        u64 value;
    } *resp = (ipc*)arg;

    displaySyncDocked = (bool)(resp -> value);
    if (displaySyncDocked) {
        FILE* file = fopen("sdmc:/SaltySD/flags/displaysyncdocked.flag", "wb");
        fclose(file);
    }
    else {
        remove("sdmc:/SaltySD/flags/displaysyncdocked.flag");
    }
    SaltySD_printf("SaltyNX: cmd SetDisplaySyncDocked handler -> %d\n", displaySyncDocked);
    return;
}

static void saltynxSetDisplaySyncRefreshRate60WhenOutOfFocus(void* arg) {
    struct ipc {
        u32 value;
        u32 inDocked;
    } *resp = (ipc*)arg;

    bool inDocked = (bool)(resp -> inDocked);
    if (inDocked) {
        displaySyncDockedOutOfFocus60 = (bool)(resp -> value);
    }
    else {
        displaySyncOutOfFocus60 = (bool)(resp -> value);
        if (displaySyncOutOfFocus60) {
            FILE* file = fopen("sdmc:/SaltySD/flags/displaysync_outoffocus.flag", "wb");
            fclose(file);
        }
        else {
            remove("sdmc:/SaltySD/flags/displaysync_outoffocus.flag");
        }
    }
    SaltySD_printf("SaltyNX: cmd SetDisplaySyncRefreshRate60WhenOutOfFocus -> %d, inDocked: %d\n", displaySyncOutOfFocus60, inDocked);

    return;
}

static void saltynxLog(void* arg) {
    struct ipc {
        char log[64];
    } *resp = (ipc*)arg;

    char text[65];
    text[64] = 0;
    memcpy(text, resp->log, 64);
    SaltySD_printf(text);
}

static consteval u32 getVersion() {
    const char version[] = APP_VERSION;
    u32 return_value = APP_VERSION[0] - 0x30;
    u8 value;
    std::from_chars(&version[2], &version[sizeof(version)], value);
    return_value |= ((u32)value << 8);
    const size_t offset = value >= 10 ? 5 : 4;
    std::from_chars(&version[offset], &version[sizeof(version)], value);
    return_value |= ((u32)value << 16);
    return return_value;
}

void saltynxGetSaltynxVersion(IpcCommand* c) {
    
    struct opc {
        u64 magic;
        u64 result;
        u8 major;
        u8 minor;
        u8 micro;
    } *raw;

    raw = (opc*)ipcPrepareHeader(c, sizeof(*raw));

    raw->magic = SFCO_MAGIC;
    raw->result = 0;
    u32 version = getVersion();
    raw->major = (u8)version;
    raw->minor = (u8)(version >> 8);
    raw->micro = (u8)(version >> 16);
}

typedef enum {
    saltynxServiceIpcCmd_EndSession,
    saltynxServiceIpcCmd_LoadELF,
    saltynxServiceIpcCmd_RestoreBootstrapCode,
    saltynxServiceIpcCmd_Memcpy,
    saltynxServiceIpcCmd_GetSDCard,
    saltynxServiceIpcCmd_Log,
    saltynxServiceIpcCmd_CheckIfSharedMemoryAvailable,
    saltynxServiceIpcCmd_GetSharedMemoryHandle,
    saltynxServiceIpcCmd_GetBID,
    saltynxServiceIpcCmd_Exception,
    saltynxServiceIpcCmd_GetDisplayRefreshRate,
    saltynxServiceIpcCmd_SetDisplayRefreshRate,
    saltynxServiceIpcCmd_SetDisplaySync,
    saltynxServiceIpcCmd_SetAllowedDockedRefreshRates,
    saltynxServiceIpcCmd_SetDontForce60InDocked,
    saltynxServiceIpcCmd_SetMatchLowestRR,
    saltynxServiceIpcCmd_GetDockedHighestRefreshRate,
    saltynxServiceIpcCmd_IsPossiblyRetroRemake,
    saltynxServiceIpcCmd_SetDisplaySyncDocked,
    saltynxServiceIpcCmd_SetDisplaySyncRefreshRate60WhenOutOfFocus,
    saltynxServiceIpcCmd_GetSaltynxVersion
} saltynxServiceIpcCmd;

static Result handleServiceCmd(int cmd)
{
    Result ret = 0;

    // Send reply
    IpcCommand c;
    ipcInitialize(&c);
    ipcSendPid(&c);
    IpcParsedCommand r;
    ipcParse(&r);

    ssize_t rawSize = r.RawSize-0x10;
    if (rawSize < 0) rawSize = 0;
    void* raw_r = malloc(rawSize);

    if (rawSize > 0) memcpy(raw_r, (void*)((uintptr_t)(r.Raw) + 0x10), rawSize);
    
    SaltySD_printf("SaltyNX: cmd %d handler\n", cmd);

    switch(cmd) {
        case saltynxServiceIpcCmd_EndSession:
            should_terminate = true;
            break;

        case saltynxServiceIpcCmd_LoadELF:
            saltynxLoadELF(&c, raw_r, r.Handles[0], r.Pid);
            return 0;

        case saltynxServiceIpcCmd_RestoreBootstrapCode:
            saltynxRestoreBootstrapCode(r.Pid);
            break;

        case saltynxServiceIpcCmd_Memcpy:
            saltynxMemcpy(&c, raw_r, r.Pid);
            return 0;

        case saltynxServiceIpcCmd_GetSDCard:
            ipcSendHandleCopy(&c, sdcard);
            break;

        case saltynxServiceIpcCmd_Log:
            saltynxLog(raw_r);
            break;

        case saltynxServiceIpcCmd_CheckIfSharedMemoryAvailable:
            saltynxCheckIfSharedMemoryAvailable(&c, raw_r);
            return 0;

        case saltynxServiceIpcCmd_GetSharedMemoryHandle:
            ipcSendHandleCopy(&c, _sharedMemory.handle);
            break;

        case saltynxServiceIpcCmd_GetBID:
            saltynxGetBID(&c);
            return 0;

        case saltynxServiceIpcCmd_Exception:
            saltynxException(&c);
            return 0;

        case saltynxServiceIpcCmd_GetDisplayRefreshRate:
            saltynxGetDisplayRefreshRate(&c);
            return 0;

        case saltynxServiceIpcCmd_SetDisplayRefreshRate:
            saltynxSetDisplayRefreshRate(raw_r);
            break;

        case saltynxServiceIpcCmd_SetDisplaySync:
            saltynxSetDisplaySync(raw_r);
            break;

        case saltynxServiceIpcCmd_SetAllowedDockedRefreshRates:
            saltynxSetAllowedDockedRefreshRates(raw_r);
            break;

        case saltynxServiceIpcCmd_SetDontForce60InDocked:
            saltynxSetDontForce60InDocked(raw_r);
            break;

        case saltynxServiceIpcCmd_SetMatchLowestRR:
            saltynxSetMatchLowestRR(raw_r);
            break;

        case saltynxServiceIpcCmd_GetDockedHighestRefreshRate:
            saltynxGetDockedHighestRefreshRate(&c);
            return 0;

        case saltynxServiceIpcCmd_IsPossiblyRetroRemake:
            saltynxIsPossiblyRetroRemake(&c);
            return 0;

        case saltynxServiceIpcCmd_SetDisplaySyncDocked:
            saltynxSetDisplaySyncDocked(raw_r);
            break;

        case saltynxServiceIpcCmd_SetDisplaySyncRefreshRate60WhenOutOfFocus:
            saltynxSetDisplaySyncRefreshRate60WhenOutOfFocus(raw_r);
            break;
        
        case saltynxServiceIpcCmd_GetSaltynxVersion:
            saltynxGetSaltynxVersion(&c);
            return 0;
        
        default:
            ret = 0xEE01;
    }

    free(raw_r);
    
    struct opc {
        u64 magic;
        u64 result;
        u64 reserved[2];
    } *raw;

    raw = (opc*)ipcPrepareHeader(&c, sizeof(*raw));

    raw->magic = SFCO_MAGIC;
    raw->result = ret;
    
    return ret;
}

extern "C" void serviceThread(void* buf)
{
    Result ret;
    SaltySD_printf("SaltyNX: accepting service calls\n");
    should_terminate = false;

    while (1)
    {
        Handle session;
        ret = svcAcceptSession(&session, saltyport);
        if (ret && ret != 0xf201)
        {
            SaltySD_printf("SaltyNX: svcAcceptSession returned %x\n", ret);
        }
        else if (!ret)
        {
            SaltySD_printf("SaltyNX: session %x being handled\n", session);

            int handle_index;
            Handle replySession = 0;
            while (1)
            {
                ret = svcReplyAndReceive(&handle_index, &session, 1, replySession, UINT64_MAX);
                
                if (should_terminate || ret) break;
                                
                IpcParsedCommand r;
                ipcParse(&r);

                struct ipc {
                    u64 magic;
                    u64 command;
                    u64 reserved[2];
                } *resp = (ipc*)r.Raw;

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
    
    SaltySD_printf("SaltyNX: done accepting service calls\n");
}