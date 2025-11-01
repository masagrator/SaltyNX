#ifndef USEFUL_H
#define USEFUL_H

#include <switch.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <sys/stat.h>
#ifdef __cplusplus
	#include <cctype>
#else
	#include <ctype.h>
#endif

struct resolutionCalls {
	uint16_t width;
	uint16_t height;
	uint16_t calls;
};

struct NxFpsSharedBlock {
	uint32_t MAGIC;
	uint8_t FPS;
	float FPSavg;
	bool pluginActive;
	uint8_t FPSlocked;
	uint8_t FPSmode;
	uint8_t ZeroSync;
	uint8_t patchApplied;
	uint8_t API;
	uint32_t FPSticks[10];
	uint8_t Buffers;
	uint8_t SetBuffers;
	uint8_t ActiveBuffers;
	uint8_t SetActiveBuffers;
	union {
		struct {
			bool handheld: 1;
			bool docked: 1;
			unsigned int reserved: 6;
		} NX_PACKED ds;
		uint8_t general;
	} displaySync;
	struct resolutionCalls renderCalls[8];
	struct resolutionCalls viewportCalls[8];
	bool forceOriginalRefreshRate;
	bool dontForce60InDocked;
	bool forceSuspend;
	uint8_t currentRefreshRate;
	float readSpeedPerSecond;
	uint8_t FPSlockedDocked;
	uint64_t frameNumber;
	int8_t expectedSetBuffers;
} NX_PACKED;

static_assert(sizeof(struct NxFpsSharedBlock) == 174);

#ifdef SWITCH
    #define systemtickfrequency 19200000
#elif OUNCE
    #define systemtickfrequency 31250000
#else
	extern uint64_t systemtickfrequency;
#endif

static inline void remove_spaces(char* str_trimmed, const char* str_untrimmed)
{
  while (str_untrimmed[0] != '\0')
  {
    if(!isspace((int)str_untrimmed[0]))
    {
      str_trimmed[0] = str_untrimmed[0];
      str_trimmed++;
    }
    str_untrimmed++;
  }
  str_trimmed[0] = '\0';
}

static inline bool file_or_directory_exists(const char *filename)
{
    struct stat buffer;
    return stat(filename, &buffer) == 0 ? true : false;
}

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
		else if (previous_line_had_endline) {
			char timer[] = "[244444444:24:24] ";
			uint64_t deltaTick = svcGetSystemTick() - tick;
			uint64_t deltaSeconds = deltaTick / systemtickfrequency;
			snprintf(timer, sizeof(timer), "[%02ld:%02ld:%02ld] ", (deltaSeconds/3600) % 1000000000, ((deltaSeconds/60) % 60), deltaSeconds % 60);
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


#define debug_log(...) \
	{char log_buf[0x200]; snprintf(log_buf, 0x200, __VA_ARGS__); \
	svcOutputDebugString(log_buf, strlen(log_buf));}
	
#endif // USEFUL_H
