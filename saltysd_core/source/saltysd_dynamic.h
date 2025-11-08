#pragma once
#ifndef SALTYSD_DYNAMIC_H
#define SALTYSD_DYNAMIC_H

#include <stdint.h>

#include "useful.h"

#ifdef __cplusplus
extern "C" {
#endif

struct Module {
	void* ModuleObject;
};

struct ReplacedSymbol
{
	void* address;
	const char* name;
};

extern void SaltySDCore_fillRoLoadModule() LINKABLE;
extern Result LoadModule(struct Module* pOutModule, const void* pImage, void* buffer, size_t bufferSize, int flag) LINKABLE;
extern uintptr_t SaltySDCore_GetSymbolAddr(void* base, const char* name) LINKABLE;
extern uintptr_t SaltySDCore_FindSymbol(const char* name) LINKABLE;
extern uintptr_t SaltySDCore_FindSymbolBuiltin(const char* name) LINKABLE;
extern void SaltySDCore_RegisterModule(uintptr_t base) LINKABLE;
extern void SaltySDCore_RegisterBuiltinModule(uintptr_t base) LINKABLE;
extern void SaltySDCore_DynamicLinkModule(void* base) LINKABLE;
extern void SaltySDCore_ReplaceModuleImport(void* base, const char* name, void* newfunc, bool update) LINKABLE;
extern void SaltySDCore_ReplaceImport(const char* name, void* newfunc) LINKABLE;
extern bool SaltySDCore_isRelrAvailable();
extern void SaltySDCore_getDataForUpdate(uint32_t* num_builtin_elfs_ptr, int32_t* num_replaced_symbols_ptr, struct ReplacedSymbol** replaced_symbols_ptr, void*** builtin_elfs_ptr);

#ifdef __cplusplus
}
#endif

#endif // SALTYSD_DYNAMIC_H
