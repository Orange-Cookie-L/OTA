/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : system_stm32f1xx.c
  * @brief          : CMSIS Cortex-M3 Device Peripheral Access Layer System Source File
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

/* System Clock Configuration */
void SystemClock_Config(void)
{
  // 使能HSE
  RCC_HSEConfig(RCC_HSE_ON);
  // 等待HSE就绪
  while (RCC_GetFlagStatus(RCC_FLAG_HSERDY) == RESET);
  
  // 使能PLL
  RCC_PLLConfig(RCC_PLLSource_HSE_Div1, RCC_PLLMul_9);
  RCC_PLLCmd(ENABLE);
  // 等待PLL就绪
  while (RCC_GetFlagStatus(RCC_FLAG_PLLRDY) == RESET);
  
  // 设置系统时钟源为PLL
  RCC_SYSCLKConfig(RCC_SYSCLKSource_PLLCLK);
  // 等待系统时钟切换完成
  while (RCC_GetSYSCLKSource() != 0x08);
  
  // 设置AHB预分频器
  RCC_HCLKConfig(RCC_SYSCLK_Div1);
  // 设置APB1预分频器
  RCC_PCLK1Config(RCC_HCLK_Div2);
  // 设置APB2预分频器
  RCC_PCLK2Config(RCC_HCLK_Div1);
  
  // 配置Flash等待周期
  FLASH_SetLatency(FLASH_Latency_2);
}
