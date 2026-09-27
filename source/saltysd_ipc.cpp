// Variant B: one shared, out-of-line transaction. Each command is a small
// constant descriptor plus a stack payload, so the IPC dance exists exactly
// once in .text instead of 25 inlined copies.
#if defined(SWITCH32)
#include <switch_min.h>
#elif defined(SWITCH)
#include <switch.h>
#include "ipc.h"
#else
#error "Unsupported base architecture!"
#endif
#include <stdarg.h>
#include <stddef.h>

#include "saltysd_core.h"
#include "useful.h"
#include "nanoprintf.h"
#include <errno.h>

extern "C" Handle saltysd;
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

namespace {

struct Empty {};

template <class Body>
struct Request { u64 magic; u64 cmd_id; [[no_unique_address]] Body body; };

template <class B> constexpr u16 RawSize = (u16)sizeof(Request<B>);
static_assert(RawSize<Empty> == 16);

enum : u16 { F_SendBuf = 1, F_RecvBuf = 2, F_ProcHandle = 4 };

struct SdCmd {
	u32 cmd_id;
	u16 raw_size;
	u16 body_size;
	u16 resp_size;
	u16 flags;
	const void* body     = nullptr;
	const void* buf      = nullptr;
	size_t      buf_size = 0;
};

[[gnu::noinline]]
bool Transact(const SdCmd& cmd, u64& result, void* resp_out = nullptr, Handle* handle_out = nullptr)
{
	IpcCommand c;
	ipcInitialize(&c);
	ipcSendPid(&c);

	if (cmd.flags & F_ProcHandle) ipcSendHandleCopy(&c, CUR_PROCESS_HANDLE);
	if (cmd.flags & F_SendBuf)    ipcAddSendBuffer(&c, cmd.buf, cmd.buf_size, BufferType_Normal);
	if (cmd.flags & F_RecvBuf)    ipcAddRecvBuffer(&c, (void*)cmd.buf, cmd.buf_size, BufferType_Normal);

	u64* raw = (u64*)ipcPrepareHeader(&c, cmd.raw_size);
	raw[0] = SFCI_MAGIC;
	raw[1] = cmd.cmd_id;
	if (cmd.body_size) memcpy((u8*)raw + 16, cmd.body, cmd.body_size);

	Result ret = ipcDispatch(saltysd);
	if (R_FAILED(ret)) { result = ret; return false; }

	IpcParsedCommand r;
	ipcParse(&r);
	u64* resp = (u64*)r.Raw;

	result = resp[1];
	if (cmd.resp_size) memcpy(resp_out, (u8*)resp + 16, cmd.resp_size);
	if (handle_out)    *handle_out = r.Handles[0];
	return true;
}

} // namespace

extern "C" {

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
}

Result SaltySD_Term()
{	
	u64 r;
	Transact({handleService_EndSession, RawSize<Empty>, 0, 0, 0, nullptr}, r);

	// Session terminated works too.
	if ((Result)r == 0xf601) return 0;
	return (Result)r;
}

Result SaltySD_Deinit()
{
	Result ret = SaltySD_Term();
	if (ret) return ret;

	svcCloseHandle(saltysd);
	return ret;
}

Result SaltySD_Restore()
{
	u64 r;
	Transact({handleService_RestoreBootstrapCode, RawSize<Empty>, 0, 0, 0, nullptr}, r);
	return (Result)r;
}

#if defined(SWITCH) || defined(OUNCE)
Result SaltySD_LoadELF(u64 heap, u64* elf_addr, u64* elf_size, char* name)
{
	struct ReqLoadELF { 
		u64 heap; 
		char name[64];
	} b{heap, ""};
	static_assert(RawSize<ReqLoadELF> == 88);
	b.heap = heap;
	strncpy(b.name, name, 63);

	u64 out[2]{};
	u64 r;
	if (Transact({handleService_LoadELF, RawSize<ReqLoadELF>, sizeof(ReqLoadELF),
	              sizeof(out), F_ProcHandle, &b}, r, out)) {
		*elf_addr = out[0];
		*elf_size = out[1];
	}
	return (Result)r;
}
#endif

Result SaltySD_Memcpy(uintptr_t to, uintptr_t from, size_t size)
{
	struct ReqMemcpy { 
		u64 to; 
		u64 from; 
		u64 size;
	} b{to, from, size};
	static_assert(RawSize<ReqMemcpy> == 40);
	u64 r;
	Transact({handleService_Memcpy, RawSize<ReqMemcpy>, sizeof(b), 0, 0, &b}, r);
	return (Result)r;
}

Result SaltySD_Exception()
{
	u64 r;
	Transact({handleService_Exception, RawSize<Empty>, 0, 0, 0, nullptr}, r);
	return (Result)r;
}

Result SaltySD_GetSDCard(Handle *retrieve)
{
	u64 r;
	Handle h;
	if (Transact({handleService_GetSDCard, RawSize<Empty>, 0, 0, 0, nullptr}, r, nullptr, &h) && !r)
	{
		*retrieve = h;

		// Init fs stuff
		FsFileSystem sdcardfs;
		#if defined(SWITCH32) || defined(OUNCE32)
		sdcardfs.s.handle = *retrieve;
		#else
		sdcardfs.s.own_handle = *retrieve;
		#endif
		int dev = fsdevMountDevice("sdmc", sdcardfs);
		setDefaultDevice(dev);

		SaltySDCore_printf(MODULE_NAME ": got SD card handle %x\n", h);
	}
	return (Result)r;
}

Result SaltySD_CheckIfSharedMemoryAvailable(ptrdiff_t *new_offset, size_t new_size)
{
	struct ReqShmem { 
		u64 size; 
	} b{new_size};
	static_assert(RawSize<ReqShmem> == 24);

	u64 r;
	u64 offset = 0;
	if (Transact({handleService_CheckIfSharedMemoryAvailable, RawSize<ReqShmem>,
	              sizeof(ReqShmem), sizeof(offset), 0, &b}, r, &offset))
		*new_offset = r ? -1 : (ptrdiff_t)offset;
	return (Result)r;
}

Result SaltySD_GetSharedMemoryHandle(Handle *retrieve)
{
	u64 r;
	Handle h;
	if (Transact({handleService_GetSharedMemoryHandle, RawSize<Empty>, 0, 0, 0, nullptr}, r, nullptr, &h) && !r)
	{
		SaltySDCore_printf(MODULE_NAME ": got SharedMemory handle %x\n", h);
		*retrieve = h;
	}
	return (Result)r;
}

Result SaltySD_printf(const char* format, ...)
{
	char tmp[256];

	va_list args;
	va_start(args, format);
	npf_vsnprintf(tmp, 256, format, args);
	va_end(args);

	// cmd_id was the literal 5 in the original, i.e. handleService_Log.
	u64 r;
	Transact({handleService_Log, RawSize<Empty>, 0, 0, F_SendBuf, nullptr, tmp, strlen(tmp) + 1}, r);
	return (Result)r;
}

u64 SaltySD_GetBID()
{
	u64 r;
	if (!Transact({handleService_GetBID, RawSize<Empty>, 0, 0, 0, nullptr}, r)) return 0;

	if (r) {
		#if defined(SWITCH32) || defined(OUNCE32)
		SaltySDCore_printf(MODULE_NAME ": BID: %016llX\n", r);
		#else
		SaltySDCore_printf(MODULE_NAME ": BID: %016lX\n", r);
		#endif
		return r;
	}

	SaltySDCore_printf(MODULE_NAME ": getBID failed!\n");
	return 0;
}

static Result SetSync(handleService cmd, bool isTrue)
{
	struct ReqValue { 
		u64 value;
	} b{isTrue};
	static_assert(RawSize<ReqValue> == 24);

	u64 r;
	Transact({cmd, RawSize<ReqValue>, sizeof(ReqValue), 0, 0, &b}, r);
	return (Result)r;
}

Result SaltySD_SetDisplaySyncDocked(bool isTrue) { 
	return SetSync(handleService_SetDisplaySyncDocked, isTrue);
}
Result SaltySD_SetDisplaySync(bool isTrue)       {
	return SetSync(handleService_SetDisplaySync, isTrue);
}

FILE* SaltySDCore_fopen(const char* filename, const char* mode)
{
	struct ReqMode { 
		char mode[4]; 
	} b{};
	static_assert(RawSize<ReqMode> == 24);
	strncpy(b.mode, mode, 3);

	u64 r; 
	u32 id = 0;
	if (Transact({handleService_SdcardFopen, RawSize<ReqMode>, sizeof(b), sizeof(id),
	              F_SendBuf, &b, filename, strlen(filename) + 1}, r, &id) && !r)
		return (FILE*)(uintptr_t)id;
	return nullptr;
}

size_t SaltySDCore_fread(void* ptr, size_t size, size_t count, FILE* stream)
{
	struct ReqSized {
		u64 size; 
		u64 count; 
		u32 id;
	} b{size, count, (u32)(uintptr_t)stream};
	static_assert(RawSize<ReqSized> == 40);

	u64 r;
	u64 got = 0;
	if (Transact({handleService_SdcardFread, RawSize<ReqSized>, sizeof(b), sizeof(got),
	              F_RecvBuf, &b, ptr, size * count}, r, &got) && !r)
		return (size_t)got;
	return 0;
}

int SaltySDCore_fclose(FILE* stream)
{
	u32 id = (u32)(uintptr_t)stream;
	u64 r;
	if (Transact({handleService_SdcardFclose, RawSize<u32>, sizeof(id), 0, 0, &id}, r))
		return (int)r;
	return EOF;
}

int SaltySDCore_fseek(FILE* stream, int64_t offset, int origin)
{
	struct ReqSeek {
		s64 offset;
		int origin;
		u32 id;
	} b{offset, origin, (u32)(uintptr_t)stream};

	u64 r;
	if (Transact({handleService_SdcardFseek, RawSize<ReqSeek>, sizeof(b), 0, 0, &b}, r))
		return (int)r;
	return EOF;
}

size_t SaltySDCore_ftell(FILE* stream)
{
	u32 b = (u32)(uintptr_t)stream;
	u64 r;
	u64 off = 0;
	if (Transact({handleService_SdcardFtell, RawSize<u32>, sizeof(b), sizeof(off), 0, &b}, r, &off) && !r)
		return (size_t)off;
	return (size_t)-1;
}

int SaltySDCore_remove(const char* filename)
{
	u64 r;
	if (Transact({handleService_SdcardRemove, RawSize<Empty>, 0, 0, F_SendBuf,
	              nullptr, filename, strlen(filename) + 1}, r))
		return (int)r;
	return 1;
}

size_t SaltySDCore_fwrite(const void* ptr, size_t size, size_t count, FILE* stream)
{
	struct ReqSized {
		u64 size; 
		u64 count; 
		u32 id;
	} b{size, count, (u32)(uintptr_t)stream};
	static_assert(RawSize<ReqSized> == 40);

	u64 r;
	u64 put = 0;
	if (Transact({handleService_SdcardFwrite, RawSize<ReqSized>, sizeof(b), sizeof(put),
	              F_SendBuf, &b, ptr, size * count}, r, &put) && !r)
		return (size_t)put;
	return 0;
}

DIR* SaltySDCore_opendir(const char* dirname)
{
	u64 r;
	u32 id = 0;
	if (Transact({handleService_SdcardOpendir, RawSize<Empty>, 0, sizeof(id), F_SendBuf,
	              nullptr, dirname, strlen(dirname) + 1}, r, &id) && !r)
		return (DIR*)(uintptr_t)id;
	return nullptr;
}

int SaltySDCore_mkdir(const char* dirname, mode_t mode = (mode_t)777)
{
	u64 r;
	if (Transact({handleService_SdcardMkdir, RawSize<Empty>, 0, 0, F_SendBuf,
	              nullptr, dirname, strlen(dirname) + 1}, r))
		return (int)r;
	return 1;
}

#if defined(SWITCH) || defined(OUNCE)
static_assert(sizeof(ino_t) == 2);
#endif

struct dirent output = {};

struct dirent* SaltySDCore_readdir(DIR* dirp)
{
	u32 b = (u32)(uintptr_t)dirp;
	u64 r;
	if (Transact({handleService_SdcardReaddir, RawSize<u32>, sizeof(b), 0, F_RecvBuf,
	              &b, &output, sizeof(output)}, r) && !r)
	{
		#if defined(SWITCH32) || defined(OUNCE32)
		struct direntSalty {
			uint16_t       d_ino;
			unsigned char  d_type;
			char           d_name[NAME_MAX+1];
		};
		struct direntSalty* dir = (struct direntSalty*)&output;
		memmove(&output.d_name, dir->d_name, NAME_MAX+1);
		output.d_type = dir->d_type;
		output.d_ino  = dir->d_ino;
		#endif
		return &output;
	}
	return nullptr;
}

int SaltySDCore_closedir(DIR *dirp)
{
	u32 b = (u32)(uintptr_t)dirp;
	u64 r;
	if (Transact({handleService_SdcardClosedir, RawSize<u32>, sizeof(b), 0, 0, &b}, r))
		return (int)r;
	return 1;
}

} // extern "C"