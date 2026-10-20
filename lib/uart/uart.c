#include "uart.h"

#include <avr/io.h>

#ifndef F_CPU
#define F_CPU 16000000UL
#endif

void uart_init(uint32_t baud)
{
    uint16_t ubrr = (F_CPU / (16UL * baud)) - 1;

    UBRR0H = (uint8_t)(ubrr >> 8);
    UBRR0L = (uint8_t)ubrr;

    // Enable transmitter and receiver
    UCSR0B = (1 << TXEN0) | (1 << RXEN0);

    // 8 data bits, 1 stop bit, no parity
    UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);
}

void uart_write(char c)
{
    // Wait until transmit buffer is empty
    while (!(UCSR0A & (1 << UDRE0))) {
    }

    UDR0 = c;
}

char uart_read(void)
{
    // Wait until data is received
    while (!(UCSR0A & (1 << RXC0))) {
    }

    return UDR0;
}

uint8_t uart_available(void)
{
    return (UCSR0A & (1 << RXC0)) != 0;
}

void uart_print(const char *str)
{
    while (*str) {
        uart_write(*str);
        str++;
    }
}

void uart_println(const char *str)
{
    uart_print(str);
    uart_write('\r');
    uart_write('\n');
}

void uart_print_uint(uint32_t value)
{
    char buffer[11];
    uint8_t i = 0;

    if (value == 0) {
        uart_write('0');
        return;
    }

    while (value > 0) {
        buffer[i++] = '0' + (value % 10);
        value /= 10;
    }

    while (i > 0) {
        uart_write(buffer[--i]);
    }
}

void uart_print_int(int32_t value)
{
    if (value < 0) {
        uart_write('-');

        // Avoid overflow for INT32_MIN
        uint32_t positive = -(value + 1);
        positive += 1;

        uart_print_uint(positive);
    } else {
        uart_print_uint((uint32_t)value);
    }
}

void uart_print_bin(uint8_t value)
{
    for (int8_t i = 7; i >= 0; i--) {
        if (value & (1 << i)) {
            uart_write('1');
        } else {
            uart_write('0');
        }
    }

    uart_write('\r');
    uart_write('\n');
}