#include <avr/io.h>
#include "spi.h"

void spi_init(void) {
    DDRB |= (1 << SPI_MOSI) | (1 << SPI_SCK);
    DDRB &= ~(1 << SPI_MISO);
    SPCR = (1 << SPE) | (1 << MSTR) | (1 << SPR0);
}

uint8_t spi_transfer(uint8_t data) {
    SPDR = data;
    while (!(SPSR & (1 << SPIF)));
    return SPDR;
}

uint8_t spi_read(void) {
    return spi_transfer(0xFF);
}
