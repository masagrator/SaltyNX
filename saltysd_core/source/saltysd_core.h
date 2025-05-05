#ifndef SALTYSD_CORE_H
#define SALTYSD_CORE_H

#include <switch_min.h>
#include <dirent.h>

#include "useful.h"

#ifdef __cplusplus
extern "C" {
#endif

extern u64 SaltySDCore_getCodeStart() LINKABLE;
extern u64 SaltySDCore_getCodeSize() LINKABLE;
extern u64 SaltySDCore_findCode(u8* code, size_t size) LINKABLE;
extern u64 SaltySDCore_findCodeEx(u8* code, size_t size) LINKABLE;
extern FILE* SaltySDCore_fopen(const char* filename, const char* mode) LINKABLE;
extern size_t SaltySDCore_fread(void* ptr, size_t size, size_t count, FILE* stream) LINKABLE;
extern int SaltySDCore_fclose(FILE* stream) LINKABLE;
extern int SaltySDCore_fseek(FILE* stream, int64_t offset, int origin) LINKABLE;
extern int64_t SaltySDCore_ftell(FILE* stream) LINKABLE;
extern int SaltySDCore_remove(const char* filename) LINKABLE;
extern size_t SaltySDCore_fwrite(const void* ptr, size_t size, size_t count, FILE* stream) LINKABLE;
extern DIR* SaltySDCore_opendir(const char* dirname) LINKABLE;
extern int SaltySDCore_mkdir(const char* dirname, mode_t mode) LINKABLE;
extern struct dirent* SaltySDCore_readdir(DIR* dirp) LINKABLE;
extern int SaltySDCore_closedir(DIR *dirp) LINKABLE;

#ifdef __cplusplus
}
#endif

#endif // SALTYSD_CORE_H
