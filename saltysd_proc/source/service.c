#include "shared.h"
#include "ipc.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "useful.h"
#include "loadelf.h"
#include "display_refresh_rate.h"
#include <dirent.h>
#include <errno.h>

#define SERVICE_LOG(fmt, ...) \
    SaltySD_printf("SaltyNX: [%s] " fmt "\n", __func__, ##__VA_ARGS__)

#define SALTYSD_RESULT(id, val) MAKERESULT(MODULE_SALTYSD, 9000 + ((id) * 10) + (val))

#if defined(_DIRENT_HAVE_D_NAMLEN) || defined(_DIRENT_HAVE_D_RECLEN) || defined(_DIRENT_HAVE_D_OFF)
#error "Wrong DIR structure detected!"
#endif
#if !defined(_DIRENT_HAVE_D_TYPE)
#error "Wrong DIR structure detected!"
#endif
static_assert(sizeof(ino_t) == 2);

struct FileId {
    int id;
    FILE* file;
};

struct DirId {
    int id;
    DIR* dir;
};

size_t openedFilesAmount = 0;
struct FileId openedFilesArray[FOPEN_MAX-1] = {0};
size_t openedDirsAmount = 0;
struct DirId openedDirsArray[OPEN_MAX-1] = {0};

typedef enum {
    handleService_EndSession,
    handleService_LoadELF,
    handleService_RestoreBootstrapCode,
    handleService_Memcpy,
    handleService_GetSDCard,
    handleService_Log,
    handleService_CheckIfSharedMemoryAvailable,
    handleService_GetSharedMemoryHandle,
    handleService_GetBID,
    handleService_Exception,
    handleService_GetDisplayRefreshRate,
    handleService_SetDisplayRefreshRate,
    handleService_SetDisplaySync,
    handleService_SetAllowedDockedRefreshRates,
    handleService_SetDontForce60InDocked,
    handleService_SetMatchLowestRR,
    handleService_GetDockedHighestRefreshRate,
    handleService_IsPossiblyRetroRemake,
    handleService_SetDisplaySyncDocked,
    handleService_SetDisplaySyncRefreshRate60WhenOutOfFocus,
    handleService_SdcardFopen,
    handleService_SdcardFread,
    handleService_SdcardFclose,
    handleService_SdcardFseek,
    handleService_SdcardFtell,
    handleService_SdcardRemove,
    handleService_SdcardFwrite,
    handleService_SdcardOpendir,
    handleService_SdcardMkdir,
    handleService_SdcardReaddir,
    handleService_SdcardClosedir
} handleService;

static Result serviceEndSession() {
    should_terminate = true;
    SERVICE_LOG();
    return 0;
}

static Result serviceLoadELF(IpcCommand* c) {
    Result ret = 0;
    IpcParsedCommand r = {0};
    ipcParse(&r);

    if (r.HasPid != true || r.Pid != PIDnow) {
        SERVICE_LOG("This call is reserved only for Core.");
        return SALTYSD_RESULT(handleService_LoadELF, 3); // This call is reserved only for Core
    }

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
    
    SERVICE_LOG("proc handle %x, heap %lx, path %s", proc, heap, name);
    
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
        SERVICE_LOG("failed to load plugin `%s'!", name);
        elf_size = 0;
    }
    else
    {
        fseek(f, 0, SEEK_END);
        elf_size = ftell(f);
        fseek(f, 0, SEEK_SET);
        
        SERVICE_LOG("loading %s, size 0x%x", path, elf_size);
    }
    free(path);
    
    u64 new_start = 0, new_size = 0;
    if (f && elf_size) {
        if (!arm32)
            ret = load_elf_proc(proc, r.Pid, heap, &new_start, &new_size, f, elf_size);
        else ret = load_elf32_proc(proc, r.Pid, (u32)heap, (u32*)&new_start, (u32*)&new_size, f, elf_size);
        if (ret) SERVICE_LOG("Load_elf arm32: %d, ret: 0x%x", arm32, ret);
    }
    else
        ret = SALTYSD_RESULT(handleService_LoadELF, 1);

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

    if (R_SUCCEEDED(ret)) SERVICE_LOG("new_addr to %lx, %x", new_start, ret);

    raw = ipcPrepareHeader(c, sizeof(*raw));

    raw->magic = SFCO_MAGIC;
    raw->result = ret;
    raw->new_addr = new_start;
    raw->new_size = new_size;
    
    return 0;
}

static Result serviceRestoreBootstrapCode() {
    Result ret = 0;
    IpcParsedCommand r = {0};
    ipcParse(&r);

    if (r.HasPid != true || r.Pid != PIDnow) {
        SERVICE_LOG("This call is reserved only for Core.");
        return SALTYSD_RESULT(handleService_RestoreBootstrapCode, 3); //// This call is reserved only for Core
    }

    SERVICE_LOG();
    
    Handle debug;
    ret = svcDebugActiveProcess(&debug, r.Pid);
    if (!ret)
    {
        ret = restore_elf_debug(debug);
    }
    
    // Bootstrapping is done, we can handle another process now.
    already_hijacking = false;
    svcCloseHandle(debug);
    return ret;
}

static Result serviceMemcpy(IpcCommand* c) {
    Result ret = 0;
    IpcParsedCommand r = {0};
    ipcParse(&r);

    if (r.HasPid != true || r.Pid != PIDnow) {
        SERVICE_LOG("This call is reserved only for Core.");
        return SALTYSD_RESULT(handleService_Memcpy, 3); // This call is reserved only for Core
    }

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

    raw = ipcPrepareHeader(c, sizeof(*raw));

    raw->magic = SFCO_MAGIC;
    raw->result = ret;

    SERVICE_LOG("memcpy(%lx, %lx, %lx)", to, from, size);

    return 0;
}

static Result serviceGetSDCard(IpcCommand* c) {
    SERVICE_LOG("stubbed");

    IpcParsedCommand r = {0};
    ipcParse(&r);

    if (r.HasPid != true || r.Pid != PIDnow) return SALTYSD_RESULT(handleService_GetSDCard, 3); // This call is reserved only for Core

    //ipcSendHandleCopy(c, sdcard);

    //return 0;
    return SALTYSD_RESULT(handleService_GetSDCard, 1);
}

static Result serviceLog() {
    IpcParsedCommand r = {0};
    ipcParse(&r);

    if (r.NumBuffers != 1) return SALTYSD_RESULT(handleService_Log, 4); // Buffer not received
    if (r.HasPid != true || r.Pid != PIDnow) return SALTYSD_RESULT(handleService_Log, 3); //// This call is reserved only for Core

    const char* log = r.Buffers[0];

    SaltySD_printf("SaltyNX: [log] %s", log);

    return 0;
}

static Result serviceCheckIfSharedMemoryAvailable(IpcCommand* c) {
    IpcParsedCommand r = {0};
    ipcParse(&r);

    struct {
        u64 magic;
        u64 cmd_id;
        u64 size;
        u64 reserved;
    } *resp = r.Raw;

    u64 new_size = resp->size;

    SERVICE_LOG("size: %d", new_size);

    struct {
        u64 magic;
        u64 result;
        u64 offset;
        u64 reserved;
    } *raw;

    raw = ipcPrepareHeader(c, sizeof(*raw));

    raw->magic = SFCO_MAGIC;
    if (!new_size) {
        SERVICE_LOG("Failed. Wrong size.");
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
            SERVICE_LOG("Failed. shmemMap error.");
            raw->offset = -1;
            raw->result = 0xFFE;
        }
    }
    else {
        SERVICE_LOG("Failed. Not enough free space. Left: %d B", (shmem_size - reservedSharedMemory));
        raw->offset = -1;
        raw->result = 0xFFE;
    }

    return 0;
}

static Result serviceGetSharedMemoryHandle(IpcCommand* c) {
    SERVICE_LOG();

    ipcSendHandleCopy(c, _sharedMemory.handle);

    return 0;
}

static Result serviceGetBID(IpcCommand* c) {
    IpcParsedCommand r = {0};
    ipcParse(&r);

    if (r.HasPid != true || r.Pid != PIDnow) {
        SERVICE_LOG("This call is reserved only for Core.");
        return SALTYSD_RESULT(handleService_GetBID, 3); // This call is reserved only for Core
    }

    SERVICE_LOG("PID: %ld", PIDnow);

    struct {
        u64 magic;
        u64 result;
    } *raw;

    raw = ipcPrepareHeader(c, sizeof(*raw));
    raw->magic = SFCO_MAGIC;
    raw->result = BIDnow;

    return 0;
}

static Result serviceException(IpcCommand* c) {
    IpcParsedCommand r = {0};
    ipcParse(&r);

    SERVICE_LOG();
    
    // Ship off results
    struct {
        u64 magic;
        u64 result;
        u64 reserved[2];
    } *raw;

    return exception;

    raw = ipcPrepareHeader(c, sizeof(*raw));

    raw->magic = SFCO_MAGIC;
    raw->result = exception;

    return 0;
}

static Result serviceGetDisplayRefreshRate(IpcCommand* c) {
    IpcParsedCommand r = {0};
    ipcParse(&r);
    
    // Ship off results
    struct {
        u64 magic;
        u64 result;
        u64 refreshRate;
        u64 reserved[2];
    } *raw;

    raw = ipcPrepareHeader(c, sizeof(*raw));

    raw->magic = SFCO_MAGIC;
    uint32_t temp_refreshRate = 0;
    raw->result = !GetDisplayRefreshRate(&temp_refreshRate, false);
    raw->refreshRate = temp_refreshRate;

    return 0;
}

static Result serviceSetDisplayRefreshRate() {
    Result ret = 0;
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
    else ret = SALTYSD_RESULT(handleService_SetDisplayRefreshRate, 1);
    SERVICE_LOG("refresh rate requested: %d, ret: 0x%x", refreshRate_temp, ret);
    return ret;
}

static Result serviceSetDisplaySync() {
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
    }
    else {
        remove("sdmc:/SaltySD/flags/displaysync.flag");
    }
    SERVICE_LOG("%d", displaySync);

    return 0;
}

static Result serviceSetAllowedDockedRefreshRates() {
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
    SERVICE_LOG();

    return 0;
}

static Result serviceSetDontForce60InDocked() {
    IpcParsedCommand r = {0};
    ipcParse(&r);

    struct {
        u64 magic;
        u64 cmd_id;
        u64 force;
        u64 reserved;
    } *resp = r.Raw;

    dontForce60InDocked = (bool)(resp -> force);
    SERVICE_LOG();

    return 0;
}

static Result serviceSetMatchLowestRR() {
    IpcParsedCommand r = {0};
    ipcParse(&r);

    struct {
        u64 magic;
        u64 cmd_id;
        u64 force;
        u64 reserved;
    } *resp = r.Raw;

    matchLowestDocked = (bool)(resp -> force);
    SERVICE_LOG();

    return 0;
}

static Result serviceGetDockedHighestRefreshRate(IpcCommand* c) {
    IpcParsedCommand r = {0};
    ipcParse(&r);

    SERVICE_LOG();
    
    // Ship off results
    struct {
        u64 magic;
        u64 result;
        u32 refreshRate;
        u32 linkRate;
        u32 laneCount;
        u32 reserved;
    } *raw;

    raw = ipcPrepareHeader(c, sizeof(*raw));

    raw->magic = SFCO_MAGIC;
    raw->result = 0;
    raw->refreshRate = dockedHighestRefreshRate;
    raw->linkRate = dockedLinkRate;
    raw->laneCount = dockedLaneCount;

    return 0;
}

static Result serviceIsPossiblyRetroRemake(IpcCommand* c) {
    IpcParsedCommand r = {0};
    ipcParse(&r);

    SERVICE_LOG();
    
    // Ship off results
    struct {
        u64 magic;
        u64 result;
        u64 value;
        u64 reserved;
    } *raw;

    raw = ipcPrepareHeader(c, sizeof(*raw));

    raw->magic = SFCO_MAGIC;
    raw->result = 0;
    raw->value = isPossiblySpoofedRetro;

    return 0;
}

static Result serviceSetDisplaySyncDocked() {
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
    }
    else {
        remove("sdmc:/SaltySD/flags/displaysyncdocked.flag");
    }
    SERVICE_LOG("%d", displaySyncDocked);

    return 0;
}

static Result serviceSetDisplaySyncRefreshRate60WhenOutOfFocus() {
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
        SERVICE_LOG("Docked %d", displaySyncDockedOutOfFocus60);
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
        SERVICE_LOG("Handheld %d", displaySyncOutOfFocus60);
    }

    return 0;
}

static Result serviceSdcardFopen(IpcCommand* c) {

    IpcParsedCommand r = {0};
    ipcParse(&r);

    if (r.NumBuffers != 1) {
        SERVICE_LOG("Buffers received: %d.", r.NumBuffers);
        return SALTYSD_RESULT(handleService_SdcardFopen, 4);
    }
    if (r.HasPid != true || r.Pid != PIDnow) {
        SERVICE_LOG("This call is reserved only for Core.");
        return SALTYSD_RESULT(handleService_SdcardFopen, 3);
    }

    struct {
        u64 magic;
        u64 command;
        char mode[4];
    } *resp = r.Raw;

    struct {
        u64 magic;
        u64 result;
        u32 id;
    } *raw;

    if (openedFilesAmount >= FOPEN_MAX-1) {
        SERVICE_LOG("Too much files opened at once.");
        raw = ipcPrepareHeader(c, sizeof(*raw));
        raw->magic = SFCO_MAGIC;
        raw->result = SALTYSD_RESULT(handleService_SdcardFopen, 1);
        raw->id = 0;
        return 0;
    }

    const char* filepath = r.Buffers[0];
    char filemode[4] = {0};
    strncpy(filemode, resp->mode, 3);

    FILE* file = fopen(filepath, filemode);
    int id = 0;
    if (file) {
        id = rand();
        if (!id) id = rand();
        openedFilesArray[openedFilesAmount++] = (struct FileId){id, file};
        SERVICE_LOG("Opened file: %s, FileId: 0x%x.", filepath, id);
    }
    else {
        SERVICE_LOG("Bad file: %s, errno: %d.", filepath, errno);
        return SALTYSD_RESULT(handleService_SdcardFopen, 2);
    }

    raw = ipcPrepareHeader(c, sizeof(*raw));

    raw->magic = SFCO_MAGIC;
    raw->result = 0;
    raw->id = id;

    return 0;
}

static Result serviceSdcardFread(IpcCommand* c) {
    if (openedFilesAmount == 0) {
        SERVICE_LOG("No file is opened.");
        return SALTYSD_RESULT(handleService_SdcardFread, 1);
    }

    IpcParsedCommand r = {0};
    ipcParse(&r);
    if (r.NumBuffers != 1) {
        SERVICE_LOG("Buffers received: %d.", r.NumBuffers);
        return SALTYSD_RESULT(handleService_SdcardFread, 4);
    }
    if (r.HasPid != true || r.Pid != PIDnow) {
        SERVICE_LOG("This call is reserved only for Core.");
        return SALTYSD_RESULT(handleService_SdcardFread, 3);
    }

    struct {
        u64 magic;
        u64 command;
        u64 size;
        u64 count;
        u32 id;
    } *resp = r.Raw;

    s32 id = resp->id;

    if (id == 0) {
        SERVICE_LOG("File not found.");
        return SALTYSD_RESULT(handleService_SdcardFread, 1); //File not found
    }

    FILE* file = 0;
    size_t size = resp->size;
    size_t count = resp->count;
    void* addr = r.Buffers[0];
    for (size_t i = 0; i < openedFilesAmount; i++) {
        if (openedFilesArray[i].id == id) {
            file = openedFilesArray[i].file;
            break;
        }
    }

    if (!file) {
        SERVICE_LOG("File not found.");
        return SALTYSD_RESULT(handleService_SdcardFread, 1); //File not found
    }

    size_t read = fread(addr, size, count, file);

    SERVICE_LOG("FileId: 0x%lx, read: %ld B.", file, read * size);

    struct {
        u64 magic;
        u64 result;
        u64 count_read;
    } *raw;

    raw = ipcPrepareHeader(c, sizeof(*raw));

    raw->magic = SFCO_MAGIC;
    raw->result = 0;
    raw->count_read = read;

    return 0;
}

static Result serviceSdcardFclose(IpcCommand* c) {
    if (openedFilesAmount == 0) {
        SERVICE_LOG("No file is opened.");
        return SALTYSD_RESULT(handleService_SdcardFclose, 1); //File not found
    }

    IpcParsedCommand r = {0};
    ipcParse(&r);
    if (r.HasPid != true || r.Pid != PIDnow) {
        SERVICE_LOG("This call is reserved only for Core.");
        return SALTYSD_RESULT(handleService_SdcardFclose, 3); // This call is reserved only for Core
    }

    struct {
        u64 magic;
        u64 command;
        u32 id;
    } *resp = r.Raw;
    
    s32 id = resp->id;

    if (id == 0) {
        SERVICE_LOG("File not found.");
        return SALTYSD_RESULT(handleService_SdcardFclose, 1); //File not found
    }

    FILE* file = 0;
    size_t i = 0;
    for (; i < openedFilesAmount; i++) {
        if (openedFilesArray[i].id == id) {
            file = openedFilesArray[i].file;
            break;
        }
    }

    if (!file) {
        SERVICE_LOG("File not found.");
        return SALTYSD_RESULT(handleService_SdcardFclose, 1); //File not found
    }

    int result = fclose(file);
    if (!result) {
        memmove(&openedFilesArray[i], &openedFilesArray[i+1], sizeof(openedFilesArray[0]) * (openedFilesAmount - (i+1)));
        memset(&openedFilesArray[FOPEN_MAX-2], 0, sizeof(openedFilesArray[0]));
        openedFilesAmount--;
    }

    SERVICE_LOG("FileId: 0x%lx, res: %d", file, result);

    return result;
}

static Result serviceSdcardFseek(IpcCommand* c) {
    if (openedFilesAmount == 0) {
        SERVICE_LOG("No file is opened.");
        return SALTYSD_RESULT(handleService_SdcardFseek, 1); //File not found
    }

    IpcParsedCommand r = {0};
    ipcParse(&r);
    if (r.HasPid != true || r.Pid != PIDnow) {
        SERVICE_LOG("This call is reserved only for Core.");
        return SALTYSD_RESULT(handleService_SdcardFseek, 3); // This call is reserved only for Core
    }

    struct {
        u64 magic;
        u64 command;
        s64 offset;
        int origin;
        u32 id;
    } *resp = r.Raw;

    u32 id = resp->id;

    if (id == 0) {
        SERVICE_LOG("File not found.");
        return SALTYSD_RESULT(handleService_SdcardFseek, 1); //File not found
    }

    FILE* file = 0;
    int origin = resp->origin;
    long offset = resp->offset;

    for (size_t i = 0; i < openedFilesAmount; i++) {
        if (openedFilesArray[i].id == id) {
            file = openedFilesArray[i].file;
            break;
        }
    }

    if (!file) {
        SERVICE_LOG("File not found.");
        return SALTYSD_RESULT(handleService_SdcardFseek, 1); //File not found
    }

    int ret = fseek(file, offset, origin);

    SERVICE_LOG("FileId: 0x%lx, Offset: 0x%lx, origin: %d, ret: %d.", file, offset, origin, ret);

    return ret;
}

static Result serviceSdcardFtell(IpcCommand* c) {
    if (openedFilesAmount == 0) {
        SERVICE_LOG("No file is opened.");
        return SALTYSD_RESULT(handleService_SdcardFtell, 1); //File not found
    }

    IpcParsedCommand r = {0};
    ipcParse(&r);
    if (r.HasPid != true || r.Pid != PIDnow) {
        SERVICE_LOG("This call is reserved only for Core.");
        return SALTYSD_RESULT(handleService_SdcardFtell, 3); // This call is reserved only for Core
    }

    struct {
        u64 magic;
        u64 command;
        s32 id;

    } *resp = r.Raw;

    u32 id = resp->id;

    if (id == 0) {
        SERVICE_LOG("File not found.");
        return SALTYSD_RESULT(handleService_SdcardFtell, 1); //File not found
    }

    FILE* file = 0;
    for (size_t i = 0; i < openedFilesAmount; i++) {
        if (openedFilesArray[i].id == id) {
            file = openedFilesArray[i].file;
            break;
        }
    }

    if (!file) {
        SERVICE_LOG("File not found.");
        return SALTYSD_RESULT(handleService_SdcardFtell, 1); //File not found
    }

    size_t offset = ftell(file);

    SERVICE_LOG("FileId: 0x%lx, offset: 0x%lx.", file, offset);

    struct {
        u64 magic;
        u64 result;
        u64 offset;
    } *raw;

    raw = ipcPrepareHeader(c, sizeof(*raw));
    raw->magic = SFCO_MAGIC;
    raw->result = 0;
    raw->offset = offset;

    return 0;
}

static Result serviceSdcardRemove(IpcCommand* c) {

    IpcParsedCommand r = {0};
    ipcParse(&r);

    if (r.NumBuffers != 1) {
        SERVICE_LOG("Buffers received: %d.", r.NumBuffers);
        return SALTYSD_RESULT(handleService_SdcardRemove, 4); // Buffer not received
    }
    if (r.HasPid != true || r.Pid != PIDnow) {
        SERVICE_LOG("This call is reserved only for Core.");
        return SALTYSD_RESULT(handleService_SdcardRemove, 3); // This call is reserved only for Core
    }

    const char* filepath = r.Buffers[0];

    int ret = remove(filepath);

    SERVICE_LOG("Path: %s, ret: %d", filepath, ret);

    return ret;
}

static Result serviceSdcardFwrite(IpcCommand* c) {
    if (openedFilesAmount == 0) {
        SERVICE_LOG("No file is opened.");
        return SALTYSD_RESULT(handleService_SdcardFwrite, 1); //File not found
    }

    IpcParsedCommand r = {0};
    ipcParse(&r);
    if (r.NumBuffers != 1) {
        SERVICE_LOG("Buffers received: %d.", r.NumBuffers);
        return SALTYSD_RESULT(handleService_SdcardFwrite, 2); //Buffer not received
    }
    if (r.HasPid != true || r.Pid != PIDnow) {
        SERVICE_LOG("This call is reserved only for Core.");
        return SALTYSD_RESULT(handleService_SdcardFwrite, 3); // This call is reserved only for Core
    }

    struct {
        u64 magic;
        u64 command;
        u64 size;
        u64 count;
        u32 id;
    } *resp = r.Raw;

    s32 id = resp->id;

    if (id == 0) {
        SERVICE_LOG("File not found.");
        return SALTYSD_RESULT(handleService_SdcardFwrite, 1); //File not found
    }

    FILE* file = 0;
    size_t size = resp->size;
    size_t count = resp->count;
    void* addr = r.Buffers[0];

    for (size_t i = 0; i < openedFilesAmount; i++) {
        if (openedFilesArray[i].id == id) {
            file = openedFilesArray[i].file;
            break;
        }
    }

    if (!file) {
        SERVICE_LOG("File not found.");
        return SALTYSD_RESULT(handleService_SdcardFwrite, 1); //File not found
    }

    size_t written = fwrite(addr, size, count, file);

    SERVICE_LOG("FileId: 0x%lx, written: %ld B.", file, written * size);

    struct {
        u64 magic;
        u64 result;
        u64 count_write;
    } *raw;

    raw = ipcPrepareHeader(c, sizeof(*raw));

    raw->magic = SFCO_MAGIC;
    raw->result = 0;
    raw->count_write = written;

    return 0;
}

static Result serviceSdcardOpendir(IpcCommand* c) {

    IpcParsedCommand r = {0};
    ipcParse(&r);

    if (r.NumBuffers != 1) {
        SERVICE_LOG("Buffers received: %d.", r.NumBuffers);
        return SALTYSD_RESULT(handleService_SdcardOpendir, 4); // Buffer not received
    }
    if (r.HasPid != true || r.Pid != PIDnow) {
        SERVICE_LOG("This call is reserved only for Core.");
        return SALTYSD_RESULT(handleService_SdcardOpendir, 3); // This call is reserved only for Core
    }

    struct {
        u64 magic;
        u64 result;
        u32 id;
    } *raw;

    if (openedDirsAmount >= OPEN_MAX-1) {
        SERVICE_LOG("Too much dirs opened at once.");
        raw = ipcPrepareHeader(c, sizeof(*raw));
        raw->magic = SFCO_MAGIC;
        raw->result = SALTYSD_RESULT(handleService_SdcardOpendir, 1);
        raw->id = 0;
        return 0;
    }

    const char* filepath = r.Buffers[0];

    DIR* dir = opendir(filepath);

    int id = 0;
    if (dir) {
        id = rand();
        if (!id) id = rand();
        openedDirsArray[openedDirsAmount++] = (struct DirId){id, dir};
        SERVICE_LOG("Opened dir: %s, DirId: 0x%x", filepath, id);
    }
    else {
        SERVICE_LOG("Bad dir: %s, errno: %d.", filepath, errno);
        return SALTYSD_RESULT(handleService_SdcardOpendir, 2); // Bad dir
    }

    raw = ipcPrepareHeader(c, sizeof(*raw));

    raw->magic = SFCO_MAGIC;
    raw->result = 0;
    raw->id = id;

    return 0;
}

static Result serviceSdcardMkdir(IpcCommand* c) {

    IpcParsedCommand r = {0};
    ipcParse(&r);

    if (r.NumBuffers != 1) {
        SERVICE_LOG("Buffers received: %d.", r.NumBuffers);
        return SALTYSD_RESULT(handleService_SdcardMkdir, 4); // Buffer not received
    }
    if (r.HasPid != true || r.Pid != PIDnow) {
        SERVICE_LOG("This call is reserved only for Core.");
        return SALTYSD_RESULT(handleService_SdcardMkdir, 3); // This call is reserved only for Core
    }

    const char* filepath = r.Buffers[0];

    int ret = mkdir(filepath, 420);

    SERVICE_LOG("%s, ret: %d", filepath, ret);

    return ret;
}

static Result serviceSdcardReaddir(IpcCommand* c) {

    IpcParsedCommand r = {0};
    ipcParse(&r);

    if (r.NumBuffers != 1) {
        SERVICE_LOG("Buffers received: %d.", r.NumBuffers);
        return SALTYSD_RESULT(handleService_SdcardReaddir, 4); // Buffer not received
    }
    if (r.HasPid != true || r.Pid != PIDnow) {
        SERVICE_LOG("This call is reserved only for Core.");
        return SALTYSD_RESULT(handleService_SdcardReaddir, 3); // This call is reserved only for Core
    }

    struct {
        u64 magic;
        u64 command;
        u32 id;
    } *resp = r.Raw;

    u32 id = resp->id;

    if (id == 0) {
        SERVICE_LOG("No valid dir detected.");
        return SALTYSD_RESULT(handleService_SdcardReaddir, 2);        
    }

    DIR* dir = 0;
    for (size_t i = 0; i < openedDirsAmount; i++) {
        if (openedDirsArray[i].id == id) {
            dir = openedDirsArray[i].dir;
            break;
        }
    }

    if (!dir) {
        SERVICE_LOG("No valid dir detected.");
        return SALTYSD_RESULT(handleService_SdcardReaddir, 2);
    }

    struct dirent* data = readdir(dir);

    if (!data) {
        SERVICE_LOG("DirId: 0x%lx, errno: %d", dir, errno);
        return SALTYSD_RESULT(handleService_SdcardReaddir, 1); // Received nullptr
    }
    else SERVICE_LOG("DirId: 0x%lx, no error", dir);

    memcpy(r.Buffers[0], data, sizeof(struct dirent));

    struct {
        u64 magic;
        u64 result;
    } *raw;

    raw = ipcPrepareHeader(c, sizeof(*raw));
    raw->magic = SFCO_MAGIC;
    raw->result = 0;

    return 0;
}

static Result serviceSdcardClosedir(IpcCommand* c) {
    if (openedDirsAmount == 0) {
        SERVICE_LOG("No dir is opened.");
        return SALTYSD_RESULT(handleService_SdcardClosedir, 1); //Dir not found
    }

    IpcParsedCommand r = {0};
    ipcParse(&r);
    if (r.HasPid != true || r.Pid != PIDnow) {
        SERVICE_LOG("This call is reserved only for Core.");
        return SALTYSD_RESULT(handleService_SdcardClosedir, 3); // This call is reserved only for Core
    }

    struct {
        u64 magic;
        u64 command;
        u32 id;
    } *resp = r.Raw;

    u32 id = resp->id;

    DIR* dir = 0;
    size_t i = 0;
    for (; i < openedDirsAmount; i++) {
        if (openedDirsArray[i].id == id) {
            dir = openedDirsArray[i].dir;
            break;
        }
    }

    if (!dir) {
        SERVICE_LOG("Wrong dir.");
        return SALTYSD_RESULT(handleService_SdcardClosedir, 1); //Dir not found
    }

    int result = closedir(dir);
    if (!result) {
        memmove(&openedDirsArray[i], &openedDirsArray[i+1], sizeof(openedDirsArray[0]) * (openedDirsAmount - (i+1)));
        memset(&openedDirsArray[OPEN_MAX-2], 0, sizeof(openedDirsArray[0]));
        openedDirsAmount--;
    }

    SERVICE_LOG("DirId: 0x%lx, res: %d", dir, result);

    return result;
}

static_assert(sizeof(ino_t) == 2);

static Result handleServiceCmd(int cmd)
{
    Result ret = 0;

    // Send reply
    IpcCommand c;
    ipcInitialize(&c);

    switch(cmd) {
        case handleService_EndSession:                                {ret = serviceEndSession(); break;}
        case handleService_LoadELF:                                   {ret = serviceLoadELF(&c); if (ret) break; return 0;}
        case handleService_RestoreBootstrapCode:                      {ret = serviceRestoreBootstrapCode(); break;}
        case handleService_Memcpy:                                    {ret = serviceMemcpy(&c); if (ret) break; return 0;}
        case handleService_GetSDCard:                                 {ret = serviceGetSDCard(&c); break;}
        case handleService_Log:                                       {ret = serviceLog(); break;}
        case handleService_CheckIfSharedMemoryAvailable:              {ret = serviceCheckIfSharedMemoryAvailable(&c); if (ret) break; return 0;}
        case handleService_GetSharedMemoryHandle:                     {ret = serviceGetSharedMemoryHandle(&c); break;}
        case handleService_GetBID:                                    {ret = serviceGetBID(&c); if (ret) break; return 0;}
        case handleService_Exception:                                 {ret = serviceException(&c); if (ret) break; return 0;}
        case handleService_GetDisplayRefreshRate:                     {ret = serviceGetDisplayRefreshRate(&c); if (ret) break; return 0;}
        case handleService_SetDisplayRefreshRate:                     {ret = serviceSetDisplayRefreshRate(); break;}
        case handleService_SetDisplaySync:                            {ret = serviceSetDisplaySync(); break;}
        case handleService_SetAllowedDockedRefreshRates:              {ret = serviceSetAllowedDockedRefreshRates(); break;}
        case handleService_SetDontForce60InDocked:                    {ret = serviceSetDontForce60InDocked(); break;}
        case handleService_SetMatchLowestRR:                          {ret = serviceSetMatchLowestRR(); break;}
        case handleService_GetDockedHighestRefreshRate:               {ret = serviceGetDockedHighestRefreshRate(&c); if (ret) break; return 0;}
        case handleService_IsPossiblyRetroRemake:                     {ret = serviceIsPossiblyRetroRemake(&c); if (ret) break; return 0;}
        case handleService_SetDisplaySyncDocked:                      {ret = serviceSetDisplaySyncDocked(); break;}
        case handleService_SetDisplaySyncRefreshRate60WhenOutOfFocus: {ret = serviceSetDisplaySyncRefreshRate60WhenOutOfFocus(); break;}
        case handleService_SdcardFopen:                               {ret = serviceSdcardFopen(&c); if (ret) break; return 0;}
        case handleService_SdcardFread:                               {ret = serviceSdcardFread(&c); if (ret) break; return 0;}
        case handleService_SdcardFclose:                              {ret = serviceSdcardFclose(&c); break;}
        case handleService_SdcardFseek:                               {ret = serviceSdcardFseek(&c); break;}
        case handleService_SdcardFtell:                               {ret = serviceSdcardFtell(&c); if (ret) break; return 0;}
        case handleService_SdcardRemove:                              {ret = serviceSdcardRemove(&c); break;}
        case handleService_SdcardFwrite:                              {ret = serviceSdcardFwrite(&c); if (ret) break; return 0;}
        case handleService_SdcardOpendir:                             {ret = serviceSdcardOpendir(&c); if (ret) break; return 0;}
        case handleService_SdcardMkdir:                               {ret = serviceSdcardMkdir(&c); break;}
        case handleService_SdcardReaddir:                             {ret = serviceSdcardReaddir(&c); if (ret) break; return 0;}
        case handleService_SdcardClosedir:                            {ret = serviceSdcardClosedir(&c); if (ret) break; return 0;}
        default: ret = 0xEE01;
    }
    
    struct {
        u64 magic;
        u64 result;
    } *raw;

    raw = ipcPrepareHeader(&c, sizeof(*raw));

    raw->magic = SFCO_MAGIC;
    raw->result = ret;
    
    return ret;
}

void serviceThread()
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