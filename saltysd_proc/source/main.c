#include <switch_min.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <inttypes.h>
#include <switch_min/kernel/svc_extra.h>
#include <switch_min/kernel/ipc.h>
#include "saltysd_bootstrap_elf.h"

#include "spawner_ipc.h"

#include "loadelf.h"
#include "useful.h"

#define MODULE_SALTYSD 420

u32 __nx_applet_type = AppletType_None;

void serviceThread(void* buf);

Handle saltyport, sdcard, injectserv;
static char g_heap[0xB0000];
bool should_terminate = false;
bool already_hijacking = false;
DebugEventInfo eventinfo;
bool check = false;
u64 exception = 0x0;
SharedMemory _sharedMemory = {};
size_t reservedSharedMemory = 0;

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

void renametocheatstemp() {
	char cheatspath[60];
	char cheatspathtemp[64];
	TIDnow = eventinfo.tid;
	snprintf(cheatspath, sizeof cheatspath, "sdmc:/Atmosphere/titles/%016"PRIx64"/cheats", TIDnow);
	snprintf(cheatspathtemp, sizeof cheatspathtemp, "%stemp", cheatspath);
	rename(cheatspath, cheatspathtemp);
	snprintf(cheatspath, sizeof cheatspath, "sdmc:/Atmosphere/contents/%016"PRIx64"/cheats", TIDnow);
	snprintf(cheatspathtemp, sizeof cheatspathtemp, "%stemp", cheatspath);
	rename(cheatspath, cheatspathtemp);
	check = true;
	return;
}

void renametocheats() {
	char cheatspath[60];
	char cheatspathtemp[64];
	snprintf(cheatspath, sizeof cheatspath, "sdmc:/Atmosphere/titles/%016"PRIx64"/cheats", TIDnow);
	snprintf(cheatspathtemp, sizeof cheatspathtemp, "%stemp", cheatspath);
	rename(cheatspathtemp, cheatspath);
	snprintf(cheatspath, sizeof cheatspath, "sdmc:/Atmosphere/contents/%016"PRIx64"/cheats", TIDnow);
	snprintf(cheatspathtemp, sizeof cheatspathtemp, "%stemp", cheatspath);
	rename(cheatspathtemp, cheatspath);
	check = false;
	return;
}

void hijack_bootstrap(Handle* debug, u64 pid, u64 tid)
{
	ThreadContext context;
	Result ret;

	reservedSharedMemory = 0;
	
	ret = svcGetDebugThreadContext(&context, *debug, tid, RegisterGroup_All);
	if (ret)
	{
		SaltySD_printf("SaltySD: svcGetDebugThreadContext returned %x, aborting...\n", ret);
		
		svcCloseHandle(*debug);
		return;
	}
	
	// Load in the ELF
	//svcReadDebugProcessMemory(backup, debug, context.pc.x, 0x1000);
	u8* elf = malloc(saltysd_bootstrap_elf_size);
	memcpy(elf, saltysd_bootstrap_elf, saltysd_bootstrap_elf_size);
	
	uint64_t new_start;
	load_elf_debug(*debug, &new_start, elf, saltysd_bootstrap_elf_size);
	free(elf);

	// Set new PC
	context.pc.x = new_start;
	ret = svcSetDebugThreadContext(*debug, tid, &context, RegisterGroup_All);
	if (ret)
	{
		SaltySD_printf("SaltySD: svcSetDebugThreadContext returned %x!\n", ret);
	}
	 
	svcCloseHandle(*debug);
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
	char titleidnumF[20];

	while (1)
	{
		ret = svcGetDebugEventInfo(&eventinfo, debug);
		if (check == false) {
			exception = 0;
			renametocheatstemp();
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
				SaltySD_printf("SaltySD: TID %016llx is a system application, aborting bootstrap...\n", eventinfo.tid);
				goto abort_bootstrap;
			}
			if (eventinfo.tid > 0x01FFFFFFFFFFFFFF || (eventinfo.tid & 0x1F00) != 0)
			{
				SaltySD_printf("SaltySD: TID %016llx is a homebrew application, aborting bootstrap...\n", eventinfo.tid);
				goto abort_bootstrap;
			}
			if (!eventinfo.isA64)
			{
				SaltySD_printf("SaltySD: ARM32 applications plugins are not supported, aborting bootstrap...\n");
				goto abort_bootstrap;
			}
			char* hbloader = "hbloader";
			if (strcasecmp(eventinfo.name, hbloader) == 0)
			{
				SaltySD_printf("SaltySD: Detected title replacement mode, aborting bootstrap...\n");
				goto abort_bootstrap;
			}
			
			snprintf(titleidnum, sizeof titleidnum, "%016"PRIx64, eventinfo.tid);
			snprintf(titleidnumF, sizeof titleidnumF, "X%016"PRIx64, eventinfo.tid);
			
			FILE* except = fopen("sdmc:/SaltySD/exceptions.txt", "r");
			if (except) {
				while (fgets(line, sizeof(line), except)) {
					snprintf(exceptions, sizeof exceptions, "%s", line); 
					if (!strncasecmp(exceptions, titleidnumF, 17)) {
						SaltySD_printf("SaltySD: TID %016llx is forced in exceptions.txt, aborting bootstrap...\n", eventinfo.tid);
						fclose(except);
						goto abort_bootstrap;
					}
					else if (!strncasecmp(exceptions, titleidnum, 16)) {
						SaltySD_printf("SaltySD: TID %016llx is in exceptions.txt, aborting loading plugins...\n", eventinfo.tid);
						exception = 0x1;
					}
				}
				fclose(except);
			}
			SaltySD_printf("SaltySD: found valid AttachProcess event:\n");
			SaltySD_printf("		 tid %016llx pid %016llx\n", eventinfo.tid, eventinfo.pid);
			SaltySD_printf("		 name %s\n", eventinfo.name);
			SaltySD_printf("		 isA64 %01x addrSpace %01x enableDebug %01x\n", eventinfo.isA64, eventinfo.addrSpace, eventinfo.enableDebug);
			SaltySD_printf("		 enableAslr %01x useSysMemBlocks %01x poolPartition %01x\n", eventinfo.enableAslr, eventinfo.useSysMemBlocks, eventinfo.poolPartition);
			SaltySD_printf("		 exception %016llx\n", eventinfo.userExceptionContextAddr);
		}
		else
		{
			SaltySD_printf("SaltySD: debug event %x, passing...\n", eventinfo.type);
			continue;
		}
	}
	// Poll for new threads (svcStartProcess) while stuck in debug
	
	do
	{
		ret = svcGetThreadList(&threads, tids, 0x200, debug);
		svcSleepThread(-1);
	}
	while (!threads);
	renametocheats();
	
	hijack_bootstrap(&debug, pid, tids[0]);
	
	free(tids);
	return;

abort_bootstrap:
	disable = 0;
	free(tids);
	renametocheats();
				
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
		if (elf_data && elf_size)
			ret = load_elf_proc(proc, r.Pid, heap, &new_start, &new_size, elf_data, elf_size);
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

		SaltySD_printf("SaltySD: cmd 3 handler, memcpy(%llx, %llx, %llx)\n", to, from, size);
		
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

		return 0;
	}
	else if (cmd == 4) // GetSDCard
	{		
		SaltySD_printf("SaltySD: cmd 4 handler\n");

		ipcSendHandleCopy(&c, sdcard);
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
	//SaltySD_printf("SaltySD: accepting service calls\n");
	should_terminate = false;

	while (1)
	{
		Handle session;
		ret = svcAcceptSession(&session, saltyport);
		if (ret && ret != 0xf201)
		{
			//SaltySD_printf("SaltySD: svcAcceptSession returned %x\n", ret);
		}
		else if (!ret)
		{
			//SaltySD_printf("SaltySD: session %x being handled\n", session);

			int handle_index;
			Handle replySession = 0;
			while (1)
			{
				ret = svcReplyAndReceive(&handle_index, &session, 1, replySession, U64_MAX);
				
				if (should_terminate) break;
				
				//SaltySD_printf("SaltySD: IPC reply ret %x, index %x, sess %x\n", ret, handle_index, session);
				if (ret) break;
				
				IpcParsedCommand r;
				ipcParse(&r);

				struct {
					u64 magic;
					u64 command;
					u64 reserved[2];
				} *resp = r.Raw;

				handleServiceCmd(resp->command);
				
				if (should_terminate) break;

				replySession = session;
				svcSleepThread(1000*1000);
			}
			
			svcCloseHandle(session);
		}

		if (should_terminate) break;
		
		svcSleepThread(1000*1000*100);
	}
	
	//SaltySD_printf("SaltySD: done accepting service calls\n");
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
	smExit();
	smInitialize();
	FsFileSystem sdcardfs;
	sdcardfs.s.handle = sdcard;
	fsdevMountDevice("sdmc", sdcardfs);
	SaltySD_printf("SaltySD: got SD card.\n");

	// Start our port
	// For some reason, we only have one session maximum (0 reslimit handle related?)	
	svcManageNamedPort(&saltyport, "SaltySD", 1);
	svcManageNamedPort(&injectserv, "InjectServ", 1);

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

		// Detected new PID
		if (max != old_max && max > 0x80)
		{
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

		svcSleepThread(50*1000*1000);
	}
	free(pids);
	
	fsdevUnmountAll();
	fsExit();
	smExit();

	return 0;
}

