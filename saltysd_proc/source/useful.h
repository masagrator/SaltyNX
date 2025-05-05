#ifndef USEFUL_H
#define USEFUL_H

#include <switch.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

static inline void SaltySD_printf(const char* format, ...)
{
	FILE* logflag = fopen("sdmc:/SaltySD/flags/log.flag", "r");
	if (logflag == NULL) return;
	fclose(logflag);
	
	char buffer[256];

	va_list args;
	va_start(args, format);
	vsnprintf(buffer, 256, format, args);
	va_end(args);
	
	svcOutputDebugString(buffer, strlen(buffer));
	
	static bool previous_line_had_endline = false;
	FILE* f = fopen("sdmc:/SaltySD/saltysd.log", "ab");
	if (f)
	{
		static uint64_t tick = 0;
		if (!tick) {
			tick = svcGetSystemTick();
		}
		if (previous_line_had_endline) {
			char timer[] = "[244444444:24:24] ";
			uint64_t deltaTick = svcGetSystemTick() - tick;
			uint64_t deltaSeconds = deltaTick / 19200000;
			snprintf(timer, sizeof(timer), "[%02ld:%02ld:%02ld] ", (deltaSeconds/3600), ((deltaSeconds/60) % 60), deltaSeconds % 60);
			fwrite(timer, strlen(timer), 1, f);
		}
		else {
			if (buffer[strlen(buffer)-1] == '\n') {
				previous_line_had_endline = true;
			}
			else previous_line_had_endline = false;
		}
		fwrite(buffer, strlen(buffer), 1, f);
		fclose(f);
	}
}


#define debug_log(...) \
	{char log_buf[0x200]; snprintf(log_buf, 0x200, __VA_ARGS__); \
	svcOutputDebugString(log_buf, strlen(log_buf));}
	
#endif // USEFUL_H
