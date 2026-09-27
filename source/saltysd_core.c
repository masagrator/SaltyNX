#include "saltysd_core.h"

#include "bm.h"

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