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

void   set_program_args(int argc, const char **argv);
int    argc(void);
const char *argv(int idx);
void free_program_args(void);

#ifdef __cplusplus
}
#endif

#endif
