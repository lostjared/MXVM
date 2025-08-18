#include "mx_std.h"
#include <stddef.h>
#include <time.h>
#include <string.h>
#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <sys/time.h>
#endif

void *mx_memcpy(void *dst, const void *src, size_t n) {
    if (!dst || !src || n == 0) return dst;
    return memcpy(dst, src, n);
}

int mx_memcmp(const void *a, const void *b, size_t n) {
    if (n == 0) return 0;
    return memcmp(a, b, n);
}

void *mx_memmove(void *dst, const void *src, size_t n) {
    if (!dst || !src || n == 0) return dst;
    return memmove(dst, src, n);
}

void *mx_memset(void *dst, int val, size_t n) {
    if (!dst || n == 0) return dst;
    return memset(dst, val, n);
} 