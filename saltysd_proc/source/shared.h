#include <switch.h>

#define MODULE_SALTYSD 420

extern bool should_terminate;
extern bool already_hijacking;
extern Handle sdcard;
extern Handle saltyport;
extern size_t shmem_size;
extern size_t reservedSharedMemory;
extern SharedMemory _sharedMemory;
extern u64 PIDnow;
extern u64 BIDnow;
extern u64 TIDnow;
extern u64 exception;
extern uint8_t refreshRate;
extern bool displaySync;
extern bool displaySyncOutOfFocus60;
extern bool displaySyncDocked;
extern bool displaySyncDockedOutOfFocus60;
extern bool dontForce60InDocked;
extern bool matchLowestDocked;
extern uint64_t dsiVirtAddr;
extern bool isDocked;
extern bool isLite;
extern uint64_t clkVirtAddr;
extern struct NxFpsSharedBlock* nx_fps;
extern uintptr_t game_start_address;
extern s64 lastAppPID;