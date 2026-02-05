#ifndef __FLASH_H
#define __FLASH_H

#include <stdint.h>

#define FLASH_SECTOR_SIZE      0x20000
#define FLASH_PAGE_SIZE        0x400
#define FLASH_VALID_FLAG_ADDR  0x0800FFF0
#define FLASH_VALID_FLAG_VALUE 0xA5A5A5A5

uint8_t Flash_EraseApplication(void);
uint8_t Flash_WriteData(uint32_t address, uint8_t *data, uint32_t length);
uint8_t Flash_VerifyApplication(void);
void Flash_SetValidFlag(void);
uint32_t Flash_CalculateCRC(uint32_t start_addr, uint32_t length);

#endif
