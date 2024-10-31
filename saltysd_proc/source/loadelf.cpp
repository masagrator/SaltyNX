#include "loadelf.h"

#include <istream>
#include <streambuf>

#include "useful.h"
#include "elf_parser.hpp"
#include "elf32_parser.hpp"

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
		
		if (info.perm == perm && info.size >= size && (info.type == MemType_CodeMutable || info.type == MemType_CodeStatic) && info.addr >= min)
			return info.addr;

		addr = info.addr + info.size;
		
		if (!addr || ret) break;
	}
	
	return 0;
}

Result load_elf_debug(Handle debug, uint64_t* start, uint8_t* elf_data, u32 elf_size)
{
	Result ret = 0;

	Elf_parser elf(elf_data);

	segment_t text_seg = elf.get_segments()[0];
	segment_t data_seg = elf.get_segments()[1];
	text_addr = find_memory(debug, 0, text_seg.phdr->p_memsz, Perm_Rx);
	data_addr = find_memory(debug, text_addr, data_seg.phdr->p_memsz, Perm_Rw);
	text_fsize = text_seg.phdr->p_filesz;
	data_fsize = data_seg.phdr->p_filesz;
	text_msize = text_seg.phdr->p_memsz;
	data_msize = data_seg.phdr->p_memsz;
	SaltySD_printf(".text to %llx, .data to %llx\n", text_addr, data_addr);
	
	elf.relocate_segment(0, text_addr);
	elf.relocate_segment(1, data_addr);
	
	*start = text_addr;
	
	text_save = malloc(text_msize);
	data_save = malloc(data_msize);
	
	ret = svcReadDebugProcessMemory(text_save, debug, text_addr, text_msize);
	ret = svcReadDebugProcessMemory(data_save, debug, data_addr, data_msize);

	ret = svcWriteDebugProcessMemory(debug, text_seg.data, text_addr, text_fsize);
	ret = svcWriteDebugProcessMemory(debug, data_seg.data, data_addr, data_fsize);
	
	return ret;
}

Result load_elf32_debug(Handle debug, uint64_t* start)
{
	Result ret = 0;
	text_addr = find_memory(debug, 0, 1000, Perm_Rx);
	data_addr = find_memory(debug, text_addr, 1000, Perm_Rw);
	ptrdiff_t offset = data_addr - text_addr;

	FILE* file = 0;
	uint8_t* elf_data = 0;
	u32 elf_size = 0;
	
	switch(offset) {
		case 0x3000: {
			file = fopen("sdmc:/SaltySD/saltysd_bootstrap32_3k.elf", "rb");
			if (!file) {
				SaltySD_printf("SaltySD: SaltySD/saltysd_bootstrap32_3k.elf not found, aborting...\n", ret);
				svcCloseHandle(debug);
				return false;
			}
			break;
		}
		case 0x5000: {
			file = fopen("sdmc:/SaltySD/saltysd_bootstrap32_5k.elf", "rb");
			if (!file) {
				SaltySD_printf("SaltySD: SaltySD/saltysd_bootstrap32_5k.elf not found, aborting...\n", ret);
				svcCloseHandle(debug);
				return false;
			}
			break;
		}
		default: {
				SaltySD_printf("SaltySD: Detected rtld text-data offset 0x%lx, not supported...\n", offset);
				svcCloseHandle(debug);
				return false;			
		}
	}

	fseek(file, 0, 2);
	elf_size = ftell(file);
	fseek(file, 0, 0);
	elf_data = (uint8_t*)malloc(elf_size);
	fread(elf_data, elf_size, 1, file);
	fclose(file);

	elf32_parser::Elf32_parser elf(elf_data);

	elf32_parser::segment_t text_seg = elf.get_segments()[0];
	elf32_parser::segment_t data_seg = elf.get_segments()[1];
	text_addr = find_memory(debug, 0, text_seg.phdr->p_memsz, Perm_Rx);
	data_addr = find_memory(debug, text_addr, data_seg.phdr->p_memsz, Perm_Rw);
	text_fsize = text_seg.phdr->p_filesz;
	data_fsize = data_seg.phdr->p_filesz;
	text_msize = text_seg.phdr->p_memsz;
	data_msize = data_seg.phdr->p_memsz;
	SaltySD_printf("32bit .text to %llx, .data to %llx\n", text_addr, data_addr);
	
	elf.relocate(text_addr, data_addr, 0);
	
	*start = text_addr;
	
	text_save = malloc(text_msize);
	data_save = malloc(data_msize);
	
	ret = svcReadDebugProcessMemory(text_save, debug, text_addr, text_msize);
	ret = svcReadDebugProcessMemory(data_save, debug, data_addr, data_msize);

	ret = svcWriteDebugProcessMemory(debug, text_seg.data, text_addr, text_fsize);
	ret = svcWriteDebugProcessMemory(debug, data_seg.data, data_addr, data_fsize);
	
	free(elf_data);

	return ret;
}

Result restore_elf_debug(Handle debug)
{
	Result ret;

	ret = svcWriteDebugProcessMemory(debug, text_save, text_addr, text_msize);
	ret = svcWriteDebugProcessMemory(debug, data_save, data_addr, data_msize);
	
	free(text_save);
	free(data_save);
	
	text_addr = 0;
	data_addr = 0;
	text_save = NULL;
	data_save = NULL;
	text_fsize = 0;
	data_fsize = 0;
	text_msize = 0;
	data_msize = 0;
	
	return ret;
}

Result load_elf_proc(Handle proc, uint64_t pid, uint64_t heap, uint64_t* start, uint64_t* total_size, uint8_t* elf_data, u32 elf_size)
{
	Result ret;
	Handle debug;
	
	*start = 0;
	*total_size = 0;
	
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
	if (ret) return ret;

	for (auto seg : elf.get_segments())
	{
		ret = svcWriteDebugProcessMemory(debug, seg.data, heap + seg.phdr->p_vaddr, seg.phdr->p_filesz);
		if (ret) break;
	}

	svcCloseHandle(debug);
	if (ret) return ret;
	
	// Unmap heap, map new code
	
	u64 load_addr;
	SaltySD_printf("SaltySD: Search for size %llx\n", (max_vaddr - min_vaddr));
	do
	{
		load_addr = randomGet64() & 0xFFFFFF000ull;
		ret = svcMapProcessCodeMemory(proc, load_addr, heap, (max_vaddr - min_vaddr));
	}
	while (ret == 0xDC01 || ret == 0xD401);
	if (ret) return ret;
	
	SaltySD_printf("SaltySD: Found free address space at %llx, size %llx\n", load_addr, (max_vaddr - min_vaddr));
	
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
	
	return ret;
}

Result load_elf32_proc(Handle proc, uint64_t pid, uint32_t heap, uint32_t* start, uint32_t* total_size, uint8_t* elf_data, u32 elf_size)
{
	Result ret;
	Handle debug;
	
	*start = 0;
	*total_size = 0;
	
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
	if (ret) return ret;

	for (auto seg : elf.get_segments())
	{
		ret = svcWriteDebugProcessMemory(debug, seg.data, heap + seg.phdr->p_vaddr, seg.phdr->p_filesz);
		if (ret) break;
	}

	svcCloseHandle(debug);
	if (ret) return ret;
	
	// Unmap heap, map new code
	
	u32 load_addr;
	SaltySD_printf("SaltySD: Search for size %llx\n", (max_vaddr - min_vaddr));
	do
	{	
		randomGet(&load_addr, 4);
		load_addr &= 0xFFFF000ul;
		ret = svcMapProcessCodeMemory(proc, load_addr, heap, (max_vaddr - min_vaddr));
	}
	while (ret == 0xDC01 || ret == 0xD401);
	if (ret) return ret;
	
	SaltySD_printf("SaltySD: Found free address space at %llx, size %llx\n", load_addr, (max_vaddr - min_vaddr));
	
	uint32_t data_addr = 0;
	uint32_t read_addr = 0;
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

		if (perms == Perm_Rw && !data_addr) {
			data_addr = load_addr + seg.phdr->p_vaddr;
		}
		else if (perms == Perm_R && !read_addr) {
			read_addr = load_addr + seg.phdr->p_vaddr;
		}
		svcSetProcessMemoryPermission(proc, load_addr + seg.phdr->p_vaddr, (seg.phdr->p_memsz + 0xFFF) & ~0xFFF, perms);
	}

	elf.relocate(load_addr, data_addr, read_addr);

	ret = svcDebugActiveProcess(&debug, pid);
	if (ret) return ret;

	for (auto seg : elf.get_segments())
	{
		ret = svcWriteDebugProcessMemory(debug, seg.data, load_addr + seg.phdr->p_vaddr, seg.phdr->p_filesz);
		if (ret) break;
	}
	svcCloseHandle(debug);

	*start = load_addr;
	*total_size = (max_vaddr - min_vaddr);
	
	return ret;
}