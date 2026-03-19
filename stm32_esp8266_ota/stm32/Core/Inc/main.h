/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
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

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f10x.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

// 串口配置
#define UART_BAUD_RATE           115200

// LED配置
#define LED_PORT                 GPIOC
#define LED_PIN                  GPIO_Pin_13

// 继电器配置
#define RELAY_PORT               GPIOB
#define RELAY_PIN                GPIO_Pin_0

// 设备信息
#define DEVICE_ID                "STM32F103C8T6"
#define DEVICE_TYPE              "SmartDevice"
#define CURRENT_VERSION          "1.0.0"

// 命令定义
#define CMD_REGISTER             "REGISTER,"
#define CMD_HEARTBEAT            "HEARTBEAT"
#define CMD_CHECK_UPDATE         "CHECK_UPDATE"
#define CMD_DOWNLOAD_FIRMWARE    "DOWNLOAD_FIRMWARE,"

// 响应定义
#define RESP_OK                  "OK"
#define RESP_ERROR               "ERROR"
#define RESP_UPDATE_AVAILABLE    "UPDATE_AVAILABLE"
#define RESP_NO_UPDATE           "NO_UPDATE"

// 时间间隔
#define LED_TOGGLE_INTERVAL      1000  // 1秒
#define HEARTBEAT_INTERVAL       30000 // 30秒
#define CHECK_UPDATE_INTERVAL    60000 // 60秒

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART1_UART_Init(void);

/* USER CODE BEGIN EFP */

// 串口发送函数
void send_uart_command(const char* command);

// LED控制函数
void toggle_led(void);
void handleFirmwareDownloadLED(void);

// 处理ESP8266响应
void process_esp_response(char* response);

// 处理固件数据
void process_firmware_data(uint8_t data);

// 系统函数
uint32_t GetTick(void);
void delay_ms(uint32_t ms);

/* USER CODE END EFP */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
