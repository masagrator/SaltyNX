#include "saltysd_core.h"

#include "bm.h"
#include <errno.h>

extern void _start();
uintptr_t code_start = 0;

uintptr_t SaltySDCore_getCodeStart()
{
	if (code_start) return code_start;

	uintptr_t addr = 0;
	while (1)
	{
		MemoryInfo info;
		u32 pageinfo;
		Result ret = svcQueryMemory(&info, &pageinfo, addr);
		
		if (info.addr != (uintptr_t)_start && info.perm == Perm_Rx)
		{
			addr = info.addr;
			break;
		}

		addr = info.addr + info.size;

		if (!addr || ret) break;
	}
	
	code_start = addr;
	return addr;
}

size_t SaltySDCore_getCodeSize()
{
	MemoryInfo info;
	u32 pageinfo;
	Result ret = svcQueryMemory(&info, &pageinfo, SaltySDCore_getCodeStart());
	
	if (ret) return 0;
	
	return info.size;
}

uintptr_t SaltySDCore_findCode(u8* code, size_t size)
{
	Result ret = 0;
	uintptr_t addr = SaltySDCore_getCodeStart();
	size_t addr_size = SaltySDCore_getCodeSize();

	while (1)
	{
		void* out = boyer_moore_search((void*)addr, addr_size, code, size);
		if (out) return (uintptr_t)out;
		
		addr += addr_size;

		while (1)
		{
			MemoryInfo info;
			u32 pageinfo;
			ret = svcQueryMemory(&info, &pageinfo, addr);
			
			if (info.perm != Perm_Rx && info.perm != Perm_R && info.perm != Perm_Rw)
			{
				addr = info.addr + info.size;
			}
			else
			{
				addr = info.addr;
				addr_size = info.size;
				break;
			}
			if (!addr || ret) break;
		}
		
		if (!addr || ret) break;
	}

	return 0;
}

uintptr_t SaltySDCore_findCodeEx(u8* code, size_t size)
{
	Result ret = 0;
	uintptr_t addr = SaltySDCore_getCodeStart();
	size_t addr_size = SaltySDCore_getCodeSize();

	while (1)
	{
		void* out = boyer_moore_search((void*)addr, addr_size, code, size);
		if (out) return (uintptr_t)out;
		
		addr += addr_size;

		while (1)
		{
			MemoryInfo info;
			u32 pageinfo;
			ret = svcQueryMemory(&info, &pageinfo, addr);
			
			if (info.perm != Perm_Rx)
			{
				addr = info.addr + info.size;
			}
			else
			{
				addr = info.addr;
				addr_size = info.size;
				break;
			}
			if (!addr || ret) break;
		}
		
		if (!addr || ret) break;
	}

	return 0;
}

FILE* SaltySDCore_fopen(const char* filename, const char* mode)
{
	return fopen(filename, mode);
}

size_t SaltySDCore_fread(void* ptr, size_t size, size_t count, FILE* stream)
{
	return fread(ptr, size, count, stream);
}

int SaltySDCore_fclose(FILE* stream)
{
	return fclose(stream);
}

int SaltySDCore_fseek(FILE* stream, int64_t offset, int origin)
{
	int ret = fseek(stream, offset, origin);
	if (ret)
		return errno;
	
	return 0;
}

size_t SaltySDCore_ftell(FILE* stream) {
	return ftell(stream);
}

int SaltySDCore_remove(const char* filename) {
	return remove(filename);
}

size_t SaltySDCore_fwrite(const void* ptr, size_t size, size_t count, FILE* stream) 
{
	return fwrite(ptr, size, count, stream);
}

DIR* SaltySDCore_opendir(const char* dirname)
{
	return opendir(dirname);
}

int SaltySDCore_mkdir(const char* dirname, mode_t mode)
{
	return mkdir(dirname, mode);
}

struct dirent* SaltySDCore_readdir(DIR* dirp)
{
	return readdir(dirp);
}

int SaltySDCore_closedir(DIR *dirp) 
{
	return closedir(dirp);
}