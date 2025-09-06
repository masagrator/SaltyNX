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
	
	u64 addr = SaltySDCore_getCodeStart();

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
			if (dot && !strcmp(dot, ".asm64")) {
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
				uint64_t position = SaltySDCore_FindSymbol(filename);
				if (position) {
					SaltySDCore_printf("SaltySD Patcher: Symbol Position: %016llx\n", position);
					SaltySD_Memcpy(position, (uint64_t)instr, size);
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
	
	snprintf(tmp4, 0x100, "sdmc:/SaltySD/patches/%016lx/", tid);

	d = opendir(tmp4);
	if (d)
	{
		while ((dir = readdir(d)) != NULL)
		{
			char *dot = strrchr(dir->d_name, '.');
			if (dot && !strcmp(dot, ".asm64")) {
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
				uint64_t position = SaltySDCore_FindSymbol(filename);
				if (position) {
					SaltySDCore_printf("SaltySD Patcher: Symbol Position: %016llx\n", position);
					SaltySD_Memcpy(position, (uint64_t)instr, size);
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
	Result rc = 0;

	rc = svcSetHeapSize(&addr, ((elf_area_size+0x200000) & 0xffe00000));

	if (rc || addr == NULL)
	{
		debug_log("SaltySD Bootstrap: svcSetHeapSize failed with err %x\n", rc);
	}

	g_heapAddr = (u64)addr;
	g_heapSize = ((elf_area_size+0x200000) & 0xffe00000);
}

u64 find_next_elf_heap()
{
	u64 addr = g_heapAddr;
	while (1)
	{
		MemoryInfo info;
		u32 pageinfo;
		Result ret = svcQueryMemory(&info, &pageinfo, addr);
		
		if (info.perm == Perm_Rw)
			return info.addr;

		addr = info.addr + info.size;
		
		if (!addr || ret) break;
	}
	
	return 0;
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
			SaltySDCore_RegisterModule((void*)info.addr);
			u64 compaddr = (u64)info.addr;
			if ((u64*)compaddr != (u64*)_start)
				SaltySDCore_RegisterBuiltinModule((void*)info.addr);
		}

		addr = info.addr + info.size;
		
		if (!addr || ret) break;
	}
	
	return;
}

Result svcSetHeapSizeIntercept(u64 *out, u64 size)
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

Result svcGetInfoIntercept (u64 *out, u64 id0, Handle handle, u64 id1)	
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
	static u8 orig_1[0x8] = {0xE0, 0x0F, 0x1F, 0xF8, 0x21, 0x00, 0x00, 0xD4}; //STR [sp, #-0x10]!; SVC #0x1
	static u8 orig_2[0x8] = {0xE0, 0x0F, 0x1F, 0xF8, 0x21, 0x05, 0x00, 0xD4}; //STR [sp, #-0x10]!; SVC #0x29
	static u8 patch[0x10] = {0x44, 0x00, 0x00, 0x58, 0x80, 0x00, 0x1F, 0xD6, 0x0F, 0xF0, 0x0F, 0xF0, 0x0F, 0xF0, 0x0F, 0xF0}; // LDR X4 #8; BR X4; ADRP X15, #0x1FE03000; ADRP X15, #0x1FE03000
	u64 dst_1 = SaltySDCore_findCodeEx(orig_1, 8);
	u64 dst_2 = SaltySDCore_findCodeEx(orig_2, 8);
	
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
	SaltySD_Memcpy(dst_1, (u64)patch, 0x10);
	
	*(u64*)&patch[8] = (u64)svcGetInfoIntercept;	
	SaltySD_Memcpy(dst_2, (u64)patch, 0x10);
}

void** SaltySDCore_LoadPluginsInDir(char* path, void** entries, size_t* num_elfs)
{
	char* tmp = malloc(0x100);
	DIR *d;
	struct dirent *dir;

	SaltySDCore_printf("SaltySD Core: Searching plugin dir `%s'...\n", path);
	
	snprintf(tmp, 0x100, "sdmc:/SaltySD/plugins/%s", path);

	d = opendir(tmp);
	if (d)
	{
		while ((dir = readdir(d)) != NULL)
		{
			char *dot = strrchr(dir->d_name, '.');
			if (dot && !strcmp(dot, ".elf"))
			{
				u64 elf_addr, elf_size;
				setupELFHeap();
				snprintf(tmp, 0x100, "%s%s", path, dir->d_name);
				SaltySD_LoadELF(find_next_elf_heap(), &elf_addr, &elf_size, tmp);
				*num_elfs = *num_elfs + 1;
				entries = realloc(entries, *num_elfs * sizeof(void*));
				entries[*num_elfs-1] = (void*)elf_addr;

				SaltySDCore_RegisterModule(entries[*num_elfs-1]);
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
	snprintf(tmp3, 0x20, "%016lx/", tid);
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
uint64_t strtod_ptr = 0;

double strtod(const char* str, char** endptr) {
	return ((strtod_0)(strtod_ptr))(str, endptr);
}

//Fix required by Unity 6, possibly newer versions too

size_t GetRequiredBufferSizeForGetAllModuleInfo() {
	return (101 * 13); //101 per each module * 13 possible modules
}

struct ModuleInfo {
	const char* module_name;
	uintptr_t base_address;
	size_t module_size;
	uint8_t* BID;
	size_t BID_length;
	uint64_t unk;
} modules_in[13];

extern u64 code_start;

size_t GetAllModuleInfo(struct ModuleInfo** modules, void* buffer, size_t buffer_size) {
	u64 addr = code_start;
	size_t module_count = 0;
	uintptr_t addresses[13] = {0};
	size_t sizes[13] = {0};
	while (1)
	{
		MemoryInfo info;
		u32 pageinfo;
		Result ret = svcQueryMemory(&info, &pageinfo, addr);

		if (addr > info.addr) break;
		
		if (info.addr != (u64)_start && info.perm == Perm_Rx)
		{
			addresses[module_count] = info.addr;
			sizes[module_count] = info.size; //this should be size of whole module, but games that use it seems to not care ¯\_(ツ)_/¯
			module_count++;
		}

		addr = info.addr + info.size;

		if (!addr || ret) break;
	}

	for (size_t i = 0; i < module_count; i++) {
		if (modules_in[i].module_name != 0) continue;
		if (i == 0) {
			modules_in[0].module_name = (const char*)malloc(strlen("nnrtld") + 1);
			strcpy((char*)modules_in[0].module_name, "nnrtld");
			modules_in[0].base_address = addresses[0];
			modules_in[0].module_size = sizes[0];
			uint8_t BID_temp[0x14] = {0x93, 0xCB, 0x83, 0x83, 0xD7, 0xA5, 0x4C, 0x0B, 0x42, 0x37, 0x25, 0x3F, 0x20, 0xDE, 0x50, 0x21, 0x00, 0x83, 0x74, 0xDD};
			modules_in[0].BID = (uint8_t*)malloc(sizeof(BID_temp));
			memcpy(modules_in[0].BID, &BID_temp, sizeof(BID_temp));
			modules_in[0].BID_length = sizeof(BID_temp);
			modules_in[0].unk = 0;
		}

		else if (i == 1) {
			modules_in[i].module_name = (const char*)malloc(strlen("GameAssembly.nss") + 1);
			strcpy((char*)modules_in[i].module_name, "GameAssembly.nss");
			modules_in[i].base_address = addresses[i];
			modules_in[i].module_size = sizes[i];
			uint8_t BID_temp[0x10] = {0xDC, 0x3E, 0x4B, 0x89, 0x20, 0x43, 0xEF, 0x23, 0x45, 0xC3, 0xEA, 0xF9, 0x7A, 0x17, 0x7E, 0x7C};
			modules_in[i].BID = (uint8_t*)malloc(sizeof(BID_temp));
			memcpy(modules_in[i].BID, &BID_temp, sizeof(BID_temp));
			modules_in[i].BID_length = sizeof(BID_temp);
			modules_in[i].unk = 0x6272617200000000;
		}

		else if (i+1 == module_count) {
			modules_in[i].module_name = (const char*)malloc(strlen("nnSdk") + 1);
			strcpy((char*)modules_in[i].module_name, "nnSdk");
			modules_in[i].base_address = addresses[i];
			modules_in[i].module_size = sizes[i];
			uint8_t BID_temp[0x14] = {0xE0, 0x19, 0x50, 0xC5, 0x8E, 0x17, 0xB0, 0x80, 0xC9, 0x04, 0x94, 0x78, 0x4E, 0xA1, 0xDD, 0x29, 0xE9, 0xDB, 0xD4, 0x86};
			modules_in[i].BID = (uint8_t*)malloc(sizeof(BID_temp));
			memcpy(modules_in[i].BID, &BID_temp, sizeof(BID_temp));
			modules_in[i].BID_length = sizeof(BID_temp);
			modules_in[i].unk = addresses[0] & ~0xFFFFFFFF;
		}
		else {
			modules_in[i].module_name = (const char*)malloc(strlen("multimedia") + 1);
			strcpy((char*)modules_in[i].module_name, "multimedia");
			modules_in[i].base_address = addresses[i];
			modules_in[i].module_size = sizes[i];
			uint8_t BID_temp[0x14] = {0xC8, 0x9C, 0x80, 0x4F, 0xC3, 0xB2, 0x67, 0x57, 0x66, 0x1A, 0x70, 0xFC, 0x12, 0x5A, 0xA9, 0x23, 0x31, 0xEC, 0xC6, 0xFE};
			modules_in[i].BID = (uint8_t*)malloc(sizeof(BID_temp));
			memcpy(modules_in[i].BID, &BID_temp, sizeof(BID_temp));
			modules_in[i].BID_length = sizeof(BID_temp);
			modules_in[i].unk = 0x66645c4700000000;
		}		
	}

	modules[0] = &modules_in[0];
	
	return module_count;
}

int main(int argc, char *argv[])
{
	Result ret;

	SaltySDCore_RegisterExistingModules();
	strtod_ptr = SaltySDCore_FindSymbolBuiltin("strtod");
	SaltySD_Init();
	
	ret = SaltySD_GetSDCard(&sdcard);
	if (ret) goto fail;

	SaltySDCore_printf("SaltySD Core " APP_VERSION ": restoring code...\n");
	ret = SaltySD_Restore();
	if (ret) goto fail;

	SaltySDCore_PatchSVCs();
	SaltySDCore_LoadPatches(true);

	SaltySDCore_fillRoLoadModule();
	SaltySDCore_ReplaceImport("_ZN2nn2ro10LoadModuleEPNS0_6ModuleEPKvPvmi", (void*)LoadModule);
	
	Result exc = SaltySD_Exception();
	if (exc == 0x0) SaltySDCore_LoadPlugins();
	else SaltySDCore_printf("SaltySD Core: Detected exception title, aborting loading plugins...\n");

	ptrdiff_t SMO = -1;
	ret = SaltySD_CheckIfSharedMemoryAvailable(&SMO, 1);
	SaltySDCore_printf("SaltySD_CheckIfSharedMemoryAvailable ret: 0x%X\n", ret);
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
				SaltySDCore_ReplaceImport("_ZN2nn4diag40GetRequiredBufferSizeForGetAllModuleInfoEv", (void*)GetRequiredBufferSizeForGetAllModuleInfo);
				SaltySDCore_ReplaceImport("_ZN2nn4diag16GetAllModuleInfoEPPNS0_10ModuleInfoEPvm", (void*)GetAllModuleInfo);
			}
		}
		else {
			SaltySDCore_printf("SaltySD Core: shmemMap failed: 0x%X\n", shmemMapRc);
		}
	}

	ret = SaltySD_Deinit();
	if (ret) goto fail;

	__libnx_exit(0);

fail:
	debug_log("SaltySD Core: failed with retcode %x\n", ret);
	SaltySDCore_printf("SaltySD Core: failed with retcode %x\n", ret);
	__libnx_exit(0);
}
