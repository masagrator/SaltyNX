#pragma once
// Bootstrap32 has no data segment: the proc writes it over rtld's code only, so one binary works whatever
// the distance between rtld's code and data is. Its few variables live in a block on the stack that crt0
// sets up and r9 points to for the whole run (every 32-bit file is built with -ffixed-r9). The names below
// stay the same as the 64-bit bootstrap's globals.
#if defined(__arm__)
#include <stddef.h>
#include <stdint.h>

typedef struct BootState {
	void*     stack_top;         // 0x00: sp at entry, restored by __nx_exit (crt0.s)
	int       argc;              // 0x04: crt0.s
	char**    argv;              // 0x08: crt0.s
	void*     orig_ctx;
	Handle    orig_main_thread;
	void*     exit_func;
	Handle    saltysd;
	uintptr_t heap_addr;
	size_t    heap_size;
} BootState;

// crt0.s reserves and zeroes BOOTSTATE_SIZE bytes and reads the first three fields by offset.
#define BOOTSTATE_SIZE 64
_Static_assert(sizeof(BootState) <= BOOTSTATE_SIZE, "BootState grew: raise BOOTSTATE_SIZE here and in crt0.s");
_Static_assert(offsetof(BootState, stack_top) == 0 && offsetof(BootState, argc) == 4 && offsetof(BootState, argv) == 8,
	"crt0.s reads these by offset");

register BootState* __boot __asm__("r9");

#define __stack_top         (__boot->stack_top)
#define __system_argc       (__boot->argc)
#define __system_argv       (__boot->argv)
#define orig_ctx            (__boot->orig_ctx)
#define orig_main_thread    (__boot->orig_main_thread)
#define __saltysd_exit_func (__boot->exit_func)
#define saltysd             (__boot->saltysd)
#define g_heapAddr          (__boot->heap_addr)
#define g_heapSize          (__boot->heap_size)
#endif
