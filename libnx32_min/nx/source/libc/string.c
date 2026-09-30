// Minimal string/memory functions for Core32 (replaces newlib's, which isn't built position independent).
// No builtins and no loop idiom recognition: GCC must not turn these loops into calls to themselves.
#pragma GCC optimize ("no-tree-loop-distribute-patterns")
#undef _FORTIFY_SOURCE // the checked variants are macros named like the functions defined here
#include <stddef.h>
#include <stdint.h>
#include <string.h>

typedef uint32_t __attribute__((may_alias)) word;

void* memcpy(void* restrict dst, const void* restrict src, size_t n)
{
	unsigned char* d = dst;
	const unsigned char* s = src;
	if ((((uintptr_t)d | (uintptr_t)s) & 3) == 0) {
		for (; n >= 16; n -= 16, d += 16, s += 16) {
			uint32_t a = ((const word*)s)[0], b = ((const word*)s)[1];
			uint32_t c = ((const word*)s)[2], e = ((const word*)s)[3];
			((word*)d)[0] = a; ((word*)d)[1] = b;
			((word*)d)[2] = c; ((word*)d)[3] = e;
		}
		for (; n >= 4; n -= 4, d += 4, s += 4)
			*(word*)d = *(const word*)s;
	}
	while (n--) *d++ = *s++;
	return dst;
}

void* memmove(void* dst, const void* src, size_t n)
{
	unsigned char* d = dst;
	const unsigned char* s = src;
	if (d == s || n == 0) return dst;
	if (d < s || d >= s + n) return memcpy(dst, src, n);
	d += n;
	s += n;
	while (n--) *--d = *--s;
	return dst;
}

void* memset(void* dst, int c, size_t n)
{
	unsigned char* d = dst;
	const unsigned char v = (unsigned char)c;
	if (((uintptr_t)d & 3) == 0) {
		const uint32_t w = v * 0x01010101u;
		for (; n >= 4; n -= 4, d += 4) *(word*)d = w;
	}
	while (n--) *d++ = v;
	return dst;
}

int memcmp(const void* a, const void* b, size_t n)
{
	const unsigned char* x = a;
	const unsigned char* y = b;
	for (; n; n--, x++, y++)
		if (*x != *y) return *x - *y;
	return 0;
}

size_t strlen(const char* s)
{
	const char* p = s;
	while (*p) p++;
	return (size_t)(p - s);
}

int strcmp(const char* a, const char* b)
{
	for (; *a && *a == *b; a++, b++);
	return *(const unsigned char*)a - *(const unsigned char*)b;
}

int strncmp(const char* a, const char* b, size_t n)
{
	if (!n) return 0;
	for (; --n && *a && *a == *b; a++, b++);
	return *(const unsigned char*)a - *(const unsigned char*)b;
}

char* strcpy(char* restrict dst, const char* restrict src)
{
	char* d = dst;
	while ((*d++ = *src++));
	return dst;
}

char* strncpy(char* restrict dst, const char* restrict src, size_t n)
{
	char* d = dst;
	for (; n && *src; n--) *d++ = *src++;
	for (; n; n--) *d++ = 0;
	return dst;
}

char* strchr(const char* s, int c)
{
	const char ch = (char)c;
	for (;; s++) {
		if (*s == ch) return (char*)s;
		if (!*s) return NULL;
	}
}

char* strrchr(const char* s, int c)
{
	const char ch = (char)c;
	const char* last = NULL;
	for (;; s++) {
		if (*s == ch) last = s;
		if (!*s) return (char*)last;
	}
}

// ARM EABI helpers the compiler emits for struct copies and clears (argument order differs for memset).
void __aeabi_memcpy(void* d, const void* s, size_t n) { memcpy(d, s, n); }
void __aeabi_memcpy4(void* d, const void* s, size_t n) __attribute__((alias("__aeabi_memcpy")));
void __aeabi_memcpy8(void* d, const void* s, size_t n) __attribute__((alias("__aeabi_memcpy")));
void __aeabi_memmove(void* d, const void* s, size_t n) { memmove(d, s, n); }
void __aeabi_memmove4(void* d, const void* s, size_t n) __attribute__((alias("__aeabi_memmove")));
void __aeabi_memmove8(void* d, const void* s, size_t n) __attribute__((alias("__aeabi_memmove")));
void __aeabi_memset(void* d, size_t n, int c) { memset(d, c, n); }
void __aeabi_memset4(void* d, size_t n, int c) __attribute__((alias("__aeabi_memset")));
void __aeabi_memset8(void* d, size_t n, int c) __attribute__((alias("__aeabi_memset")));
void __aeabi_memclr(void* d, size_t n) { memset(d, 0, n); }
void __aeabi_memclr4(void* d, size_t n) __attribute__((alias("__aeabi_memclr")));
void __aeabi_memclr8(void* d, size_t n) __attribute__((alias("__aeabi_memclr")));
