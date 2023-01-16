#include <err.h>
#include <stdio.h>
#include <string.h>
#include "../util.h"

#define ASSERT(condition)                                           \
do {                                                                \
    if(!(condition))                                                \
        errx(1, "Failed at %s:%d.", __FILE__, __LINE__);             \
} while (0)

#define BUFLEN(v) (sizeof(v) / sizeof(v[0]))

static void test_clock_t() {
  display_clock_t c;
  display_clock_init(&c);
  for (int i = 0; i < 1000073; ++i) {
    display_clock_advance(&c);
  }
  char buf[10];
  display_clock_sprint(&c, buf);
  ASSERT(strcmp(buf, "666:47:30") == 0);  // overflow from 1000
}

/*static my_print(char *dat, size_t len) {
  for (size_t i = 0; i < len; ++i) {
    printf("%c", dat[i]);
  }
  printf("\n");
}*/

static void test_queue_t() {
  char data[7];
  queue_t q;
  queue_init(&q, data, BUFLEN(data));

  char *dat;
  size_t len;
  queue_emplace_literal(&q, "a");
  // a-------

#define QLDASSERT(s)                                   \
        do {                                           \
          dat = queue_longest_data(&q, &len);          \
          ASSERT(len == BUFLEN(s) - 1); \
          ASSERT(strncmp(dat, s, len) == 0);           \
        } while (0)

  QLDASSERT("a");

  queue_emplace_literal(&q, "bc");
  // abc----
  QLDASSERT("abc");

  queue_consume(&q, 2);
  // --c----
  QLDASSERT("c");

  queue_emplace_literal(&q, "defg");
  // --cdefg
  QLDASSERT("cdefg");

  queue_emplace_literal(&q, "h");
  // h-cdefg
  QLDASSERT("cdefg");

  queue_consume(&q, 4);
  // h-----g
  QLDASSERT("g");

  queue_emplace_literal(&q, "ij");
  // hij---g
  queue_consume(&q, 1);
  // hij----
  QLDASSERT("hij");

  queue_emplace_literal(&q, "klm");
  // hijklm-
  QLDASSERT("hijklm");

  queue_consume(&q, 4);
  // ----lm-
  QLDASSERT("lm");

  queue_emplace_literal(&q, "nop");
  // op--lmn
  queue_consume(&q, 4);
  // -p-----
  QLDASSERT("p");

  queue_consume(&q, 200);
  ASSERT(q.end - q.begin == 0);

#undef QLDASSERT
}

void test_train_command() {
  char str1[] = "tr 12 16";
  train_command_t c = try_parse_train_command(str1, BUFLEN(str1) - 1);
  ASSERT(c.kind == TRAIN_COMMAND_TR);
  ASSERT(c.cmd.tr.train_num == 12);
  ASSERT(c.cmd.tr.speed = 16);

  char str2[] = "tr 6 9";
  c = try_parse_train_command(str2, BUFLEN(str2) - 1);
  ASSERT(c.kind == TRAIN_COMMAND_TR);
  ASSERT(c.cmd.tr.train_num == 6);
  ASSERT(c.cmd.tr.speed = 9);

  char str22[] = "tr 6 93";
  c = try_parse_train_command(str22, BUFLEN(str22) - 1);
  ASSERT(c.kind == TRAIN_COMMAND_INVALID);

  char str3[] = "rv  03";
  c = try_parse_train_command(str3, BUFLEN(str3) - 1);
  ASSERT(c.kind == TRAIN_COMMAND_RV);
  ASSERT(c.cmd.rv.train_num == 3);

  char str4[] = "sw 32 S";
  c = try_parse_train_command(str4, BUFLEN(str4) - 1);
  ASSERT(c.kind == TRAIN_COMMAND_SW);
  ASSERT(c.cmd.sw.switch_num == 32);
  ASSERT(c.cmd.sw.straight);

  char str5[] = " sw 1  C";
  c = try_parse_train_command(str5, BUFLEN(str5) - 1);
  ASSERT(c.kind == TRAIN_COMMAND_SW);
  ASSERT(c.cmd.sw.switch_num == 1);
  ASSERT(!c.cmd.sw.straight);

  char str6[] = "sw 153 S";
  c = try_parse_train_command(str6, BUFLEN(str6) - 1);
  ASSERT(c.kind == TRAIN_COMMAND_SW);
  ASSERT(c.cmd.sw.switch_num == 153);
  ASSERT(c.cmd.sw.straight);

  c = try_parse_train_command("q", 1);
  ASSERT(c.kind == TRAIN_COMMAND_Q);
}

int main() {
  test_clock_t();
  test_queue_t();
  test_train_command();
  puts("Tests passed.");
}
