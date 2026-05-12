#if defined(SWITCH32)
#include <switch_min.h>
#elif defined(SWITCH)
#include <switch.h>
#include "ipc.h"
#else
#error "Unsupported base architecture!"
#endif
#include <stdarg.h>

#include "saltysd_core.h"
#include "useful.h"
#include "nanoprintf.h"
#include <errno.h>

Handle saltysd;

#if defined(_DIRENT_HAVE_D_NAMLEN) || defined(_DIRENT_HAVE_D_RECLEN) || defined(_DIRENT_HAVE_D_OFF)
#error "Wrong DIR structure detected!"
#endif
#if !defined(_DIRENT_HAVE_D_TYPE)
#error "Wrong DIR structure detected!"
#endif

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

Result SaltySD_Init()
{
	Result ret;

	for (int i = 0; i < 200; i++)
	{
		ret = svcConnectToNamedPort(&saltysd, "SaltySD");
		svcSleepThread(1000*1000);
		
		if (!ret) break;
	}

	return ret;
	
	//debug_log("SaltySD Core: Got handle %x\n", saltysd);
}

Result SaltySD_Term()
{
	Result ret;
	IpcCommand c;

	ipcInitialize(&c);
	ipcSendPid(&c);

	struct 
	{
		u64 magic;
		u64 cmd_id;
		u64 zero;
		u64 reserved[2];
	} *raw;

	raw = ipcPrepareHeader(&c, sizeof(*raw));

	raw->magic = SFCI_MAGIC;
	raw->cmd_id = handleService_EndSession;
	raw->zero = 0;

	ret = ipcDispatch(saltysd);

	if (R_SUCCEEDED(ret)) 
	{
		IpcParsedCommand r;
		ipcParse(&r);

		struct respond {
			u64 magic;
			u64 result;
		} *resp = (struct respond*)r.Raw;

		ret = resp->result;
	}
	
	// Session terminated works too.
	if (ret == 0xf601) return 0;

	return ret;
}

Result SaltySD_Deinit()
{
	Result ret;

	//debug_log("SaltySD Core: terminating\n");
	ret = SaltySD_Term();
	if (ret) return ret;

	svcCloseHandle(saltysd);
	return ret;
}

Result SaltySD_Restore()
{
	Result ret;
	IpcCommand c;

	ipcInitialize(&c);
	ipcSendPid(&c);

	struct 
	{
		u64 magic;
		u64 cmd_id;
		u64 zero;
		u64 reserved[2];
	} *raw;

	raw = ipcPrepareHeader(&c, sizeof(*raw));

	raw->magic = SFCI_MAGIC;
	raw->cmd_id = handleService_RestoreBootstrapCode;
	raw->zero = 0;

	ret = ipcDispatch(saltysd);

	if (R_SUCCEEDED(ret)) 
	{
		IpcParsedCommand r;
		ipcParse(&r);

		struct respond {
			u64 magic;
			u64 result;
		} *resp = (struct respond*)r.Raw;

		ret = resp->result;
	}

	return ret;
}

#if defined(SWITCH) || defined(OUNCE)

Result SaltySD_LoadELF(u64 heap, u64* elf_addr, u64* elf_size, char* name)
{
	Result ret;
	IpcCommand c;

	ipcInitialize(&c);
	ipcSendPid(&c);
	ipcSendHandleCopy(&c, CUR_PROCESS_HANDLE);

	struct 
	{
		u64 magic;
		u64 cmd_id;
		u64 heap;
		char name[64];
		u32 reserved[2];
	} *raw;

	raw = ipcPrepareHeader(&c, sizeof(*raw));

	raw->magic = SFCI_MAGIC;
	raw->cmd_id = handleService_LoadELF;
	raw->heap = heap;
	strncpy(raw->name, name, 63);

	ret = ipcDispatch(saltysd);

	if (R_SUCCEEDED(ret)) 
	{
		IpcParsedCommand r;
		ipcParse(&r);

		struct {
			u64 magic;
			u64 result;
			u64 elf_addr;
			u64 elf_size;
		} *resp = r.Raw;

		ret = resp->result;
		*elf_addr = resp->elf_addr;
		*elf_size = resp->elf_size;
	}

	return ret;
}

#endif

Result SaltySD_Memcpy(uintptr_t to, uintptr_t from, size_t size)
{
	Result ret;
	IpcCommand c;

	ipcInitialize(&c);
	ipcSendPid(&c);

	struct 
	{
		u64 magic;
		u64 cmd_id;
		u64 to;
		u64 from;
		u64 size;
	} *raw;

	raw = ipcPrepareHeader(&c, sizeof(*raw));

	raw->magic = SFCI_MAGIC;
	raw->cmd_id = handleService_Memcpy;
	raw->from = from;
	raw->to = to;
	raw->size = size;

	ret = ipcDispatch(saltysd);

	if (R_SUCCEEDED(ret)) 
	{
		IpcParsedCommand r;
		ipcParse(&r);

		struct respond {
			u64 magic;
			u64 result;
		} *resp = (struct respond*)r.Raw;

		ret = resp->result;
	}

	return ret;
}

Result SaltySD_Exception()
{
	Result ret;
	IpcCommand c;

	ipcInitialize(&c);
	ipcSendPid(&c);

	struct 
	{
		u64 magic;
		u64 cmd_id;
		u32 reserved[4];
	} *raw;

	raw = ipcPrepareHeader(&c, sizeof(*raw));

	raw->magic = SFCI_MAGIC;
	raw->cmd_id = handleService_Exception;

	ret = ipcDispatch(saltysd);

	if (R_SUCCEEDED(ret)) 
	{
		IpcParsedCommand r;
		ipcParse(&r);

		struct respond {
			u64 magic;
			u64 result;
			u64 reserved[2];
		} *resp = (struct respond*)r.Raw;

		ret = resp->result;
	}

	return ret;
}

Result SaltySD_GetSDCard(Handle *retrieve)
{
	Result ret = 0;

	// Send a command
	IpcCommand c;
	ipcInitialize(&c);
	ipcSendPid(&c);

	struct {
		u64 magic;
		u64 cmd_id;
		u32 reserved[4];
	} *raw;

	raw = ipcPrepareHeader(&c, sizeof(*raw));

	raw->magic = SFCI_MAGIC;
	raw->cmd_id = handleService_GetSDCard;

	ret = ipcDispatch(saltysd);

	if (R_SUCCEEDED(ret)) {
		IpcParsedCommand r;
		ipcParse(&r);

		struct respond {
			u64 magic;
			u64 result;
			u64 reserved[2];
		} *resp = (struct respond*)r.Raw;

		ret = resp->result;
		
		if (!ret)
		{
			*retrieve = r.Handles[0];
			
			// Init fs stuff
			FsFileSystem sdcardfs;
			#if defined(SWITCH32) || defined(OUNCE32)
			sdcardfs.s.handle = *retrieve;
			#else
			sdcardfs.s.own_handle = *retrieve;
			#endif
			int dev = fsdevMountDevice("sdmc", sdcardfs);
			setDefaultDevice(dev);

			SaltySDCore_printf("SaltySD Core: got SD card handle %x\n", r.Handles[0]);
		}
	}
	
	return ret;
}

Result SaltySD_CheckIfSharedMemoryAvailable(ptrdiff_t *new_offset, size_t new_size)
{
	Result ret = 0;

	// Send a command
	IpcCommand c;
	ipcInitialize(&c);
	ipcSendPid(&c);

	struct {
		u64 magic;
		u64 cmd_id;
		u64 size;
		u64 reserved;
	} *raw;

	raw = ipcPrepareHeader(&c, sizeof(*raw));

	raw->magic = SFCI_MAGIC;
	raw->cmd_id = handleService_CheckIfSharedMemoryAvailable;
	raw->size = new_size;

	ret = ipcDispatch(saltysd);

	if (R_SUCCEEDED(ret)) {
		IpcParsedCommand r;
		ipcParse(&r);

		struct respond {
			u64 magic;
			u64 result;
			u64 offset;
			u64 reserved;
		} *resp = (struct respond*)r.Raw;

		ret = resp->result;
		
		if (!ret)
		{
			*new_offset = resp->offset;
		}
		else *new_offset = -1;
	}
	
	return ret;
}

Result SaltySD_GetSharedMemoryHandle(Handle *retrieve)
{
	Result ret = 0;

	// Send a command
	IpcCommand c;
	ipcInitialize(&c);
	ipcSendPid(&c);

	struct {
		u64 magic;
		u64 cmd_id;
		u32 reserved[4];
	} *raw;

	raw = ipcPrepareHeader(&c, sizeof(*raw));

	raw->magic = SFCI_MAGIC;
	raw->cmd_id = handleService_GetSharedMemoryHandle;

	ret = ipcDispatch(saltysd);

	if (R_SUCCEEDED(ret)) {
		IpcParsedCommand r;
		ipcParse(&r);

		struct respond {
			u64 magic;
			u64 result;
			u64 reserved[2];
		} *resp = (struct respond*)r.Raw;

		ret = resp->result;
		
		if (!ret)
		{
			SaltySDCore_printf("SaltySD Core: got SharedMemory handle %x\n", r.Handles[0]);
			*retrieve = r.Handles[0];
		}
	}
	
	return ret;
}

Result SaltySD_printf(const char* format, ...)
{
	Result ret;
	char tmp[256];

	va_list args;
	va_start(args, format);
	npf_vsnprintf(tmp, 256, format, args);
	va_end(args);
	
	IpcCommand c;

	ipcInitialize(&c);
	ipcSendPid(&c);
	ipcAddSendBuffer(&c, tmp, strlen(tmp) + 1, BufferType_Normal);

	struct 
	{
		u64 magic;
		u64 cmd_id;
	} *raw;

	raw = ipcPrepareHeader(&c, sizeof(*raw));

	raw->magic = SFCI_MAGIC;
	raw->cmd_id = 5;

	ret = ipcDispatch(saltysd);

	if (R_SUCCEEDED(ret)) 
	{
		IpcParsedCommand r;
		ipcParse(&r);

		struct respond {
			u64 magic;
			u64 result;
		} *resp = (struct respond*)r.Raw;

		ret = resp->result;
	}

	return ret;
}

u64 SaltySD_GetBID()
{
	Result ret;
	IpcCommand c;

	ipcInitialize(&c);
	ipcSendPid(&c);

	struct 
	{
		u64 magic;
		u64 cmd_id;
		u64 reserved;
	} *raw;

	raw = ipcPrepareHeader(&c, sizeof(*raw));

	raw->magic = SFCI_MAGIC;
	raw->cmd_id = handleService_GetBID;

	ret = ipcDispatch(saltysd);

	if (R_SUCCEEDED(ret)) 
	{
		IpcParsedCommand r;
		ipcParse(&r);

		struct respond {
			u64 magic;
			u64 result;
		} *resp = (struct respond*)r.Raw;

		uint64_t rett = resp->result;
		if (rett) {
			#if defined(SWITCH32) || defined(OUNCE32)
			SaltySDCore_printf("SaltySD Core: BID: %016llX\n", rett);
			#else
			SaltySDCore_printf("SaltySD Core: BID: %016lX\n", rett);
			#endif
			return rett;
		}
		else {
			SaltySDCore_printf("SaltySD Core: getBID failed!\n");
			return 0;
		}
	}
	return 0;
}

Result SaltySD_SetDisplaySyncDocked(bool isTrue)
{
	Result ret = 0;

	// Send a command
	IpcCommand c;
	ipcInitialize(&c);
	ipcSendPid(&c);

	struct input {
		u64 magic;
		u64 cmd_id;
		u64 value;
		u64 reserved;
	} *raw;

	raw = ipcPrepareHeader(&c, sizeof(*raw));

	raw->magic = SFCI_MAGIC;
	raw->cmd_id = handleService_SetDisplaySyncDocked;
	raw->value = isTrue;

	ret = ipcDispatch(saltysd);

	if (R_SUCCEEDED(ret)) {
		IpcParsedCommand r;
		ipcParse(&r);

		struct output {
			u64 magic;
			u64 result;
			u64 reserved[2];
		} *resp = (struct output*)r.Raw;

		ret = resp->result;
	}
	
	return ret;
}

Result SaltySD_SetDisplaySync(bool isTrue)
{
	Result ret = 0;

	// Send a command
	IpcCommand c;
	ipcInitialize(&c);
	ipcSendPid(&c);

	struct input {
		u64 magic;
		u64 cmd_id;
		u64 value;
		u64 reserved;
	} *raw;

	raw = ipcPrepareHeader(&c, sizeof(*raw));

	raw->magic = SFCI_MAGIC;
	raw->cmd_id = handleService_SetDisplaySync;
	raw->value = isTrue;

	ret = ipcDispatch(saltysd);

	if (R_SUCCEEDED(ret)) {
		IpcParsedCommand r;
		ipcParse(&r);

		struct output {
			u64 magic;
			u64 result;
			u64 reserved[2];
		} *resp = (struct output*)r.Raw;

		ret = resp->result;
	}
	
	return ret;
}


FILE* SaltySDCore_fopen(const char* filename, const char* mode)
{
	// Send a command
	IpcCommand c;
	ipcInitialize(&c);
	ipcSendPid(&c);
	ipcAddSendBuffer(&c, filename, strlen(filename) + 1, BufferType_Normal);	

	struct input {
		u64 magic;
		u64 cmd_id;
		char mode[4];
	} *raw;

	raw = ipcPrepareHeader(&c, sizeof(*raw));

	raw->magic = SFCI_MAGIC;
	raw->cmd_id = handleService_SdcardFopen;
	memset(raw->mode, 0, 4);
	strncpy(raw->mode, mode, 3);

	Result ret = ipcDispatch(saltysd);

	if (R_SUCCEEDED(ret)) {
		IpcParsedCommand r;
		ipcParse(&r);

		struct output {
			u64 magic;
			u64 result;
			u32 id;
		} *resp = (struct output*)r.Raw;

		u64 ret = resp->result;
		if (!ret) return (FILE*)(uintptr_t)resp->id;
	}

	return nullptr;
}

size_t SaltySDCore_fread(void* ptr, size_t size, size_t count, FILE* stream)
{
	// Send a command
	IpcCommand c;
	ipcInitialize(&c);
	ipcSendPid(&c);
	ipcAddRecvBuffer(&c, ptr, size * count, BufferType_Normal);

	struct input {
		u64 magic;
		u64 cmd_id;
		u64 size;
		u64 count;
		u32 id;
	} *raw;

	raw = ipcPrepareHeader(&c, sizeof(*raw));

	raw->magic = SFCI_MAGIC;
	raw->cmd_id = handleService_SdcardFread;
	raw->id = (u64)stream;
	raw->size = size;
	raw->count = count;

	Result ret = ipcDispatch(saltysd);

	if (R_SUCCEEDED(ret)) {
		IpcParsedCommand r;
		ipcParse(&r);

		struct output {
			u64 magic;
			u64 result;
			u64 count_read;
		} *resp = (struct output*)r.Raw;

		u64 ret = resp->result;
		size_t count_read = (size_t)resp->count_read;
		if (!ret) return count_read;
	}

	return 0;
}

int SaltySDCore_fclose(FILE* stream)
{
	// Send a command
	IpcCommand c;
	ipcInitialize(&c);
	ipcSendPid(&c);

	struct input {
		u64 magic;
		u64 cmd_id;
		u32 id;
	} *raw;

	raw = ipcPrepareHeader(&c, sizeof(*raw));

	raw->magic = SFCI_MAGIC;
	raw->cmd_id = handleService_SdcardFclose;
	raw->id = (u32)(uintptr_t)stream;

	Result ret = ipcDispatch(saltysd);

	if (R_SUCCEEDED(ret)) {
		IpcParsedCommand r;
		ipcParse(&r);

		struct output {
			u64 magic;
			u64 result;
		} *resp = (struct output*)r.Raw;

		return (int)resp->result;
	}

	return EOF;
}

int SaltySDCore_fseek(FILE* stream, int64_t offset, int origin)
{
	// Send a command
	IpcCommand c;
	ipcInitialize(&c);
	ipcSendPid(&c);

	struct input {
		u64 magic;
		u64 cmd_id;
        s64 offset;
        int origin;
        u32 id;
	} *raw;

	raw = ipcPrepareHeader(&c, sizeof(*raw));

	raw->magic = SFCI_MAGIC;
	raw->cmd_id = handleService_SdcardFseek;
	raw->offset = offset;
	raw->origin = origin;
	raw->id = (u32)(uintptr_t)stream;

	Result ret = ipcDispatch(saltysd);

	if (R_SUCCEEDED(ret)) {
		IpcParsedCommand r;
		ipcParse(&r);

		struct output {
			u64 magic;
			u64 result;
		} *resp = (struct output*)r.Raw;

		return (int)resp->result;
	}

	return EOF;
}

size_t SaltySDCore_ftell(FILE* stream) {
	// Send a command
	IpcCommand c;
	ipcInitialize(&c);
	ipcSendPid(&c);

	struct input {
		u64 magic;
		u64 cmd_id;
        u32 id;
	} *raw;

	raw = ipcPrepareHeader(&c, sizeof(*raw));

	raw->magic = SFCI_MAGIC;
	raw->cmd_id = handleService_SdcardFtell;
	raw->id = (u32)(uintptr_t)stream;

	Result ret = ipcDispatch(saltysd);

	if (R_SUCCEEDED(ret)) {
		IpcParsedCommand r;
		ipcParse(&r);

		struct output {
			u64 magic;
			u64 result;
			u64 offset;
		} *resp = (struct output*)r.Raw;

		u64 ret = resp->result;
		size_t offset = (size_t)resp->offset;
		if (!ret) return offset;
	}

	return -1;
}

int SaltySDCore_remove(const char* filename) {
	// Send a command
	IpcCommand c;
	ipcInitialize(&c);
	ipcSendPid(&c);
	ipcAddSendBuffer(&c, filename, strlen(filename) + 1, BufferType_Normal);

	struct input {
		u64 magic;
		u64 cmd_id;
	} *raw;

	raw = ipcPrepareHeader(&c, sizeof(*raw));

	raw->magic = SFCI_MAGIC;
	raw->cmd_id = handleService_SdcardRemove;

	Result ret = ipcDispatch(saltysd);

	if (R_SUCCEEDED(ret)) {
		IpcParsedCommand r;
		ipcParse(&r);

		struct output {
			u64 magic;
			u64 result;
		} *resp = (struct output*)r.Raw;

		return (int)resp->result;
	}

	return 1;
}

size_t SaltySDCore_fwrite(const void* ptr, size_t size, size_t count, FILE* stream) 
{
	// Send a command
	IpcCommand c;
	ipcInitialize(&c);
	ipcSendPid(&c);
	ipcAddSendBuffer(&c, ptr, size * count, BufferType_Normal);

	struct input {
		u64 magic;
		u64 cmd_id;
		u64 size;
		u64 count;
		u32 id;
	} *raw;

	raw = ipcPrepareHeader(&c, sizeof(*raw));

	raw->magic = SFCI_MAGIC;
	raw->cmd_id = handleService_SdcardFwrite;
	raw->id = (u32)(uintptr_t)stream;
	raw->size = size;
	raw->count = count;

	Result ret = ipcDispatch(saltysd);

	if (R_SUCCEEDED(ret)) {
		IpcParsedCommand r;
		ipcParse(&r);

		struct output {
			u64 magic;
			u64 result;
			u64 count_write;
		} *resp = (struct output*)r.Raw;

		u64 ret = resp->result;
		size_t count_write = (size_t)resp->count_write;
		if (!ret) return count_write;
	}

	return 0;
}

DIR* SaltySDCore_opendir(const char* dirname)
{
	// Send a command
	IpcCommand c;
	ipcInitialize(&c);
	ipcSendPid(&c);
	ipcAddSendBuffer(&c, dirname, strlen(dirname) + 1, BufferType_Normal);

	struct input {
		u64 magic;
		u64 cmd_id;
	} *raw;

	raw = ipcPrepareHeader(&c, sizeof(*raw));

	raw->magic = SFCI_MAGIC;
	raw->cmd_id = handleService_SdcardOpendir;

	Result ret = ipcDispatch(saltysd);

	if (R_SUCCEEDED(ret)) {
		IpcParsedCommand r;
		ipcParse(&r);

		struct output {
			u64 magic;
			u64 result;
			u32 id;
		} *resp = (struct output*)r.Raw;

		u64 ret = resp->result;
		if (!ret) return (DIR*)(uintptr_t)resp->id;
	}

	return nullptr;
}

int SaltySDCore_mkdir(const char* dirname, mode_t mode)
{
	// Send a command
	IpcCommand c;
	ipcInitialize(&c);
	ipcSendPid(&c);
	ipcAddSendBuffer(&c, dirname, strlen(dirname) + 1, BufferType_Normal);

	struct input {
		u64 magic;
		u64 cmd_id;
	} *raw;

	raw = ipcPrepareHeader(&c, sizeof(*raw));

	raw->magic = SFCI_MAGIC;
	raw->cmd_id = handleService_SdcardMkdir;

	Result ret = ipcDispatch(saltysd);

	if (R_SUCCEEDED(ret)) {
		IpcParsedCommand r;
		ipcParse(&r);

		struct output {
			u64 magic;
			u64 result;
		} *resp = (struct output*)r.Raw;

		return (int)resp->result;
	}

	return 1;
}

#if defined(SWITCH) || defined(OUNCE)
static_assert(sizeof(ino_t) == 2);
#endif

struct dirent output = {0};

struct dirent* SaltySDCore_readdir(DIR* dirp)
{

	// Send a command
	IpcCommand c;
	ipcInitialize(&c);
	ipcSendPid(&c);
	ipcAddRecvBuffer(&c, &output, sizeof(output), BufferType_Normal);

	struct input {
		u64 magic;
		u64 cmd_id;
		u32 id;
	} *raw;

	raw = ipcPrepareHeader(&c, sizeof(*raw));

	raw->magic = SFCI_MAGIC;
	raw->cmd_id = handleService_SdcardReaddir;
	raw->id = (u32)(uintptr_t)dirp;

	Result ret = ipcDispatch(saltysd);

	if (R_SUCCEEDED(ret)) {
		IpcParsedCommand r;
		ipcParse(&r);

		struct output {
			u64 magic;
			u64 result;
		} *resp = (struct output*)r.Raw;

		u64 ret = resp->result;
		if (!ret) {
			#if defined(SWITCH32) || defined(OUNCE32)
			struct direntSalty {
				uint16_t	   d_ino;
				unsigned char  d_type;
				char	       d_name[NAME_MAX+1];
			};
			struct direntSalty* dir = (struct direntSalty*)&output;
			memmove(&output.d_name, dir->d_name, NAME_MAX+1);
			output.d_type = dir->d_type;
			output.d_ino = dir->d_ino;
			#endif
			return &output;
		}
	}
	return nullptr;
}

int SaltySDCore_closedir(DIR *dirp) 
{
	// Send a command
	IpcCommand c;
	ipcInitialize(&c);
	ipcSendPid(&c);

	struct input {
		u64 magic;
		u64 cmd_id;
		u32 id;
	} *raw;

	raw = ipcPrepareHeader(&c, sizeof(*raw));

	raw->magic = SFCI_MAGIC;
	raw->cmd_id = handleService_SdcardClosedir;
	raw->id = (u32)(uintptr_t)dirp;

	Result ret = ipcDispatch(saltysd);

	if (R_SUCCEEDED(ret)) {
		IpcParsedCommand r;
		ipcParse(&r);

		struct output {
			u64 magic;
			u64 result;
		} *resp = (struct output*)r.Raw;

		return (int)resp->result;
	}

	return 1;
}