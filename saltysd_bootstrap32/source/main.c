#include <switch_min.h>

#include <string.h>
#include <stdio.h>

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

Result saltySDLoadELF(Handle salt, u32 heap, u32* elf_addr, u32* elf_size, char* name)
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

u32 g_heapAddr;
size_t g_heapSize;

void setupAppHeap(void)
{
	void* addr = 0;
	Result rc = 0;

	rc = svcSetHeapSize(&addr, 0x200000);

	g_heapAddr = (u32)addr;
	g_heapSize = 0x200000;
}

Result rc_check = 0;

int main(int argc, char *argv[])
{
	Result ret;
	Handle saltysd;
	
	setupAppHeap();
	
	svcConnectToNamedPort(&saltysd, "SaltySD");

	u32 new_addr, new_size;
	ret = saltySDLoadELF(saltysd, g_heapAddr, &new_addr, &new_size, "saltysd_core32.elf");
	if (ret) goto fail;
	
	ret = saltySDTerm(saltysd);
	if (ret) goto fail;
	
	__saltysd_exit_func = (void*)new_addr;

	svcCloseHandle(saltysd);

fail:
	return 0;
}

