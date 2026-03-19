/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : bootloader.c
  * @brief          : Bootloader for OTA updates
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

#include "stm32f10x.h"
#include "../Inc/bootloader.h"
#include "../../Core/Inc/main.h"
#include <string.h>

// 闪存配置
#define FLASH_BASE_ADDRESS       0x08000000
#define BOOTLOADER_SIZE          0x4000  // 16KB
#define APPLICATION_SIZE         0x1C000 // 112KB
#define APPLICATION_START_ADDRESS (FLASH_BASE_ADDRESS + BOOTLOADER_SIZE)
#define FLASH_PAGE_SIZE          1024    // 1KB

// 固件下载状态
uint32_t firmware_size = 0;
uint32_t firmware_offset = 0;
uint8_t firmware_buffer[FLASH_PAGE_SIZE];
uint16_t buffer_index = 0;

/**
  * @brief  初始化Bootloader
  * @retval None
  */
void bootloader_init(void)
{
  // 初始化系统时钟
  SystemInit();
  
  // 初始化GPIO
  RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);
  GPIO_InitTypeDef GPIO_InitStructure;
  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_13;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
  GPIO_Init(GPIOC, &GPIO_InitStructure);
  GPIO_ResetBits(GPIOC, GPIO_Pin_13);
  
  // 初始化串口
  RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1, ENABLE);
  USART_InitTypeDef USART_InitStructure;
  USART_InitStructure.USART_BaudRate = 115200;
  USART_InitStructure.USART_WordLength = USART_WordLength_8b;
  USART_InitStructure.USART_StopBits = USART_StopBits_1;
  USART_InitStructure.USART_Parity = USART_Parity_No;
  USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
  USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
  USART_Init(USART1, &USART_InitStructure);
  USART_Cmd(USART1, ENABLE);
  
  // 检查是否需要进入OTA模式
  if (check_ota_trigger()) {
    enter_ota_mode();
  } else {
    jump_to_application();
  }
}

/**
  * @brief  检查是否需要进入OTA模式
  * @retval true: 需要进入OTA模式, false: 直接启动应用
  */
bool check_ota_trigger(void)
{
  // 检查特定的GPIO状态
  // 这里可以根据需要修改触发条件
  return false; // 默认直接启动应用
}

/**
  * @brief  进入OTA模式
  * @retval None
  */
void enter_ota_mode(void)
{
  // 闪烁LED表示进入OTA模式
  for (int i = 0; i < 3; i++) {
    GPIO_SetBits(GPIOC, GPIO_Pin_13);
    for (volatile uint32_t j = 0; j < 100000; j++);
    GPIO_ResetBits(GPIOC, GPIO_Pin_13);
    for (volatile uint32_t j = 0; j < 100000; j++);
  }
  
  // 等待ESP8266发送固件数据
  while (1) {
    handle_ota_communication();
  }
}

/**
  * @brief  处理OTA通信
  * @retval None
  */
void handle_ota_communication(void)
{
  if (USART_GetFlagStatus(USART1, USART_FLAG_RXNE) != RESET) {
    uint8_t data = USART_ReceiveData(USART1);
    
    // 处理固件数据
    process_firmware_data(data);
  }
}

/**
  * @brief  处理固件数据
  * @param  data: 固件数据
  * @retval None
  */
void process_firmware_data(uint8_t data)
{
  // 简单的固件数据处理
  // 实际项目中需要实现完整的协议
  firmware_buffer[buffer_index++] = data;
  
  // 当缓冲区满时，写入闪存
  if (buffer_index >= FLASH_PAGE_SIZE) {
    write_flash_page(firmware_offset, firmware_buffer, FLASH_PAGE_SIZE);
    firmware_offset += FLASH_PAGE_SIZE;
    buffer_index = 0;
    
    // 发送确认
    while (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET);
    USART_SendData(USART1, 'A');
    while (USART_GetFlagStatus(USART1, USART_FLAG_TC) == RESET);
  }
}

/**
  * @brief  写入闪存页
  * @param  address: 闪存地址
  * @param  data: 数据
  * @param  size: 大小
  * @retval None
  */
void write_flash_page(uint32_t address, uint8_t* data, uint32_t size)
{
  // 解锁闪存
  FLASH_Unlock();
  
  // 擦除页
  FLASH_ErasePage(APPLICATION_START_ADDRESS + address);
  
  // 写入数据
  for (uint32_t i = 0; i < size; i += 4) {
    uint32_t word_data = *((uint32_t*)&data[i]);
    FLASH_ProgramWord(APPLICATION_START_ADDRESS + address + i, word_data);
  }
  
  // 锁定闪存
  FLASH_Lock();
}

/**
  * @brief  跳转到应用程序
  * @retval None
  */
void jump_to_application(void)
{
  // 检查应用程序是否存在
  if (((*(__IO uint32_t*)APPLICATION_START_ADDRESS) & 0x2FFE0000) == 0x20000000) {
    // 跳转到应用程序
    void (*application_entry)(void) = (void (*)(void))(*(__IO uint32_t*)(APPLICATION_START_ADDRESS + 4));
    
    // 设置栈指针
    __set_MSP(*(__IO uint32_t*)APPLICATION_START_ADDRESS);
    
    // 跳转到应用程序
    application_entry();
  }
  
  // 如果没有应用程序，进入OTA模式
  enter_ota_mode();
}
