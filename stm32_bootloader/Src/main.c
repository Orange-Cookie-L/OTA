#include "main.h"
#include "flash.h"
#include "uart_comm.h"
#include "ota_protocol.h"
#include "crc32.h"

#define BOOTLOADER_VERSION    0x0001
#define APP_START_ADDR        0x08010000
#define TEMP_ADDR             0x08100000
#define TIMEOUT_MS            5000
#define MAX_RETRY             3

static OTA_App_Info_t app_info;
static uint32_t firmware_size = 0;
static uint32_t received_bytes = 0;

void Bootloader_Init(void)
{
    HAL_Init();
    SystemClock_Config();
    UART_Init();
    Flash_Init();
}

uint8_t Bootloader_CheckUpdate(void)
{
    OTA_Packet_t packet;
    uint32_t timeout;

    UART_SendByte(OTA_CMD_QUERY_VERSION);

    timeout = HAL_GetTick() + TIMEOUT_MS;
    while (HAL_GetTick() < timeout) {
        if (UART_ReceivePacket(&packet, 100) == HAL_OK) {
            if (packet.cmd == OTA_CMD_VERSION_RESP) {
                return 1;
            }
        }
    }
    return 0;
}

uint8_t Bootloader_StartUpdate(void)
{
    OTA_Packet_t packet;
    uint8_t retry = 0;

    packet.cmd = OTA_CMD_START;
    packet.offset = 0;
    packet.length = 0;

    while (retry < MAX_RETRY) {
        UART_SendPacket(&packet);

        if (UART_ReceivePacket(&packet, TIMEOUT_MS) == HAL_OK) {
            if (packet.cmd == OTA_CMD_ACK) {
                firmware_size = *(uint32_t*)packet.data;
                received_bytes = 0;
                return 1;
            }
        }
        retry++;
        HAL_Delay(1000);
    }
    return 0;
}

uint8_t Bootloader_ReceiveFirmware(void)
{
    OTA_Packet_t packet;
    uint32_t write_addr = TEMP_ADDR;
    uint32_t crc = 0xFFFFFFFF;
    uint8_t retry;

    Flash_EraseSector(TEMP_ADDR, firmware_size);

    while (received_bytes < firmware_size) {
        retry = 0;
        while (retry < MAX_RETRY) {
            if (UART_ReceivePacket(&packet, TIMEOUT_MS) == HAL_OK) {
                if (packet.cmd == OTA_CMD_DATA) {
                    if (Flash_Write(write_addr, packet.data, packet.length) == HAL_OK) {
                        crc = CRC32_Calculate(packet.data, packet.length, crc);
                        write_addr += packet.length;
                        received_bytes += packet.length;

                        packet.cmd = OTA_CMD_ACK;
                        UART_SendPacket(&packet);
                        break;
                    }
                }
            }
            retry++;
            packet.cmd = OTA_CMD_NACK;
            UART_SendPacket(&packet);
        }
        if (retry >= MAX_RETRY) {
            return 0;
        }
    }

    return 1;
}

uint8_t Bootloader_VerifyFirmware(void)
{
    OTA_Packet_t packet;
    uint32_t server_crc;
    uint32_t local_crc;

    packet.cmd = OTA_CMD_END;
    UART_SendPacket(&packet);

    if (UART_ReceivePacket(&packet, TIMEOUT_MS) == HAL_OK) {
        if (packet.cmd == OTA_CMD_ACK) {
            server_crc = *(uint32_t*)packet.data;
            local_crc = CRC32_CalculateFlash(TEMP_ADDR, firmware_size);

            if (server_crc == local_crc) {
                return 1;
            }
        }
    }
    return 0;
}

void Bootloader_CopyFirmware(void)
{
    uint32_t src_addr = TEMP_ADDR;
    uint32_t dst_addr = APP_START_ADDR;
    uint8_t buffer[256];
    uint32_t offset = 0;

    Flash_EraseSector(APP_START_ADDR, firmware_size);

    while (offset < firmware_size) {
        uint16_t chunk_size = (firmware_size - offset) > 256 ? 256 : (firmware_size - offset);
        Flash_Read(src_addr + offset, buffer, chunk_size);
        Flash_Write(dst_addr + offset, buffer, chunk_size);
        offset += chunk_size;
    }
}

void Bootloader_JumpToApp(void)
{
    typedef void (*pFunction)(void);
    pFunction jump_to_app;

    __disable_irq();
    SCB->VTOR = APP_START_ADDR;

    jump_to_app = (pFunction)(*(__IO uint32_t*)(APP_START_ADDR + 4));
    __set_MSP(*(__IO uint32_t*)APP_START_ADDR);

    jump_to_app();
}

uint8_t Bootloader_CheckAppValid(void)
{
    uint32_t stack_ptr = *(__IO uint32_t*)APP_START_ADDR;
    uint32_t reset_ptr = *(__IO uint32_t*)(APP_START_ADDR + 4);

    if ((stack_ptr & 0xFFFF0000) != 0x20000000) {
        return 0;
    }

    if (reset_ptr < APP_START_ADDR || reset_ptr > 0x08100000) {
        return 0;
    }

    return 1;
}

int main(void)
{
    Bootloader_Init();

    if (Bootloader_CheckUpdate()) {
        if (Bootloader_StartUpdate()) {
            if (Bootloader_ReceiveFirmware()) {
                if (Bootloader_VerifyFirmware()) {
                    Bootloader_CopyFirmware();
                    Bootloader_JumpToApp();
                }
            }
        }
    }

    if (Bootloader_CheckAppValid()) {
        Bootloader_JumpToApp();
    }

    while (1) {
        HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
        HAL_Delay(500);
    }
}
