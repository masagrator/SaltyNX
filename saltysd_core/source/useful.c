#if defined(SWITCH32)
#include <switch_min.h>
#elif defined(SWITCH)
#include <switch.h>
#else
#error "Unsupported base architecture!"
#endif
#include "nanoprintf.h"
#include "saltysd_core.h"
#include <string.h>

void SaltySDCore_printf(const char* format, ...)
{
	FILE* logflag = SaltySDCore_fopen("sdmc:/SaltySD/flags/log.flag", "r");
	if (logflag == NULL) return;
	SaltySDCore_fclose(logflag);
	
	char buffer[256];

	va_list args;
	va_start(args, format);
	npf_vsnprintf(buffer, 256, format, args);
	va_end(args);
	
	#if defined(SWITCH32) || defined(OUNCE32)
	FILE* f = SaltySDCore_fopen("sdmc:/SaltySD/saltysd_core32.log", "ab");	
	#else
	FILE* f = SaltySDCore_fopen("sdmc:/SaltySD/saltysd_core.log", "ab");
	#endif
	if (f)
	{
		SaltySDCore_fwrite(buffer, strlen(buffer), 1, f);
		SaltySDCore_fclose(f);
	}
}