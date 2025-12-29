#include "useful.h"

void SaltySD_printf(const char* format, ...)
{
	FILE* logflag = fopen("sdmc:/SaltySD/flags/log.flag", "r");
	if (logflag == NULL) return;
	fclose(logflag);
	
	char buffer[256];

	va_list args;
	va_start(args, format);
	npf_vsnprintf(buffer, 256, format, args);
	va_end(args);
		
	static bool previous_line_had_endline = false;
	FILE* f = fopen("sdmc:/SaltySD/saltysd.log", "ab");
	if (f)
	{
		static uint64_t tick = 0;
		if (!tick) {
			tick = svcGetSystemTick();
		}
		else if (previous_line_had_endline) {
			char timer[] = "[244444444:24:24] ";
			uint64_t deltaTick = svcGetSystemTick() - tick;
			uint64_t deltaSeconds = deltaTick / systemtickfrequency;
			npf_snprintf(timer, sizeof(timer), "[%02ld:%02ld:%02ld] ", (deltaSeconds/3600) % 1000000000, ((deltaSeconds/60) % 60), deltaSeconds % 60);
			fwrite(timer, strlen(timer), 1, f);
		}
		if (buffer[strlen(buffer)-1] == '\n') {
			previous_line_had_endline = true;
		}
		else previous_line_had_endline = false;
		fwrite(buffer, strlen(buffer), 1, f);
		fclose(f);
	}
}