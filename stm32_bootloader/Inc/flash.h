#ifndef __FLASH_H
#define __FLASH_H

#include "stm32f4xx_hal.h"

HAL_StatusTypeDef Flash_EraseSector(uint32_t start_addr, uint32_t size);
HAL_StatusTypeDef Flash_Write(uint32_t addr, uint8_t *data, uint16_t length);
HAL_StatusTypeDef Flash_Read(uint32_t addr, uint8_t *buffer, uint16_t length);

#endif
