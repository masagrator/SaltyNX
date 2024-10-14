/*
 * Copyright (c) Atmosphère-NX
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "dmntcht.h"

static Service g_dmntchtSrv;
static u64 g_refCnt;

Result dmntchtInitialize(void) {
    Result rc = 0;
    
    atomicIncrement64(&g_refCnt);

    if (serviceIsActive(&g_dmntchtSrv))
        return 0;

    rc = smGetService(&g_dmntchtSrv, "dmnt:cht");

    if (R_FAILED(rc)) spsmExit();

    return rc;
}

void dmntchtExit(void) {
    if (atomicDecrement64(&g_refCnt) == 0) {
        serviceClose(&g_dmntchtSrv);
    }
}

Result dmntchtHasCheatProcess(bool* out) {
    IpcCommand c;
    ipcInitialize(&c);

    struct {
        u64 magic;
        u64 cmd_id;
    } *raw;

    raw = serviceIpcPrepareHeader(&g_dmntchtSrv, &c, sizeof(*raw));

    raw->magic = SFCI_MAGIC;
    raw->cmd_id = 65000;

    Result rc = serviceIpcDispatch(&g_dmntchtSrv);

    if (R_SUCCEEDED(rc)) {
        IpcParsedCommand r;

        struct {
            u64 magic;
            u64 result;
            bool out;
        } *resp;

        serviceIpcParse(&g_dmntchtSrv, &r, sizeof(*resp));
        resp = r.Raw;

        rc = resp->result;
        *out = resp->out & 1;
    }

    return rc;
}

Result dmntchtForceOpenCheatProcess(void) {
    IpcCommand c;
    ipcInitialize(&c);

    struct {
        u64 magic;
        u64 cmd_id;
    } *raw;

    raw = serviceIpcPrepareHeader(&g_dmntchtSrv, &c, sizeof(*raw));

    raw->magic = SFCI_MAGIC;
    raw->cmd_id = 65003;

    Result rc = serviceIpcDispatch(&g_dmntchtSrv);

    if (R_SUCCEEDED(rc)) {
        IpcParsedCommand r;

        struct {
            u64 magic;
            u64 result;
        } *resp;

        serviceIpcParse(&g_dmntchtSrv, &r, sizeof(*resp));
        resp = r.Raw;

        rc = resp->result;
    }

    return rc;
}