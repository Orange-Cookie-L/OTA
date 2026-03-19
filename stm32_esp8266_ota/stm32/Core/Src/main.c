/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
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
/* Includes ------------------------------------------------------------------*/
#include "stm32f10x.h"   // STM32F10x标准库头文件
#include "main.h"        // 主头文件
#include "../bootloader/Inc/bootloader.h"   // Bootloader头文件
#include "string.h"      // 标准字符串库

/* OTA 配置 */
#define DEVICE_ID "STM32F103C8T6_001"           // 设备唯一标识符
#define DEVICE_TYPE "stm32f103c8t6"             // 设备类型
#define CURRENT_VERSION "1.0.0"                 // 当前固件版本

#define UART_BAUD_RATE 115200                    // 串口波特率

#define LED_PIN GPIO_Pin_13                      // LED引脚
#define LED_PORT GPIOC                           // LED端口
#define LED_TOGGLE_INTERVAL 1000                 // LED闪烁间隔（毫秒）

#define HEARTBEAT_INTERVAL 30000                // 心跳包间隔（毫秒）
#define CHECK_UPDATE_INTERVAL 60000             // 检查更新间隔（毫秒）

#define CMD_REGISTER "REGISTER:"                // 注册命令
#define CMD_HEARTBEAT "HEARTBEAT"              // 心跳命令
#define CMD_CHECK_UPDATE "CHECK_UPDATE"        // 检查更新命令

#define RESP_UPDATE_AVAILABLE "UPDATE_AVAILABLE" // 更新可用响应

// 应用程序起始地址
#define APPLICATION_START_ADDRESS 0x08004000

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
uint8_t uart_rx_buffer[256];      // 串口接收缓冲区
uint16_t uart_rx_index = 0;        // 串口接收缓冲区索引

uint32_t last_led_toggle_time = 0;   // 上次LED闪烁时间
uint32_t last_heartbeat_time = 0;     // 上次心跳包发送时间
uint32_t last_check_update_time = 0;  // 上次检查更新时间

// 固件下载状态
bool firmware_downloading = false;    // 固件是否正在下载
uint32_t firmware_size = 0;           // 固件大小
uint32_t firmware_offset = 0;         // 固件写入偏移量
uint8_t firmware_buffer[64];          // 固件数据缓冲区
uint16_t buffer_index = 0;            // 缓冲区索引
uint32_t last_led_blink_time = 0;     // 上次LED闪烁时间（用于下载时的快速闪烁）

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART1_UART_Init(void);
/* USER CODE BEGIN PFP */
/**
  * @brief 发送串口命令
  * @param command: 要发送的命令字符串
  * @retval None
  */
void send_uart_command(const char* command);

/**
  * @brief 处理ESP8266响应
  * @param response: ESP8266发送的响应字符串
  * @retval None
  */
void process_esp_response(const char* response);

/**
  * @brief 切换LED状态
  * @retval None
  */
void toggle_led(void);

/**
  * @brief 处理固件下载中的LED闪烁
  * @retval None
  */
void handleFirmwareDownloadLED();

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

// 系统时钟计数器
static uint32_t system_tick = 0;

/**
  * @brief 获取系统 tick
  * @retval 当前系统 tick 值
  */
uint32_t GetTick(void)
{
  return system_tick;
}

/**
  * @brief 延时函数（毫秒）
  * @param ms: 延时毫秒数
  * @retval None
  */
void delay_ms(uint32_t ms)
{
  uint32_t start = GetTick();
  while ((GetTick() - start) < ms);
}

/**
  * @brief SysTick 中断处理函数
  * @retval None
  */
void SysTick_Handler(void)
{
  system_tick++;
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */
  
  // 初始化串口
  USART_InitTypeDef USART_InitStructure;
  USART_InitStructure.USART_BaudRate = UART_BAUD_RATE;
  USART_InitStructure.USART_WordLength = USART_WordLength_8b;
  USART_InitStructure.USART_StopBits = USART_StopBits_1;
  USART_InitStructure.USART_Parity = USART_Parity_No;
  USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
  USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
  USART_Init(USART1, &USART_InitStructure);
  USART_Cmd(USART1, ENABLE);
  USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);
  
  // 初始化LED（关闭）
  GPIO_ResetBits(LED_PORT, LED_PIN);
  
  // 注册设备到OTA系统
  char register_cmd[100];
  sprintf(register_cmd, "%s%s,%s,%s\n", CMD_REGISTER, DEVICE_ID, DEVICE_TYPE, CURRENT_VERSION);
  send_uart_command(register_cmd);
  
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    // 处理固件下载中的LED闪烁（快速闪烁）
    handleFirmwareDownloadLED();
    
    // 闪灯（仅在固件未下载时，正常闪烁）
    if (!firmware_downloading && GetTick() - last_led_toggle_time > LED_TOGGLE_INTERVAL) {
      toggle_led();
      last_led_toggle_time = GetTick();
    }
    
    // 发送心跳包（保持在线状态）
    if (GetTick() - last_heartbeat_time > HEARTBEAT_INTERVAL) {
      send_uart_command(CMD_HEARTBEAT "\n");
      last_heartbeat_time = GetTick();
    }
    
    // 检查更新（获取最新固件）
    if (GetTick() - last_check_update_time > CHECK_UPDATE_INTERVAL) {
      send_uart_command(CMD_CHECK_UPDATE "\n");
      last_check_update_time = GetTick();
    }
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
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

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{
  // 使能USART1时钟
  RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1, ENABLE);
  
  // 配置USART1
  USART_InitTypeDef USART_InitStructure;
  USART_InitStructure.USART_BaudRate = UART_BAUD_RATE;
  USART_InitStructure.USART_WordLength = USART_WordLength_8b;
  USART_InitStructure.USART_StopBits = USART_StopBits_1;
  USART_InitStructure.USART_Parity = USART_Parity_No;
  USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
  USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
  USART_Init(USART1, &USART_InitStructure);
  
  // 使能USART1
  USART_Cmd(USART1, ENABLE);
  
  // 使能USART1接收中断
  USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);
  
  // 配置NVIC
  NVIC_InitTypeDef NVIC_InitStructure;
  NVIC_InitStructure.NVIC_IRQChannel = USART1_IRQn;
  NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
  NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
  NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
  NVIC_Init(&NVIC_InitStructure);
}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  // 使能GPIOC时钟
  RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);
  // 使能GPIOA时钟
  RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
  // 使能GPIOB时钟
  RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
  
  // 配置LED引脚
  GPIO_InitTypeDef GPIO_InitStructure;
  GPIO_InitStructure.GPIO_Pin = LED_PIN;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
  GPIO_Init(LED_PORT, &GPIO_InitStructure);
  
  // 配置RELAY引脚
  GPIO_InitStructure.GPIO_Pin = RELAY_PIN;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
  GPIO_Init(RELAY_PORT, &GPIO_InitStructure);
  
  // 初始化LED和RELAY状态（关闭）
  GPIO_ResetBits(LED_PORT, LED_PIN);
  GPIO_ResetBits(RELAY_PORT, RELAY_PIN);
}

/* USER CODE BEGIN 4 */

/**
  * @brief 发送串口命令
  * @param command: 要发送的命令字符串
  * @retval None
  */
void send_uart_command(const char* command)
{
  // 通过串口1发送命令
  uint16_t len = strlen(command);
  for (uint16_t i = 0; i < len; i++) {
    while (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET);
    USART_SendData(USART1, command[i]);
  }
  while (USART_GetFlagStatus(USART1, USART_FLAG_TC) == RESET);
}

/**
  * @brief 处理ESP8266响应
  * @param response: ESP8266发送的响应字符串
  * @retval None
  */
// 更新标志位
bool update_available = false; // 是否有更新可用

/**
  * @brief 写入闪存页
  * @param address: 闪存地址偏移量
  * @param data: 数据
  * @param size: 大小
  * @retval None
  */
void write_flash_page(uint32_t address, uint8_t* data, uint32_t size)
{
  // 解锁闪存
  HAL_FLASH_Unlock();
  
  // 擦除页
  FLASH_EraseInitTypeDef erase_init;
  erase_init.TypeErase = FLASH_TYPEERASE_PAGES;
  erase_init.PageAddress = APPLICATION_START_ADDRESS + address;
  erase_init.NbPages = 1;
  
  uint32_t page_error = 0;
  HAL_FLASHEx_Erase(&erase_init, &page_error);
  
  // 写入数据
  for (uint32_t i = 0; i < size; i += 4) {
    uint32_t word_data = *((uint32_t*)&data[i]);
    HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, APPLICATION_START_ADDRESS + address + i, word_data);
  }
  
  // 锁定闪存
  HAL_FLASH_Lock();
}

void process_esp_response(const char* response)
{
  // 检查是否有更新可用（使用标志位）
  if (strcmp(response, RESP_UPDATE_AVAILABLE) == 0) {
    // 设置更新标志位
    update_available = true;
    // 开始固件下载
    firmware_downloading = true;
    // 重置下载状态
    firmware_size = 0;
    firmware_offset = 0;
    buffer_index = 0;
    Serial.println("开始下载固件...");
  } 
  // 检查是否接收到固件大小
  else if (strstr(response, "FIRMWARE_SIZE,") != NULL) {
    // 解析固件大小
    char* size_str = strchr(response, ',') + 1;
    firmware_size = atol(size_str);
    Serial.print("固件大小: ");
    Serial.println(firmware_size);
    // 确认接收
    Serial.write('A');
  }
  // 检查是否下载完成
  else if (strcmp(response, "DOWNLOAD_COMPLETE") == 0) {
    // 清除更新标志位
    update_available = false;
    // 固件下载完成
    firmware_downloading = false;
    Serial.println("固件下载完成!");
    // 闪烁LED表示完成（3次）
    for (int i = 0; i < 3; i++) {
      GPIO_SetBits(LED_PORT, LED_PIN);
      delay_ms(200);
      GPIO_ResetBits(LED_PORT, LED_PIN);
      delay_ms(200);
    }
    // 重启设备
    NVIC_SystemReset();
  } 
  // 检查是否下载失败
  else if (strcmp(response, "DOWNLOAD_FAILED") == 0) {
    // 清除更新标志位
    update_available = false;
    // 固件下载失败
    firmware_downloading = false;
    Serial.println("固件下载失败!");
    // 快速闪烁LED表示失败（5次）
    for (int i = 0; i < 5; i++) {
      GPIO_SetBits(LED_PORT, LED_PIN);
      delay_ms(100);
      GPIO_ResetBits(LED_PORT, LED_PIN);
      delay_ms(100);
    }
  }
}

/**
  * @brief 处理固件数据
  * @param data: 固件数据
  * @retval None
  */
void process_firmware_data(uint8_t data)
{
  if (firmware_downloading) {
    // 存储数据到缓冲区
    firmware_buffer[buffer_index++] = data;
    
    // 当缓冲区满时，写入闪存
    if (buffer_index >= 64) {
      write_flash_page(firmware_offset, firmware_buffer, 64);
      firmware_offset += 64;
      buffer_index = 0;
      
      // 发送确认
      Serial.write('A');
    }
  }
}

/**
  * @brief 处理固件下载中的LED闪烁
  * @retval None
  */
void handleFirmwareDownloadLED() {
  // 当固件正在下载时，快速闪烁LED
  if (firmware_downloading) {
    static uint32_t last_time = 0;
    if ((uint32_t)GetTick() - last_time > 100) { // 100ms闪烁一次
      if (GPIO_ReadInputDataBit(LED_PORT, LED_PIN)) {
        GPIO_ResetBits(LED_PORT, LED_PIN);
      } else {
        GPIO_SetBits(LED_PORT, LED_PIN);
      }
      last_time = (uint32_t)GetTick();
    }
  }
}

/**
  * @brief 切换LED状态
  * @retval None
  */
void toggle_led(void)
{
  // 切换LED的状态（开->关 或 关->开）
  if (GPIO_ReadInputDataBit(LED_PORT, LED_PIN)) {
    GPIO_ResetBits(LED_PORT, LED_PIN);
  } else {
    GPIO_SetBits(LED_PORT, LED_PIN);
  }
}

/**
  * @brief UART接收中断回调函数
  * @param huart: UART句柄
  * @retval None
  */
// UART1接收中断处理函数
void USART1_IRQHandler(void)
{
  if (USART_GetITStatus(USART1, USART_IT_RXNE) != RESET) {
    uint8_t data = USART_ReceiveData(USART1);
    
    // 如果正在下载固件，直接处理数据
    if (firmware_downloading) {
      process_firmware_data(data);
    } else {
      // 检查是否接收到换行符（命令结束）
      if (data == '\n') {
        // 接收完成，处理数据
        uart_rx_buffer[uart_rx_index] = '\0'; // 添加字符串结束符
        process_esp_response((char*)uart_rx_buffer); // 处理响应
        uart_rx_index = 0; // 重置缓冲区索引
      } else {
        // 继续接收
        uart_rx_buffer[uart_rx_index] = data;
        uart_rx_index++;
        // 防止缓冲区溢出
        if (uart_rx_index >= sizeof(uart_rx_buffer) - 1) {
          uart_rx_index = 0;
        }
      }
    }
    
    // 清除中断标志位
    USART_ClearITPendingBit(USART1, USART_IT_RXNE);
  }
}

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,*/
  /* ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/  
