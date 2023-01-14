#include "rpi.h"
#include "util.h"

static const unsigned *TIMER_CLO = (unsigned *)(0xfe003000 + 0x04);
static const unsigned TIMER_TICK = 1000000 / 10;  // 1 mhz => .1 mhz every tick
static const unsigned TIMER_TICK_NEAREST_ROUND = 4294900000;

int main() {
  init_gpio();
  init_spi(0);
  init_uart(0);
  //init_timer();

  /*display_clock_t clock;
  clock.min = clock.sec = clock.tenth = 0;
  unsigned last_timer = 0;

  char msg1[] = "M, this is iotest (" __TIME__ ")\r\nPress 'q' to reboot\r\n";
  uart_puts(0, 0, msg1, sizeof(msg1) - 1);

  for (;;) {
    unsigned curr_timer = *TIMER_CLO;
    if (curr_timer - last_timer >= TIMER_TICK) {
      display_clock_advance(&clock);
      char buf[10];
      int len = display_clock_sprint(&clock, buf);
      uart_puts(0, 0, buf, len);
      memset(buf, '\r', len);
      uart_puts(0, 0, buf, len);
      last_timer = curr_timer > last_timer ? (curr_timer / TIMER_TICK * TIMER_TICK) : TIMER_TICK_NEAREST_ROUND;
    }
  }*/

  int lights = 0;

  char prompt[] = "PI> ";
  uart_puts(0, 0, prompt, sizeof(prompt) - 1);
  char c = ' ';
  while (c != 'q') {
    c = uart_getc(0, 0);
    if (c == '\r') {
      uart_puts(0, 0, "\r\n", 2);
      uart_puts(0, 0, prompt, sizeof(prompt) - 1);
    } else {
      uart_putc(0, 0, c);
      //uart_putc(0, 1, lights ? 23 : 0);
      //uart_putc(0, 1, 24);
      char buf[2];
      buf[0] = lights ? 23 : 0;
      buf[1] = 24;
      uart_puts(0, 1, buf, 2);
      lights = !lights;
    }
  }
  uart_puts(0, 0, "\r\n", 2);
  return 0;
}
