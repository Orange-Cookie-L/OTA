#include "stm32f4xx_hal.h"
#include "bootloader.h"
#include "ota_protocol.h"
#include "flash.h"

#define BOOTLOADER_VERSION    0x0001
#define APP_START_ADDR        0x08010000
#define APP_SIZE_MAX          0x000F0000
#define TIMEOUT_MS            30000

UART_HandleTypeDef huart1;
uint8_t rx_buffer[OTA_PACKET_HEADER_SIZE + OTA_MAX_PACKET_SIZE];
uint32_t firmware_size = 0;
uint32_t bytes_written = 0;
uint32_t last_activity_time = 0;
uint8_t update_in_progress = 0;

void SystemClock_Config(void);
void GPIO_Init(void);
void UART_Init(void);
void jump_to_application(void);

int main(void) {
    HAL_Init();
    SystemClock_Config();
    GPIO_Init();
    UART_Init();

    last_activity_time = HAL_GetTick();

    while (1) {
        if (UART_ReceivePacket()) {
            last_activity_time = HAL_GetTick();
        }

        if (!update_in_progress && 
            (HAL_GetTick() - last_activity_time > TIMEOUT_MS)) {
            if (Flash_VerifyApplication()) {
                jump_to_application();
            }
        }
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
    OTA_Packet_t response;

    switch (packet->cmd) {
        case OTA_CMD_START:
            if (packet->offset <= APP_SIZE_MAX) {
                firmware_size = packet->offset;
                bytes_written = 0;
                update_in_progress = 1;

                if (Flash_EraseApplication()) {
                    response.cmd = OTA_CMD_ACK;
                    response.offset = firmware_size;
                    response.length = 0;
                    SendPacket(&response);
                    return 1;
                }
            }
            response.cmd = OTA_CMD_NACK;
            response.offset = 0;
            response.length = 0;
            SendPacket(&response);
            return 0;

        case OTA_CMD_DATA:
            if (update_in_progress) {
                if (Flash_WriteData(APP_START_ADDR + packet->offset, 
                                    packet->data, packet->length)) {
                    bytes_written += packet->length;
                    response.cmd = OTA_CMD_ACK;
                    response.offset = bytes_written;
                    response.length = 0;
                    SendPacket(&response);
                    return 1;
                }
            }
            response.cmd = OTA_CMD_NACK;
            response.offset = bytes_written;
            response.length = 0;
            SendPacket(&response);
            return 0;

        case OTA_CMD_END:
            if (update_in_progress && bytes_written == firmware_size) {
                uint32_t received_crc;
                memcpy(&received_crc, packet->data, 4);

                uint32_t calculated_crc = Flash_CalculateCRC(APP_START_ADDR, firmware_size);

                if (received_crc == calculated_crc) {
                    Flash_SetValidFlag();
                    response.cmd = OTA_CMD_ACK;
                    response.offset = 0;
                    response.length = 0;
                    SendPacket(&response);
                    update_in_progress = 0;
                    return 1;
                }
            }
            response.cmd = OTA_CMD_NACK;
            response.offset = 0;
            response.length = 0;
            SendPacket(&response);
            return 0;

        case OTA_CMD_QUERY_VERSION:
            response.cmd = OTA_CMD_VERSION_RESP;
            response.offset = BOOTLOADER_VERSION;
            response.length = 0;
            SendPacket(&response);
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

void jump_to_application(void) {
    uint32_t app_stack_ptr = *((uint32_t *)APP_START_ADDR);
    uint32_t app_reset_handler = *((uint32_t *)(APP_START_ADDR + 4));

    __disable_irq();

    SCB->VTOR = APP_START_ADDR;

    __set_MSP(app_stack_ptr);

    void (*app_reset)(void) = (void (*)(void))app_reset_handler;
    app_reset();
}
