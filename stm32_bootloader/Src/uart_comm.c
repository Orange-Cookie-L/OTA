#include "uart_comm.h"

UART_HandleTypeDef huart1;

void UART_Init(void)
{
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

HAL_StatusTypeDef UART_SendByte(uint8_t byte)
{
    return HAL_UART_Transmit(&huart1, &byte, 1, 1000);
}

HAL_StatusTypeDef UART_SendPacket(OTA_Packet_t *packet)
{
    uint8_t header[7];

    header[0] = packet->cmd;
    header[1] = (packet->offset >> 24) & 0xFF;
    header[2] = (packet->offset >> 16) & 0xFF;
    header[3] = (packet->offset >> 8) & 0xFF;
    header[4] = packet->offset & 0xFF;
    header[5] = (packet->length >> 8) & 0xFF;
    header[6] = packet->length & 0xFF;

    HAL_UART_Transmit(&huart1, header, 7, 1000);

    if (packet->length > 0) {
        HAL_UART_Transmit(&huart1, packet->data, packet->length, 1000);
    }

    return HAL_OK;
}

HAL_StatusTypeDef UART_ReceivePacket(OTA_Packet_t *packet, uint32_t timeout)
{
    uint8_t header[7];
    HAL_StatusTypeDef status;

    status = HAL_UART_Receive(&huart1, header, 7, timeout);
    if (status != HAL_OK) {
        return status;
    }

    packet->cmd = header[0];
    packet->offset = (header[1] << 24) | (header[2] << 16) | 
                   (header[3] << 8) | header[4];
    packet->length = (header[5] << 8) | header[6];

    if (packet->length > 0) {
        status = HAL_UART_Receive(&huart1, packet->data, packet->length, timeout);
        if (status != HAL_OK) {
            return status;
        }
    }

    return HAL_OK;
}
