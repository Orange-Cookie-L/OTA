#include "flash.h"

HAL_StatusTypeDef Flash_EraseSector(uint32_t start_addr, uint32_t size)
{
    uint32_t sector_error = 0;
    FLASH_EraseInitTypeDef erase_init;
    HAL_StatusTypeDef status;

    erase_init.TypeErase = FLASH_TYPEERASE_SECTORS;
    erase_init.VoltageRange = FLASH_VOLTAGE_RANGE_3;
    erase_init.Sector = FLASH_SECTOR_4;
    erase_init.NbSectors = 1;

    HAL_FLASH_Unlock();

    while (size > 0) {
        status = HAL_FLASHEx_Erase(&erase_init, &sector_error);
        if (status != HAL_OK) {
            HAL_FLASH_Lock();
            return status;
        }

        start_addr += 0x20000;
        size -= 0x20000;
        erase_init.Sector++;
    }

    HAL_FLASH_Lock();
    return HAL_OK;
}

HAL_StatusTypeDef Flash_Write(uint32_t addr, uint8_t *data, uint16_t length)
{
    HAL_StatusTypeDef status;
    uint32_t word_data;

    HAL_FLASH_Unlock();

    for (uint16_t i = 0; i < length; i += 4) {
        if (i + 4 <= length) {
            word_data = *(uint32_t*)(data + i);
        } else {
            word_data = 0;
            for (uint8_t j = 0; j < (length - i); j++) {
                word_data |= (uint32_t)data[i + j] << (j * 8);
            }
        }

        status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, addr + i, word_data);
        if (status != HAL_OK) {
            HAL_FLASH_Lock();
            return status;
        }
    }

    HAL_FLASH_Lock();
    return HAL_OK;
}

HAL_StatusTypeDef Flash_Read(uint32_t addr, uint8_t *buffer, uint16_t length)
{
    for (uint16_t i = 0; i < length; i++) {
        buffer[i] = *(volatile uint8_t*)(addr + i);
    }
    return HAL_OK;
}
