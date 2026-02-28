#ifndef __OTA_PROTOCOL_H
#define __OTA_PROTOCOL_H

#include <stdint.h>

#define OTA_MAGIC_HEADER        0x4F54414F
#define OTA_VERSION             0x0001
#define OTA_MAX_PACKET_SIZE     1024
#define OTA_PACKET_HEADER_SIZE  7

typedef enum {
    OTA_CMD_START = 0x01,
    OTA_CMD_DATA = 0x02,
    OTA_CMD_END = 0x03,
    OTA_CMD_ACK = 0x04,
    OTA_CMD_NACK = 0x05,
    OTA_CMD_QUERY_VERSION = 0x06,
    OTA_CMD_VERSION_RESP = 0x07
} OTA_Command_t;

typedef struct {
    uint8_t cmd;
    uint32_t offset;
    uint16_t length;
    uint8_t data[OTA_MAX_PACKET_SIZE];
} OTA_Packet_t;

#endif
