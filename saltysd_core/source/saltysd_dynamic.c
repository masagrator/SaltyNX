#include <elf.h>

#if defined(SWITCH) || defined(OUNCE)
#define DT_RELRENT	37
typedef Elf64_Xword Elf64_Relr;
#endif

#include <switch_min.h>
#include <stdlib.h>
#include "saltysd_ipc.h"

#include "useful.h"

void** elfs = NULL;
uint32_t num_elfs = 0;

void** builtin_elfs = NULL;
uint32_t num_builtin_elfs = 0;

struct nso_header
{
	uint32_t start;
	uint32_t mod;
};

struct mod0_header
{
	uint32_t magic;
	uint32_t dynamic;
};

struct ReplacedSymbol
{
	void* address;
	const char* name;
};

uintptr_t roLoadModule = 0;

struct ReplacedSymbol* replaced_symbols = NULL;
int32_t num_replaced_symbols = 0;

bool relr_available = false;

uintptr_t SaltySDCore_GetSymbolAddr(void* base, const char* name)
{
	#if defined(SWITCH32) || defined(OUNCE32)
	const Elf32_Dyn* dyn = NULL;
	const Elf32_Sym* symtab = NULL;
	#else
	const Elf64_Dyn* dyn = NULL;
	const Elf64_Sym* symtab = NULL;
	#endif
	const char* strtab = NULL;
	
	uint64_t numsyms = 0;
	
	struct nso_header* header = (struct nso_header*)base;
	struct mod0_header* modheader = (struct mod0_header*)(base + header->mod);
	#if defined(SWITCH32) || defined(OUNCE32)
	dyn = (const Elf32_Dyn*)((void*)modheader + modheader->dynamic);
	#else
	dyn = (const Elf64_Dyn*)((void*)modheader + modheader->dynamic);
	#endif

	for (; dyn->d_tag != DT_NULL; dyn++)
	{
		switch (dyn->d_tag)
		{
			case DT_SYMTAB:
				#if defined(SWITCH32) || defined(OUNCE32)
				symtab = (const Elf32_Sym*)(base + dyn->d_un.d_ptr);
				#else
				symtab = (const Elf64_Sym*)(base + dyn->d_un.d_ptr);
				#endif
				break;
			case DT_STRTAB:
				strtab = (const char*)(base + dyn->d_un.d_ptr);
				break;
			case DT_RELA:
				break;
			case DT_RELR:
				break;
			case DT_RELRSZ:
				break;
			case DT_RELASZ:
				break;
		}
	}
	
	#if defined(SWITCH32) || defined(OUNCE32)
	numsyms = ((void*)strtab - (void*)symtab) / sizeof(Elf32_Sym);
	#else
	numsyms = ((void*)strtab - (void*)symtab) / sizeof(Elf64_Sym);
	#endif

	for (int i = 0; i < numsyms; i++)
	{
		if (!strcmp(strtab + symtab[i].st_name, name) && symtab[i].st_value)
		{
			return (uint64_t)base + symtab[i].st_value;
		}
	}

	return 0;
}

uintptr_t SaltySDCore_FindSymbol(const char* name)
{
	if (!elfs) return 0;

	for (int i = 0; i < num_elfs; i++)
	{
		uintptr_t ptr = SaltySDCore_GetSymbolAddr(elfs[i], name);
		if (ptr) return ptr;
	}
	
	return 0;
}

uintptr_t SaltySDCore_FindSymbolBuiltin(const char* name)
{
	if (!builtin_elfs) return 0;

	for (int i = 0; i < num_builtin_elfs; i++)
	{
		uintptr_t ptr = SaltySDCore_GetSymbolAddr(builtin_elfs[i], name);
		if (ptr) return ptr;
	}
	
	return 0;
}

void SaltySDCore_RegisterModule(uintptr_t base)
{
	elfs = realloc(elfs, ++num_elfs * sizeof(void*));
	elfs[num_elfs-1] = (void*)base;
}

void SaltySDCore_RegisterBuiltinModule(uintptr_t base)
{
	builtin_elfs = realloc(builtin_elfs, ++num_builtin_elfs * sizeof(void*));
	builtin_elfs[num_builtin_elfs-1] = (void*)base;
}

void SaltySDCore_ReplaceModuleImport(void* base, const char* name, void* newfunc, bool update)
{
	
	#if defined(SWITCH32) || defined(OUNCE32)
	const Elf32_Dyn* dyn = NULL;
	const Elf32_Rel* rela = NULL;
	const Elf32_Sym* symtab = NULL;
	#else
	const Elf64_Dyn* dyn = NULL;
	const Elf64_Rela* rela = NULL;
	const Elf64_Sym* symtab = NULL;
	#endif
	char* strtab = NULL;
	uint64_t relasz = 0;
	
	struct nso_header* header = (struct nso_header*)base;
	struct mod0_header* modheader = (struct mod0_header*)(base + header->mod);
	#if defined(SWITCH32) || defined(OUNCE32)
	dyn = (const Elf32_Dyn*)((void*)modheader + modheader->dynamic);
	#else
	dyn = (const Elf64_Dyn*)((void*)modheader + modheader->dynamic);
	#endif

	for (; dyn->d_tag != DT_NULL; dyn++)
	{
		switch (dyn->d_tag)
		{
			case DT_SYMTAB:
				#if defined(SWITCH32) || defined(OUNCE32)
				symtab = (const Elf32_Sym*)(base + dyn->d_un.d_ptr);
				#else
				symtab = (const Elf64_Sym*)(base + dyn->d_un.d_ptr);
				#endif
				break;
			case DT_STRTAB:
				strtab = (char*)(base + dyn->d_un.d_ptr);
				break;
			#if defined(SWITCH32) || defined(OUNCE32)
			case DT_REL:
				rela = (const Elf32_Rel*)(base + dyn->d_un.d_ptr);
				break;
			case DT_RELSZ:
				relasz += dyn->d_un.d_val / sizeof(Elf32_Rel);
				break;
			#else
			case DT_RELA:
				rela = (const Elf64_Rela*)(base + dyn->d_un.d_ptr);
				break;
			case DT_RELR:
				relr_available = true;
				break;
			case DT_RELRSZ:
				break;
			case DT_RELASZ:
				relasz += dyn->d_un.d_val / sizeof(Elf64_Rela);
				break;
			#endif
			case DT_PLTRELSZ:
				#if defined(SWITCH32) || defined(OUNCE32)
				relasz += dyn->d_un.d_val / sizeof(Elf32_Rel);
				#else
				relasz += dyn->d_un.d_val / sizeof(Elf64_Rela);
				#endif
				break;
		}
	}

	if (rela == NULL || symtab == NULL || strtab == NULL)
	{
		return;
	}
	
	#if defined(SWITCH32) || defined(OUNCE32)
	size_t numsyms = ((void*)strtab - (void*)symtab) / sizeof(Elf32_Sym);
	#else
	size_t numsyms = ((void*)strtab - (void*)symtab) / sizeof(Elf64_Sym);
	#endif

	if (!update) {
		bool detected = false;
		int detecteditr = 0;
		for (int i = 0; i < num_replaced_symbols; i++) {
			if (!strcmp(name, replaced_symbols[i].name)) {
				detected = true;
				detecteditr = i;
			}
		}
		if (!detected) {
			replaced_symbols = realloc(replaced_symbols, ++num_replaced_symbols * sizeof(struct ReplacedSymbol));
			replaced_symbols[num_replaced_symbols-1].address = newfunc;
			replaced_symbols[num_replaced_symbols-1].name = name;
		}
		else {
			newfunc = replaced_symbols[detecteditr].address;
		}
	}

	
	for (int rela_idx = 0; relasz--; rela++, rela_idx++)
	{
		#if defined(SWITCH32) || defined(OUNCE32)
		if (ELF32_R_TYPE(rela->r_info) == R_ARM_RELATIVE) continue;
		uint32_t sym_idx = ELF32_R_SYM(rela->r_info);
		#else
		if (ELF64_R_TYPE(rela->r_info) == R_AARCH64_RELATIVE) continue;
		uint32_t sym_idx = ELF64_R_SYM(rela->r_info);
		#endif
		
		if (sym_idx >= numsyms) continue;

		char* rel_name = strtab + symtab[sym_idx].st_name;
		if (strcmp(name, rel_name)) continue;
		#if defined(SWITCH32) || defined(OUNCE32)
		SaltySDCore_printf("SaltySD Core: %x %x %x %s to %p, %p + %x = %p\n", symtab[sym_idx].st_value, (uint32_t)rela - (uint32_t)base, rela_idx, rel_name, newfunc, base, rela->r_offset, base + rela->r_offset);
		Elf32_Rel replacement;
		replacement.r_offset = rela->r_offset;
		replacement.r_info = 0x17;

		SaltySD_Memcpy((u32)rela, (u32)&replacement, sizeof(Elf32_Rel));
		*(void**)(base + rela->r_offset) = newfunc;

		#else
		SaltySDCore_printf("SaltySD Core: %x %s to 0x%lx, %lx 0x%lx\n", rela_idx, rel_name, newfunc, rela->r_offset, base + rela->r_offset);
		
		if (!update) {
			Elf64_Rela replacement = *rela;
			replacement.r_addend = rela->r_addend + (uint64_t)newfunc - SaltySDCore_FindSymbolBuiltin(rel_name);

			SaltySD_Memcpy((u64)rela, (u64)&replacement, sizeof(Elf64_Rela));
		}
		else {
			*(void**)(base + rela->r_offset) = newfunc;
		}
		#endif
	}
}

void SaltySDCore_ReplaceImport(const char* name, void* newfunc)
{
	if (!builtin_elfs) return;

	for (int i = 0; i < num_builtin_elfs; i++)
	{
		SaltySDCore_ReplaceModuleImport(builtin_elfs[i], name, newfunc, false);
	}
}

void SaltySDCore_DynamicLinkModule(void* base)
{
	#if defined(SWITCH32) || defined(OUNCE32)
	const Elf32_Dyn* dyn = NULL;
	const Elf32_Rel* rel = NULL;
	const Elf32_Sym* symtab = NULL;
	uint64_t relsz = 0;
	#else
	const Elf64_Dyn* dyn = NULL;
	const Elf64_Rela* rela = NULL;
	const Elf64_Sym* symtab = NULL;
	uint64_t relasz = 0;
	#endif
	char* strtab = NULL;
	
	struct nso_header* header = (struct nso_header*)base;
	struct mod0_header* modheader = (struct mod0_header*)(base + header->mod);

	#if defined(SWITCH32) || defined(OUNCE32)
	dyn = (const Elf32_Dyn*)((void*)modheader + modheader->dynamic);
	#else
	dyn = (const Elf64_Dyn*)((void*)modheader + modheader->dynamic);
	#endif

	for (; dyn->d_tag != DT_NULL; dyn++)
	{
		switch (dyn->d_tag)
		{
			case DT_SYMTAB:
				#if defined(SWITCH32) || defined(OUNCE32)
				symtab = (const Elf32_Sym*)(base + dyn->d_un.d_ptr);
				#else
				symtab = (const Elf64_Sym*)(base + dyn->d_un.d_ptr);
				#endif
				break;
			case DT_STRTAB:
				strtab = (char*)(base + dyn->d_un.d_ptr);
				break;
			#if defined(SWITCH32) || defined(OUNCE32)
			case DT_REL:
				rel = (const Elf32_Rel*)(base + dyn->d_un.d_ptr);
				break;
			case DT_RELSZ:
				relsz += dyn->d_un.d_val / sizeof(Elf32_Rel);
				break;
			#else
			case DT_RELA:
				rela = (const Elf64_Rela*)(base + dyn->d_un.d_ptr);
				break;
			case DT_RELASZ:
				relasz += dyn->d_un.d_val / sizeof(Elf64_Rela);
				break;
			case DT_RELR:
				relr_available = true;
				break;
			case DT_RELRSZ:
				break;
			#endif
			case DT_PLTRELSZ:
				#if defined(SWITCH32) || defined(OUNCE32)
				relsz += dyn->d_un.d_val / sizeof(Elf32_Rel);
				#else
				relasz += dyn->d_un.d_val / sizeof(Elf64_Rela);
				#endif
				break;
		}
	}

	#if defined(SWITCH32) || defined(OUNCE32)
	if (rel == NULL)
	{
		return;
	}

	for (; relsz--; rel++)
	{
		if (ELF32_R_TYPE(rel->r_info) == R_ARM_RELATIVE) continue;

		uint32_t sym_idx = ELF32_R_SYM(rel->r_info);
		char* name = strtab + symtab[sym_idx].st_name;

		uint32_t sym_val = (uint32_t)base + symtab[sym_idx].st_value;
		if (!symtab[sym_idx].st_value)
			sym_val = 0;

		if (!symtab[sym_idx].st_shndx && sym_idx)
			sym_val = SaltySDCore_FindSymbol(name);

		uint32_t sym_val_and_addend = sym_val + 0;

		SaltySDCore_printf("SaltySD Core: %x 0x%lx->0x%lx %s\n", sym_idx, symtab[sym_idx].st_value + 0, sym_val_and_addend, name);

		switch (ELF32_R_TYPE(rel->r_info))
		{
			case R_ARM_GLOB_DAT:
			case R_ARM_JUMP_SLOT:
			case R_ARM_ABS32:
			{
				uint64_t* ptr = (uint64_t*)(base + rel->r_offset);
				*ptr = sym_val_and_addend;
				break;
			}
		}
	}	
	#else
	if (rela == NULL)
	{
		return;
	}

	for (; relasz--; rela++)
	{
		if (ELF64_R_TYPE(rela->r_info) == R_AARCH64_RELATIVE) continue;

		uint32_t sym_idx = ELF64_R_SYM(rela->r_info);
		char* name = strtab + symtab[sym_idx].st_name;

		uint64_t sym_val = (uint64_t)base + symtab[sym_idx].st_value;
		if (!symtab[sym_idx].st_value)
			sym_val = 0;

		if (!symtab[sym_idx].st_shndx && sym_idx)
			sym_val = SaltySDCore_FindSymbol(name);

		uint64_t sym_val_and_addend = sym_val + rela->r_addend;

		SaltySDCore_printf("SaltySD Core: %x 0x%lx->0x%lx %s\n", sym_idx, symtab[sym_idx].st_value + rela->r_addend, sym_val_and_addend, name);

		switch (ELF64_R_TYPE(rela->r_info))
		{
			case R_AARCH64_GLOB_DAT:
			case R_AARCH64_JUMP_SLOT:
			case R_AARCH64_ABS64:
			{
				uint64_t* ptr = (uint64_t*)(base + rela->r_offset);
				*ptr = sym_val_and_addend;
				break;
			}
		}
	}
	#endif
}

struct Object {
	void* next;
	void* prev;
	void* rela_or_rel_plt;
	void* rela_or_rel;
	void* module_base;
	void* module_base_new;
};

struct Module {
	struct Object* ModuleObject;
};

void SaltySDCore_fillRoLoadModule() {
	roLoadModule = SaltySDCore_FindSymbolBuiltin("_ZN2nn2ro10LoadModuleEPNS0_6ModuleEPKvPvmi");
	return;
}

bool SaltySDCore_isRelrAvailable() {
	#if defined(SWITCH) || defined(OUNCE)
	return relr_available;
	#else
	return false;
	#endif

}

void SaltySDCore_getDataForUpdate(uint32_t* num_builtin_elfs_ptr, int32_t* num_replaced_symbols_ptr, struct ReplacedSymbol** replaced_symbols_ptr, void*** builtin_elfs_ptr) {
	*num_builtin_elfs_ptr = num_builtin_elfs;
	*num_replaced_symbols_ptr = num_replaced_symbols;
	*builtin_elfs_ptr = builtin_elfs;
	*replaced_symbols_ptr = replaced_symbols;
}

typedef Result (*_ZN2nn2ro10LoadModuleEPNS0_6ModuleEPKvPvmi)(struct Module* pOutModule, const void* pImage, void* buffer, size_t bufferSize, int flag);
Result LoadModule(struct Module* pOutModule, const void* pImage, void* buffer, size_t bufferSize, int flag) {
	if (flag)
		flag = 0;
	Result ret = ((_ZN2nn2ro10LoadModuleEPNS0_6ModuleEPKvPvmi)(roLoadModule))(pOutModule, pImage, buffer, bufferSize, flag);
	if (R_SUCCEEDED(ret)) {
		for (int x = 0; x < num_replaced_symbols; x++) {
			if (pOutModule->ModuleObject->module_base)
				SaltySDCore_ReplaceModuleImport(pOutModule->ModuleObject->module_base, replaced_symbols[x].name, replaced_symbols[x].address, true);
			else SaltySDCore_ReplaceModuleImport(pOutModule->ModuleObject->module_base_new, replaced_symbols[x].name, replaced_symbols[x].address, true);
		}
	}
	return ret;
}