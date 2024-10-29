#include <switch_min.h>
#include <sys/iosupport.h>

int __system_argc;
char** __system_argv;
void* __stack_top;
void NORETURN __nx_exit(void* ctx, Handle thread, LoaderReturnFn retaddr);

Handle orig_main_thread;
void* orig_ctx;

__attribute__((weak)) void* __saltysd_exit_func = svcExitProcess;

void __attribute__((weak)) __rel_init(void* ctx, Handle main_thread, void* saved_lr)
{
	__system_argc = 0;
	__system_argv = NULL;
	
	orig_ctx = ctx;
	orig_main_thread = main_thread;
}

void __attribute__((weak)) NORETURN __rel_exit(int rc)
{
	__nx_exit(orig_ctx, orig_main_thread, __saltysd_exit_func);
}
