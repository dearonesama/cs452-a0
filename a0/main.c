#include "rpi.h"
#include "util.h"

static const unsigned TIMER_FREQ = 1000000;
static const unsigned *TIMER_CLO = (unsigned *)(0xfe003000 + 0x04);
static const unsigned TIMER_TICK = TIMER_FREQ / 10;  // 1 mhz => .1 s every tick
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
static const char MOVSCR[] = "\033[;H";
static const char CLRLNE[] = "\033[2K\r";
static const char HIDCSR[] = "\033[?25l";
static const char SHWCSR[] = "\033[?25h";

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
  queue_emplace_literal(scr_queue, "\r\n\r\nTrain # ");
  char num_buf[3];
  for (size_t i = 0; i < train_speeds_end; ++i) {
    format_two_digits(train_speeds[i].number, num_buf);
    queue_emplace(scr_queue, num_buf, 3);
  }
  queue_emplace_literal(scr_queue, "\r\nSpeed   ");
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

static const unsigned MAX_SWITCHES = 22;
static const unsigned SWITCH_TIMEOUT = TIMER_TICK * 3;

typedef char switch_status_t;

static size_t switch2idx(size_t i) {
  return ((i) < 19 ? ((i)-1) : ((i)-135));
}

static size_t idx2switch(size_t i) {
  return i < 18 ? i + 1 : i + 135;
}

static void format_three_digits(unsigned num, char *out) {
  out[0] = '0' + (num / 100);
  out[1] = '0' + (num / 10 % 10);
  out[2] = '0' + (num % 10);
  out[3] = ' ';
}

static void draw_switches_row(queue_t *scr_queue, switch_status_t *switches, size_t start, size_t end) {
  queue_emplace_literal(scr_queue, "\r\nSwitch # ");
  char num_buf[4];
  for (size_t i = start; i < end; ++i) {
    format_three_digits(idx2switch(i), num_buf);
    queue_emplace(scr_queue, num_buf, 4);
  }
  queue_emplace_literal(scr_queue, "\r\nStatus   ");
  num_buf[0] = num_buf[2] = num_buf[3] = ' ';
  for (size_t i = start; i < end; ++i) {
    num_buf[1] = switches[i];
    queue_emplace(scr_queue, num_buf, 4);
  }
}

static void draw_switches(queue_t *scr_queue, switch_status_t *switches) {
  queue_emplace_literal(scr_queue, "\r\n");
  draw_switches_row(scr_queue, switches, 0, MAX_SWITCHES / 2);
  draw_switches_row(scr_queue, switches, MAX_SWITCHES / 2, MAX_SWITCHES);
}

static const size_t MAX_SENSOR_OUT = 10;
static const unsigned TRAIN_CMD_TIMEOUT = TIMER_TICK;

typedef struct {
  char alp, num;
} sensor_elem_t;

static void add_active_sensor(char alpha, char num, sensor_elem_t *list, size_t *list_idx) {
  list[*list_idx].alp = alpha;
  list[*list_idx].num = num;
  ++*list_idx;
  if (*list_idx == MAX_SENSOR_OUT) {
    *list_idx = 0;
  }
}

static void add_active_sensors_from_data(char dat, int num_offset, char alpha, sensor_elem_t *list, size_t *list_idx) {
  // note: the track manual seems to use big endian, but the data from pi is little endian
  // to both bytes and bits
  for (unsigned i = 0; i < 8; ++i) {
    if (((unsigned char)dat >> (7 - i)) & 1) {
      add_active_sensor(alpha, i + 1 + num_offset, list, list_idx);
    }
  }
}

static void draw_sensors(queue_t *scr_queue, sensor_elem_t *list, size_t list_idx) {
  queue_emplace_literal(scr_queue, "\r\n\r\nMost active sensors ");
  char num_buf[3];
  size_t past_idx = (list_idx == 0 ? MAX_SENSOR_OUT : list_idx)- 1;
  for (size_t i = 0; i < MAX_SENSOR_OUT; ++i) {
    if (!list[i].alp) {  // unfilled initializer values
      break;
    }
    format_two_digits(list[i].num, num_buf);
    if (i == past_idx) {
      queue_emplace_literal(scr_queue, "\033[1m");
      queue_emplace(scr_queue, &list[i].alp, 1);
      queue_emplace(scr_queue, num_buf, 2);
      queue_emplace_literal(scr_queue, " \033[0m ");
    } else {
      queue_emplace(scr_queue, &list[i].alp, 1);
      queue_emplace(scr_queue, num_buf, 3);
    }
  }
}

typedef struct {
  unsigned last_it_timer, last_query_timer;
  struct perf_data_0_t {
    unsigned it, query_resp, query_resp_full;
  } rt, max;
} perf_data_t;

static unsigned umax(unsigned a, unsigned b) {
  return a > b ? a : b;
}

static unsigned tick2us(unsigned a) {
  return a * 1000000 / TIMER_FREQ;
}

static void draw_perf(queue_t *scr_queue, perf_data_t *perf) {
  struct perf_data_0_t lst[] = {perf->rt, perf->max};
  char num_buf[20];
  queue_emplace_literal(scr_queue, "\r\n\r\n");
  queue_emplace_literal(scr_queue, CLRLNE);
  queue_emplace_literal(scr_queue, "RT:");

  for (size_t i = 0; i < 2; ++i) {
    queue_emplace_literal(scr_queue, " IT ");
    size_t len = utoa(lst[i].it, num_buf);
    queue_emplace(scr_queue, num_buf, len); 
    queue_emplace_literal(scr_queue, " FB ");
    len = utoa(lst[i].query_resp, num_buf);
    queue_emplace(scr_queue, num_buf, len);
    queue_emplace_literal(scr_queue, " FF ");
    len = utoa(lst[i].query_resp_full, num_buf);
    queue_emplace(scr_queue, num_buf, len);
    queue_emplace_literal(scr_queue, " us");

    if (i == 0) {
      queue_emplace_literal(scr_queue, "\r\n");
      queue_emplace_literal(scr_queue, CLRLNE);
      queue_emplace_literal(scr_queue, "MX:");
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
  queue_init(&train_queue, trainbuf, sizeof(trainbuf) / sizeof(trainbuf[0]));

  int train_cmd_paused = 0;
  unsigned last_train_cmd_timer = 0;

  display_clock_t clock;
  display_clock_init(&clock);
  unsigned last_redraw_timer = 0;

  train_speed_elem_t train_speeds[MAX_TRAINS];
  size_t train_speeds_end = 0;

  struct {
    char waiting, number, speed;
    unsigned clock_from;
  } reversal = {0, 0, 0, 0};

  switch_status_t switch_statuses[22]; // 1-18, 153-156
  memset(switch_statuses, '?', sizeof switch_statuses);

  struct {
    char waiting;
    unsigned clock_from;
  } switch_halt = {0, 0};

  sensor_elem_t sensors[MAX_SENSOR_OUT];
  memset(sensors, 0, sizeof sensors);
  size_t sensor_list_idx = 0;

  // keeps track of incoming (5*16) feedback bytes
  struct {
    char current_alp, ith_byte;
  } sensor_update = {'A', 0};

  perf_data_t perf;
  memset(&perf, 0, sizeof perf);
  perf.last_it_timer = *TIMER_CLO;

  uart_putc(0, 1, 192);
  uart_puts(0, 0, CLRSCR, sizeof CLRSCR / sizeof(CLRSCR[0]) - 1);
  uart_puts(0, 0, HIDCSR, sizeof HIDCSR / sizeof(HIDCSR[0]) - 1);

  while (1) {
    int blocked = reversal.waiting || switch_halt.waiting;
    // timer updates redraw the screen
    unsigned curr_timer = *TIMER_CLO;
    if (curr_timer - last_redraw_timer >= TIMER_TICK) {
      last_redraw_timer = curr_timer > last_redraw_timer ? (curr_timer / TIMER_TICK * TIMER_TICK) : TIMER_TICK_NEAREST_ROUND;
      display_clock_advance(&clock);
      char clock_buf[10];
      int clen = display_clock_sprint(&clock, clock_buf);
      queue_consume(&scr_queue, sizeof scrbuf / sizeof(scrbuf[0]));
      queue_emplace_literal(&scr_queue, MOVSCR);
      queue_emplace_literal(&scr_queue, "T R A I N S\r\nSystem uptime: ");
      queue_emplace(&scr_queue, clock_buf, clen);

      queue_emplace_literal(&scr_queue, "\r\n");
      queue_emplace_literal(&scr_queue, CLRLNE);
      queue_emplace_literal(&scr_queue, "Command> ");

      if (blocked) {
        queue_emplace_literal(&scr_queue, "(in progress)");
      } else {
        queue_emplace(&scr_queue, user_input_line, user_input_line_end);
        queue_emplace_literal(&scr_queue, "_");
      }

      draw_speeds(&scr_queue, train_speeds, train_speeds_end);
      draw_switches(&scr_queue, switch_statuses);
      draw_sensors(&scr_queue, sensors, sensor_list_idx);
      draw_perf(&scr_queue, &perf);

      queue_emplace_literal(&scr_queue, "\r\n");
      queue_emplace_literal(&scr_queue, CLRLNE);
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

    // cancel solenoids after turnouts if applicable
    if (switch_halt.waiting && curr_timer - switch_halt.clock_from >= SWITCH_TIMEOUT) {
      switch_halt.waiting = 0;
      char cmd_buf[1] = {32};
      queue_emplace(&train_queue, cmd_buf, 1);
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
          switch_statuses[switch2idx(c.cmd.sw.switch_num)] = c.cmd.sw.straight ? 'S' : 'C';
          switch_halt.waiting = 1;
          switch_halt.clock_from = curr_timer;
          cmd_buf[0] = c.cmd.sw.straight ? 33 : 34;
          cmd_buf[1] = c.cmd.sw.switch_num;
          queue_emplace(&train_queue, cmd_buf, 2);
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

    // try getting something from trainset feedback
    if (uart_try_getc(0, 1, new_char)) {
      if (sensor_update.ith_byte == 0) {
        add_active_sensors_from_data(new_char[0], 0, sensor_update.current_alp, sensors, &sensor_list_idx);
        ++sensor_update.ith_byte;
        perf.max.query_resp = umax(perf.max.query_resp, perf.rt.query_resp = tick2us(curr_timer - perf.last_query_timer));
      } else {
        add_active_sensors_from_data(new_char[0], 8, sensor_update.current_alp, sensors, &sensor_list_idx);
        sensor_update.ith_byte = 0;
        sensor_update.current_alp += 1;
        if (sensor_update.current_alp == 'F') {
          sensor_update.current_alp = 'A';
          train_cmd_paused = 0;
          perf.max.query_resp_full = umax(perf.max.query_resp_full, perf.rt.query_resp_full = tick2us(curr_timer - perf.last_query_timer));
        }
      }
    }

    // try putting something to screen
    size_t buf_len;
    char *buf_start = queue_longest_data(&scr_queue, &buf_len);
    if (buf_len) {
      buf_len = uart_try_puts(0, 0, buf_start, buf_len);
      queue_consume(&scr_queue, buf_len);
    }

    // try putting something to train
    if (!train_cmd_paused && curr_timer - last_train_cmd_timer >= TRAIN_CMD_TIMEOUT) {
      buf_start = queue_longest_data(&train_queue, &buf_len);
      if (buf_len) {
        // this is a design mistake: some commands need to wait for some time and then fire another
        // command, but their timers are initialized when the command is pushed to the local queue,
        // not when pushed to uart, and there is time diff between the two. ideally we want to use
        // a generic queue to contain commands, and init the timer when the command is actually
        // submitted here.
        buf_len = uart_try_puts(0, 1, buf_start, buf_len);
        queue_consume(&train_queue, buf_len);
        last_train_cmd_timer = curr_timer;
      } else if (sensor_update.current_alp == 'A' && sensor_update.ith_byte == 0) {
        // if there is no command to send and feedback is done, request feedback
        char cmd_buf[1] = {128+5};
        if (uart_try_puts(0, 1, cmd_buf, 1)) {
          train_cmd_paused = 1;
          last_train_cmd_timer = curr_timer;
          perf.last_query_timer = curr_timer;
        }
      }
    }

    perf.max.it = umax(perf.max.it, perf.rt.it = tick2us(curr_timer - perf.last_it_timer));
    perf.last_it_timer = curr_timer;
  }

end:
  uart_puts(0, 0, "\r\n", 2);
  uart_puts(0, 0, SHWCSR, sizeof SHWCSR / sizeof(SHWCSR[0]) - 1);
  return 0;
}
