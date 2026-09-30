/**
 * @file tls.h
 * @brief AArch32 thread local storage.
 */
#pragma once
#include "../types.h"

/**
 * @brief Gets the thread local storage buffer.
 * @return The thread local storage buffer.
 */
static inline void* armGetTls(void) {
    void* ret;
    __asm__ ("mrc   p15, 0, %[data], c13, c0, 3" : [data] "=r" (ret));
    return ret;
}
