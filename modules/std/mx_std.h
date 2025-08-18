#ifndef _MX_STD_H__
#define _MX_STD_H__

#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<stdint.h>
#include<time.h>

#ifdef __cplusplus
extern "C" {
#endif

void *mx_memcpy(void *dst, const void *src, size_t n);
int   mx_memcmp(const void *a, const void *b, size_t n);
void *mx_memmove(void *dst, const void *src, size_t n);
void *mx_memset(void *dst, int val, size_t n);
int64_t mx_time_now_seconds(void);
int64_t mx_time_now_millis(void);
int64_t mx_clock_ms(void);

#ifdef __cplusplus
}
#endif

#endif
