#include "rpi.h"
#include "util.h"

static const unsigned *TIMER_CLO = (unsigned *)(0xfe003000 + 0x04);
static const unsigned TIMER_TICK = 1000000 / 10;  // 1 mhz => .1 s every tick
static const unsigned TIMER_TICK_NEAREST_ROUND = 4294900000;

static const unsigned TRAIN_SOLENOID_TIMEOUT = TIMER_TICK * 3;
static const unsigned TRAIN_ACCELERATION[] = {
  TIMER_TICK * 3,  // dummy
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

typedef struct {
  char number;
  char speed;
} train_speed_elem_t;

static void format_two_digits(unsigned num, char *out) {
  out[0] = '0' + (num / 10 % 10);
  out[1] = '0' + (num % 10);
  out[2] = ' ';
}

static void draw_speeds(queue_t *scr_queue, train_speed_elem_t *train_speeds, size_t train_speeds_end) {
  queue_emplace(scr_queue, "\r\n\r\nTrain # ", 12);
  char num_buf[3];
  for (size_t i = 0; i < train_speeds_end; ++i) {
    format_two_digits(train_speeds[i].number, num_buf);
    queue_emplace(scr_queue, num_buf, 3);
  }
  queue_emplace(scr_queue, "\r\nSpeed   ", 10);
  for (size_t i = 0; i < train_speeds_end; ++i) {
    format_two_digits(train_speeds[i].speed, num_buf);
    queue_emplace(scr_queue, num_buf, 3);
  }
}

static train_speed_elem_t *find_train_speed(char number, train_speed_elem_t *train_speeds, size_t train_speeds_end) {
  for (size_t i = 0; i < train_speeds_end; ++i) {
    if (train_speeds[i].number == number) {
      return &train_speeds[i];
    }
  }
  return 0;
}

static void update_train_speed(char number, char speed, train_speed_elem_t *train_speeds, size_t *train_speeds_end) {
  train_speed_elem_t *train_speed = find_train_speed(number, train_speeds, *train_speeds_end);
  if (train_speed) {
    train_speed->speed = speed;
  } else {
    train_speeds[*train_speeds_end].number = number;
    train_speeds[*train_speeds_end].speed = speed;
    ++*train_speeds_end;
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
  queue_init(&train_queue, trainbuf, sizeof(trainbuf) / sizeof(trainbuf[0]));
  queue_t *queues[] = { &scr_queue, &train_queue };

  display_clock_t clock;
  display_clock_init(&clock);
  unsigned last_redraw_timer = 0;

  train_speed_elem_t train_speeds[MAX_TRAINS];
  size_t train_speeds_end = 0;

  struct {
    char waiting, number, speed;
    unsigned clock_from;
  } reversal = {0, 0, 0, 0};

  while (1) {
    int blocked = reversal.waiting;
    // timer updates redraw the screen
    unsigned curr_timer = *TIMER_CLO;
    if (curr_timer - last_redraw_timer >= TIMER_TICK) {
      last_redraw_timer = curr_timer > last_redraw_timer ? (curr_timer / TIMER_TICK * TIMER_TICK) : TIMER_TICK_NEAREST_ROUND;
      display_clock_advance(&clock);
      char clock_buf[10];
      int clen = display_clock_sprint(&clock, clock_buf);
      queue_emplace(&scr_queue, CLRSCR, sizeof(CLRSCR) / sizeof(CLRSCR[0]) - 1);
      queue_emplace(&scr_queue, "T R A I N S\r\nSystem uptime: ", 28);
      queue_emplace(&scr_queue, clock_buf, clen);

      queue_emplace(&scr_queue, "\r\nCommand> ", 11);

      if (blocked) {
        queue_emplace(&scr_queue, "(in progress)", 13);
      } else {
        queue_emplace(&scr_queue, user_input_line, user_input_line_end);
        queue_emplace(&scr_queue, "_", 1);
      }

      draw_speeds(&scr_queue, train_speeds, train_speeds_end);
      queue_emplace(&scr_queue, "\r\n", 2);
    }

    // submit pending re-acceleration if applicable
    if (reversal.waiting == 1 && curr_timer - reversal.clock_from >= TRAIN_ACCELERATION[(size_t)reversal.speed % 16]) {
      reversal.waiting = 2;
      char cmd_buf[2];
      cmd_buf[0] = 15;
      cmd_buf[1] = reversal.number;
      queue_emplace(&train_queue, cmd_buf, 2);
      reversal.clock_from = curr_timer;
    } else if (reversal.waiting == 2 && curr_timer - reversal.clock_from >= TRAIN_ACCELERATION[0]) {
      reversal.waiting = 0;
      update_train_speed(reversal.number, reversal.speed, train_speeds, &train_speeds_end);
      char cmd_buf[2];
      cmd_buf[0] = reversal.speed;
      cmd_buf[1] = reversal.number;
      queue_emplace(&train_queue, cmd_buf, 2);
    }

    // try getting something from screen
    // (ignore chars when waiting for command to finish)
    char new_char[1];
    if (uart_try_getc(0, 0, new_char) && !blocked) {
      if (new_char[0] == '\r') {
        // do commands
        train_command_t c = try_parse_train_command(user_input_line, user_input_line_end);
        char cmd_buf[2];

        switch (c.kind) {
        case TRAIN_COMMAND_TR:
          update_train_speed(c.cmd.tr.train_num, c.cmd.tr.speed, train_speeds, &train_speeds_end);
          cmd_buf[0] = c.cmd.tr.speed;
          cmd_buf[1] = c.cmd.tr.train_num;
          queue_emplace(&train_queue, cmd_buf, 2);
          break;
        case TRAIN_COMMAND_RV: {
          train_speed_elem_t *sp = find_train_speed(c.cmd.rv.train_num, train_speeds, train_speeds_end);
          if (sp) {
            reversal.waiting = 1;
            reversal.speed = sp->speed;
            reversal.clock_from = curr_timer;
            sp->speed = cmd_buf[0] = (sp->speed >= 16 ? 16 : 0);
            cmd_buf[1] = reversal.number = c.cmd.rv.train_num;
            queue_emplace(&train_queue, cmd_buf, 2);
          }
          break;
        }
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
