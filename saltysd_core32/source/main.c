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
size_t elf_area_size = 0;

ThreadVars vars_orig;
ThreadVars vars_mine;

uint64_t tid = 0;

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
	char* tmp4 = malloc(0x100);
	char* tmp2 = malloc(0x100);
	DIR *d;
	struct dirent *dir;
	char* instr;
	
	SaltySDCore_printf("SaltySD Patcher: Searching patches in dir '/'...\n");
	
	snprintf(tmp4, 0x100, "sdmc:/SaltySD/patches/");

	d = opendir(tmp4);
	if (d)
	{
		while ((dir = readdir(d)) != NULL)
		{
			char *dot = strrchr(dir->d_name, '.');
			if (Aarch64 == true) {
				if (dot && !strcmp(dot, ".asm64")) {
					snprintf(tmp2, 0x100, "%s%s", tmp4, dir->d_name);
					SaltySDCore_printf("SaltySD Patcher: Found %s\n", dir->d_name);
					FILE* patch = fopen(tmp2, "rb");
					fseek(patch, 0, SEEK_END);
					uint32_t size = ftell(patch);
					fseek(patch, 0, SEEK_SET);
					//Test if filesize is valid
					float val = (float)size;
					val = val / 4;
					uint32_t trunc = (uint32_t)val;
					if ((float)trunc != val) {
						fclose(patch);
						SaltySDCore_printf("%s doesn't have valid filesize...\n", tmp2);
						break;
					}
					fread(&instr, 4, trunc, patch);
					fclose(patch);
					char* filename = dir->d_name;
					uint8_t namelen = strlen(filename);
					filename[namelen - 6] = 0;
					uint64_t position = SaltySDCore_FindSymbol(filename);
					SaltySDCore_printf("SaltySD Patcher: Symbol Position: %016llx\n", position);
					SaltySD_Memcpy(position, (uint64_t)&instr, 4*trunc);
				}
			}
			else {
				if (dot && !strcmp(dot, ".asm32")) {
					snprintf(tmp2, 0x100, "%s%s", tmp4, dir->d_name);
					SaltySDCore_printf("SaltySD Patcher: Found %s\n", dir->d_name);
					FILE* patch = fopen(tmp2, "rb");
					fseek(patch, 0, SEEK_END);
					uint32_t size = ftell(patch);
					fseek(patch, 0, SEEK_SET);
					//Test if filesize is valid
					float val = (float)size;
					val = val / 4;
					uint32_t trunc = (uint32_t)val;
					if ((float)trunc != val) {
						fclose(patch);
						SaltySDCore_printf("%s doesn't have valid filesize...\n", tmp2);
						break;
					}
					fread(&instr, 4, trunc, patch);
					fclose(patch);
					char* filename = dir->d_name;
					uint8_t namelen = strlen(filename);
					filename[namelen - 6] = 0;
					uint64_t position = SaltySDCore_FindSymbol(filename);
					SaltySDCore_printf("SaltySD Patcher: Symbol Position: %0l6llx$$$\n", position);
					SaltySD_Memcpy(position, (uint64_t)&instr, 4*trunc);
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
			if (Aarch64 == true) {
				if (dot && !strcmp(dot, ".asm64")) {
					snprintf(tmp2, 0x100, "%s%s", tmp4, dir->d_name);
					SaltySDCore_printf("SaltySD Patcher: Found %s\n", dir->d_name);
					FILE* patch = fopen(tmp2, "rb");
					fseek(patch, 0, SEEK_END);
					uint32_t size = ftell(patch);
					fseek(patch, 0, SEEK_SET);
					//Test if filesize is valid
					float val = (float)size;
					val = val / 4;
					uint32_t trunc = (uint32_t)val;
					if ((float)trunc != val) {
						fclose(patch);
						SaltySDCore_printf("%s doesn't have valid filesize...\n", tmp2);
						break;
					}
					fread(&instr, 4, trunc, patch);
					fclose(patch);
					char* filename = dir->d_name;
					uint8_t namelen = strlen(filename);
					filename[namelen - 6] = 0;
					uint64_t position = SaltySDCore_FindSymbol(filename);
					SaltySDCore_printf("SaltySD Patcher: Symbol Position: %016llx\n", position);
					SaltySD_Memcpy(position, (uint64_t)&instr, 4*trunc);
				}
			}
			else {
				if (dot && !strcmp(dot, ".asm32")) {
					snprintf(tmp2, 0x100, "%s%s", tmp4, dir->d_name);
					SaltySDCore_printf("SaltySD Patcher: Found %s\n", dir->d_name);
					FILE* patch = fopen(tmp2, "rb");
					fseek(patch, 0, SEEK_END);
					uint32_t size = ftell(patch);
					fseek(patch, 0, SEEK_SET);
					//Test if filesize is valid
					float val = (float)size;
					val = val / 4;
					uint32_t trunc = (uint32_t)val;
					if ((float)trunc != val) {
						fclose(patch);
						SaltySDCore_printf("%s doesn't have valid filesize...\n", tmp2);
						break;
					}
					fread(&instr, 4, trunc, patch);
					fclose(patch);
					char* filename = dir->d_name;
					uint8_t namelen = strlen(filename);
					filename[namelen - 6] = 0;
					uint64_t position = SaltySDCore_FindSymbol(filename);
					SaltySDCore_printf("SaltySD Patcher: Symbol Position: %0l6llx$$$\n", position);
					SaltySD_Memcpy(position, (uint64_t)&instr, 4*trunc);
				}
			}
		}
		closedir(d);
	}
	free(tmp4);
	free(tmp2);
	
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

Result svcSetHeapSizeIntercept(u32 *out, u64 size)
{
	static bool Initialized = false;
	Result ret = 1;
	if (!Initialized)
		size += ((elf_area_size+0x200000) & 0xffe00000);
	ret = svcSetHeapSize((void*)out, size);
	
	//SaltySDCore_printf("SaltySD Core: svcSetHeapSize intercept %x %llx %llx\n", ret, *out, size+((elf_area_size+0x200000) & 0xffe00000));
	
	if (!ret && !Initialized)
	{
		*out += ((elf_area_size+0x200000) & 0xffe00000);
		Initialized = true;
	}
	
	return ret;
}

Result svcGetInfoIntercept (u64 *out, u32 id0, Handle handle, u64 id1)	
{	

	Result ret = svcGetInfo(out, id0, handle, id1);	

	//SaltySDCore_printf("SaltySD Core: svcGetInfo intercept %p (%llx) %llx %x %llx ret %x\n", out, *out, id0, handle, id1, ret);	

	if (id0 == 6 && id1 == 0 && handle == 0xffff8001)	
	{	
		*out -= elf_area_size;
	}		

	return ret;	
}

void SaltySDCore_PatchSVCs()
{
	Result ret;
	static u8 orig_1[0x8] = {0x04, 0x00, 0x2D, 0xE5, 0x01, 0x00, 0x00, 0xEF}; //PUSH {r0}; SVC #0x1
	static u8 orig_2[0x10] = {0x04, 0x00, 0x2D, 0xE5, 0x04, 0x00, 0x9D, 0xE5, 0x08, 0x30, 0x9D, 0xE5, 0x29, 0x00, 0x00, 0xEF}; //PUSH {R0}; LDR r0, [sp, #4]; LDR r3, [sp, #8]; SVC 0x29
	const u8 nop[0x4] = {0x00, 0xF0, 0x20, 0xE3}; // NOP
	static u8 patch[0x8] = {0x04, 0xF0, 0x1F, 0xE5, 0xDE, 0xAD, 0xBE, 0xEF}; // LDR pc, [pc, #-4]; 0xDEADBEEF
	u64 dst_1 = SaltySDCore_findCode(orig_1, 8);
	u64 dst_2 = SaltySDCore_findCode(orig_2, 16);
	
	if (!dst_1 || !dst_2)
	{
		SaltySDCore_printf("SaltySD Core: Failed to find svcSetHeapSize! %llx\n", dst_1);
		return;
	}

	*(u32*)&patch[4] = (u32)svcSetHeapSizeIntercept;
	if (dst_1 & 4)
	{
		ret = SaltySD_Memcpy(dst_1, (u64)nop, 0x4);
		if (ret)
		{
			debug_log("svcSetHeapSize memcpy failed!\n");
		}
		else
		{
			ret = SaltySD_Memcpy(dst_1+4, (u64)patch, 8);
		}
	}
	else
	{
		ret = SaltySD_Memcpy(dst_1, (u64)patch, 8);
	}
	if (ret) debug_log("svcSetHeapSize memcpy failed!\n");
	
	*(u32*)&patch[4] = (u32)svcGetInfoIntercept;	
	if (dst_2 & 4)	
	{	
		ret = SaltySD_Memcpy(dst_2, (u64)nop, 0x4);	
		if (ret)	
		{	
			debug_log("svcSetHeapSize memcpy failed!\n");	
		}	
		else	
		{	
			ret = SaltySD_Memcpy(dst_2+4, (u64)patch, 8);	
		}	
	}	
	else	
	{	
		ret = SaltySD_Memcpy(dst_2, (u64)patch, 8);	
	}	
	if (ret) debug_log("svcSetHeapSize memcpy failed!\n");
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

int main(int argc, char *argv[])
{
	Result ret;

	SaltySDCore_RegisterExistingModules();
	
	SaltySD_Init();

	ret = SaltySD_Restore();
	if (ret) goto fail;
	

	ret = SaltySD_GetSDCard(&sdcard);
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
			NX_FPS(&_sharedmemory);

			uint64_t titid = 0;
			svcGetInfo(&titid, 18, CUR_PROCESS_HANDLE, 0);
			ReverseNX(&_sharedmemory);
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
