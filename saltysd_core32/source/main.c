#include <switch_min.h>

#include "NX-FPS.h"
#include "ReverseNX.h"

#include <dirent.h>
#include <switch_min/kernel/ipc.h>
#include <switch_min/runtime/threadvars.h>
#include <stdlib.h>

#include "useful.h"
#include "saltysd_ipc.h"
#include "saltysd_core.h"
#include "saltysd_dynamic.h"

#include "bm.h"

u32 __nx_applet_type = AppletType_None;

static char g_heap[0x20000];

extern void __nx_exit_clear(void* ctx, Handle thread, void* addr);
extern void elf_trampoline(void* context, Handle thread, void* func);
void* __stack_tmp;

Handle orig_main_thread;
void* orig_ctx;

Handle sdcard;
size_t elf_area_size = 0x200000; //We assume that Core itself won't take more than 0x200000 bytes;

ThreadVars vars_orig;
ThreadVars vars_mine;

uint64_t tid = 0;

static uint32_t sharedOperationMode = 0;
static bool focusModeChanged = false;

typedef double (*strtod_0)(const char* str, char** endptr);
uintptr_t strtod_ptr = 0;

double strtod(const char* str, char** endptr) {
	return ((strtod_0)(strtod_ptr))(str, endptr);
}

void __libnx_init(void* ctx, Handle main_thread, void* saved_lr)
{
	extern char* fake_heap_start;
	extern char* fake_heap_end;

	fake_heap_start = &g_heap[0];
	fake_heap_end   = &g_heap[sizeof g_heap];

	orig_ctx = ctx;
	orig_main_thread = main_thread;
	
	// Hacky TLS stuff, TODO: just stop using libnx t b h
	vars_mine.magic = 0x21545624;
	vars_mine.handle = main_thread;
	vars_mine.thread_ptr = NULL;
	vars_mine.reent = _impure_ptr;
	vars_mine.tls_tp = (void*)malloc(0x1000);
	vars_orig = *getThreadVars();
	*getThreadVars() = vars_mine;
	virtmemSetup();
}

void __attribute__((weak)) __libnx_exit(int rc)
{
	fsdevUnmountAll();
	
	// Restore TLS stuff
	*getThreadVars() = vars_orig;
	
	u32 addr = SaltySDCore_getCodeStart();

	__nx_exit_clear(orig_ctx, orig_main_thread, (void*)addr);
}

u64  g_heapAddr;
size_t g_heapSize;

void SaltySDCore_LoadPatches (bool Aarch64) {
	char tmp4[256] = "";
	char tmp2[256] = "";
	char instr[256] = "";
	DIR *d;
	struct dirent *dir;
	
	SaltySDCore_printf("SaltySD Patcher: Searching patches in dir '/'...\n");
	
	snprintf(tmp4, 0x100, "sdmc:/SaltySD/patches/");

	d = opendir(tmp4);
	if (d)
	{
		while ((dir = readdir(d)) != NULL)
		{
			char *dot = strrchr(dir->d_name, '.');
			if (dot && !strcmp(dot, ".asm32")) {
				snprintf(tmp2, 0x100, "%s%s", tmp4, dir->d_name);
				SaltySDCore_printf("SaltySD Patcher: Found %s\n", dir->d_name);
				FILE* patch = fopen(tmp2, "rb");
				fseek(patch, 0, SEEK_END);
				uint32_t size = ftell(patch);
				fseek(patch, 0, SEEK_SET);
				//Test if filesize is valid
				if (size % 4 != 0) {
					fclose(patch);
					SaltySDCore_printf("%s doesn't have valid filesize...\n", tmp2);
					break;
				}
				fread(&instr, 1, size, patch);
				fclose(patch);
				char* filename = dir->d_name;
				uint8_t namelen = strlen(filename);
				filename[namelen - 6] = 0;
				uint32_t position = SaltySDCore_FindSymbol(filename);
				if (position) {
					SaltySDCore_printf("SaltySD Patcher: Symbol Position: %08lx\n", position);
					SaltySD_Memcpy(position, (uint32_t)instr, size);
				}
				else {
					SaltySDCore_printf("SaltySD Patcher: Symbol Position: not found\n", position);
				}
			}
		}
		closedir(d);
	}

	svcGetInfo(&tid, 18, CUR_PROCESS_HANDLE, 0);
	
	SaltySDCore_printf("SaltySD Patcher: Searching patches in dir '/%016llx'...\n", tid);
	
	snprintf(tmp4, 0x100, "sdmc:/SaltySD/patches/%016llx/", tid);

	d = opendir(tmp4);
	if (d)
	{
		while ((dir = readdir(d)) != NULL)
		{
			char *dot = strrchr(dir->d_name, '.');
			if (dot && !strcmp(dot, ".asm32")) {
				snprintf(tmp2, 0x100, "%s%s", tmp4, dir->d_name);
				SaltySDCore_printf("SaltySD Patcher: Found %s\n", dir->d_name);
				FILE* patch = fopen(tmp2, "rb");
				fseek(patch, 0, SEEK_END);
				uint32_t size = ftell(patch);
				fseek(patch, 0, SEEK_SET);
				//Test if filesize is valid
				if (size % 4 != 0) {
					fclose(patch);
					SaltySDCore_printf("%s doesn't have valid filesize...\n", tmp2);
					break;
				}
				fread(&instr, 1, size, patch);
				fclose(patch);
				char* filename = dir->d_name;
				uint8_t namelen = strlen(filename);
				filename[namelen - 6] = 0;
				uint32_t position = SaltySDCore_FindSymbol(filename);
				if (position) {
					SaltySDCore_printf("SaltySD Patcher: Symbol Position: %08lx\n", position);
					SaltySD_Memcpy(position, (uint32_t)instr, size);
				}
				else {
					SaltySDCore_printf("SaltySD Patcher: Symbol Position: not found\n", position);
				}
			}
		}
		closedir(d);
	}
	
	return;
}

extern void _start();

void SaltySDCore_RegisterExistingModules()
{
	u64 addr = 0;
	while (1)
	{
		MemoryInfo info;
		u32 pageinfo;
		Result ret = svcQueryMemory(&info, &pageinfo, addr);
		
		if (info.perm == Perm_Rx)
		{
			SaltySDCore_RegisterModule((void*)((u32)(info.addr)));
			u32 compaddr = ((u32)(info.addr));
			if ((u32*)compaddr != (u32*)_start)
				SaltySDCore_RegisterBuiltinModule((void*)((u32)(info.addr)));
		}

		addr = (u32)(info.addr) + (u32)(info.size);
		
		if (!addr || ret) break;
	}
	
	return;
}

Result svcSetHeapSizeIntercept(uintptr_t *out, size_t size)
{	
	size_t addon = ((elf_area_size+0x1FFFFF) & ~0x1FFFFF);
	Result ret = svcSetHeapSize((void*)out, size+addon);
	
	//SaltySDCore_printf("SaltySD Core: svcSetHeapSize intercept %x %llx %llx\n", ret, *out, size+((elf_area_size+0x200000) & 0xffe00000));
	
	if (!ret)
	{
		*out += addon;
	}
	
	return ret;
}

Result svcGetInfoIntercept (u64 *out, size_t id0, Handle handle, u64 id1)	
{	

	Result ret = svcGetInfo(out, id0, handle, id1);	

	//SaltySDCore_printf("SaltySD Core: svcGetInfo intercept %p (%llx) %llx %x %llx ret %x\n", out, *out, id0, handle, id1, ret);	

	if (id1 == 0 && handle == CUR_PROCESS_HANDLE)	
	{	
		switch(id0) {
			case InfoType_HeapRegionAddress:
				*out += ((elf_area_size+0x1FFFFF) & ~0x1FFFFF);
				break;
		}
	}

	return ret;	
}

void SaltySDCore_PatchSVCs()
{
	static u8 orig_1[] = {0x04, 0x00, 0x2D, 0xE5, 0x01, 0x00, 0x00, 0xEF, 0x00, 0x20, 0x9D, 0xE5}; //PUSH {r0}; SVC #0x1; LDR r2, [sp]
	static u8 orig_2[] = {0x04, 0x00, 0x2D, 0xE5, 0x04, 0x00, 0x9D, 0xE5, 0x08, 0x30, 0x9D, 0xE5, 0x29, 0x00, 0x00, 0xEF, 0x00, 0x30, 0x9D, 0xE5}; //PUSH {R0}; LDR r0, [sp, #4]; LDR r3, [sp, #8]; SVC 0x29; LDR r3, [sp]
	static u8 orig_1_alt[] = {0x04, 0x00, 0x2D, 0xE5, 0x01, 0x00, 0x00, 0xEF}; //PUSH {r0}; SVC #0x1
	static u8 orig_2_alt[] = {0x04, 0x00, 0x2D, 0xE5, 0x04, 0x00, 0x9D, 0xE5, 0x08, 0x30, 0x9D, 0xE5, 0x29, 0x00, 0x00, 0xEF}; //PUSH {R0}; LDR r0, [sp, #4]; LDR r3, [sp, #8]; SVC 0x29
	static u8 patch[0x8] = {0x04, 0xF0, 0x1F, 0xE5, 0xDE, 0xAD, 0xBE, 0xEF}; // LDR pc, [pc, #-4]; 0xDEADBEEF
	u32 dst_1 = SaltySDCore_findCodeEx(orig_1, sizeof(orig_1));
	u32 dst_2 = SaltySDCore_findCodeEx(orig_2, sizeof(orig_2));
	if (!dst_1) {
		dst_1 = SaltySDCore_findCodeEx(orig_1_alt, sizeof(orig_1_alt));
	}
	if (!dst_2) {
		dst_2 = SaltySDCore_findCodeEx(orig_2_alt, sizeof(orig_2_alt));
	}
	
	if (!dst_1 || !dst_2)
	{
		SaltySDCore_printf("SaltySD Core: Failed to find svcSetHeapSize! %llx\n", dst_1);
		return;
	}

	*(u32*)&patch[4] = (u32)svcSetHeapSizeIntercept;
	SaltySD_Memcpy((u64)dst_1, (u64)patch, 8);
	*(u32*)&patch[4] = (u32)svcGetInfoIntercept;
	SaltySD_Memcpy((u64)dst_2, (u64)patch, 8);		
}

typedef void (*nnosQueryMemoryInfo)(void* memoryinfo);
uintptr_t Address_weak_QueryMemoryInfo = 0;

void QueryMemoryInfo(void* memoryinfo) {
	static bool initialized = false;
	if (!initialized) {
		void** builtin_elfs = NULL;
		uint32_t num_builtin_elfs = 0;

		struct ReplacedSymbol* replaced_symbols = NULL;
		int32_t num_replaced_symbols = 0;

		SaltySDCore_getDataForUpdate(&num_builtin_elfs, &num_replaced_symbols, &replaced_symbols, &builtin_elfs);

		for (uint32_t i = 0; i < num_builtin_elfs; i++) {
			for (int x = 0; x < num_replaced_symbols; x++) {
				SaltySDCore_ReplaceModuleImport(builtin_elfs[i], replaced_symbols[x].name, replaced_symbols[x].address, true);
			}
		}
		initialized = true;
	}
	return ((nnosQueryMemoryInfo)(Address_weak_QueryMemoryInfo))(memoryinfo);
}

typedef double (*vsnprintf_0)(char * s, size_t n, const char * format, va_list arg);
uint32_t vsnprintf_ptr = 0;

int main(int argc, char *argv[])
{
	Result ret;

	SaltySDCore_RegisterExistingModules();

	strtod_ptr = SaltySDCore_FindSymbolBuiltin("strtod");
	
	SaltySD_Init();
	
	ret = SaltySD_GetSDCard(&sdcard);
	if (ret) goto fail;

	SaltySDCore_printf("SaltySD Core32 " APP_VERSION ": restoring code...\n");

	ret = SaltySD_Restore();
	if (ret) goto fail;

	SaltySDCore_PatchSVCs();
	SaltySDCore_LoadPatches(false);

	SaltySDCore_fillRoLoadModule();
	SaltySDCore_ReplaceImport("_ZN2nn2ro10LoadModuleEPNS0_6ModuleEPKvPvmi", (void*)LoadModule);

	SaltySDCore_printf("SaltySD Core32: Plugins are not supported...\n");

	ptrdiff_t SMO = -1;
	ret = SaltySD_CheckIfSharedMemoryAvailable(&SMO, 1);
	SaltySDCore_printf("SaltySD_CheckIfSharedMemoryAvailable ret: 0x%lX\n", ret);
	if (R_SUCCEEDED(ret)) {
		SharedMemory _sharedmemory = {};
		Handle remoteSharedMemory = 0;
		Result shmemMapRc = -1;
		SaltySD_GetSharedMemoryHandle(&remoteSharedMemory);
		shmemLoadRemote(&_sharedmemory, remoteSharedMemory, 0x1000, Perm_Rw);
		shmemMapRc = shmemMap(&_sharedmemory);
		if (R_SUCCEEDED(shmemMapRc)) {
			NX_FPS(&_sharedmemory, &sharedOperationMode, &focusModeChanged);

			uint64_t titid = 0;
			svcGetInfo(&titid, 18, CUR_PROCESS_HANDLE, 0);
			ReverseNX(&_sharedmemory, &sharedOperationMode, &focusModeChanged);
		}
		else {
			SaltySDCore_printf("SaltySD Core: shmemMap failed: 0x%lX\n", shmemMapRc);
		}
	}

	ret = SaltySD_Deinit();
	if (ret) goto fail;

	/*
	for (size_t i = INT32_MAX; i > 0; i--) {
		svcSleepThread(i);
	}
	*/
	

	__libnx_exit(0);

fail:
	debug_log("SaltySD Core: failed with retcode %lx\n", ret);
	SaltySDCore_printf("SaltySD Core: failed with retcode %lx\n", ret);
	__libnx_exit(0);
}
