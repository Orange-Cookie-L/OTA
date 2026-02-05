#include "stm32f4xx_hal.h"
#include "flash.h"

uint8_t Flash_EraseApplication(void) {
    HAL_FLASH_Unlock();
    
    FLASH_EraseInitTypeDef erase_init;
    uint32_t sector_error;

    erase_init.TypeErase = FLASH_TYPEERASE_SECTORS;
    erase_init.VoltageRange = FLASH_VOLTAGE_RANGE_3;
    erase_init.Sector = FLASH_SECTOR_1;
    erase_init.NbSectors = 7;
    erase_init.Banks = FLASH_BANK_1;

    if (HAL_FLASHEx_Erase(&erase_init, &sector_error) != HAL_OK) {
        HAL_FLASH_Lock();
        return 0;
    }

    HAL_FLASH_Lock();
    return 1;
}

uint8_t Flash_WriteData(uint32_t address, uint8_t *data, uint32_t length) {
    HAL_FLASH_Unlock();

    for (uint32_t i = 0; i < length; i += 4) {
        uint32_t word = 0;
        uint32_t bytes_to_copy = (length - i >= 4) ? 4 : (length - i);

        for (uint32_t j = 0; j < bytes_to_copy; j++) {
            word |= data[i + j] << (j * 8);
        }

        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, address + i, word) != HAL_OK) {
            HAL_FLASH_Lock();
            return 0;
        }
    }

    HAL_FLASH_Lock();
    return 1;
}

uint8_t Flash_VerifyApplication(void) {
    uint32_t valid_flag = *((uint32_t *)FLASH_VALID_FLAG_ADDR);
    uint32_t app_stack_ptr = *((uint32_t *)0x08010000);

    if (valid_flag == FLASH_VALID_FLAG_VALUE && 
        app_stack_ptr != 0xFFFFFFFF) {
        return 1;
    }
    return 0;
}

void Flash_SetValidFlag(void) {
    HAL_FLASH_Unlock();

    HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, FLASH_VALID_FLAG_ADDR, 
                      FLASH_VALID_FLAG_VALUE);

    HAL_FLASH_Lock();
}

uint32_t Flash_CalculateCRC(uint32_t start_addr, uint32_t length) {
    uint32_t crc = 0xFFFFFFFF;
    uint8_t *data = (uint8_t *)start_addr;

    for (uint32_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 1) {
                crc = (crc >> 1) ^ 0xEDB88320;
            } else {
                crc >>= 1;
            }
        }
    }

    return crc ^ 0xFFFFFFFF;
}
