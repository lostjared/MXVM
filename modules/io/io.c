#include<stdio.h>

size_t fsize(FILE *fptr) {
	fseek(fptr, 0, SEEK_END);
	size_t total = ftell(fptr);
	fseek(fptr, 0, SEEK_SET);
	return total;
}
