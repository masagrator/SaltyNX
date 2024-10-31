#include <elf.h>
#define DT_RELRENT	37
typedef Elf64_Xword Elf64_Relr;

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

uint64_t roLoadModule = 0;

struct ReplacedSymbol* replaced_symbols = NULL;
int32_t num_replaced_symbols = 0;

bool relr_available = false;

uint64_t SaltySDCore_GetSymbolAddr(void* base, const char* name)
{
	const Elf64_Dyn* dyn = NULL;
	const Elf64_Sym* symtab = NULL;
	const char* strtab = NULL;
	
	uint64_t numsyms = 0;
	
	struct nso_header* header = (struct nso_header*)base;
	struct mod0_header* modheader = (struct mod0_header*)(base + header->mod);
	dyn = (const Elf64_Dyn*)((void*)modheader + modheader->dynamic);

	for (; dyn->d_tag != DT_NULL; dyn++)
	{
		switch (dyn->d_tag)
		{
			case DT_SYMTAB:
				symtab = (const Elf64_Sym*)(base + dyn->d_un.d_ptr);
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
	
	numsyms = ((void*)strtab - (void*)symtab) / sizeof(Elf64_Sym);
	
	for (int i = 0; i < numsyms; i++)
	{
		if (!strcmp(strtab + symtab[i].st_name, name) && symtab[i].st_value)
		{
			return (uint64_t)base + symtab[i].st_value;
		}
	}

	return 0;
}

uint64_t SaltySDCore_FindSymbol(const char* name)
{
	if (!elfs) return 0;

	for (int i = 0; i < num_elfs; i++)
	{
		uint64_t ptr = SaltySDCore_GetSymbolAddr(elfs[i], name);
		if (ptr) return ptr;
	}
	
	return 0;
}

uint64_t SaltySDCore_FindSymbolBuiltin(const char* name)
{
	if (!builtin_elfs) return 0;

	for (int i = 0; i < num_builtin_elfs; i++)
	{
		uint64_t ptr = SaltySDCore_GetSymbolAddr(builtin_elfs[i], name);
		if (ptr) return ptr;
	}
	
	return 0;
}

void SaltySDCore_RegisterModule(void* base)
{
	elfs = realloc(elfs, ++num_elfs * sizeof(void*));
	elfs[num_elfs-1] = base;
}

void SaltySDCore_RegisterBuiltinModule(void* base)
{
	builtin_elfs = realloc(builtin_elfs, ++num_builtin_elfs * sizeof(void*));
	builtin_elfs[num_builtin_elfs-1] = base;
}

void SaltySDCore_ReplaceModuleImport(void* base, const char* name, void* newfunc, bool update)
{
	const Elf64_Dyn* dyn = NULL;
	const Elf64_Rela* rela = NULL;
	const Elf64_Sym* symtab = NULL;
	char* strtab = NULL;
	uint64_t relasz = 0;
	
	struct nso_header* header = (struct nso_header*)base;
	struct mod0_header* modheader = (struct mod0_header*)(base + header->mod);
	dyn = (const Elf64_Dyn*)((void*)modheader + modheader->dynamic);

	for (; dyn->d_tag != DT_NULL; dyn++)
	{
		switch (dyn->d_tag)
		{
			case DT_SYMTAB:
				symtab = (const Elf64_Sym*)(base + dyn->d_un.d_ptr);
				break;
			case DT_STRTAB:
				strtab = (char*)(base + dyn->d_un.d_ptr);
				break;
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
			case DT_PLTRELSZ:
				relasz += dyn->d_un.d_val / sizeof(Elf64_Rela);
				break;
		}
	}

	if (rela == NULL || symtab == NULL || strtab == NULL)
	{
		return;
	}
	
	uint64_t numsyms = ((void*)strtab - (void*)symtab) / sizeof(Elf64_Sym);

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

	int rela_idx = 0;
	for (; relasz--; rela++, rela_idx++)
	{
		if (ELF64_R_TYPE(rela->r_info) == R_AARCH64_RELATIVE) continue;
		
		uint32_t sym_idx = ELF64_R_SYM(rela->r_info);
		if (sym_idx >= numsyms) continue;

		char* rel_name = strtab + symtab[sym_idx].st_name;
		if (strcmp(name, rel_name)) continue;
		
		SaltySDCore_printf("SaltySD Core: %x %s to %p, %llx %p\n", rela_idx, rel_name, newfunc, rela->r_offset, base + rela->r_offset);
		
		if (!update) {
			Elf64_Rela replacement = *rela;
			replacement.r_addend = rela->r_addend + (uint64_t)newfunc - SaltySDCore_FindSymbolBuiltin(rel_name);

			SaltySD_Memcpy((u64)rela, (u64)&replacement, sizeof(Elf64_Rela));
		}
		else {
			*(void**)(base + rela->r_offset) = newfunc;
		}
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
	const Elf64_Dyn* dyn = NULL;
	const Elf64_Rela* rela = NULL;
	const Elf64_Sym* symtab = NULL;
	char* strtab = NULL;
	uint64_t relasz = 0;
	
	struct nso_header* header = (struct nso_header*)base;
	struct mod0_header* modheader = (struct mod0_header*)(base + header->mod);
	dyn = (const Elf64_Dyn*)((void*)modheader + modheader->dynamic);

	for (; dyn->d_tag != DT_NULL; dyn++)
	{
		switch (dyn->d_tag)
		{
			case DT_SYMTAB:
				symtab = (const Elf64_Sym*)(base + dyn->d_un.d_ptr);
				break;
			case DT_STRTAB:
				strtab = (char*)(base + dyn->d_un.d_ptr);
				break;
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
			case DT_PLTRELSZ:
				relasz += dyn->d_un.d_val / sizeof(Elf64_Rela);
				break;
		}
	}

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

		SaltySDCore_printf("SaltySD Core: %x %llx->%llx %s\n", sym_idx, symtab[sym_idx].st_value + rela->r_addend, sym_val_and_addend, name);

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
	return relr_available;
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