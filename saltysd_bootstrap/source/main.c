#include <switch_min.h>

#include <string.h>
#include <stdio.h>

#if defined(SWITCH) || defined(OUNCE)
#include "printf.h"

#define write_log(...) \
	{char log_buf[0x200]; snprintf_(log_buf, 0x200, __VA_ARGS__); \
	svcOutputDebugString(log_buf, _strlen(log_buf));}

#endif

void* __saltysd_exit_func = svcExitProcess;

Result saltySDTerm(Handle salt)
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

	ret = ipcDispatch(salt);

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

Result saltySDLoadELF(Handle salt, uintptr_t heap, uintptr_t* elf_addr, size_t* elf_size, char* name)
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
	memcpy(raw->name, name, 63);

	ret = ipcDispatch(salt);

	if (R_SUCCEEDED(ret)) 
	{
		IpcParsedCommand r;
		ipcParse(&r);

		struct respond {
			u64 magic;
			u64 result;
			u64 elf_addr;
			u64 elf_size;
		} *resp = (struct respond*)r.Raw;

		ret = resp->result;
		*elf_addr = resp->elf_addr;
		*elf_size = resp->elf_size;
	}

	return ret;
}

uintptr_t g_heapAddr;
size_t g_heapSize;

void setupAppHeap(void)
{
	void* addr = 0;
	Result rc = 0;

	rc = svcSetHeapSize(&addr, 0x200000);

	if (rc || addr == 0)
	{
		#if defined(SWITCH) || defined(OUNCE)
		write_log("SaltySD Bootstrap: svcSetHeapSize failed with err %x\n", rc);
		#endif
	}

	g_heapAddr = (uintptr_t)addr;
	g_heapSize = 0x200000;
}

int main(int argc, char *argv[])
{
	Result ret;
	Handle saltysd;

	#if defined(SWITCH) || defined(OUNCE)
	write_log("SaltySD Bootstrap: we in here\n");
	#endif
	
	setupAppHeap();
	
	do
	{
		ret = svcConnectToNamedPort(&saltysd, "SaltySD");
		svcSleepThread(1000*1000);
	}
	while (ret);

	#if defined(SWITCH) || defined(OUNCE)
	write_log("SaltySD Bootstrap: Got handle %x, loading ELF...\n", saltysd);
	#endif
	uintptr_t new_addr;
	size_t new_size;
	#if defined(SWITCH32) || defined(OUNCE32)
	ret = saltySDLoadELF(saltysd, g_heapAddr, &new_addr, &new_size, "saltysd_core32.elf");
	#else
	ret = saltySDLoadELF(saltysd, g_heapAddr, &new_addr, &new_size, "saltysd_core.elf");
	#endif
	if (ret) goto fail;
	
	ret = saltySDTerm(saltysd);
	if (ret) goto fail;
	
	#if defined(SWITCH) || defined(OUNCE)
	write_log("SaltySD Bootstrap: ELF loaded to %p\n", (void*)new_addr);
	#endif
	__saltysd_exit_func = (void*)new_addr;

	svcCloseHandle(saltysd);

	return 0;

fail:
	#if defined(SWITCH) || defined(OUNCE)
	write_log("SaltySD Bootstrap: failed with retcode %x\n", ret);
	#endif
	return 0;
}

