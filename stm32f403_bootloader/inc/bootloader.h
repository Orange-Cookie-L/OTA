#ifndef __BOOTLOADER_H
#define __BOOTLOADER_H

#include <stdint.h>
#include "ota_protocol.h"

uint8_t UART_ReceivePacket(void);
uint8_t ProcessPacket(OTA_Packet_t *packet);
void SendPacket(OTA_Packet_t *packet);
void jump_to_application(void);

#endif
