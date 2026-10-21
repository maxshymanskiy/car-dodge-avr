#ifndef TEST_AVR_PGMSPACE_H
#define TEST_AVR_PGMSPACE_H
#include <string.h>
#define PROGMEM
#define pgm_read_byte(address) (*(const unsigned char *)(address))
#define memcpy_P memcpy
#endif
