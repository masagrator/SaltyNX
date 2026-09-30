#include "loadelf.h"

#include <cstdio>
#include <cstring>

#include "useful.h"
#include "elf_parser.hpp"
#include "elf32_parser.hpp"
#include "shared.h"

using namespace elf_parser;

void *text_save;
void *data_save;
u64 text_addr, data_addr, text_fsize, data_fsize, text_msize, data_msize;

u64 find_memory(Handle debug, u64 min, u64 size, u8 perm)
{
	u64 addr = 0;
	while (1)
	{
		MemoryInfo info;
		u32 pageinfo;
		Result ret = svcQueryDebugProcessMemory(&info, &pageinfo, debug, addr);
		if (R_FAILED(ret)) break;
		
		if (info.perm == perm && info.size >= size && (info.type == MemType_CodeMutable || info.type == MemType_CodeStatic) && info.addr >= min)
			return info.addr;

		addr = info.addr + info.size;
		
		if (!addr) break;
	}
	
	return 0;
}

// The loaders below never close the debug handle: on any error they return a failed Result and leave the
// process as it was (whatever was already overwritten is put back), the caller closes the handle and the
// game starts without SaltyNX.

// Frees the backups and forgets the bootstrap area. With restore, writes the original code/data back first.
static void release_backup(Handle debug, bool restore)
{
	if (restore && text_save && text_msize) svcWriteDebugProcessMemory(debug, text_save, text_addr, text_msize);
	if (restore && data_save && data_msize) svcWriteDebugProcessMemory(debug, data_save, data_addr, data_msize);
	free(text_save);
	free(data_save);
	text_save = NULL;
	data_save = NULL;
	text_addr = data_addr = 0;
	text_fsize = data_fsize = text_msize = data_msize = 0;
}

// Backs up [text] and optional [data] of the process, then writes the bootstrap over them.
static Result write_bootstrap(Handle debug, const void* text, const void* data)
{
	text_save = malloc(text_msize);
	data_save = data_msize ? malloc(data_msize) : NULL;
	if (!text_save || (data_msize && !data_save)) {
		release_backup(debug, false);
		return MAKERESULT(Module_Libnx, LibnxError_OutOfMemory);
	}

	Result ret = svcReadDebugProcessMemory(text_save, debug, text_addr, text_msize);
	if (R_SUCCEEDED(ret) && data_msize) ret = svcReadDebugProcessMemory(data_save, debug, data_addr, data_msize);
	if (R_FAILED(ret)) {
		release_backup(debug, false);
		return ret;
	}

	// Only what's in the file is written: a segment can be all .bss (file size 0), which the bootstrap clears
	// itself, and the kernel rejects 0 byte writes.
	if (text_fsize) ret = svcWriteDebugProcessMemory(debug, text, text_addr, text_fsize);
	if (R_SUCCEEDED(ret) && data_fsize) ret = svcWriteDebugProcessMemory(debug, data, data_addr, data_fsize);
	if (R_FAILED(ret)) release_backup(debug, true);
	return ret;
}

Result load_elf_debug(Handle debug, uint64_t* start, uint8_t* elf_data, u32 elf_size)
{
	Elf_parser elf(elf_data);

	if (elf_size < sizeof(Elf64_Ehdr) || memcmp(elf_data, ELFMAG, SELFMAG)) return MAKERESULT(Module_Libnx, LibnxError_BadInput);
	auto segments = elf.get_segments();
	if (segments.size() < 2) return MAKERESULT(Module_Libnx, LibnxError_BadInput);
	segment_t text_seg = segments[0];
	segment_t data_seg = segments[1];
	text_addr = find_memory(debug, 0, text_seg.phdr->p_memsz, Perm_Rx);
	data_addr = text_addr ? find_memory(debug, text_addr, data_seg.phdr->p_memsz, Perm_Rw) : 0;
	if (!text_addr || !data_addr) {
		SaltyNX_printf(APP_NAME ": no place for the bootstrap (.text %lx, .data %lx), aborting...\n", text_addr, data_addr);
		text_addr = data_addr = 0;
		return MAKERESULT(Module_Libnx, LibnxError_NotFound);
	}
	text_fsize = text_seg.phdr->p_filesz;
	data_fsize = data_seg.phdr->p_filesz;
	text_msize = text_seg.phdr->p_memsz;
	data_msize = data_seg.phdr->p_memsz;
	SaltyNX_printf(".text to %lx, .data to %lx\n", text_addr, data_addr);
	
	elf.relocate_segment(0, text_addr);
	elf.relocate_segment(1, data_addr);
	
	Result ret = write_bootstrap(debug, text_seg.data, data_seg.data);
	if (R_SUCCEEDED(ret)) *start = text_addr;
	return ret;
}

Result load_elf32_debug(Handle debug, uint64_t* start)
{
	// One binary for every rtld layout: Bootstrap32 is a single segment of pure code (no data, no relocations,
	// its variables live on its stack), so it's written over rtld's code only.
	FILE* file = fopen("sdmc:/SaltySD/saltynx_bootstrap32.elf", "rb");
	if (!file) {
		SaltyNX_printf(APP_NAME ": SaltySD/saltynx_bootstrap32.elf not found, aborting...\n");
		return MAKERESULT(Module_Libnx, LibnxError_NotFound);
	}

	fseek(file, 0, 2);
	long elf_size = ftell(file);
	fseek(file, 0, 0);
	uint8_t* elf_data = elf_size > 0 ? (uint8_t*)malloc(elf_size) : NULL;
	if (!elf_data) {
		SaltyNX_printf(APP_NAME ": There was not enough space for loading saltynx_bootstrap32.elf! Aborting...\n");
		fclose(file);
		return MAKERESULT(Module_Libnx, LibnxError_OutOfMemory);
	}
	const bool read = fread(elf_data, elf_size, 1, file) == 1;
	fclose(file);

	elf32_parser::Elf32_parser elf(elf_data);
	std::vector<elf32_parser::segment_t> segments;
	if (read && (size_t)elf_size >= sizeof(Elf32_Ehdr) && !memcmp(elf_data, ELFMAG, SELFMAG)) segments = elf.get_segments();
	if (segments.empty() || segments[0].phdr->p_type != PT_LOAD) {
		SaltyNX_printf(APP_NAME ": SaltySD/saltynx_bootstrap32.elf is not valid, aborting...\n");
		free(elf_data);
		return MAKERESULT(Module_Libnx, LibnxError_BadInput);
	}

	elf32_parser::segment_t text_seg = segments[0];
	text_addr = find_memory(debug, 0, text_seg.phdr->p_memsz, Perm_Rx);
	if (!text_addr) {
		SaltyNX_printf(APP_NAME ": no place for the 32-bit bootstrap, aborting...\n");
		free(elf_data);
		return MAKERESULT(Module_Libnx, LibnxError_NotFound);
	}
	text_fsize = text_seg.phdr->p_filesz;
	text_msize = text_seg.phdr->p_memsz;
	data_addr = 0;
	data_fsize = 0;
	data_msize = 0;
	SaltyNX_printf("32bit .text to %llx\n", text_addr);
	
	Result ret = write_bootstrap(debug, text_seg.data, NULL);
	if (R_SUCCEEDED(ret)) *start = text_addr;
	free(elf_data);
	return ret;
}

Result restore_elf_debug(Handle debug)
{
	Result ret = svcWriteDebugProcessMemory(debug, text_save, text_addr, text_msize);
	if (data_msize) // Bootstrap32 has no data segment
		ret = svcWriteDebugProcessMemory(debug, data_save, data_addr, data_msize);
	release_backup(debug, false);
	return ret;
}

Result load_elf_proc(Handle proc, uint64_t pid, uint64_t heap, uint64_t* start, uint64_t* total_size, FILE* f, u32 elf_size)
{
	Result ret;
	Handle debug;
	
	*start = 0;
	*total_size = 0;
	uint8_t* elf_data = 0;
	u64 map_addr = 0;
	uint64_t heap_buffer_address = ((heap+0x200000) & ~0x1FFFFF);

	MemoryInfo meminfo;
	u32 pageinfo;

	Result rc = svcQueryProcessMemory(&meminfo, &pageinfo, proc, heap_buffer_address);
	if (R_FAILED(rc)) {
		SaltyNX_printf(APP_NAME ": load_elf_proc failed query mapping 0x%lx with 0x%x\n", heap_buffer_address, rc);
	}
	else {
		SaltyNX_printf(APP_NAME ": load_elf_proc query mapping 0x%lx, addr: 0x%lx, Perm: %d, size: 0x%lx\n", heap_buffer_address, meminfo.addr, meminfo.perm, meminfo.size);
		do
		{
			map_addr = randomGet64() & 0xFFFFFF000ull;
			ret = svcMapProcessCodeMemory(proc, map_addr, heap_buffer_address, 0x200000);
		}
		while (ret == 0xDC01 || ret == 0xD401);
		svcSetProcessMemoryPermission(proc, map_addr, 0x200000, Perm_Rw);
		virtmemLock();
		elf_data = (uint8_t*)virtmemFindAslr(0x200000, 0);
		rc = svcMapProcessMemory(elf_data, proc, map_addr, 0x200000);
		virtmemUnlock();
	}

	if (R_FAILED(rc)) {
		SaltyNX_printf(APP_NAME ": load_elf_proc failed mapping 0x%lx to 0x%lx with 0x%x\n", heap_buffer_address, elf_data, rc);
		return 1;
	}

	fread(elf_data, elf_size, 1, f);

	SaltyNX_printf(APP_NAME ": load_elf_proc loaded ELF to buffer.\n");
	
	Elf_parser elf(elf_data);

	// Figure out our number of pages
	u64 min_vaddr = -1, max_vaddr = 0;
	for (auto seg : elf.get_segments())
	{
		u64 min = seg.phdr->p_vaddr;
		u64 max = seg.phdr->p_vaddr + ((seg.phdr->p_memsz + 0xFFF) & ~0xFFF);
		if (min < min_vaddr)
			min_vaddr = min;

		if (max > max_vaddr)
			max_vaddr = max;
	}
	
	// Debug the process to write into the heap addr provided
	// Note: Could probably just use buffer descs for this but whatever.
	ret = svcDebugActiveProcess(&debug, pid);
	if (ret) {
		svcUnmapProcessMemory(elf_data, proc, map_addr, 0x200000);
		svcUnmapProcessCodeMemory(proc, map_addr, heap_buffer_address, 0x200000);
		return ret;
	}

	for (auto seg : elf.get_segments())
	{
		ret = svcWriteDebugProcessMemory(debug, seg.data, heap + seg.phdr->p_vaddr, seg.phdr->p_filesz);
		if (ret) break;
	}

	svcCloseHandle(debug);
	if (ret) {
		svcUnmapProcessMemory(elf_data, proc, map_addr, 0x200000);
		svcUnmapProcessCodeMemory(proc, map_addr, heap_buffer_address, 0x200000);
		return ret;
	}
	
	// Unmap heap, map new code
	
	u64 load_addr = game_start_address - 0x200000;
	ret = svcMapProcessCodeMemory(proc, load_addr, heap, (max_vaddr - min_vaddr));
	if (R_FAILED(ret)) {
		SaltyNX_printf(APP_NAME ": Search for size %lx\n", (max_vaddr - min_vaddr));
		do
		{
			load_addr = randomGet64() & 0xFFFFFF000ull;
			ret = svcMapProcessCodeMemory(proc, load_addr, heap, (max_vaddr - min_vaddr));
		}
		while (ret == 0xDC01 || ret == 0xD401);
		if (ret) {
			svcUnmapProcessMemory(elf_data, proc, map_addr, 0x200000);
			svcUnmapProcessCodeMemory(proc, map_addr, heap_buffer_address, 0x200000);
			return ret;
		}
	}
	
	SaltyNX_printf(APP_NAME ": Found free address space at %lx, size %lx\n", load_addr, (max_vaddr - min_vaddr));
	
	// Adjust permissions and then return
	for (auto seg : elf.get_segments())
	{
		u8 perms = 0;
		for (auto c : seg.segment_flags)
		{
			switch (c)
			{
				case 'R':
					perms |= Perm_R;
					break;
				case 'W':
					perms |= Perm_W;
					break;
				case 'E':
					perms |= Perm_X;
					break;
			}
		}

		svcSetProcessMemoryPermission(proc, load_addr + seg.phdr->p_vaddr, (seg.phdr->p_memsz + 0xFFF) & ~0xFFF, perms);
	}
	
	*start = load_addr;
	*total_size = (max_vaddr - min_vaddr);
	svcUnmapProcessMemory(elf_data, proc, map_addr, 0x200000);
	svcUnmapProcessCodeMemory(proc, map_addr, heap_buffer_address, 0x200000);
	return ret;
}

Result load_elf32_proc(Handle proc, uint64_t pid, uint32_t heap, uint32_t* start, uint32_t* total_size, FILE* f, u32 elf_size)
{
	Result ret;
	Handle debug;
	
	*start = 0;
	*total_size = 0;
	u64 map_addr = 0;

	uint8_t* elf_data = 0;
	uint64_t heap_buffer_address = ((heap+0x200000) & ~0x1FFFFF);

	MemoryInfo meminfo;
	u32 pageinfo;

	Result rc = svcQueryProcessMemory(&meminfo, &pageinfo, proc, heap);
	if (R_FAILED(rc)) {
		SaltyNX_printf(APP_NAME ": load_elf_proc failed query mapping 0x%lx with 0x%x\n", heap, rc);
	}
	else {
		SaltyNX_printf(APP_NAME ": load_elf_proc query mapping 0x%lx, addr: 0x%lx, Perm: %d, size: 0x%lx\n", heap, meminfo.addr, meminfo.perm, meminfo.size);
		do
		{
			map_addr = randomGet64() & 0xFFFFFF000ull;
			ret = svcMapProcessCodeMemory(proc, map_addr, heap_buffer_address, 0x200000);
		}
		while (ret == 0xDC01 || ret == 0xD401);
		svcSetProcessMemoryPermission(proc, map_addr, 0x200000, Perm_Rw);
		virtmemLock();
		elf_data = (uint8_t*)virtmemFindAslr(0x200000, 0);
		rc = svcMapProcessMemory(elf_data, proc, map_addr, 0x200000);
		virtmemUnlock();
	}

	if (R_FAILED(rc)) {
		SaltyNX_printf(APP_NAME ": load_elf_proc failed mapping 0x%lx to 0x%lx with 0x%x\n", heap, elf_data, rc);
		return 1;
	}

	fread(elf_data, elf_size, 1, f);
	
	elf32_parser::Elf32_parser elf(elf_data);

	// Figure out our number of pages
	u32 min_vaddr = -1, max_vaddr = 0;
	for (auto seg : elf.get_segments())
	{
		u32 min = seg.phdr->p_vaddr;
		u32 max = seg.phdr->p_vaddr + ((seg.phdr->p_memsz + 0xFFF) & ~0xFFF);
		if (min < min_vaddr)
			min_vaddr = min;

		if (max > max_vaddr)
			max_vaddr = max;
	}
	
	// Debug the process to write into the heap addr provided
	// Note: Could probably just use buffer descs for this but whatever.
	ret = svcDebugActiveProcess(&debug, pid);
	if (ret) {
		svcUnmapProcessMemory(elf_data, proc, map_addr, 0x200000);
		svcUnmapProcessCodeMemory(proc, map_addr, heap_buffer_address, 0x200000);
		return ret;
	}

	for (auto seg : elf.get_segments())
	{
		ret = svcWriteDebugProcessMemory(debug, seg.data, heap + seg.phdr->p_vaddr, seg.phdr->p_filesz);
		if (ret) break;
	}

	svcCloseHandle(debug);
	if (ret) {
		svcUnmapProcessMemory(elf_data, proc, map_addr, 0x200000);
		svcUnmapProcessCodeMemory(proc, map_addr, heap_buffer_address, 0x200000);
		return ret;
	}
	
	// Unmap heap, map new code
	
	// Try to place Core32 right below the game (like the 64-bit loader does), so A32 branches (+-32 MiB)
	// from the game's code can reach the code cave inside Core32. Fall back to a random address.
	u32 load_size = (max_vaddr - min_vaddr);
	u32 load_addr = 0;
	ret = 1;
	if (game_start_address > ((load_size + 0xFFFF) & ~0xFFFF) && game_start_address <= 0xFFFFFFFF) {
		load_addr = (u32)(game_start_address - ((load_size + 0xFFFF) & ~0xFFFF));
		ret = svcMapProcessCodeMemory(proc, load_addr, heap, load_size);
	}
	if (R_FAILED(ret)) {
		SaltyNX_printf(APP_NAME ": Search for size %lx\n", load_size);
		do
		{	
			randomGet(&load_addr, 4);
			load_addr &= 0xFFFF000ul;
			ret = svcMapProcessCodeMemory(proc, load_addr, heap, load_size);
		}
		while (ret == 0xDC01 || ret == 0xD401);
	}
	if (ret) {
		svcUnmapProcessMemory(elf_data, proc, map_addr, 0x200000);
		svcUnmapProcessCodeMemory(proc, map_addr, heap_buffer_address, 0x200000);
		return ret;
	}
	
	SaltyNX_printf(APP_NAME ": Found free address space at %lx, size %lx\n", load_addr, (max_vaddr - min_vaddr));
	
	// Adjust permissions and then return. Core32 relocates itself (crt0 -> __nx_dynamic).
	for (auto seg : elf.get_segments())
	{
		u8 perms = 0;
		for (auto c : seg.segment_flags)
		{
			switch (c)
			{
				case 'R':
					perms |= Perm_R;
					break;
				case 'W':
					perms |= Perm_W;
					break;
				case 'E':
					perms |= Perm_X;
					break;
			}
		}

		svcSetProcessMemoryPermission(proc, load_addr + seg.phdr->p_vaddr, (seg.phdr->p_memsz + 0xFFF) & ~0xFFF, perms);
	}

	*start = load_addr;
	*total_size = (max_vaddr - min_vaddr);

	svcUnmapProcessMemory(elf_data, proc, map_addr, 0x200000);
	svcUnmapProcessCodeMemory(proc, map_addr, heap_buffer_address, 0x200000);
	return ret;
}