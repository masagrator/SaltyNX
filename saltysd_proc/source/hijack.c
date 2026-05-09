#include "shared.h"
#include "useful.h"
#include <dirent.h>
#include <stdlib.h>
#include "loadelf.h"

DebugEventInfo event;
bool check = false;
uintptr_t game_start_address = 0;

static bool isModInstalled() {
    char romfspath[0x40] = "";
    bool flag = false;

    npf_snprintf(romfspath, 0x40, "sdmc:/atmosphere/contents/%016lx/romfs", TIDnow);

    DIR* dir = opendir(romfspath);
    if (dir) {
        if (readdir(dir))
            flag = true;
        closedir(dir);
    }

    return flag;
}

static void renameCheatsFolder() {
    char cheatspath[0x3C] = "";
    char cheatspathtemp[0x40] = "";

    npf_snprintf(cheatspath, 0x3C, "sdmc:/atmosphere/contents/%016lx/cheats", TIDnow);
    npf_snprintf(cheatspathtemp, 0x40, "%stemp", cheatspath);
    if (!check) {
        rename(cheatspath, cheatspathtemp);
        check = true;
    }
    else {
        rename(cheatspathtemp, cheatspath);
        check = false;
    }
    return;
}

static bool hijack_bootstrap(Handle* debug, u64 pid, u64 tid, bool isA64)
{
    ThreadContext context;
    Result ret;

    reservedSharedMemory = 0;
    
    ret = svcGetDebugThreadContext(&context, *debug, tid, RegisterGroup_All);
    if (ret)
    {
        SaltySD_printf("SaltySD: svcGetDebugThreadContext returned %x, aborting...\n", ret);
        
        svcCloseHandle(*debug);
        return false;
    }
    
    // Load in the ELF
    //svcReadDebugProcessMemory(backup, debug, context.pc.x, 0x1000);
    game_start_address = context.pc.x;
    uint64_t new_start;
    if (isA64) {
        FILE* file = 0;
        file = fopen("sdmc:/SaltySD/saltysd_bootstrap.elf", "rb");
        if (!file) {
            SaltySD_printf("SaltySD: SaltySD/saltysd_bootstrap.elf not found, aborting...\n");
            svcCloseHandle(*debug);
            return false;
        }
        fseek(file, 0, 2);
        size_t saltysd_bootstrap_elf_size = ftell(file);
        fseek(file, 0, 0);
        u8* elf = malloc(saltysd_bootstrap_elf_size);
        fread(elf, saltysd_bootstrap_elf_size, 1, file);
        fclose(file);
        load_elf_debug(*debug, &new_start, elf, saltysd_bootstrap_elf_size);
        free(elf);
    }
    else load_elf32_debug(*debug, &new_start);

    // Set new PC
    context.pc.x = new_start;
    ret = svcSetDebugThreadContext(*debug, tid, &context, RegisterGroup_All);
    if (ret)
    {
        SaltySD_printf("SaltySD: svcSetDebugThreadContext returned %x!\n", ret);
    }
     
    svcCloseHandle(*debug);
    if (ret) return false;
    else return true;
}

void hijack_pid(u64 pid)
{
    Result ret = -1;
    s32 threads = 0;
    Handle debug;
        
    if (file_or_directory_exists("sdmc:/SaltySD/flags/disable.flag") == true) {
        SaltySD_printf("SaltySD: Detected disable.flag, aborting bootstrap...\n");
        return;
    }
    
    if (lastAppPID == -1) {
        already_hijacking = false;
    }

    if (already_hijacking)
    {
        SaltySD_printf("SaltySD: PID %d spawned before last hijack finished bootstrapping! Ignoring...\n", pid);
        return;
    }
    
    already_hijacking = true;
    Result rc = svcDebugActiveProcess(&debug, pid);
    if (R_FAILED(rc)) {
        SaltySD_printf("SaltySD: PID %d is not allowing debugging, error 0x%x, aborting...\n", pid, rc);
        goto abort_bootstrap;
    }

    bool isA64 = true;

    while (1)
    {
        ret = svcGetDebugEvent(&event, debug);

        switch(ret) {
            case 0:
                break;
            case 0xE401:
                SaltySD_printf("SaltySD: PID %d is not allowing debugging, aborting...\n", pid);
                goto abort_bootstrap;
            case 0x8C01:
                SaltySD_printf("SaltySD: PID %d svcGetDebugevent: end of events...\n", pid);
                break;
            default:
                SaltySD_printf("SaltySD: PID %d svcGetDebugevent returned %x, breaking...\n", pid, ret);
                break;
        }
        if (ret)
            break;

        if (!check) {
            TIDnow = event.info.create_process.program_id;
            exception = 0;
            renameCheatsFolder();
        }

        if (event.type == DebugEventType_CreateProcess)
        {

            if (event.info.create_process.program_id <= 0x010000000000FFFF)
            {
                SaltySD_printf("SaltySD: %s TID %016lx is a system application, aborting bootstrap...\n", event.info.create_process.name, event.info.create_process.program_id);
                goto abort_bootstrap;
            }
            if (event.info.create_process.program_id > 0x01FFFFFFFFFFFFFF || (event.info.create_process.program_id & 0x1F00) != 0)
            {
                SaltySD_printf("SaltySD: %s TID %016lx is a homebrew application, aborting bootstrap...\n", event.info.create_process.name, event.info.create_process.program_id);
                goto abort_bootstrap;
            }
            uintptr_t shmem = (uintptr_t)shmemGetAddr(&_sharedMemory);
            if (shmem) {
                memset((void*)(shmem+4), 0, shmem_size-4);
            }
            char* hbloader = "hbloader";
            if (strcasecmp(event.info.create_process.name, hbloader) == 0)
            {
                SaltySD_printf("SaltySD: Detected title replacement mode, aborting bootstrap...\n");
                goto abort_bootstrap;
            }
            
            FILE* except = fopen("sdmc:/SaltySD/exceptions.txt", "r");
            if (except) {
                char exceptions[20];
                char titleidnumX[20];

                npf_snprintf(titleidnumX, sizeof titleidnumX, "X%016lx", event.info.create_process.program_id);
                while (fgets(exceptions, sizeof(exceptions), except)) {
                    titleidnumX[0] = 'X';
                    if (!strncasecmp(exceptions, titleidnumX, 17)) {
                        SaltySD_printf("SaltySD: %s TID %016lx is forced in exceptions.txt, aborting bootstrap...\n", event.info.create_process.name, event.info.create_process.program_id);
                        fclose(except);
                        goto abort_bootstrap;
                    }
                    else {
                        titleidnumX[0] = 'R';
                        if (!strncasecmp(exceptions, titleidnumX, 17)) {
                            if (isModInstalled()) {
                                SaltySD_printf("SaltySD: %s TID %016lx is in exceptions.txt as romfs excluded, aborting bootstrap...\n", event.info.create_process.name, event.info.create_process.program_id);
                                fclose(except);
                                goto abort_bootstrap;
                            }
                            else SaltySD_printf("SaltySD: %s TID %016lx is in exceptions.txt as romfs excluded, but no romfs mod was detected...\n", event.info.create_process.name, event.info.create_process.program_id);
                        }
                        else if (!strncasecmp(exceptions, &titleidnumX[1], 16)) {
                            SaltySD_printf("SaltySD: %s TID %016lx is in exceptions.txt, aborting loading plugins...\n", event.info.create_process.name, event.info.create_process.program_id);
                            exception = 0x1;
                        }
                    }
                }
                fclose(except);
            }
            CreateProcessFlags ProcessFlags;
            memcpy(&ProcessFlags, &event.info.create_process.flags, 4);
            SaltySD_printf("SaltySD: found valid CreateProcess event:\n");
            SaltySD_printf("		 tid %016lx pid %lu\n", event.info.create_process.program_id, event.info.create_process.process_id);
            SaltySD_printf("		 name %s\n", event.info.create_process.name);
            SaltySD_printf("		 isA64 %01x addrSpace %01x enableDebug %01x\n", ProcessFlags.is_64bit, ProcessFlags.address_space, ProcessFlags.enable_debug);
            SaltySD_printf("		 enableAslr %01x poolPartition %01x\n", ProcessFlags.enable_aslr, ProcessFlags.pool_partition);
            SaltySD_printf("		 exception 0x%p\n", event.info.create_process.user_exception_context_address);
            isA64 = ProcessFlags.is_64bit;
        }
        else
        {
            SaltySD_printf("SaltySD: debug event %x, passing...\n", event.type);
            continue;
        }
    }

    u64 threadid = 0;

    uint64_t tick_start = svcGetSystemTick();
    do {
        if (svcGetSystemTick() - tick_start > systemtickfrequency * 30) {
            SaltySD_printf("SaltySD: Waiting for main thread timeout! Aborting...\n");
            goto abort_bootstrap;
        }
        ret = svcGetThreadList(&threads, &threadid, 1, debug);
        svcSleepThread(10000);
    } while (!threads);

    uint64_t passed_time_in_ticks = svcGetSystemTick() - tick_start;
    
    renameCheatsFolder();

    if (passed_time_in_ticks > systemtickfrequency * 10) {
        SaltySD_printf("SaltySD: Waiting for main thread: %d ms, longer than normal!\n", passed_time_in_ticks / (systemtickfrequency / 1000));
    }
    
    if (hijack_bootstrap(&debug, pid, threadid, isA64)) {
        lastAppPID = pid;
        
        LoaderModuleInfo module_infos[2] = {0};
        s32 module_infos_count = 0;
        ret = ldrDmntGetProcessModuleInfo(pid, module_infos, 2, &module_infos_count);
        if (R_SUCCEEDED(ret)) {
            BIDnow = __builtin_bswap64(*(uint64_t*)&module_infos[1].build_id[0]);
            SaltySD_printf("SaltySD: BID: %016lX\n", BIDnow);
            ret = 0;
        }
        else SaltySD_printf("SaltySD: cmd 8 ldrDmntGetProcessModuleInfo failed! RC: 0x%X\n", ret);
    }
    else {
        already_hijacking = false;
    }

    return;

abort_bootstrap:
    if (check) renameCheatsFolder();
                
    already_hijacking = false;
    svcCloseHandle(debug);
}