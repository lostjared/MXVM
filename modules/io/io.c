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

int64_t seed_random() {
	srand(time(0));
	return 0;
}

int64_t rand_number(int64_t size) {
	return rand()%size;
}

void probe0(void) { puts("probe0"); }

void probeI8(
  int64_t a,int64_t b,int64_t c,int64_t d,
  int64_t e,int64_t f,int64_t g,int64_t h) {
  printf("I8: %ld %ld %ld %ld %ld %ld %ld %ld\n",
         a,b,c,d,e,f,g,h);
}

void probeF5(double a,double b,double c,double d,double e){
  printf("F5: %.0f %.0f %.0f %.0f %.0f\n", a,b,c,d,e);
}

void probeMix(
  int64_t a,double b,int64_t c,double d,int64_t e) {
  printf("Mix: %ld %.0f %ld %.0f %ld\n", a,b,c,d,e);
}