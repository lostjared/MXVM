/**
 * @file io.c
 * @brief I/O module C implementation — file operations, random numbers, and time functions
 * @author Jared Bruni
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int64_t fsize(FILE *fptr) {
    fseek(fptr, 0, SEEK_END);
    int64_t total = ftell(fptr);
    fseek(fptr, 0, SEEK_SET);
    return total;
}

int64_t seed_random() {
    struct timespec ts;
    timespec_get(&ts, TIME_UTC);
    unsigned int seed = (unsigned int)(ts.tv_sec ^ ts.tv_nsec ^ (uintptr_t)&ts);
    srand(seed);
    return 0;
}

int64_t rand_number(int64_t size) {
    return rand() % size;
}
