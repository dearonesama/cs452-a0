#pragma once

#include <stdint.h>
#include <stddef.h>

void init_gpio();
void init_spi(uint32_t channel);
void init_uart(uint32_t spiChannel);
// check if we can get a char; if yes, then write it to out
int uart_try_getc(size_t spiChannel, size_t uartChannel, char *out);
// try writing, returns chars actually written
int uart_try_puts(size_t spiChannel, size_t uartChannel, const char* buf, size_t blen);
char uart_getc(size_t spiChannel, size_t uartChannel);
void uart_putc(size_t spiChannel, size_t uartChannel, char c);
void uart_puts(size_t spiChannel, size_t uartChannel, const char* buf, size_t blen);
//void init_timer();
