#include "saltysd_core.h"

#include "bm.h"
#include <errno.h>

extern void _start();
u32 code_start = 0;

u32 SaltySDCore_getCodeStart()
{
	if (code_start) return code_start;

	u32 addr = 0;
	while (1)
	{
		MemoryInfo info;
		u32 pageinfo;
		Result ret = svcQueryMemory(&info, &pageinfo, addr);
		
		if (info.addr != (u32)_start && info.perm == Perm_Rx)
		{
			addr = info.addr;
			break;
		}

		addr = info.addr + info.size;

		if (!addr || ret) break;
	}
	
	code_start = (u32)addr;
	return addr;
}

u64 SaltySDCore_getCodeSize()
{
	MemoryInfo info;
	u32 pageinfo;
	Result ret = svcQueryMemory(&info, &pageinfo, SaltySDCore_getCodeStart());
	
	if (ret) return 0;
	
	return info.size;
}

u32 SaltySDCore_findCode(u8* code, size_t size)
{
	Result ret = 0;
	u32 addr = SaltySDCore_getCodeStart();
	u32 addr_size = SaltySDCore_getCodeSize();

	while (1)
	{
		void* out = boyer_moore_search((void*)addr, addr_size, code, size);
		if (out) return (u32)out;
		
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