#include "types.h"
#include "result.h"
#include "services/fatal.h"
#include "kernel/mutex.h"
#include "kernel/svc.h"
#include "kernel/virtmem.h"

typedef struct {
    u64  start;
    u64  end;
} VirtualRegion;

enum {
    REGION_STACK=0,
    REGION_HEAP=1,
    REGION_LEGACY_ALIAS=2,
    REGION_MAX
};

static VirtualRegion g_AddressSpace;
static VirtualRegion g_Region[REGION_MAX];
static u64 g_CurrentAddr;
static u64 g_CurrentMapAddr;
static Mutex g_VirtMemMutex;

static Result _GetRegionFromInfo(VirtualRegion* r, u32 id0_addr, u32 id0_sz) {
    u64 base;
    Result rc = svcGetInfo(&base, id0_addr, CUR_PROCESS_HANDLE, 0);

    if (R_SUCCEEDED(rc)) {
        u64 size;
        rc = svcGetInfo(&size, id0_sz, CUR_PROCESS_HANDLE, 0);

        if (R_SUCCEEDED(rc)) {
            r->start = base;
            r->end   = base + size;
        }
    }

    return rc;
}

static inline bool _InRegion(VirtualRegion* r, u64 addr) {
    return (addr >= r->start) && (addr < r->end);
}

void virtmemSetup(void) {
    if (R_FAILED(_GetRegionFromInfo(&g_AddressSpace, InfoType_AslrRegionAddress, InfoType_AslrRegionSize))) {

		g_AddressSpace.start = 0x200000ul;
		g_AddressSpace.end   = 0x100000000ul;

		g_Region[REGION_STACK].start = 0x200000ul;
		g_Region[REGION_STACK].end = 0x40000000ul;

    } else {
        if (R_FAILED(_GetRegionFromInfo(&g_Region[REGION_STACK], InfoType_StackRegionAddress, InfoType_StackRegionSize))) {
            fatalSimple(MAKERESULT(Module_Libnx, LibnxError_BadGetInfo_Stack));
        }
    }

    if (R_FAILED(_GetRegionFromInfo(&g_Region[REGION_HEAP], InfoType_HeapRegionAddress, InfoType_HeapRegionSize))) {
        fatalSimple(MAKERESULT(Module_Libnx, LibnxError_BadGetInfo_Heap));
    }

    _GetRegionFromInfo(&g_Region[REGION_LEGACY_ALIAS], InfoType_AliasRegionAddress, InfoType_AliasRegionSize);
}

void* virtmemReserve(size_t size) {
    Result rc;
    MemoryInfo meminfo;
    u32 pageinfo;
    size_t i;

    size = (size + 0xFFF) &~ 0xFFF;

    mutexLock(&g_VirtMemMutex);
    u64 addr = g_CurrentAddr;

    while (1)
    {
        // Add a guard page.
        addr += 0x1000;

        // If we go outside address space, let's go back to start.
        if (!_InRegion(&g_AddressSpace, addr)) {
            addr = g_AddressSpace.start;
        }
        // Query information about address.
        rc = svcQueryMemory(&meminfo, &pageinfo, addr);

        if (R_FAILED(rc)) {
            fatalSimple(MAKERESULT(Module_Libnx, LibnxError_BadQueryMemory));
        }

        if (meminfo.type != 0) {
            // Address is already taken, let's move past it.
            addr = meminfo.addr + meminfo.size;
            continue;
        }

        if (size > meminfo.size) {
            // We can't fit in this region, let's move past it.
            addr = meminfo.addr + meminfo.size;
            continue;
        }

        // Check if we end up in a reserved region.
        for(i=0; i<REGION_MAX; i++)
        {
            u32 end = addr + size - 1;

            if (_InRegion(&g_Region[i], addr) || _InRegion(&g_Region[i], end)) {
                break;
            }
        }

        // Did we?
        if (i != REGION_MAX) {
            addr = g_Region[i].end;
            continue;
        }

        // Not in a reserved region, we're good to go!
        break;
    }

    g_CurrentAddr = addr + size;

    mutexUnlock(&g_VirtMemMutex);
    return (void*)(u32)addr;
}

void  virtmemFree(void* addr, size_t size) {
    IGNORE_ARG(addr);
    IGNORE_ARG(size);
}

void* virtmemReserveStack(size_t size)
{
    Result rc;
    MemoryInfo meminfo;
    u32 pageinfo;

    size = (size + 0xFFF) &~ 0xFFF;

    mutexLock(&g_VirtMemMutex);
    u32 addr = g_CurrentMapAddr;

    while (1)
    {
        // Add a guard page.
        addr += 0x1000;

        // Make sure we stay inside the reserved map region.
        if (!_InRegion(&g_Region[REGION_STACK], addr)) {
            addr = g_Region[REGION_STACK].start;
        }

        // Query information about address.
        rc = svcQueryMemory(&meminfo, &pageinfo, addr);

        if (R_FAILED(rc)) {
            fatalSimple(MAKERESULT(Module_Libnx, LibnxError_BadQueryMemory));
        }

        if (meminfo.type != 0) {
            // Address is already taken, let's move past it.
            addr = meminfo.addr + meminfo.size;
            continue;
        }

        if (size > meminfo.size) {
            // We can't fit in this region, let's move past it.
            addr = meminfo.addr + meminfo.size;
            continue;
        }

        break;
    }

    g_CurrentMapAddr = addr + size;

    mutexUnlock(&g_VirtMemMutex);
    return (void*) addr;
}

void virtmemFreeStack(void* addr, size_t size) {
    IGNORE_ARG(addr);
    IGNORE_ARG(size);
}
