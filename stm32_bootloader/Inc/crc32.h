#ifndef __CRC32_H
#define __CRC32_H

#include <stdint.h>

uint32_t CRC32_Calculate(uint8_t *data, uint16_t length, uint32_t crc);
uint32_t CRC32_CalculateFlash(uint32_t addr, uint32_t size);

#endif
