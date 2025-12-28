/**
 * @file svc.h
 * @brief Extra wrappers for kernel syscalls.
 * @copyright libnx Authors
 */
#pragma once
#include <switch/types.h>

/// DebugEvent types
typedef enum {
    DebugEvent_CreateProcess = 0,
    DebugEvent_CreateThread  = 1,
    DebugEvent_ExitProcess   = 2,
    DebugEvent_ExitThread    = 3,
    DebugEvent_Exception     = 4,
} DebugEvent;

/// Process exit reasons
typedef enum {
    ProcessExitReason_ExitProcess      = 0,
    ProcessExitReason_TerminateProcess = 1,
    ProcessExitReason_Exception        = 2,
} ProcessExitReason;

/// Thread exit reasons
typedef enum {
    ThreadExitReason_ExitThread       = 0,
    ThreadExitReason_TerminateThread  = 1,
    ThreadExitReason_ExitProcess      = 2,
    ThreadExitReason_TerminateProcess = 3,
} ThreadExitReason;

/// Debug exception types
typedef enum {
    DebugException_UndefinedInstruction = 0,
    DebugException_InstructionAbort     = 1,
    DebugException_DataAbort            = 2,
    DebugException_AlignmentFault       = 3,
    DebugException_DebuggerAttached     = 4,
    DebugException_BreakPoint           = 5,
    DebugException_UserBreak            = 6,
    DebugException_DebuggerBreak        = 7,
    DebugException_UndefinedSystemCall  = 8,
    DebugException_MemorySystemError    = 9, ///< [2.0.0+]
} DebugException;

/// Break point types
typedef enum {
    BreakPointType_HardwareInstruction = 0,
    BreakPointType_HardwareData        = 1,
} BreakPointType;

/// DebugEvent flags
typedef enum {
    DebugEventFlag_Stopped = BIT(1),
} DebugEventFlag;

/// Address space types for CreateProcessFlags
typedef enum {
    CreateProcessFlagAddressSpace_32bit = 0,
    CreateProcessFlagAddressSpace_64bitDeprecated = 1,
    CreateProcessFlagAddressSpace_32bitWithoutAlias = 2,
    CreateProcessFlagAddressSpace_64bit = 3,
} CreateProcessFlagAddressSpace;

/// Flags for svcCreateProcess and CreateProcess event
typedef union {
    struct {
        u32 is_64bit: 1;
        u32 address_space: 3;                      ///< \ref CreateProcessFlagAddressSpace
        u32 enable_debug: 1;                       ///< [2.0.0+]
        u32 enable_aslr: 1;
        u32 is_application: 1;
        u32 use_secure_memory: 1;                  ///< [1.0.0-3.0.2]
        u32 pool_partition: 4;                     ///< [5.0.0+] \ref PhysicalMemorySystemInfo
        u32 optimize_memory_allocation: 1;         ///< [7.0.0+] Only allowed in combination with is_application
        u32 disable_device_address_space_merge: 1; ///< [11.0.0+]
        u32 enable_alias_region_extra_size: 1;     ///< [18.0.0+]
        u32 reserved: 17;
    } flags;
    u32 raw;
} CreateProcessFlags;

typedef struct {
    u32 type;                                              ///< \ref DebugEvent
    u32 flags;                                             ///< \ref DebugEventFlag
    u64 thread_id;

    union {
        struct {
            u64 program_id;
            u64 process_id;
            char name[0xC];
            u32 flags;                                     ///< \ref CreateProcessFlags
            void* user_exception_context_address;          ///< [5.0.0+]
        } create_process;                                  ///< DebugEvent_CreateProcess

        struct {
            u64 thread_id;
            void* tls_address;
            void* entrypoint;                              ///< [1.0.0-10.2.0]
        } create_thread;                                   ///< DebugEvent_CreateThread

        struct {
            u32 reason;                                    ///< \ref ProcessExitReason
        } exit_process;                                    ///< DebugEvent_ExitProcess

        struct {
            u32 reason;                                    ///< \ref ThreadExitReason
        } exit_thread;                                     ///< DebugEvent_ExitThread

        struct {
            u32 type;                                      ///< \ref DebugException
            void* address;
            union {
                struct {
                    u32 insn;
                } undefined_instruction;                   ///< DebugException_UndefinedInstruction

                struct {
                    void* address;
                } data_abort;                              ///< DebugException_DataAbort

                struct {
                    void* address;
                } alignment_fault;                         ///< DebugException_AlignmentFault

                struct {
                    u32 type;                              ///< \ref BreakPointType
                    void* address;
                } break_point;                             ///< \ref DebugException_BreakPoint

                struct {
                    u32 break_reason;                      ///< \ref BreakReason
                    void* address;
                    size_t size;
                } user_break;                              ///< \ref DebugException_UserBreak

                struct {
                    u64 active_thread_ids[4];
                } debugger_break;                          ///< DebugException_DebuggerBreak

                struct {
                    u32 id;
                } undefined_system_call;                   ///< DebugException_UndefinedSystemCall

                u64 raw;
            } specific;
        } exception;                                       ///< DebugEvent_Exception
    } info;
} DebugEventInfo;


#ifndef LIBNX_NO_EXTRA_ADAPT
/**
 * @brief Gets an incoming debug event from a debugging session.
 * @return Result code.
 * @note Syscall number 0x63.
 * @warning This is a privileged syscall. Use \ref envIsSyscallHinted to check if it is available.
 */
inline Result svcGetDebugEventInfo(DebugEventInfo* event_out, Handle debug)
{
    return svcGetDebugEvent((u8*)event_out, debug);
}
#endif
