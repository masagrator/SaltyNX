#include "types.h"
#include <elf.h>

#ifndef DT_RELRSZ
#define DT_RELRSZ 35
#endif
#ifndef DT_RELR
#define DT_RELR 36
#endif

// Called by crt0 before anything else, while nothing is relocated yet: it must not touch any global
// (they would be reached through the unrelocated GOT). Only R_ARM_RELATIVE is applied, everything else
// (f.e. R_ARM_ABS32 against undefined weak symbols, which stay 0) is left alone.
void __nx_dynamic(uintptr_t base, const Elf32_Dyn* dyn)
{
	const Elf32_Rel* rel = NULL;
	u32 relsz = 0;
	const u32* relr = NULL;
	u32 relrsz = 0;

	for (; dyn->d_tag != DT_NULL; dyn++)
	{
		switch (dyn->d_tag)
		{
			case DT_REL:
				rel = (const Elf32_Rel*)(base + dyn->d_un.d_ptr);
				break;
			case DT_RELSZ:
				relsz = dyn->d_un.d_val / sizeof(Elf32_Rel);
				break;
			case DT_RELR:
				relr = (const u32*)(base + dyn->d_un.d_ptr);
				break;
			case DT_RELRSZ:
				relrsz = dyn->d_un.d_val / sizeof(u32);
				break;
		}
	}

	for (; rel && relsz--; rel++)
	{
		if (ELF32_R_TYPE(rel->r_info) == R_ARM_RELATIVE)
			*(u32*)(base + rel->r_offset) += base;
	}

	// RELR: an even word is an address to relocate, an odd word is a bitmap of the next 31 words.
	u32* where = NULL;
	for (; relr && relrsz--; relr++)
	{
		const u32 entry = *relr;
		if ((entry & 1) == 0)
		{
			where = (u32*)(base + entry);
			*where++ += base;
		}
		else
		{
			for (u32 bits = entry >> 1, i = 0; bits; bits >>= 1, i++)
				if (bits & 1) where[i] += base;
			where += 31;
		}
	}
}
