/**
 * @file svc.h
 * @brief Extra wrappers for kernel syscalls.
 * @copyright libnx Authors
 */
#pragma once
#include <switch/types.h>

/// DebugEvent types
typedef enum {
    DebugEventType_CreateProcess = 0,
    DebugEventType_CreateThread  = 1,
    DebugEventType_ExitProcess   = 2,
    DebugEventType_ExitThread    = 3,
    DebugEventType_Exception     = 4,
} DebugEventType;

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
    CreateProcessFlagAddressSpace_32bit             = 0,
    CreateProcessFlagAddressSpace_64bitDeprecated   = 1,
    CreateProcessFlagAddressSpace_32bitWithoutAlias = 2,
    CreateProcessFlagAddressSpace_64bit             = 3,
} CreateProcessFlagAddressSpace;

/// Flags for svcCreateProcess and CreateProcess event
typedef union {
    struct {
        u32 is_64bit: 1;
        u32 address_space: 3;                      ///< \ref CreateProcessFlagAddressSpace
        u32 enable_debug: 1;                       ///< [2.0.0+]
        u32 enable_aslr: 1;
        u32 is_application: 1;
        u32 pool_partition: 4;                     ///< [4.0.0-4.1.0] 1 = UseSecureMemory, [5.0.0+] \ref PhysicalMemorySystemInfo
        u32 optimize_memory_allocation: 1;         ///< [7.0.0+] Only allowed in combination with is_application
        u32 disable_device_address_space_merge: 1; ///< [11.0.0+]
        u32 enable_alias_region_extra_size: 1;     ///< [18.0.0+]
        u32 reserved: 18;
    } flags;
    u32 raw;
} CreateProcessFlags;

/// DebugEvent structure
typedef struct {
    u32 type;                                              ///< \ref DebugEventType
    u32 flags;                                             ///< \ref DebugEventFlag
    u64 thread_id;

    union {
        /// DebugEventType_CreateProcess
        struct {
            u64 program_id;
            u64 process_id;
            char name[0xC];
            u32 flags;                                     ///< \ref CreateProcessFlags
            void* user_exception_context_address;          ///< [5.0.0+]
        } create_process; 

        /// DebugEventType_CreateThread
        struct {
            u64 thread_id;
            void* tls_address;
            void* entrypoint;                              ///< [1.0.0-10.2.0]
        } create_thread;

        /// DebugEventType_ExitProcess
        struct {
            u32 reason;                                    ///< \ref ProcessExitReason
        } exit_process;

        /// DebugEventType_ExitThread
        struct {
            u32 reason;                                    ///< \ref ThreadExitReason
        } exit_thread;

        /// DebugEventType_Exception
        struct {
            u32 type;                                      ///< \ref DebugException
            void* address;
            union {
                /// DebugException_UndefinedInstruction
                struct {
                    u32 insn;
                } undefined_instruction;

                /// DebugException_DataAbort
                struct {
                    void* address;
                } data_abort;

                /// DebugException_AlignmentFault
                struct {
                    void* address;
                } alignment_fault;

                /// DebugException_BreakPoint
                struct {
                    u32 type;                              ///< \ref BreakPointType
                    void* address;
                } break_point;

                /// DebugException_UserBreak
                struct {
                    u32 break_reason;                      ///< \ref BreakReason
                    void* address;
                    size_t size;
                } user_break;

                /// DebugException_DebuggerBreak
                struct {
                    u64 active_thread_ids[4];
                } debugger_break;

                /// DebugException_UndefinedSystemCall
                struct {
                    u32 id;
                } undefined_system_call;

                u64 raw;
            } specific;
        } exception;
    } info;
} DebugEvent;