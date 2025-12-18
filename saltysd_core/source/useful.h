#ifndef USEFUL_H
#define USEFUL_H
#define NANOPRINTF_IMPLEMENTATION

#include <switch_min.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include "nanoprintf.h"

#define LINKABLE __attribute__ ((weak))

extern void SaltySDCore_printf(const char* format, ...) LINKABLE;
	
void SaltySDCore_printf(const char* format, ...)
{
	FILE* logflag = fopen("sdmc:/SaltySD/flags/log.flag", "r");
	if (logflag == NULL) return;
	fclose(logflag);
	
	char buffer[256];

	va_list args;
	va_start(args, format);
	npf_vsnprintf(buffer, 256, format, args);
	va_end(args);
	
	#if defined(SWITCH32) || defined(OUNCE32)
	FILE* f = fopen("sdmc:/SaltySD/saltysd_core32.log", "ab");	
	#else
	FILE* f = fopen("sdmc:/SaltySD/saltysd_core.log", "ab");
	#endif
	if (f)
	{
		fwrite(buffer, strlen(buffer), 1, f);
		fclose(f);
	}
}

#define debug_log(...) \
	{char log_buf[0x200]; npf_snprintf(log_buf, 0x200, __VA_ARGS__); \
	svcOutputDebugString(log_buf, strlen(log_buf));}
	
#endif // USEFUL_H
