#include "stm32f4xx_hal.h"
#include "bootloader.h"
#include "ota_protocol.h"
#include "flash.h"

#define APP_VERSION            0x0001
#define APP_VERSION_STR        "1.0.0"
#define BOOTLOADER_MAGIC_ADDR  0x0800FFF8
#define BOOTLOADER_MAGIC_VALUE 0x5A5A5A5A
#define LED_PIN                GPIO_PIN_13
#define LED_PORT               GPIOC

UART_HandleTypeDef huart1;
uint8_t rx_buffer[OTA_PACKET_HEADER_SIZE + OTA_MAX_PACKET_SIZE];

uint8_t ota_requested = 0;
uint32_t last_heartbeat = 0;

void SystemClock_Config(void);
void GPIO_Init(void);
void UART_Init(void);
void request_ota_update(void);
void send_heartbeat(void);
void toggle_led(void);

int main(void) {
    HAL_Init();
    SystemClock_Config();
    GPIO_Init();
    UART_Init();

    while (1) {
        if (UART_ReceivePacket()) {
            OTA_Packet_t response;
            response.cmd = OTA_CMD_ACK;
            response.offset = 0;
            response.length = 0;
            SendPacket(&response);
        }

        if (ota_requested) {
            request_ota_update();
            ota_requested = 0;
        }

        toggle_led();
        HAL_Delay(1000);
    }
}

void SystemClock_Config(void) {
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLM = 8;
    RCC_OscInitStruct.PLL.PLLN = 168;
    RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
    RCC_OscInitStruct.PLL.PLLQ = 4;
    HAL_RCC_OscConfig(&RCC_OscInitStruct);

    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                                  |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;
    HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5);
}

void GPIO_Init(void) {
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();

    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = LED_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(LED_PORT, &GPIO_InitStruct);
}

void toggle_led(void) {
    HAL_GPIO_TogglePin(LED_PORT, LED_PIN);
}

void UART_Init(void) {
    __HAL_RCC_USART1_CLK_ENABLE();

    huart1.Instance = USART1;
    huart1.Init.BaudRate = 115200;
    huart1.Init.WordLength = UART_WORDLENGTH_8B;
    huart1.Init.StopBits = UART_STOPBITS_1;
    huart1.Init.Parity = UART_PARITY_NONE;
    huart1.Init.Mode = UART_MODE_TX_RX;
    huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart1.Init.OverSampling = UART_OVERSAMPLING_16;
    HAL_UART_Init(&huart1);
}

uint8_t UART_ReceivePacket(void) {
    OTA_Packet_t packet;
    uint8_t header[OTA_PACKET_HEADER_SIZE];
    uint16_t total_length;

    if (HAL_UART_Receive(&huart1, header, OTA_PACKET_HEADER_SIZE, 100) != HAL_OK) {
        return 0;
    }

    packet.cmd = header[0];
    packet.offset = (header[1] << 24) | (header[2] << 16) | 
                    (header[3] << 8) | header[4];
    packet.length = (header[5] << 8) | header[6];

    if (packet.length > OTA_MAX_PACKET_SIZE) {
        return 0;
    }

    if (packet.length > 0) {
        if (HAL_UART_Receive(&huart1, packet.data, packet.length, 1000) != HAL_OK) {
            return 0;
        }
    }

    return ProcessPacket(&packet);
}

uint8_t ProcessPacket(OTA_Packet_t *packet) {
    switch (packet->cmd) {
        case OTA_CMD_START:
            ota_requested = 1;
            return 1;

        case OTA_CMD_QUERY_VERSION:
            {
                OTA_Packet_t response;
                response.cmd = OTA_CMD_VERSION_RESP;
                response.offset = APP_VERSION;
                response.length = 0;
                SendPacket(&response);
            }
            return 1;

        default:
            return 0;
    }
}

void SendPacket(OTA_Packet_t *packet) {
    uint8_t buffer[OTA_PACKET_HEADER_SIZE + OTA_MAX_PACKET_SIZE];
    uint16_t offset = 0;

    buffer[offset++] = packet->cmd;
    buffer[offset++] = (packet->offset >> 24) & 0xFF;
    buffer[offset++] = (packet->offset >> 16) & 0xFF;
    buffer[offset++] = (packet->offset >> 8) & 0xFF;
    buffer[offset++] = packet->offset & 0xFF;
    buffer[offset++] = (packet->length >> 8) & 0xFF;
    buffer[offset++] = packet->length & 0xFF;

    if (packet->length > 0) {
        memcpy(&buffer[offset], packet->data, packet->length);
        offset += packet->length;
    }

    HAL_UART_Transmit(&huart1, buffer, offset, 1000);
}

void request_ota_update(void) {
    HAL_FLASH_Unlock();

    HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, BOOTLOADER_MAGIC_ADDR, 
                      BOOTLOADER_MAGIC_VALUE);

    HAL_FLASH_Lock();

    NVIC_SystemReset();
}
