#ifndef SPI_H
#define SPI_H

#include <stdint.h>

#define SPI_MOSI PB3
#define SPI_MISO PB4
#define SPI_SCK  PB5

void spi_init(void);
uint8_t spi_transfer(uint8_t data);
uint8_t spi_read(void);

#endif