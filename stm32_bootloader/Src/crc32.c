#include "crc32.h"

uint32_t CRC32_Calculate(uint8_t *data, uint16_t length, uint32_t crc)
{
    for (uint16_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 1) {
                crc = (crc >> 1) ^ 0xEDB88320;
            } else {
                crc >>= 1;
            }
        }
    }
    return crc;
}

uint32_t CRC32_CalculateFlash(uint32_t addr, uint32_t size)
{
    uint32_t crc = 0xFFFFFFFF;
    uint8_t buffer[256];
    uint32_t offset = 0;

    while (offset < size) {
        uint16_t chunk_size = (size - offset) > 256 ? 256 : (size - offset);
        Flash_Read(addr + offset, buffer, chunk_size);
        crc = CRC32_Calculate(buffer, chunk_size, crc);
        offset += chunk_size;
    }

    return crc ^ 0xFFFFFFFF;
}
