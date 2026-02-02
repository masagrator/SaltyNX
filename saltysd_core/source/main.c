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
#define NANOPRINTF_IMPLEMENTATION
#include "nanoprintf.h"

#include "bm.h"

u32 __nx_applet_type = AppletType_None;

static char g_heap[0x8000];

extern void __nx_exit_clear(void* ctx, Handle thread, void* addr);
extern void elf_trampoline(void* context, Handle thread, void* func);
void* __stack_tmp;

Handle orig_main_thread;
void* orig_ctx;

Handle sdcard;
#if defined(SWITCH32) || defined(OUNCE32)
const size_t elf_area_size = 0x200000; //We assume that Core itself won't take more than 0x200000 bytes;
#else
volatile size_t elf_area_size = 0xDEEDBEEF; //MAGIC number to be replaced after code gets compiled
#endif

ThreadVars vars_orig;
ThreadVars vars_mine;

uint64_t tid = 0;

static uint32_t sharedOperationMode = 0;

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
	vars_mine.tls_tp = (void*)malloc(0x100);
	vars_orig = *getThreadVars();
	*getThreadVars() = vars_mine;
	virtmemSetup();
}

void __attribute__((weak)) __libnx_exit(int rc)
{
	fsdevUnmountAll();
	
	// Restore TLS stuff
	*getThreadVars() = vars_orig;

	free(vars_mine.tls_tp);
	
	uintptr_t addr = SaltySDCore_getCodeStart();

	__nx_exit_clear(orig_ctx, orig_main_thread, (void*)addr);
}

uintptr_t g_heapAddr;
size_t g_heapSize;

void SaltySDCore_LoadPatches() {
	char tmp4[256] = "";
	char tmp2[256] = "";
	char instr[256] = "";
	DIR *d;
	struct dirent *dir;
	
	SaltySDCore_printf("SaltySD Patcher: Searching patches in dir '/'...\n");
	
	npf_snprintf(tmp4, 0x100, "sdmc:/SaltySD/patches/");

	d = opendir(tmp4);
	if (d)
	{
		while ((dir = readdir(d)) != NULL)
		{
			char *dot = strrchr(dir->d_name, '.');
			#if defined(SWITCH32) || defined(OUNCE32)
			if (dot && !strcmp(dot, ".asm32"))
			#else
			if (dot && !strcmp(dot, ".asm64"))
			#endif
			{
				npf_snprintf(tmp2, 0x100, "%s%s", tmp4, dir->d_name);
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
				uintptr_t position = SaltySDCore_FindSymbol(filename);
				if (position) {
					SaltySDCore_printf("SaltySD Patcher: Symbol Position: 0x%lX\n", position);
					SaltySD_Memcpy(position, (uintptr_t)instr, size);
				}
				else {
					SaltySDCore_printf("SaltySD Patcher: Symbol Position: not found\n", position);
				}
			}
		}
		closedir(d);
	}

	svcGetInfo(&tid, 18, CUR_PROCESS_HANDLE, 0);
		
	#if defined(SWITCH32) || defined(OUNCE32)
	npf_snprintf(tmp4, 0x100, "sdmc:/SaltySD/patches/%016llx/", tid);
	SaltySDCore_printf("SaltySD Patcher: Searching patches in dir '/%016llX'...\n", tid);
	#else
	npf_snprintf(tmp4, 0x100, "sdmc:/SaltySD/patches/%016lx/", tid);
	SaltySDCore_printf("SaltySD Patcher: Searching patches in dir '/%016lX'...\n", tid);
	#endif

	d = opendir(tmp4);
	if (d)
	{
		while ((dir = readdir(d)) != NULL)
		{
			char *dot = strrchr(dir->d_name, '.');
			#if defined(SWITCH32) || defined(OUNCE32)
			if (dot && !strcmp(dot, ".asm32")) 
			#else
			if (dot && !strcmp(dot, ".asm64")) 
			#endif
			{
				npf_snprintf(tmp2, 0x100, "%s%s", tmp4, dir->d_name);
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
				uintptr_t position = SaltySDCore_FindSymbol(filename);
				if (position) {
					SaltySDCore_printf("SaltySD Patcher: Symbol Position: 0x%lX\n", position);
					SaltySD_Memcpy(position, (uintptr_t)instr, size);
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

void setupELFHeap(void)
{
	void* addr = NULL;

	svcSetHeapSize(&addr, ((elf_area_size+0x1FFFFF) & ~0x1FFFFF));

	g_heapAddr = (uintptr_t)addr;
	g_heapSize = ((elf_area_size+0x1FFFFF) & ~0x1FFFFF);
}

#if defined(SWITCH) || defined(OUNCE)

uintptr_t find_next_elf_heap()
{
	uintptr_t addr = 0;
	while (1)
	{
		MemoryInfo info;
		u32 pageinfo;
		Result ret = svcQueryMemory(&info, &pageinfo, addr);
		
		if (info.perm == Perm_Rw && info.type == MemType_Heap)
			return info.addr;

		addr = info.addr + info.size;
		
		if (!addr || ret) break;
	}
	
	return 0;
}

#endif

extern void _start();

void SaltySDCore_RegisterExistingModules()
{
	uintptr_t addr = 0;
	while (1)
	{
		MemoryInfo info;
		u32 pageinfo;
		Result ret = svcQueryMemory(&info, &pageinfo, addr);
		
		if (info.perm == Perm_Rx)
		{
			SaltySDCore_RegisterModule((uintptr_t)info.addr);
			uintptr_t compaddr = info.addr;
			if (compaddr != (uintptr_t)_start)
				SaltySDCore_RegisterBuiltinModule((uintptr_t)info.addr);
		}

		addr = info.addr + info.size;
		
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

#if defined(SWITCH32) || defined(OUNCE32)
void SaltySDCore_PatchSVCs()
{
	static u8 orig_1[] = {0x04, 0x00, 0x2D, 0xE5, 0x01, 0x00, 0x00, 0xEF, 0x00, 0x20, 0x9D, 0xE5}; //PUSH {r0}; SVC #0x1; LDR r2, [sp]
	static u8 orig_2[] = {0x04, 0x00, 0x2D, 0xE5, 0x04, 0x00, 0x9D, 0xE5, 0x08, 0x30, 0x9D, 0xE5, 0x29, 0x00, 0x00, 0xEF, 0x00, 0x30, 0x9D, 0xE5}; //PUSH {R0}; LDR r0, [sp, #4]; LDR r3, [sp, #8]; SVC 0x29; LDR r3, [sp]
	static u8 orig_1_alt[] = {0x04, 0x00, 0x2D, 0xE5, 0x01, 0x00, 0x00, 0xEF}; //PUSH {r0}; SVC #0x1
	static u8 orig_2_alt[] = {0x04, 0x00, 0x2D, 0xE5, 0x04, 0x00, 0x9D, 0xE5, 0x08, 0x30, 0x9D, 0xE5, 0x29, 0x00, 0x00, 0xEF}; //PUSH {R0}; LDR r0, [sp, #4]; LDR r3, [sp, #8]; SVC 0x29
	static u8 patch[0x8] = {0x04, 0xF0, 0x1F, 0xE5, 0xDE, 0xAD, 0xBE, 0xEF}; // LDR pc, [pc, #-4]; 0xDEADBEEF
	uintptr_t dst_1 = SaltySDCore_findCodeEx(orig_1, sizeof(orig_1));
	uintptr_t dst_2 = SaltySDCore_findCodeEx(orig_2, sizeof(orig_2));
	if (!dst_1) {
		dst_1 = SaltySDCore_findCodeEx(orig_1_alt, sizeof(orig_1_alt));
	}
	if (!dst_2) {
		dst_2 = SaltySDCore_findCodeEx(orig_2_alt, sizeof(orig_2_alt));
	}
	
	if (!dst_1 || !dst_2)
	{
		SaltySDCore_printf("SaltySD Core: Failed to find svcSetHeapSize or svcGetInfo!\n");
		return;
	}

	*(uintptr_t*)&patch[4] = (uintptr_t)svcSetHeapSizeIntercept;
	SaltySD_Memcpy((uintptr_t)dst_1, (uintptr_t)patch, sizeof(patch));
	*(uintptr_t*)&patch[4] = (uintptr_t)svcGetInfoIntercept;
	SaltySD_Memcpy((uintptr_t)dst_2, (uintptr_t)patch, sizeof(patch));		
}

#else

void SaltySDCore_PatchSVCs()
{
	static u8 orig_1[0x8] = {0xE0, 0x0F, 0x1F, 0xF8, 0x21, 0x00, 0x00, 0xD4}; //STR [sp, #-0x10]!; SVC #0x1
	static u8 orig_2[0x8] = {0xE0, 0x0F, 0x1F, 0xF8, 0x21, 0x05, 0x00, 0xD4}; //STR [sp, #-0x10]!; SVC #0x29
	static u8 patch[0x10] = {0x44, 0x00, 0x00, 0x58, 0x80, 0x00, 0x1F, 0xD6, 0x0F, 0xF0, 0x0F, 0xF0, 0x0F, 0xF0, 0x0F, 0xF0}; // LDR X4 #8; BR X4; ADRP X15, #0x1FE03000; ADRP X15, #0x1FE03000
	u64 dst_1 = SaltySDCore_findCodeEx(orig_1, sizeof(orig_1));
	u64 dst_2 = SaltySDCore_findCodeEx(orig_2, sizeof(orig_2));
	
	if (!dst_1)
	{
		SaltySDCore_printf("SaltySD Core: Failed to find svcSetHeapSize!\n");
		return;
	}
	else {
		SaltySDCore_printf("SaltySD Core: Found svcSetHeapSize at address: 0x%lx!\n", dst_1);
	}
	if (!dst_2) {
		SaltySDCore_printf("SaltySD Core: Failed to find svcGetInfo!\n");
		return;		
	}
	else {
		SaltySDCore_printf("SaltySD Core: Found svcGetInfo at address: 0x%lx!\n", dst_2);
	}

	*(u64*)&patch[8] = (u64)svcSetHeapSizeIntercept;
	SaltySD_Memcpy(dst_1, (u64)patch, sizeof(patch));
	
	*(u64*)&patch[8] = (u64)svcGetInfoIntercept;	
	SaltySD_Memcpy(dst_2, (u64)patch, sizeof(patch));
}

void** SaltySDCore_LoadPluginsInDir(char* path, void** entries, size_t* num_elfs)
{
	char* tmp = malloc(0x100);
	DIR *d;
	struct dirent *dir;

	SaltySDCore_printf("SaltySD Core: Searching plugin dir `%s'...\n", path);
	
	npf_snprintf(tmp, 0x100, "sdmc:/SaltySD/plugins/%s", path);

	d = opendir(tmp);
	if (d)
	{
		while ((dir = readdir(d)) != NULL)
		{
			char *dot = strrchr(dir->d_name, '.');
			if (dot && !strcmp(dot, ".elf"))
			{
				u64 elf_addr, elf_size;
				//setupELFHeap();
				npf_snprintf(tmp, 0x100, "%s%s", path, dir->d_name);
				SaltySD_LoadELF(find_next_elf_heap(), &elf_addr, &elf_size, tmp);
				*num_elfs = *num_elfs + 1;
				entries = realloc(entries, *num_elfs * sizeof(void*));
				entries[*num_elfs-1] = (void*)elf_addr;

				SaltySDCore_RegisterModule((uintptr_t)entries[*num_elfs-1]);
				elf_area_size += elf_size;
			}
		}
		closedir(d);
	}
	free(tmp);
	
	return entries;
}

void SaltySDCore_LoadPlugins()
{
	// Load plugin ELFs
	char* tmp3 = malloc(0x20);

	void** entries = NULL;
	size_t num_elfs = 0;
	
	entries = SaltySDCore_LoadPluginsInDir("", entries, &num_elfs);
	npf_snprintf(tmp3, 0x20, "%016lX/", tid);
	entries = SaltySDCore_LoadPluginsInDir(tmp3, entries, &num_elfs);
	
	for (int i = 0; i < num_elfs; i++)
	{
		SaltySDCore_DynamicLinkModule(entries[i]);
		elf_trampoline(orig_ctx, orig_main_thread, entries[i]);
	}
	free(tmp3);
	if (num_elfs) free(entries);
	else SaltySDCore_printf("SaltySD Core: Plugins not detected...\n");
	
	return;
}

#endif

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

typedef double (*strtod_0)(const char* str, char** endptr);
uintptr_t strtod_ptr = 0;

double strtod(const char* str, char** endptr) {
	return ((strtod_0)(strtod_ptr))(str, endptr);
}

int main(int argc, char *argv[])
{
	Result ret;
	#if defined(SWITCH) || defined(OUNCE)
	svcGetInfo(&g_heapAddr, InfoType_HeapRegionAddress, CUR_PROCESS_HANDLE, 0);
	#endif

	SaltySDCore_RegisterExistingModules();
	strtod_ptr = SaltySDCore_FindSymbolBuiltin("strtod");
	SaltySD_Init();
	
	ret = SaltySD_GetSDCard(&sdcard);
	if (ret) goto fail;

	#if defined(SWITCH32) || defined(OUNCE32)
	SaltySDCore_printf("SaltySD Core32 " APP_VERSION ": restoring code...\n");
	#else
	SaltySDCore_printf("SaltySD Core " APP_VERSION ": restoring code...\n");
	#endif
	ret = SaltySD_Restore();
	if (ret) goto fail;

	SaltySDCore_PatchSVCs();
	SaltySDCore_LoadPatches();

	SaltySDCore_fillRoLoadModule();
	SaltySDCore_ReplaceImport("_ZN2nn2ro10LoadModuleEPNS0_6ModuleEPKvPvmi", (void*)LoadModule);
	
	#if defined(SWITCH32) || defined(OUNCE32)
	SaltySDCore_printf("SaltySD Core32: Plugins are not supported...\n");
	#else
	Result exc = SaltySD_Exception();
	if (exc == 0x0) SaltySDCore_LoadPlugins();
	else SaltySDCore_printf("SaltySD Core: Detected exception title, aborting loading plugins...\n");
	#endif

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
			NX_FPS(&_sharedmemory, &sharedOperationMode);

			ReverseNX(&_sharedmemory, &sharedOperationMode);

			if (SaltySDCore_isRelrAvailable()) {
				SaltySDCore_printf("SaltySD Core: Game is using RELR. Applying hacky solution.\n", ret);
				Address_weak_QueryMemoryInfo = SaltySDCore_FindSymbolBuiltin("_ZN2nn2os15QueryMemoryInfoEPNS0_10MemoryInfoE");
				SaltySDCore_ReplaceImport("_ZN2nn2os15QueryMemoryInfoEPNS0_10MemoryInfoE", (void*)QueryMemoryInfo);
			}
		}
		else {
			SaltySDCore_printf("SaltySD Core: shmemMap failed: 0x%lX\n", shmemMapRc);
		}
	}

	ret = SaltySD_Deinit();
	if (ret) goto fail;
	
	setupELFHeap();
	__libnx_exit(0);

fail:
	#if defined(SWITCH32) || defined(OUNCE32)
	debug_log("SaltySD Core: failed with retcode %lx\n", ret);
	#else
	debug_log("SaltySD Core: failed with retcode %x\n", ret);
	#endif
	SaltySDCore_printf("SaltySD Core: failed with retcode %lx\n", ret);
	__libnx_exit(0);
}
