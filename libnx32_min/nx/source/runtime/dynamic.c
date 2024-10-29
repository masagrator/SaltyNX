#include "result.h"
#include "services/fatal.h"
#include <elf.h>

void __nx_dynamic(uintptr_t base, const Elf32_Dyn* dyn)
{
	const Elf32_Rel* rel = NULL;
	u32 relsz = 0;

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
		}
	}

	if (rel == NULL)
		fatalSimple(MAKERESULT(Module_Libnx, LibnxError_BadReloc));

	for (; relsz--; rel++)
	{
		switch (ELF32_R_TYPE(rel->r_info))
		{
			case R_ARM_RELATIVE:
			{
				u32* ptr = (u32*)(base + rel->r_offset);
				*ptr += base;
				break;
			}
		}
	}
}