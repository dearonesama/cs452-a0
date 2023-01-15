#include "util.h"

#define ASSERT(x)  // TODO

void display_clock_init(display_clock_t *cl) {
  ASSERT(cl);
  cl->min = cl->sec = cl->tenth = 0;
}

void display_clock_advance(display_clock_t *cl) {
  ASSERT(cl);
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
  ASSERT(cl);
  ASSERT(buf);
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
  ASSERT(ptr);
  int count = 0, temp;
  if (value == 0) {
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

void queue_init(queue_t *q, char *data, size_t capacity) {
  ASSERT(q);
  ASSERT(data);
  ASSERT(capacity);
  q->data = data;
  q->capacity = capacity;
  q->begin = 0;
  q->end = 0;
}

void queue_emplace(queue_t *q, const char *src, size_t len) {
  ASSERT(q);
  ASSERT(src);
  size_t i = 0;
  if (q->begin <= q->end) {
    size_t end = q->begin == 0 ? q->capacity - 1 : q->capacity;
    while (i < len && q->end < end) {
      q->data[q->end++] = src[i++];
    }
    if (q->end == q->capacity) {
      q->end = 0;
from_left:
      while (i < len && q->end < q->begin - 1) {
        q->data[q->end++] = src[i++];
      }
      // we just silently reject new inputs if buffer cannot hold them
      ASSERT(i >= len);
    }
  } else {
    goto from_left;
  }
}

void queue_consume(queue_t *q, size_t len) {
  ASSERT(q);
  size_t end = q->begin <= q->end ? q->end : q->capacity;
  size_t right_size = end - q->begin;
  size_t popped_len = len < right_size ? len : right_size;
  q->begin += popped_len;
  if (q->begin == q->capacity) {
    size_t remain_len = len - popped_len;
    q->begin = remain_len < q->end ? remain_len : q->end;
  }
}

char *queue_longest_data(queue_t *q, size_t *len) {
  ASSERT(q);
  if (len) {
    size_t end = q->begin <= q->end ? q->end : q->capacity;
    *len = end - q->begin;
  }
  return q->data + q->begin;
}

static int isnum(char c) {
  return c >= '0' && c <= '9';
}

static int match_two_digits(char *buf, size_t *i, size_t len, unsigned char *out) {
  ASSERT(buf);
  ASSERT(out);
  if (!len) {
    return 0;
  }
  if (isnum(buf[*i]) && (len-*i == 1 || !isnum(buf[*i+1]))) {
    *out = buf[*i] - '0';
    ++*i;
    return 1;
  } else if (isnum(buf[*i]) && isnum(buf[*i+1])) {
    *out = (buf[*i] - '0') * 10 + (buf[*i+1] - '0');
    *i += 2;
    return 1;
  }
  return 0;
}

static void eat_whitespace(char *buf, size_t *i, size_t len) {
  while (*i < len && (buf[*i] == ' ' || buf[*i] == '\t')) {
    ++*i;
  }
}

static int match_start(char *buf, size_t *i, size_t buflen, char *target, size_t target_len) {
  if (buflen - *i < target_len) {
    return 0;
  }
  for (size_t j = *i, k = 0; k < target_len; ++j, ++k) {
    if (buf[j] != target[k]) {
      return 0;
    }
  }
  *i += target_len;
  return 1;
}

train_command_t try_parse_train_command(char *buf, size_t len) {
  ASSERT(buf);
  train_command_t c;
  c.kind = TRAIN_COMMAND_INVALID;
  size_t i = 0;
  unsigned char num;
  eat_whitespace(buf, &i, len);
  if (match_start(buf, &i, len, "tr", 2)) {
    eat_whitespace(buf, &i, len);
    if (!match_two_digits(buf, &i, len, &num)) {
      return c;
    }
    c.cmd.tr.train_num = num;
    eat_whitespace(buf, &i, len);
    if (!match_two_digits(buf, &i, len, &num)) {
      return c;
    }
    c.cmd.tr.speed = num;
    c.kind = TRAIN_COMMAND_TR;

  } else if (match_start(buf, &i, len, "rv", 2)) {
    eat_whitespace(buf, &i, len);
    if (!match_two_digits(buf, &i, len, &num)) {
      return c;
    }
    c.cmd.rv.train_num = num;
    c.kind = TRAIN_COMMAND_RV;

  } else if (match_start(buf, &i, len, "sw", 2)) {
    eat_whitespace(buf, &i, len);
    if (!match_two_digits(buf, &i, len, &num)) {
      return c;
    }
    c.cmd.sw.switch_num = num;
    eat_whitespace(buf, &i, len);
    if (match_start(buf, &i, len, "S", 1)) {
      c.cmd.sw.straight = 1;
      c.kind = TRAIN_COMMAND_SW;
    } else if (match_start(buf, &i, len, "C", 1)) {
      c.cmd.sw.straight = 0;
      c.kind = TRAIN_COMMAND_SW;
    }
  } else if (match_start(buf, &i, len, "q", 1)) {
      c.kind = TRAIN_COMMAND_Q;
  }
  return c;
}
