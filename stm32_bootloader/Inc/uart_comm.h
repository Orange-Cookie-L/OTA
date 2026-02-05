#ifndef __UART_COMM_H
#define __UART_COMM_H

#include "stm32f4xx_hal.h"
#include "ota_protocol.h"

void UART_Init(void);
HAL_StatusTypeDef UART_SendByte(uint8_t byte);
HAL_StatusTypeDef UART_SendPacket(OTA_Packet_t *packet);
HAL_StatusTypeDef UART_ReceivePacket(OTA_Packet_t *packet, uint32_t timeout);

#endif
