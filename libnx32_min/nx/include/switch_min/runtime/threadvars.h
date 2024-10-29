#pragma once
#include "switch_min/types.h"
#include "switch_min/arm/tls.h"
#include "switch_min/kernel/thread.h"

#define THREADVARS_MAGIC 0x21545624 // !TV$

// This structure is exactly 0x28 bytes
typedef struct {
    // Magic value used to check if the struct is initialized
    u32 magic;

    // Thread handle, for mutexes
    Handle handle;

    // Pointer to the current thread (if exists)
    Thread* thread_ptr;

    // Pointer to this thread's newlib state
    struct _reent* reent;

    // Pointer to this thread's thread-local segment
    void* tls_tp; // !! Offset needs to be TLS+0x1F0 for __aarch64_read_tp !!
    
    void* nintendo_context;
} ThreadVars;

static inline ThreadVars* getThreadVars(void) {
    return (ThreadVars*)((u8*)armGetTls() + 0x200 - sizeof(ThreadVars));
}
