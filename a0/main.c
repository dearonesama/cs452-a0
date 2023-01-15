#include "rpi.h"
#include "util.h"

static const unsigned *TIMER_CLO = (unsigned *)(0xfe003000 + 0x04);
static const unsigned TIMER_TICK = 1000000 / 10;  // 1 mhz => .1 s every tick
static const unsigned TIMER_TICK_NEAREST_ROUND = 4294900000;

static const unsigned TRAIN_SOLENOID_TIMEOUT = TIMER_TICK * 3;
static const unsigned TRAIN_ACCELERATION[] = {
  0,  // dummy
  TIMER_TICK * 15,
  TIMER_TICK * 25,
  TIMER_TICK * 35,
  TIMER_TICK * 45,
  TIMER_TICK * 45,
  TIMER_TICK * 45,
  TIMER_TICK * 45,
  TIMER_TICK * 55,
  TIMER_TICK * 55,
  TIMER_TICK * 55,
  TIMER_TICK * 55,
  TIMER_TICK * 55,
  TIMER_TICK * 55,
  TIMER_TICK * 55,
};
static const unsigned MAX_TRAINS = 99;

static const char CLRSCR[] = "\033[1;1H\033[2J";

static void format_two_digits(unsigned num, char *out) {
  out[0] = '0' + (num / 10 % 10);
  out[1] = '0' + (num % 10);
  out[2] = ' ';
}

static void draw_speeds(queue_t *scr_queue, signed char *train_speeds) {
  queue_emplace(scr_queue, "\r\n\r\nTrain # ", 12);
  char num_buf[3];
  for (size_t i = 0; i < MAX_TRAINS; ++i) {
    if (train_speeds[i] != -1) {
      format_two_digits(i, num_buf);
      queue_emplace(scr_queue, num_buf, 3);
    }
  }
  queue_emplace(scr_queue, "\r\nSpeed   ", 10);
  for (size_t i = 0; i < MAX_TRAINS; ++i) {
    if (train_speeds[i] != -1) {
      format_two_digits(train_speeds[i], num_buf);
      queue_emplace(scr_queue, num_buf, 3);
    }
  }
}

int main() {
  init_gpio();
  init_spi(0);
  init_uart(0);
  //init_timer();

  char user_input_line[256];
  size_t user_input_line_end = 0;

  char scrbuf[2048], trainbuf[256];
  queue_t scr_queue, train_queue;
  queue_init(&scr_queue, scrbuf, sizeof(scrbuf) / sizeof(scrbuf[0]));
  queue_init(&train_queue, scrbuf, sizeof(scrbuf) / sizeof(scrbuf[0]));
  queue_t *queues[] = { &scr_queue, &train_queue };

  display_clock_t clock;
  display_clock_init(&clock);
  unsigned last_timer = 0;

  signed char train_speeds[MAX_TRAINS];
  memset(train_speeds, -1, sizeof train_speeds);

  while (1) {
    // timer updates redraw the screen
    unsigned curr_timer = *TIMER_CLO;
    if (curr_timer - last_timer >= TIMER_TICK) {
      last_timer = curr_timer > last_timer ? (curr_timer / TIMER_TICK * TIMER_TICK) : TIMER_TICK_NEAREST_ROUND;
      display_clock_advance(&clock);
      char clock_buf[10];
      int clen = display_clock_sprint(&clock, clock_buf);
      queue_emplace(&scr_queue, CLRSCR, sizeof(CLRSCR) / sizeof(CLRSCR[0]) - 1);
      queue_emplace(&scr_queue, "T R A I N S\r\nCurrent Timer: ", 28);
      queue_emplace(&scr_queue, clock_buf, clen);

      queue_emplace(&scr_queue, "\r\nCommand> ", 11);
      queue_emplace(&scr_queue, user_input_line, user_input_line_end);
      queue_emplace(&scr_queue, "_", 1);

      draw_speeds(&scr_queue, train_speeds);
    }
    // try getting something from screen
    char new_char[1];
    if (uart_try_getc(0, 0, new_char)) {
      if (new_char[0] == '\r') {
        // do commands
        train_command_t c = try_parse_train_command(user_input_line, user_input_line_end);
        char cmd_buf[2];

        switch (c.kind) {
        case TRAIN_COMMAND_TR:
          train_speeds[c.cmd.tr.train_num] = c.cmd.tr.speed;
          cmd_buf[0] = c.cmd.tr.speed;
          cmd_buf[1] = c.cmd.tr.train_num;
          queue_emplace(&train_queue, cmd_buf, 2);
          break;
        case TRAIN_COMMAND_RV:
          break;
        case TRAIN_COMMAND_SW:
          break;
        case TRAIN_COMMAND_Q:
          goto end;
          break;
        default:
          break;
        }

        user_input_line_end = 0;
      } else {
        user_input_line[user_input_line_end] = new_char[0];
        ++user_input_line_end;
        queue_emplace(&scr_queue, new_char, 1);
      }
    }
    // try putting something to screen and trains
    for (size_t i = 0; i < 2; ++i) {
      size_t buf_len;
      char *buf_start = queue_longest_data(queues[i], &buf_len);
      if (buf_len) {
        buf_len = uart_try_puts(0, i, buf_start, buf_len);
        queue_consume(queues[i], buf_len);
      }
    }
  }

end:
  uart_puts(0, 0, "\r\n", 2);
  return 0;
}
