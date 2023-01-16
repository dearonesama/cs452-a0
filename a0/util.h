#pragma once

#include <stddef.h>

typedef struct {
  unsigned min;
  unsigned char sec, tenth;
} display_clock_t;

void display_clock_init(display_clock_t *);

// advances clock by tenth second
void display_clock_advance(display_clock_t *);

// formats the structure into string and returns length of string
int display_clock_sprint(display_clock_t *, char *);

void *memset(void *s, int c, size_t n);
void *memcpy(void* restrict dest, const void* restrict src, size_t n);

/**
 * case 0 (empty)
 * |-------------------|
 *  ^
 * begin&end
 * 
 * case 1
 * |||||||-------------|
 *  ^     ^
 * begin  end
 * 
 * case 2
 * |-----|||||||||||---|
 *       ^          ^
 *      begin     end
 * 
 * case 3
 * |-----------||||||||||
 *  ^          ^
 * end        begin
 * 
 * case 4
 * |||-------------||||||
 *    ^            ^
 *   end         begin
 */
typedef struct {
  char *data;
  size_t capacity;  // actual storable data is this number - 1
  size_t begin;  // item to dequeue
  size_t end;  // available enqueue pos
} queue_t;

void queue_init(queue_t *, char *, size_t);
// writes data and shifts end ptr; if it reaches the end of buffer, then write from the left
void queue_emplace(queue_t *, const char *, size_t);
// shifts begin ptr
void queue_consume(queue_t *, size_t);
// gets longest contigent range of data
char *queue_longest_data(queue_t *, size_t *);

#define queue_emplace_literal(q, s) queue_emplace(q, s, sizeof s / sizeof(s[0]) - 1)

typedef struct {
  enum {
    TRAIN_COMMAND_TR,
    TRAIN_COMMAND_RV,
    TRAIN_COMMAND_SW,
    TRAIN_COMMAND_Q,
    TRAIN_COMMAND_INVALID,
  } kind;
  union {
    struct { unsigned char train_num, speed; } tr;
    struct { unsigned char train_num; } rv;
    struct { unsigned char switch_num, straight; } sw;
  } cmd;
} train_command_t;

train_command_t try_parse_train_command(char *, size_t);
