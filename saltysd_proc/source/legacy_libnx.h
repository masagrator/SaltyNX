#pragma once
#include <string.h>
#include "ipc.h"

static Service_old g_smSrv;
static u64 g_refCnt;

#define MAX_OVERRIDES 32

static struct {
    u64    name;
    Handle handle;
} g_smOverrides[MAX_OVERRIDES];

static size_t g_smOverridesNum = 0;

/// Atomically increments a 64-bit value.
static inline u64 atomicIncrement64(u64* p) {
    return __atomic_fetch_add(p, 1, __ATOMIC_SEQ_CST);
}

/// Atomically decrements a 64-bit value.
static inline u64 atomicDecrement64(u64* p) {
    return __atomic_sub_fetch(p, 1, __ATOMIC_SEQ_CST);
}

bool smHasInitialized_old(void) {
    return serviceIsActive_old(&g_smSrv);
}

u64 smEncodeName_old(const char* name)
{
    u64 name_encoded = 0;
    size_t i;

    for (i=0; i<8; i++)
    {
        if (name[i] == '\0')
            break;

        name_encoded |= ((u64) name[i]) << (8*i);
    }

    return name_encoded;
}

static Result smGetServiceOriginal_old(Handle* handle_out, u64 name)
{
    IpcCommand c;
    ipcInitialize(&c);

    struct 
    {
        u64 magic;
        u64 cmd_id;
        u64 name;
    } *raw;

    raw = ipcPrepareHeader(&c, sizeof(*raw));
    raw->magic = SFCI_MAGIC;
    raw->cmd_id = 1;
    raw->name = name;

    Result rc = serviceIpcDispatch(&g_smSrv);

    if (R_SUCCEEDED(rc)) {
        IpcParsedCommand r;

        struct {
            u64 magic;
            u64 result;
        } *resp;
        serviceIpcParse(&g_smSrv, &r, sizeof(*resp));

        resp = r.Raw;
        rc = resp->result;

        if (R_SUCCEEDED(rc)) {
            *handle_out = r.Handles[0];
        }
    }

    return rc;
}

static Result smRegisterClient(void) {
    IpcCommand c;
    ipcInitialize(&c);
    ipcSendPid(&c);

    struct 
    {
        u64 magic;
        u64 cmd_id;
        u64 reserved;
    } *raw;

    raw = ipcPrepareHeader(&c, sizeof(*raw));

    raw->magic = SFCI_MAGIC;
    raw->cmd_id = 0;
    raw->reserved = 0;

    Result rc = serviceIpcDispatch(&g_smSrv);

    IpcParsedCommand r;

    struct {
        u64 magic;
        u64 result;
    } *resp;
    serviceIpcParse(&g_smSrv, &r, sizeof(*resp));

    resp = r.Raw;
    rc = resp->result;

    return rc;
}

void smExit_old(void)
{
    if (atomicDecrement64(&g_refCnt) == 0)
    {
        serviceClose_old(&g_smSrv);
    }
}

Result smInitialize_old(void)
{
    atomicIncrement64(&g_refCnt);

    if (smHasInitialized_old())
        return 0;

    Handle sm_handle;
    Result rc = svcConnectToNamedPort(&sm_handle, "sm:");
    while (R_VALUE(rc) == KERNELRESULT(NotFound)) {
        svcSleepThread(50000000ul);
        rc = svcConnectToNamedPort(&sm_handle, "sm:");
    }

    if (R_SUCCEEDED(rc)) {
        serviceCreate_old(&g_smSrv, sm_handle);
    }

    Handle tmp;
    if (R_SUCCEEDED(rc) && smGetServiceOriginal_old(&tmp, smEncodeName_old("")) == 0x415) {
        rc = smRegisterClient();
    }

    if (R_FAILED(rc))
        smExit_old();

    return rc;
}

Handle smGetServiceOverride_old(u64 name)
{
    size_t i;

    for (i=0; i<g_smOverridesNum; i++)
    {
        if (g_smOverrides[i].name == name)
            return g_smOverrides[i].handle;
    }

    return INVALID_HANDLE;
}

Result smGetService_old(Service_old* service_out, const char* name)
{
    u64 name_encoded = smEncodeName_old(name);
    Handle handle = smGetServiceOverride_old(name_encoded);
    Result rc;

    if (handle != INVALID_HANDLE)
    {
        service_out->type = ServiceType_Override;
        service_out->handle = handle;
        rc = 0;
    }
    else
    {
        rc = smGetServiceOriginal_old(&handle, name_encoded);

        if (R_SUCCEEDED(rc))
        {
            service_out->type = ServiceType_Normal;
            service_out->handle = handle;
        }
    }

    return rc;
}

Result fsp_init(Service_old fsp)
{
    Result rc;
    IpcCommand c;
    ipcInitialize(&c);
    ipcSendPid(&c);

    struct {
        u64 magic;
        u64 cmd_id;
        u64 unk;
    } *raw;

    raw = ipcPrepareHeader(&c, sizeof(*raw));

    raw->magic = SFCI_MAGIC;
    raw->cmd_id = 1;
    raw->unk = 0;

    rc = serviceIpcDispatch(&fsp);

    if (R_SUCCEEDED(rc)) {
        IpcParsedCommand r;
        ipcParse(&r);

        struct {
            u64 magic;
            u64 result;
        } *resp = r.Raw;

        rc = resp->result;
    }
    
    return rc;
}

Result fsp_getSdCard(Service_old fsp, Handle* out)
{
    Result rc;
    IpcCommand c;
    ipcInitialize(&c);

    struct {
        u64 magic;
        u64 cmd_id;
    } *raw;

    raw = ipcPrepareHeader(&c, sizeof(*raw));

    raw->magic = SFCI_MAGIC;
    raw->cmd_id = 18;

    rc = serviceIpcDispatch(&fsp);

    if (R_SUCCEEDED(rc)) {
        IpcParsedCommand r;
        ipcParse(&r);

        struct {
            u64 magic;
            u64 result;
        } *resp = r.Raw;

        *out = r.Handles[0];

        rc = resp->result;
    }
    
    return rc;
}
