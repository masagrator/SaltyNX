#ifndef USEFUL_H
#define USEFUL_H

#include <switch.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <sys/stat.h>
#include "nanoprintf.h"
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
#ifdef __cplusplus
extern "C" {
#endif
void SaltySD_printf(const char* format, ...);
#ifdef __cplusplus
}
#endif

#define debug_log(...) \
	{char log_buf[0x200]; npf_snprintf(log_buf, 0x200, __VA_ARGS__); \
	svcOutputDebugString(log_buf, strlen(log_buf));}
	
#endif // USEFUL_H
