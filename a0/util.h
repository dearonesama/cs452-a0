#pragma once

#include <stddef.h>

typedef struct {
  unsigned min;
  unsigned char sec, tenth;
} display_clock_t;

// advances clock by tenth second
void display_clock_advance(display_clock_t *);

// formats the structure into string and returns length of string
int display_clock_sprint(display_clock_t *, char *);

void *memset(void *s, int c, size_t n);
void *memcpy(void* restrict dest, const void* restrict src, size_t n);

