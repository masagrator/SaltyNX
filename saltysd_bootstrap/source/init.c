#ifdef SWITCH
#include <switch.h>
#define NORETURN NX_NORETURN
#elif SWITCH32
#include <switch_min.h>
#else
#error "Unsupported base architecture!"
#endif
#include "bootstate.h"

void NORETURN __nx_exit(void* ctx, Handle thread, LoaderReturnFn retaddr);

#if !defined(__arm__) // 32-bit: these are BootState fields (bootstate.h)
int __system_argc;
char** __system_argv;
void* __stack_top;

Handle orig_main_thread;
void* orig_ctx;

// Defined in main.c.
extern void* __saltysd_exit_func __attribute__((visibility("hidden")));
#endif

// Static: its address is taken PC-relative, while svcExitProcess' own address would need a GOT entry.
static void NORETURN exitProcess(void)
{
	svcExitProcess();
}

void __attribute__((weak)) __rel_init(void* ctx, Handle main_thread, void* saved_lr)
{
	__system_argc = 0;
	__system_argv = NULL;
	
	orig_ctx = ctx;
	orig_main_thread = main_thread;
	__saltysd_exit_func = (void*)exitProcess;
}

void __attribute__((weak)) NORETURN __rel_exit(int rc)
{
	__nx_exit(orig_ctx, orig_main_thread, __saltysd_exit_func);
}
