/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : bootloader.h
  * @brief          : Bootloader header file
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2023 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

#ifndef BOOTLOADER_H
#define BOOTLOADER_H

#include "../../Core/Inc/main.h"

// 函数声明
void bootloader_init(void);
bool check_ota_trigger(void);
void enter_ota_mode(void);
void handle_ota_communication(void);
void process_firmware_data(uint8_t data);
void write_flash_page(uint32_t address, uint8_t* data, uint32_t size);
void jump_to_application(void);

#endif /* BOOTLOADER_H */
