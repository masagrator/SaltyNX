/**
 * @file mutex.h
 * @brief Mutex synchronization primitive.
 * @author plutoo
 * @copyright libnx Authors
 */
#pragma once
#include <sys/lock.h>
#include "../types.h"

/// Mutex datatype, defined in newlib.
typedef _LOCK_T Mutex;

/**
 * @brief Initializes a mutex.
 * @param m Mutex object.
 * @note A mutex can also be statically initialized by assigning 0 to it.
 */
static inline void mutexInit(Mutex* m)
{
    *m = 0;
}

/**
 * @brief Locks a mutex.
 * @param m Mutex object.
 */
void mutexLock(Mutex* m);

/**
 * @brief Attempts to lock a mutex without waiting.
 * @param m Mutex object.
 * @return 1 if the mutex has been acquired successfully, and 0 on contention.
 */
bool mutexTryLock(Mutex* m);

/**
 * @brief Unlocks a mutex.
 * @param m Mutex object.
 */
void mutexUnlock(Mutex* m);
