#ifndef USEFUL_H
#define USEFUL_H

#include <switch_min.h>
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