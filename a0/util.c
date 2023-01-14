#include "util.h"

void display_clock_advance(display_clock_t *cl) {
  int carry_min = 0, carry_sec = 0;
  cl->tenth += 1;
  if (cl->tenth == 10) {
    cl->tenth = 0;
    carry_sec = 1;
  }
  if (carry_sec) {
    cl->sec += 1;
    if (cl->sec == 60) {
      cl->sec = 0;
      carry_min = 1;
    }
  }
  if (carry_min) {
    cl->min += 1;
    // overflow => reset to 0
    if (cl->min == 1000) {
      cl->min = 0;
    }
  }
}

int display_clock_sprint(display_clock_t *cl, char *buf) {
  // mmm:ss:m0
  buf[9] = '\0';
  buf[8] = '0';
  buf[7] = '0' + cl->tenth;
  buf[6] = ':';
  buf[5] = '0' + (cl->sec % 10);
  buf[4] = '0' + (cl->sec / 10);
  buf[3] = ':';
  buf[2] = '0' + (cl->min % 10);
  buf[1] = '0' + (cl->min / 10 % 10);
  buf[0] = '0' + (cl->min / 100);
  return 9;
}

static int itoa(unsigned value, char *ptr)
{
    int count = 0, temp;
    if (ptr==0)
        return 0;
    if (value==0) {   
        *ptr = '0';
        return 1;
    }
    for (temp = value; temp > 0; temp /= 10, ptr++);
    *ptr='\0';
    for (temp = value; temp > 0; temp /= 10) {
        *--ptr = temp % 10 + '0';
        ++count;
    }
    return count;
}

// define our own memset to avoid SIMD instructions emitted from the compiler
void *memset(void *s, int c, size_t n) {
  for (char* it = (char*)s; n > 0; --n) *it++ = c;
  return s;
}

// define our own memcpy to avoid SIMD instructions emitted from the compiler
void* memcpy(void* restrict dest, const void* restrict src, size_t n) {
    char* sit = (char*)src;
    char* cdest = (char*)dest;
    for (size_t i = 0; i < n; ++i) *(cdest++) = *(sit++);
    return dest;
}

