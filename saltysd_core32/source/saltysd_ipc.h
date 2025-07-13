#ifndef SALTYSD_IPC_H
#define SALTYSD_IPC_H

#include <switch_min.h>

#include "useful.h"

#ifdef __cplusplus
extern "C" {
#endif

extern void SaltySD_Init() LINKABLE;
extern Result SaltySD_Deinit() LINKABLE;
extern Result SaltySD_Term() LINKABLE;
extern Result SaltySD_Restore() LINKABLE;
extern Result SaltySD_Memcpy(u32 to, u32 from, u32 size) LINKABLE;
extern Result SaltySD_GetSDCard(Handle *retrieve) LINKABLE;
extern Result SaltySD_printf(const char* format, ...) LINKABLE;
extern Result SaltySD_CheckIfSharedMemoryAvailable(ptrdiff_t *offset, u32 size) LINKABLE;
extern Result SaltySD_GetSharedMemoryHandle(Handle *retrieve) LINKABLE;
extern u64 SaltySD_GetBID() LINKABLE;
extern Result SaltySD_Exception() LINKABLE;
extern Result SaltySD_SetDisplaySync(bool isTrue) LINKABLE;
extern Result SaltySD_SetDisplaySyncDocked(bool isTrue) LINKABLE;

#ifdef __cplusplus
}
#endif

#endif //SALTYSD_IPC_H
