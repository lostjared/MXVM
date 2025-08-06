    // any helper functions that can be linked into the final app in compile mode
#include<stdio.h>
#include<stdint.h>
#include<stdlib.h>
#include<time.h>

int64_t fsize(FILE *fptr) {
	fseek(fptr, 0, SEEK_END);
	int64_t total = ftell(fptr);
	fseek(fptr, 0, SEEK_SET);
	return total;
}

int64_t rand_number(int64_t size) {
	srand(time(0));
	return rand()%size;
}