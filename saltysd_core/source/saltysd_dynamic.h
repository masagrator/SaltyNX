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
	u32 State;
	void* NroPtr;
	void* BssPtr;
	void* _x20;
	void* SourceBuffer;
	char Name[256]; /* Created by retype action */
	u8 _x130;
	u8 _x131;
	bool isLoaded;  // bool
};

extern void SaltySDCore_fillRoLoadModule() LINKABLE;
extern Result LoadModule(struct Module* pOutModule, const void* pImage, void* buffer, size_t bufferSize, int flag) LINKABLE;
extern uint64_t SaltySDCore_GetSymbolAddr(void* base, char* name) LINKABLE;
extern uint64_t SaltySDCore_FindSymbol(char* name) LINKABLE;
extern uint64_t SaltySDCore_FindSymbolBuiltin(char* name) LINKABLE;
extern void SaltySDCore_RegisterModule(void* base) LINKABLE;
extern void SaltySDCore_RegisterBuiltinModule(void* base) LINKABLE;
extern void SaltySDCore_DynamicLinkModule(void* base) LINKABLE;
extern void SaltySDCore_ReplaceModuleImport(void* base, const char* name, void* newfunc, bool update) LINKABLE;
extern void SaltySDCore_ReplaceImport(char* name, void* newfunc) LINKABLE;

#ifdef __cplusplus
}
#endif

#endif // SALTYSD_DYNAMIC_H
