#ifndef USEFUL_H
#define USEFUL_H

#if defined(SWITCH32)
#include <switch_min.h>
#elif defined(SWITCH)
#include <switch.h>
#else
#error "Unsupported base architecture!"
#endif
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

#define LINKABLE __attribute__ ((weak))

#ifdef __cplusplus
extern "C" {
#endif

extern void SaltySDCore_printf(const char* format, ...) LINKABLE;

#ifdef __cplusplus
}
#endif

#define debug_log(...) \
	{char log_buf[0x200]; npf_snprintf(log_buf, 0x200, __VA_ARGS__); \
	svcOutputDebugString(log_buf, strlen(log_buf));}
	
#endif // USEFUL_H