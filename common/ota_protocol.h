#ifndef __OTA_PROTOCOL_H
#define __OTA_PROTOCOL_H

#include <stdint.h>

#define OTA_MAGIC_HEADER        0x4F54414F
#define OTA_VERSION             0x0001
#define OTA_MAX_PACKET_SIZE     1024
#define OTA_PACKET_HEADER_SIZE  16

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
    uint32_t magic;
    uint16_t version;
    uint16_t packet_size;
    uint32_t total_size;
    uint32_t crc32;
} OTA_Packet_Header_t;

typedef struct {
    OTA_Command_t cmd;
    uint32_t offset;
    uint16_t length;
    uint8_t data[OTA_MAX_PACKET_SIZE];
} OTA_Packet_t;

typedef struct {
    uint32_t app_start_addr;
    uint32_t app_size;
    uint32_t crc32;
    uint32_t version;
} OTA_App_Info_t;

#endif
