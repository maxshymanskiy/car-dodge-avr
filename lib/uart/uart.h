#ifndef UART_H
#define UART_H

#include <stdint.h>

void uart_init(uint32_t baud);

void uart_write(char c);
char uart_read(void);
uint8_t uart_available(void);

void uart_print(const char *str);
void uart_println(const char *str);

void uart_print_uint(uint32_t value);
void uart_print_int(int32_t value);
void uart_print_bin(uint8_t value);

#endif