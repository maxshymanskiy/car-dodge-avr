#include <avr/io.h>
#include <avr/interrupt.h>

volatile uint32_t milliseconds = 0;

ISR(TIMER0_COMPA_vect) {
    milliseconds++;
}

void millis_init() {
    TCCR0A |= (1 << WGM01);
    OCR0A = 249;
    TIMSK0 |= (1 << OCIE0A);
    TCCR0B |= (1 << CS01) | (1 << CS00);

    sei();
}

uint32_t millis() {
    uint32_t value;

    uint8_t sreg = SREG;
    cli();

    value = milliseconds;

    SREG = sreg;

    return value;
}