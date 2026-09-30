// Minimal C runtime pieces for Core32 (replace newlib's, which aren't built position independent).
#include <errno.h>
#include <stdlib.h>

static int s_errno;

int* __errno(void)
{
	return &s_errno;
}

typedef void (*InitFn)(void);
extern InitFn __preinit_array_start[] __attribute__((weak, visibility("hidden")));
extern InitFn __preinit_array_end[] __attribute__((weak, visibility("hidden")));
extern InitFn __init_array_start[] __attribute__((weak, visibility("hidden")));
extern InitFn __init_array_end[] __attribute__((weak, visibility("hidden")));
extern InitFn __fini_array_start[] __attribute__((weak, visibility("hidden")));
extern InitFn __fini_array_end[] __attribute__((weak, visibility("hidden")));

void __libc_init_array(void)
{
	for (InitFn* f = __preinit_array_start; f < __preinit_array_end; f++) (*f)();
	for (InitFn* f = __init_array_start; f < __init_array_end; f++) (*f)();
}

void __libc_fini_array(void)
{
	for (InitFn* f = __fini_array_end; f > __fini_array_start; ) (*--f)();
}

void __libnx_exit(int rc);

void exit(int rc)
{
	__libnx_exit(rc);
	__builtin_unreachable();
}
