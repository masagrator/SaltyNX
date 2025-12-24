#include "saltysd_ipc.h"

#include <switch_min.h>
#include <stdarg.h>

#include "saltysd_core.h"
#include "useful.h"
#include "nanoprintf.h"

Handle saltysd;

void SaltySD_Init()
{
	Result ret;

	for (int i = 0; i < 200; i++)
	{
		ret = svcConnectToNamedPort(&saltysd, "SaltySD");
		svcSleepThread(1000*1000);
		
		if (!ret) break;
	}
	
	//debug_log("SaltySD Core: Got handle %x\n", saltysd);
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
	raw->cmd_id = 0;
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
	raw->cmd_id = 2;
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
		u64 reserved[2];
	} *raw;

	raw = ipcPrepareHeader(&c, sizeof(*raw));

	raw->magic = SFCI_MAGIC;
	raw->cmd_id = 1;
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
	raw->cmd_id = 3;
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
	raw->cmd_id = 9;

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
	raw->cmd_id = 4;

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
			sdcardfs.s.handle = *retrieve;
			int dev = fsdevMountDevice("sdmc", sdcardfs);
			setDefaultDevice(dev);

			SaltySDCore_printf("SaltySD Core: got SD card handle %x\n", r.Handles[0]);
		}
	}
	
	return ret;
}

Result SaltySD_print(char* out)
{
	Result ret;
	IpcCommand c;

	ipcInitialize(&c);

	struct 
	{
		u64 magic;
		u64 cmd_id;
		char log[65];
		u64 reserved[2];
	} *raw;

	raw = ipcPrepareHeader(&c, sizeof(*raw));

	raw->magic = SFCI_MAGIC;
	raw->cmd_id = 5;
	strncpy(raw->log, out, 64);

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
	raw->cmd_id = 6;
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
	raw->cmd_id = 7;

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
	
	int i = 0;
	while(i < strlen(tmp)) {
		ret = SaltySD_print(tmp + i);
		i += 64;

		if (ret) {
			i = 0;
			break;
		}
	}

	if (ret)
	{	
		#if defined(SWITCH32) || defined(OUNCE32)
		debug_log("SaltySD Core: failed w/ error %lx, msg: %s", ret, tmp);
		#else
		debug_log("SaltySD Core: failed w/ error %x, msg: %s", ret, tmp);
		#endif
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
	raw->cmd_id = 8;

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
	raw->cmd_id = 18;
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
	raw->cmd_id = 12;
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