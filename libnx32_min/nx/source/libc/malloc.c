// Minimal heap for Core32 (replaces newlib's malloc, which isn't built position independent).
// First-fit over [fake_heap_start, fake_heap_end), set by the program before the first allocation
// (Core does it in __libnx_init). Every block has an 8 byte header (size of the previous block, own size
// with bit 0 = in use), payloads are 8 byte aligned, and neighbouring free blocks are always merged.
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <malloc.h>
#include "kernel/mutex.h"

char* fake_heap_start;
char* fake_heap_end;

typedef struct Block {
	uint32_t prev; // size of the previous block, 0 for the first one
	uint32_t size; // size of this block including the header, bit 0 = in use
} Block;

#define USED      1u
#define MIN_BLOCK 16u
#define SIZE(b)   ((b)->size & ~7u)

static Block* s_first;
static Mutex s_lock;
static int s_ready;

static inline Block* nextBlock(Block* b) { return (Block*)((char*)b + SIZE(b)); }
static inline Block* prevBlock(Block* b) { return (Block*)((char*)b - b->prev); }

static int heapInit(void)
{
	if (s_ready) return 1;
	uintptr_t start = ((uintptr_t)fake_heap_start + 7) & ~(uintptr_t)7;
	uintptr_t end = (uintptr_t)fake_heap_end & ~(uintptr_t)7;
	if (!fake_heap_start || end < start + MIN_BLOCK + sizeof(Block) || end - start > 0x7FFFFFF0u) return 0;
	s_first = (Block*)start;
	s_first->prev = 0;
	s_first->size = (uint32_t)(end - start - sizeof(Block));
	Block* sentinel = nextBlock(s_first); // size 0 and in use: stops every walk and merge
	sentinel->prev = SIZE(s_first);
	sentinel->size = USED;
	s_ready = 1;
	return 1;
}

// Merges b with the following block when that one is free.
static void mergeNext(Block* b)
{
	Block* n = nextBlock(b);
	if (n->size & USED) return;
	b->size += SIZE(n);
	nextBlock(b)->prev = SIZE(b);
}

// Shrinks b to need bytes, the rest becomes a free block (merged with a free neighbour).
static void split(Block* b, uint32_t need)
{
	const uint32_t size = SIZE(b);
	if (size - need < MIN_BLOCK) return;
	Block* rest = (Block*)((char*)b + need);
	rest->prev = need;
	rest->size = size - need;
	b->size = need | (b->size & USED);
	nextBlock(rest)->prev = SIZE(rest);
	mergeNext(rest);
}

static uint32_t blockSize(size_t n)
{
	if (n > 0x7FFFFF00u) return 0;
	uint32_t need = (uint32_t)((n + 7) & ~(size_t)7) + sizeof(Block);
	return need < MIN_BLOCK ? MIN_BLOCK : need;
}

static void* allocLocked(size_t n)
{
	const uint32_t need = blockSize(n);
	if (!need || !heapInit()) return NULL;
	for (Block* b = s_first; SIZE(b); b = nextBlock(b)) {
		if ((b->size & USED) || SIZE(b) < need) continue;
		b->size |= USED;
		split(b, need);
		return b + 1;
	}
	return NULL;
}

static void freeLocked(void* p)
{
	Block* b = (Block*)p - 1;
	b->size &= ~USED;
	mergeNext(b);
	if (b->prev) {
		Block* p2 = prevBlock(b);
		if (!(p2->size & USED)) mergeNext(p2);
	}
}

void* malloc(size_t n)
{
	mutexLock(&s_lock);
	void* p = allocLocked(n);
	mutexUnlock(&s_lock);
	return p;
}

void free(void* p)
{
	if (!p) return;
	mutexLock(&s_lock);
	freeLocked(p);
	mutexUnlock(&s_lock);
}

// Doesn't call malloc(): GCC turns "malloc() then memset() to 0" into a call to calloc(), which in here
// would be calloc() calling itself forever.
void* calloc(size_t count, size_t size)
{
	if (size && count > (size_t)-1 / size) return NULL;
	mutexLock(&s_lock);
	void* p = allocLocked(count * size);
	mutexUnlock(&s_lock);
	if (p) memset(p, 0, count * size);
	return p;
}

void* realloc(void* p, size_t n)
{
	if (!p) return malloc(n);
	if (!n) {
		free(p);
		return NULL;
	}
	const uint32_t need = blockSize(n);
	if (!need) return NULL;

	mutexLock(&s_lock);
	Block* b = (Block*)p - 1;
	if (SIZE(b) < need) {
		// Grow in place into a free block that follows.
		Block* n2 = nextBlock(b);
		if (!(n2->size & USED) && SIZE(b) + SIZE(n2) >= need) {
			b->size += SIZE(n2);
			nextBlock(b)->prev = SIZE(b);
		}
	}
	if (SIZE(b) >= need) {
		split(b, need);
		mutexUnlock(&s_lock);
		return p;
	}
	void* q = allocLocked(n);
	if (q) {
		memcpy(q, p, SIZE(b) - sizeof(Block));
		freeLocked(p);
	}
	mutexUnlock(&s_lock);
	return q;
}

void* memalign(size_t align, size_t n)
{
	if (align <= 8) return malloc(n);
	if (align & (align - 1)) return NULL;
	const uint32_t need = blockSize(n);
	if (!need) return NULL;

	mutexLock(&s_lock);
	void* result = NULL;
	if (heapInit()) {
		for (Block* b = s_first; SIZE(b); b = nextBlock(b)) {
			if (b->size & USED) continue;
			uintptr_t payload = ((uintptr_t)(b + 1) + align - 1) & ~(uintptr_t)(align - 1);
			uintptr_t gap = payload - sizeof(Block) - (uintptr_t)b;
			if (gap && gap < MIN_BLOCK) {
				payload += align;
				gap += align;
			}
			if (gap + need > SIZE(b)) continue;
			if (gap) {
				// The part in front stays a free block of its own.
				Block* a = (Block*)((char*)b + gap);
				a->prev = (uint32_t)gap;
				a->size = SIZE(b) - (uint32_t)gap;
				b->size = (uint32_t)gap;
				nextBlock(a)->prev = SIZE(a);
				b = a;
			}
			b->size |= USED;
			split(b, need);
			result = b + 1;
			break;
		}
	}
	mutexUnlock(&s_lock);
	return result;
}
