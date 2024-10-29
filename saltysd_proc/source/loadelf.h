#ifndef IPC_HANDOFFS_H
#define IPC_HANDOFFS_H

#include <switch_min.h>

#ifdef __cplusplus
extern "C" {
#endif

Result load_elf_debug(Handle debug, uint64_t* start, uint8_t* elf_data, u32 elf_size);
Result load_elf32_debug(Handle debug, uint64_t* start, uint8_t* elf_data, u32 elf_size);
Result load_elf_proc(Handle debug, uint64_t pid, uint64_t heap, uint64_t* start, uint64_t* total_size, uint8_t* elf_data, u32 elf_size);
Result load_elf32_proc(Handle debug, uint64_t pid, uint32_t heap, uint32_t* start, uint32_t* total_size, uint8_t* elf_data, u32 elf_size);
Result restore_elf_debug(Handle debug);

#ifdef __cplusplus
}
#endif

#endif // IPC_HANDOFFS_H


