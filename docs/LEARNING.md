# STM32F403 + ESP8266 远程升级系统 - 新手学习指南

> 本指南专为初学者设计，假设你已经熟悉 STM32F103 的基础知识

## 目录

1. [前置知识准备](#前置知识准备)
2. [系统架构概览](#系统架构概览)
3. [STM32F103 vs STM32F403 的区别](#stm32f103-vs-stm32f403-的区别)
4. [OTA 协议详解](#ota-协议详解)
5. [Bootloader 深入学习](#bootloader-深入学习)
6. [Flash 操作详解](#flash-操作详解)
7. [UART 通信详解](#uart-通信详解)
8. [CRC32 校验详解](#crc32-校验详解)
9. [ESP8266 固件详解](#esp8266-固件详解)
10. [Flask 服务器详解](#flask-服务器详解)
11. [实践步骤](#实践步骤)

---

## 前置知识准备

### 你需要了解的 STM32F103 知识

既然你熟悉 STM32F103，你已经掌握了以下知识：

✅ **HAL 库基础**
- HAL_Init() - 初始化 HAL 库
- HAL_GPIO_Init() - GPIO 初始化
- HAL_UART_Init() - UART 初始化
- HAL_Delay() - 延时函数

✅ **GPIO 基础**
- 输入/输出模式配置
- GPIO 读写操作
- LED 闪烁控制

✅ **UART 通信**
- 串口初始化
- 发送/接收数据
- 中断处理

✅ **Flash 基础**
- Flash 读写操作
- 扇区擦除

### STM32F403 新增知识

STM32F403 相比 F103 有以下主要区别：

| 特性 | STM32F103 | STM32F403 |
|------|-----------|-----------|
| 内核 | Cortex-M3 | Cortex-M4 |
| 主频 | 72MHz | 168MHz |
| Flash 大小 | 最大 512KB | 最大 1MB |
| Flash 扇区 | 1KB 或 2KB | 16KB 或 128KB |
| 浮点运算 | 无 | 硬件支持 |
| DSP 指令 | 无 | 支持 |

**好消息**：HAL 库的 API 基本相同，你只需要学习 Flash 操作的差异！

---

## 系统架构概览

### 整体架构

```
┌─────────────────────────────────────────────────────────────┐
│                   云端服务器                          │
│              (Flask + Python)                         │
│                                                      │
│  - 固件存储                                          │
│  - 设备管理                                          │
│  - 推送更新                                          │
└──────────────────────┬──────────────────────────────────┘
                       │ HTTP
                       │
┌──────────────────────▼──────────────────────────────────┐
│                  ESP8266 模块                        │
│                                                      │
│  - WiFi 连接                                        │
│  - HTTP 请求                                        │
│  - 固件下载                                        │
│  - UART 转发                                       │
└──────────────────────┬──────────────────────────────────┘
                       │ UART (115200)
                       │
┌──────────────────────▼──────────────────────────────────┐
│                STM32F403 MCU                         │
│                                                      │
│  ┌─────────────────────────────────────┐               │
│  │      Bootloader (64KB)          │               │
│  │  - 检查更新                       │               │
│  │  - 接收固件                       │               │
│  │  - 写入 Flash                     │               │
│  └─────────────────────────────────────┘               │
│  ┌─────────────────────────────────────┐               │
│  │    应用程序 (960KB)            │               │
│  │  - 业务逻辑                       │               │
│  │  - 检查更新                       │               │
│  │  - 跳转到 Bootloader              │               │
│  └─────────────────────────────────────┘               │
└─────────────────────────────────────────────────────────────┘
```

### 内存布局详解

```
STM32F403 Flash 地址分布：
┌─────────────────────────────────────────────────────────┐
│ 0x08000000                                     │
│ ┌─────────────────────────────────────────────┐         │
│ │  Bootloader 区域 (64KB)                  │         │
│ │  - 启动地址                                 │         │
│ │  - OTA 更新逻辑                             │         │
│ │  - Flash 操作                               │         │
│ └─────────────────────────────────────────────┘         │
│ 0x08010000                                     │
│ ┌─────────────────────────────────────────────┐         │
│ │  应用程序区域 (960KB)                 │         │
│ │  - 主业务逻辑                             │         │
│ │  - 用户功能                               │         │
│ └─────────────────────────────────────────────┘         │
│ 0x08100000                                     │
│ ┌─────────────────────────────────────────────┐         │
│ │  临时存储区 (256KB)                  │         │
│ │  - 临时存储下载的固件                     │         │
│ │  - 验证后复制到应用程序区                   │         │
│ └─────────────────────────────────────────────┘         │
│ 0x08140000                                     │
└─────────────────────────────────────────────────────────┘
```

**为什么需要临时存储区？**
- 防止升级失败导致设备变砖
- 先下载到临时区，验证通过后再复制
- 提供回滚机制

---

## STM32F103 vs STM32F403 的区别

### 1. Flash 扇区大小

**STM32F103 扇区布局：**
```
扇区 0: 0x08000000 - 0x080003FF (1KB)
扇区 1: 0x08000400 - 0x080007FF (1KB)
扇区 2: 0x08000800 - 0x08000BFF (1KB)
...
```

**STM32F403 扇区布局：**
```
扇区 0: 0x08000000 - 0x08003FFF (16KB)
扇区 1: 0x08004000 - 0x08007FFF (16KB)
扇区 2: 0x08008000 - 0x0800BFFF (16KB)
扇区 3: 0x0800C000 - 0x0800FFFF (16KB)
扇区 4: 0x08010000 - 0x0801FFFF (128KB)
...
```

### 2. Flash 操作 API 差异

**STM32F103 擦除：**
```c
FLASH_EraseInitTypeDef erase_init;
erase_init.TypeErase = FLASH_TYPEERASE_PAGES;
erase_init.PageAddress = page_addr;
erase_init.NbPages = 1;
HAL_FLASHEx_Erase(&erase_init, &error);
```

**STM32F403 擦除：**
```c
FLASH_EraseInitTypeDef erase_init;
erase_init.TypeErase = FLASH_TYPEERASE_SECTORS;
erase_init.Sector = FLASH_SECTOR_4;
erase_init.NbSectors = 1;
HAL_FLASHEx_Erase(&erase_init, &error);
```

**关键区别：**
- F103 使用 `PAGE`（页）
- F403 使用 `SECTOR`（扇区）

### 3. 时钟配置

**STM32F103：**
```c
RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
RCC_OscInitStruct.HSEState = RCC_HSE_ON;
RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;  // 8MHz * 9 = 72MHz
```

**STM32F403：**
```c
RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
RCC_OscInitStruct.HSEState = RCC_HSE_ON;
RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
RCC_OscInitStruct.PLL.PLLM = 8;  // 8MHz / 8 = 1MHz
RCC_OscInitStruct.PLL.PLLN = 168; // 1MHz * 168 = 168MHz
RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2; // 168MHz / 2 = 84MHz
```

---

## OTA 协议详解

### 1. 什么是 OTA 协议？

OTA（Over-The-Air）协议是设备之间传输固件的通信规则。

**类比理解：**
- 就像快递协议：寄件人、收件人、包裹格式、确认收货

### 2. 数据包结构

**完整数据包格式：**
```
┌─────────────────────────────────────────────────────────┐
│  字节 0     │  字节 1-4    │  字节 5-6  │  字节 7-N  │
│  命令类型    │  偏移地址     │  数据长度   │  数据内容   │
│  (1字节)    │  (4字节)      │  (2字节)   │  (可变)    │
└─────────────────────────────────────────────────────────┘
```

**代码定义：**
```c
typedef struct {
    uint8_t cmd;                    // 命令类型
    uint32_t offset;                // 偏移地址
    uint16_t length;                // 数据长度
    uint8_t data[1024];            // 数据内容
} OTA_Packet_t;
```

**为什么这样设计？**
- **固定头部**（7字节）：先接收头部，知道数据多长
- **可变数据**：根据实际需要传输的数据量
- **偏移地址**：支持断点续传

### 3. 命令类型详解

```c
typedef enum {
    OTA_CMD_START = 0x01,           // 开始升级
    OTA_CMD_DATA = 0x02,            // 数据传输
    OTA_CMD_END = 0x03,             // 升级结束
    OTA_CMD_ACK = 0x04,             // 确认
    OTA_CMD_NACK = 0x05,            // 否认
    OTA_CMD_QUERY_VERSION = 0x06,     // 查询版本
    OTA_CMD_VERSION_RESP = 0x07       // 版本响应
} OTA_Command_t;
```

**命令使用场景：**

| 命令 | 发送方 | 接收方 | 用途 |
|------|---------|---------|------|
| START | STM32 | ESP8266 | STM32 请求开始升级 |
| DATA | ESP8266 | STM32 | 传输固件数据 |
| END | ESP8266 | STM32 | 通知传输完成 |
| ACK | 任意 | 任意 | 确认收到数据 |
| NACK | 任意 | 任意 | 数据错误，请求重发 |
| QUERY_VERSION | STM32 | ESP8266 | 查询服务器版本 |
| VERSION_RESP | ESP8266 | STM32 | 返回版本信息 |

### 4. 升级流程详解

#### 主动升级流程（STM32 主动发起）

```
STM32 应用程序                      ESP8266                          服务器
    │                                │                                  │
    │  1. 检测到需要升级               │                                  │
    ├─────────────────────────────────────────>│                                  │
    │  OTA_CMD_START                   │                                  │
    │                                │  2. 下载固件信息                    │
    │                                ├─────────────────────────────────────────>│
    │                                │  GET /firmware/latest               │
    │                                │<─────────────────────────────────────────┤
    │                                │  固件信息                          │
    │<─────────────────────────────────────────┤                                  │
    │  OTA_CMD_ACK (固件大小)             │                                  │
    │                                │                                  │
    │  3. 擦除 Flash                 │                                  │
    │                                │                                  │
    │                                │  4. 分包下载固件                    │
    │                                ├─────────────────────────────────────────>│
    │                                │  GET /firmware/download/xxx.bin      │
    │<─────────────────────────────────────────┤                                  │
    │  OTA_CMD_DATA (第1包)            │  5. 转发数据                       │
    │                                │                                  │
    │  6. 写入 Flash                 │                                  │
    │                                │                                  │
    │<─────────────────────────────────────────┤                                  │
    │  OTA_CMD_DATA (第2包)            │                                  │
    │                                │                                  │
    │  ... (重复直到传输完成)           │                                  │
    │                                │                                  │
    │<─────────────────────────────────────────┤                                  │
    │  OTA_CMD_END                     │                                  │
    │                                │                                  │
    ├─────────────────────────────────────────>│                                  │
    │  OTA_CMD_ACK (CRC32)             │                                  │
    │                                │                                  │
    │  7. 验证 CRC32                 │                                  │
    │                                │                                  │
    │  8. 跳转到新程序               │                                  │
    │                                │                                  │
```

#### 推送升级流程（服务器主动推送）

```
服务器                           ESP8266                          STM32
    │                                │                                  │
    │  1. 管理员推送更新              │                                  │
    │  POST /push/update               │                                  │
    ├─────────────────────────────────────────>│                                  │
    │                                │  2. 保存到待处理队列                │
    │                                │                                  │
    │                                │  3. 发送心跳                         │
    │                                ├─────────────────────────────────────────>│
    │                                │  POST /device/heartbeat              │
    │                                │<─────────────────────────────────────────┤
    │                                │  有待处理更新                       │
    │                                │                                  │
    │                                │  4. 下载固件                        │
    │                                ├─────────────────────────────────────────>│
    │                                │  GET /firmware/download/xxx.bin      │
    │                                │<─────────────────────────────────────────┤
    │                                │  固件数据                          │
    │                                │                                  │
    │                                ├─────────────────────────────────────────>│
    │                                │  OTA_CMD_START                     │
    │                                │                                  │
    │                                │                                  │  5. 擦除 Flash
    │                                │<─────────────────────────────────────────┤
    │                                │  OTA_CMD_ACK                       │
    │                                │                                  │
    │                                │  6. 传输数据                        │
    │                                │<─────────────────────────────────────────┤
    │                                │  OTA_CMD_DATA (第1包)             │
    │                                │                                  │  7. 写入 Flash
    │                                │<─────────────────────────────────────────┤
    │                                │  OTA_CMD_DATA (第2包)             │
    │                                │                                  │
    │                                │  ... (重复直到传输完成)              │
    │                                │                                  │
    │                                │<─────────────────────────────────────────┤
    │                                │  OTA_CMD_END                       │
    │                                │                                  │
    │                                ├─────────────────────────────────────────>│
    │                                │  OTA_CMD_ACK (CRC32)               │
    │                                │                                  │  8. 验证 CRC32
    │                                │                                  │  9. 跳转到新程序
    │                                │                                  │
    │                                │  10. 发送确认                       │
    │                                ├─────────────────────────────────────────>│
    │                                │  POST /push/acknowledge            │
    │<─────────────────────────────────────────┤                                  │
    │  确认成功                         │                                  │
```

### 5. 可靠性机制

#### 5.1 CRC32 校验

**什么是 CRC32？**
- 循环冗余校验，用于检测数据是否损坏
- 32 位校验码，错误检测率极高

**计算示例：**
```c
uint32_t crc = 0xFFFFFFFF;

for (uint16_t i = 0; i < length; i++) {
    crc ^= data[i];
    for (uint8_t j = 0; j < 8; j++) {
        if (crc & 1) {
            crc = (crc >> 1) ^ 0xEDB88320;
        } else {
            crc >>= 1;
        }
    }
}

return crc ^ 0xFFFFFFFF;
```

**使用场景：**
1. 服务器计算固件 CRC32
2. ESP8266 下载固件时计算 CRC32
3. STM32 接收固件时计算 CRC32
4. 传输完成后比对 CRC32

#### 5.2 重传机制

```c
#define MAX_RETRY 3

uint8_t retry = 0;

while (retry < MAX_RETRY) {
    if (UART_ReceivePacket(&packet, timeout) == HAL_OK) {
        if (packet.cmd == OTA_CMD_DATA) {
            break;
        }
    }
    retry++;
    packet.cmd = OTA_CMD_NACK;
    UART_SendPacket(&packet);
}

if (retry >= MAX_RETRY) {
    return 0;
}
```

**重传逻辑：**
- 最多重试 3 次
- 每次失败发送 NACK
- 超过 3 次放弃

---

## Bootloader 深入学习

### 1. Bootloader 是什么？

**类比理解：**
- Bootloader 就像电脑的 BIOS
- 先启动，检查是否有更新
- 没有更新就跳转到主程序

**作用：**
1. 设备启动时首先运行
2. 检查是否有固件更新
3. 接收新固件并写入 Flash
4. 验证固件完整性
5. 跳转到应用程序

### 2. Bootloader 主函数分析

```c
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
```

**流程分解：**

```
启动
  ↓
初始化硬件
  ↓
检查是否有更新？
  ├─ 是 → 开始更新
  │         ↓
  │      接收固件
  │         ↓
  │      验证固件
  │         ↓
  │      复制到应用程序区
  │         ↓
  │      跳转到应用程序
  │
  └─ 否 → 检查应用程序是否有效？
              ├─ 是 → 跳转到应用程序
              │
              └─ 否 → LED 闪烁（错误状态）
```

### 3. 检查更新

```c
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
```

**逻辑说明：**
1. 发送版本查询命令
2. 等待 5 秒（TIMEOUT_MS）
3. 如果收到版本响应，返回 1（有更新）
4. 超时返回 0（无更新）

**为什么用超时？**
- 防止无限等待
- 没有更新时快速跳转到应用程序

### 4. 开始更新

```c
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
```

**逻辑说明：**
1. 发送 START 命令
2. 等待 ACK 响应
3. ACK 中包含固件大小
4. 最多重试 3 次

**为什么需要固件大小？**
- 知道需要擦除多少 Flash
- 知道需要接收多少数据
- 用于计算 CRC32

### 5. 接收固件

```c
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
```

**逻辑说明：**
1. 擦除临时存储区
2. 循环接收数据包
3. 每包写入 Flash
4. 计算累积 CRC32
5. 发送 ACK 确认
6. 失败重试 3 次

**为什么写入临时区？**
- 防止升级失败
- 验证通过后再复制
- 保护原有应用程序

### 6. 验证固件

```c
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
```

**逻辑说明：**
1. 发送 END 命令
2. 接收服务器 CRC32
3. 计算本地 CRC32
4. 比对是否一致

**为什么需要验证？**
- 确保固件完整
- 防止写入错误
- 避免设备变砖

### 7. 复制固件

```c
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
```

**逻辑说明：**
1. 擦除应用程序区
2. 从临时区读取数据
3. 写入应用程序区
4. 每次 256 字节

**为什么分块复制？**
- 节省 RAM（只需要 256 字节缓冲区）
- 防止一次性读取过大
- 便于错误处理

### 8. 跳转到应用程序

```c
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
```

**逻辑说明：**
1. 关闭中断
2. 设置向量表偏移
3. 设置主栈指针
4. 跳转到应用程序入口

**为什么关闭中断？**
- 防止中断跳转到错误地址
- 确保干净跳转
- 避免中断冲突

### 9. 检查应用程序有效性

```c
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
```

**逻辑说明：**
1. 读取栈指针（应用程序起始地址）
2. 读取复位指针（起始地址 + 4）
3. 验证栈指针在 RAM 区域（0x20000000）
4. 验证复位指针在 Flash 区域

**为什么需要验证？**
- 防止跳转到无效地址
- 检测 Flash 损坏
- 避免设备死机

---

## Flash 操作详解

### 1. Flash 基础知识

**STM32F403 Flash 特性：**
- 容量：1MB
- 扇区大小：16KB（扇区 0-3）、128KB（扇区 4+）
- 写入粒度：4 字节（字）
- 擦除粒度：扇区

**Flash 操作三步骤：**
1. **解锁** - 允许写入
2. **擦除** - 清除数据
3. **写入** - 写入新数据
4. **上锁** - 保护 Flash

### 2. 擦除扇区

```c
HAL_StatusTypeDef Flash_EraseSector(uint32_t start_addr, uint32_t size)
{
    uint32_t sector_error = 0;
    FLASH_EraseInitTypeDef erase_init;
    HAL_StatusTypeDef status;

    erase_init.TypeErase = FLASH_TYPEERASE_SECTORS;
    erase_init.VoltageRange = FLASH_VOLTAGE_RANGE_3;
    erase_init.Sector = FLASH_SECTOR_4;
    erase_init.NbSectors = 1;

    HAL_FLASH_Unlock();

    while (size > 0) {
        status = HAL_FLASHEx_Erase(&erase_init, &sector_error);
        if (status != HAL_OK) {
            HAL_FLASH_Lock();
            return status;
        }

        start_addr += 0x20000;
        size -= 0x20000;
        erase_init.Sector++;
    }

    HAL_FLASH_Lock();
    return HAL_OK;
}
```

**参数说明：**
- `start_addr`：起始地址
- `size`：擦除大小
- `FLASH_VOLTAGE_RANGE_3`：电压范围 2.7V-3.6V

**扇区计算：**
```
扇区 4: 0x08010000 - 0x0801FFFF (128KB)
扇区 5: 0x08020000 - 0x0803FFFF (128KB)
扇区 6: 0x08040000 - 0x0805FFFF (128KB)
...
```

**为什么循环擦除？**
- 每次只能擦除一个扇区
- 大小超过 128KB 需要多个扇区
- 自动计算需要多少扇区

### 3. 写入 Flash

```c
HAL_StatusTypeDef Flash_Write(uint32_t addr, uint8_t *data, uint16_t length)
{
    HAL_StatusTypeDef status;
    uint32_t word_data;

    HAL_FLASH_Unlock();

    for (uint16_t i = 0; i < length; i += 4) {
        if (i + 4 <= length) {
            word_data = *(uint32_t*)(data + i);
        } else {
            word_data = 0;
            for (uint8_t j = 0; j < (length - i); j++) {
                word_data |= (uint32_t)data[i + j] << (j * 8);
            }
        }

        status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, addr + i, word_data);
        if (status != HAL_OK) {
            HAL_FLASH_Lock();
            return status;
        }
    }

    HAL_FLASH_Lock();
    return HAL_OK;
}
```

**参数说明：**
- `addr`：写入地址（必须 4 字节对齐）
- `data`：数据指针
- `length`：数据长度

**写入逻辑：**
1. 按 4 字节（字）写入
2. 如果长度不是 4 的倍数，填充 0
3. 每次写入后检查状态

**为什么必须 4 字节对齐？**
- STM32F403 Flash 写入粒度是字（4 字节）
- 不能写入单个字节
- 必须按字对齐

### 4. 读取 Flash

```c
HAL_StatusTypeDef Flash_Read(uint32_t addr, uint8_t *buffer, uint16_t length)
{
    for (uint16_t i = 0; i < length; i++) {
        buffer[i] = *(volatile uint8_t*)(addr + i);
    }
    return HAL_OK;
}
```

**参数说明：**
- `addr`：读取地址
- `buffer`：缓冲区指针
- `length`：读取长度

**读取逻辑：**
- 直接读取内存映射地址
- 使用 `volatile` 防止优化
- 逐字节读取

**为什么 Flash 可以直接读取？**
- Flash 映射到内存空间
- 可以像 RAM 一样读取
- 不需要特殊指令

---

## UART 通信详解

### 1. UART 基础知识

**UART（通用异步收发器）：**
- 串行通信协议
- 异步（不需要时钟线）
- 全双工（同时收发）

**通信参数：**
- 波特率：115200（本项目）
- 数据位：8
- 停止位：1
- 校验位：无

### 2. UART 初始化

```c
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
```

**参数说明：**
- `Instance`：USART1（使用串口 1）
- `BaudRate`：115200（每秒 115200 位）
- `WordLength`：8 位数据
- `StopBits`：1 位停止位
- `Parity`：无校验
- `Mode`：收发模式
- `HwFlowCtl`：无硬件流控
- `OverSampling`：16 倍过采样

**为什么选择 115200？**
- 平衡速度和可靠性
- ESP8266 默认波特率
- 传输 1MB 固件约 90 秒

### 3. 发送字节

```c
HAL_StatusTypeDef UART_SendByte(uint8_t byte)
{
    return HAL_UART_Transmit(&huart1, &byte, 1, 1000);
}
```

**参数说明：**
- `&byte`：字节地址
- `1`：发送 1 字节
- `1000`：超时 1000ms

**返回值：**
- `HAL_OK`：发送成功
- `HAL_ERROR`：发送失败
- `HAL_TIMEOUT`：超时

### 4. 发送数据包

```c
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
```

**数据包构造：**
```
字节 0: cmd (命令)
字节 1-4: offset (偏移地址，大端序)
字节 5-6: length (数据长度，大端序)
字节 7-N: data (数据内容)
```

**大端序 vs 小端序：**
- 大端序：高位在前（网络常用）
- 小端序：低位在前（STM32 常用）
- 本项目使用大端序传输

**为什么分两次发送？**
- 先发送固定头部（7 字节）
- 再发送可变数据
- 简化内存管理

### 5. 接收数据包

```c
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
```

**接收逻辑：**
1. 先接收 7 字节头部
2. 解析命令、偏移、长度
3. 如果有数据，再接收数据部分

**为什么分两次接收？**
- 先知道数据多长
- 动态分配接收缓冲区
- 避免接收过多数据

---

## CRC32 校验详解

### 1. CRC32 基础知识

**CRC（循环冗余校验）：**
- 用于检测数据传输错误
- 32 位校验码（CRC32）
- 错误检测率极高

**应用场景：**
- 固件完整性校验
- 数据传输校验
- 文件校验

### 2. CRC32 计算算法

```c
uint32_t CRC32_Calculate(uint8_t *data, uint16_t length, uint32_t crc)
{
    for (uint16_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 1) {
                crc = (crc >> 1) ^ 0xEDB88320;
            } else {
                crc >>= 1;
            }
        }
    }
    return crc;
}
```

**算法说明：**
1. 初始化 CRC 为 0xFFFFFFFF
2. 对每个字节：
   - 与 CRC 异或
   - 移位 8 次
   - 如果最低位为 1，异或多项式
3. 返回 CRC

**多项式 0xEDB88320：**
- CRC32 标准多项式
- 反向多项式
- 广泛使用的标准

### 3. Flash CRC32 计算

```c
uint32_t CRC32_CalculateFlash(uint32_t addr, uint32_t size)
{
    uint32_t crc = 0xFFFFFFFF;
    uint8_t buffer[256];
    uint32_t offset = 0;

    while (offset < size) {
        uint16_t chunk_size = (size - offset) > 256 ? 256 : (size - offset);
        Flash_Read(addr + offset, buffer, chunk_size);
        crc = CRC32_Calculate(buffer, chunk_size, crc);
        offset += chunk_size;
    }

    return crc ^ 0xFFFFFFFF;
}
```

**逻辑说明：**
1. 初始化 CRC
2. 分块读取 Flash（每次 256 字节）
3. 累积计算 CRC
4. 返回最终 CRC

**为什么分块计算？**
- 节省 RAM（只需要 256 字节缓冲区）
- 防止一次性读取过大
- 便于错误处理

### 4. CRC32 使用场景

**场景 1：服务器上传固件**
```python
file.save(filepath)
crc32 = calculate_file_crc32(filepath)
firmware_info['crc32'] = hex(crc32)
```

**场景 2：ESP8266 下载固件**
```cpp
while (http.connected()) {
    int size = stream->readBytes(buffer, sizeof(buffer));
    crc = CRC32_Calculate(buffer, size, crc);
    sendToSTM32(offset, buffer, size);
}
```

**场景 3：STM32 接收固件**
```c
while (received_bytes < firmware_size) {
    UART_ReceivePacket(&packet, timeout);
    Flash_Write(addr, packet.data, packet.length);
    crc = CRC32_Calculate(packet.data, packet.length, crc);
}
```

**场景 4：验证固件**
```c
server_crc = *(uint32_t*)packet.data;
local_crc = CRC32_CalculateFlash(TEMP_ADDR, firmware_size);

if (server_crc == local_crc) {
    return 1;
}
```

---

## ESP8266 固件详解

### 1. ESP8266 基础知识

**ESP8266 特性：**
- WiFi 模块
- 80MHz CPU
- 4MB Flash
- 内置 TCP/IP 协议栈

**Arduino 开发：**
- 使用 Arduino 框架
- C++ 语言
- 丰富的库支持

### 2. WiFi 连接

```cpp
void connectWiFi() {
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    while (WiFi.status() != WL_CONNECTED) {
        digitalWrite(LED_PIN, !digitalRead(LED_PIN));
        delay(500);
    }

    digitalWrite(LED_PIN, HIGH);
}
```

**逻辑说明：**
1. 调用 `WiFi.begin()` 连接
2. 等待连接成功
3. LED 闪烁表示连接中
4. 连接成功 LED 常亮

**WiFi 状态：**
- `WL_CONNECTED`：已连接
- `WL_DISCONNECTED`：未连接
- `WL_IDLE_STATUS`：空闲

### 3. HTTP 请求

```cpp
void registerDevice() {
    String url = String(SERVER_BASE_URL) + ":" + String(SERVER_PORT) + "/device/register";
    
    http.begin(url);
    http.addHeader("Content-Type", "application/json");

    String payload = "{\"device_id\":\"" + String(device_id) + 
                     "\",\"device_type\":\"stm32f403\"," +
                     "\"current_version\":\"" + String(current_firmware_version) + "\"}";

    int httpCode = http.POST(payload);
    
    if (httpCode == 200) {
        String response = http.getString();
        DynamicJsonDocument doc(1024);
        deserializeJson(doc, response);
        String status = doc["status"];
    }
    
    http.end();
}
```

**HTTP 请求流程：**
1. 构造 URL
2. 设置请求头
3. 构造 JSON 载荷
4. 发送 POST 请求
5. 解析响应

**HTTP 状态码：**
- `200`：成功
- `400`：请求错误
- `404`：未找到
- `500`：服务器错误

### 4. 流式下载

```cpp
void downloadFirmware() {
    String url = String(SERVER_BASE_URL) + ":" + String(SERVER_PORT) + firmware_url;
    
    http.begin(url);
    int httpCode = http.GET();
    
    if (httpCode == HTTP_CODE_OK) {
        int len = http.getSize();
        WiFiClient * stream = http.getStreamPtr();
        
        uint32_t offset = 0;
        uint8_t buffer[PACKET_SIZE];
        
        while (http.connected() && len > 0) {
            int size = stream->available();
            
            if (size) {
                int c = stream->readBytes(buffer, ((size > sizeof(buffer)) ? sizeof(buffer) : size));
                sendToSTM32(offset, buffer, c);
                offset += c;
                len -= c;
            }
        }
    }
    
    http.end();
}
```

**流式下载优势：**
- 边下载边转发
- 不需要全部下载到 RAM
- 节省内存（ESP8266 RAM 有限）

**为什么需要流式下载？**
- ESP8266 RAM 只有 80KB
- 固件可能 1MB
- 无法全部下载到 RAM

### 5. UART 转发

```cpp
void sendToSTM32(uint32_t offset, uint8_t *data, uint16_t length) {
    uint8_t header[7];
    
    header[0] = OTA_CMD_DATA;
    header[1] = (offset >> 24) & 0xFF;
    header[2] = (offset >> 16) & 0xFF;
    header[3] = (offset >> 8) & 0xFF;
    header[4] = offset & 0xFF;
    header[5] = (length >> 8) & 0xFF;
    header[6] = length & 0xFF;
    
    Serial.write(header, 7);
    Serial.write(data, length);
}
```

**转发逻辑：**
1. 构造数据包头部
2. 发送头部
3. 发送数据
4. 使用 `Serial.write()` 发送

**为什么用 Serial？**
- ESP8266 的 UART 接口
- Arduino 框架封装
- 简化 UART 操作

---

## Flask 服务器详解

### 1. Flask 基础知识

**Flask 是什么？**
- Python Web 框架
- 轻量级、易用
- 适合 RESTful API

**RESTful API：**
- 使用 HTTP 方法（GET、POST、DELETE）
- 资源导向（/firmware、/device）
- 无状态（每次请求独立）

### 2. 应用初始化

```python
from flask import Flask, request, jsonify, send_file
from flask_cors import CORS
import os
import json
import threading

app = Flask(__name__)
CORS(app)

UPLOAD_FOLDER = 'firmware'
FIRMWARE_INFO_FILE = 'firmware_info.json'
DEVICES_FILE = 'devices.json'
PENDING_UPDATES_FILE = 'pending_updates.json'

if not os.path.exists(UPLOAD_FOLDER):
    os.makedirs(UPLOAD_FOLDER)

devices = {}
pending_updates = {}
update_lock = threading.Lock()
```

**初始化说明：**
- 创建 Flask 应用
- 启用 CORS（跨域）
- 创建必要的目录
- 初始化数据结构
- 创建线程锁

**为什么需要线程锁？**
- 多线程并发访问
- 保护共享数据
- 防止数据竞争

### 3. 上传固件

```python
@app.route('/firmware', methods=['POST'])
def upload_firmware():
    if 'file' not in request.files:
        return jsonify({'error': 'No file provided'}), 400

    file = request.files['file']
    version = request.form.get('version', '1.0.0')
    description = request.form.get('description', '')

    if file.filename == '':
        return jsonify({'error': 'No file selected'}), 400

    if not file.filename.endswith('.bin'):
        return jsonify({'error': 'Only .bin files are allowed'}), 400

    timestamp = datetime.now().strftime('%Y%m%d_%H%M%S')
    filename = f"firmware_{version}_{timestamp}.bin"
    filepath = os.path.join(UPLOAD_FOLDER, filename)

    file.save(filepath)

    file_size = os.path.getsize(filepath)
    crc32 = calculate_file_crc32(filepath)

    firmware_info = {
        'filename': filename,
        'version': version,
        'description': description,
        'size': file_size,
        'crc32': hex(crc32),
        'upload_time': datetime.now().isoformat(),
        'url': f'/firmware/download/{filename}'
    }

    return jsonify({
        'message': 'Firmware uploaded successfully',
        'firmware': firmware_info
    }), 201
```

**上传流程：**
1. 验证文件存在
2. 验证文件格式
3. 生成唯一文件名
4. 保存文件
5. 计算文件大小和 CRC32
6. 返回固件信息

**文件名格式：**
```
firmware_1.0.0_20240101_120000.bin
         版本    时间戳
```

### 4. 设备注册

```python
@app.route('/device/register', methods=['POST'])
def register_device():
    data = request.get_json()
    
    device_id = data.get('device_id')
    device_type = data.get('device_type')
    current_version = data.get('current_version')
    
    if not device_id:
        return jsonify({'error': 'Device ID is required'}), 400
    
    with update_lock:
        devices[device_id] = {
            'device_id': device_id,
            'device_type': device_type,
            'current_version': current_version,
            'registered_at': datetime.now().isoformat(),
            'last_heartbeat': datetime.now().isoformat(),
            'status': 'online'
        }
    
    return jsonify({
        'message': 'Device registered successfully',
        'device': devices[device_id]
    }), 200
```

**注册流程：**
1. 解析 JSON 数据
2. 验证设备 ID
3. 使用线程锁保护
4. 保存设备信息
5. 返回注册结果

**设备信息结构：**
```python
{
    'device_id': 'STM32_AABBCCDDEEFF',
    'device_type': 'stm32f403',
    'current_version': '1.0.0',
    'registered_at': '2024-01-01T12:00:00',
    'last_heartbeat': '2024-01-01T12:30:00',
    'status': 'online'
}
```

### 5. 设备心跳

```python
@app.route('/device/heartbeat', methods=['POST'])
def device_heartbeat():
    data = request.get_json()
    device_id = data.get('device_id')
    
    if device_id not in devices:
        return jsonify({'error': 'Device not registered'}), 404
    
    with update_lock:
        devices[device_id]['last_heartbeat'] = datetime.now().isoformat()
        devices[device_id]['status'] = 'online'
        
        if device_id in pending_updates:
            return jsonify({
                'status': 'update_available',
                'firmware': pending_updates[device_id]
            }), 200
    
    return jsonify({'status': 'no_update'}), 200
```

**心跳处理：**
1. 更新最后心跳时间
2. 设置设备状态为在线
3. 检查待处理更新
4. 如果有更新，返回更新信息

**心跳作用：**
- 检测设备在线状态
- 触发推送更新
- 维护设备列表

### 6. 推送更新

```python
@app.route('/push/update', methods=['POST'])
def push_update():
    data = request.get_json()
    
    device_ids = data.get('device_ids', [])
    firmware_filename = data.get('firmware_filename')
    force_update = data.get('force_update', False)
    
    if not firmware_filename:
        return jsonify({'error': 'Firmware filename is required'}), 400
    
    firmware_path = os.path.join(UPLOAD_FOLDER, firmware_filename)
    if not os.path.exists(firmware_path):
        return jsonify({'error': 'Firmware not found'}), 404
    
    with update_lock:
        if not device_ids:
            device_ids = list(devices.keys())
        
        for device_id in device_ids:
            if device_id in devices:
                pending_updates[device_id] = {
                    'firmware_filename': firmware_filename,
                    'force_update': force_update,
                    'pushed_at': datetime.now().isoformat()
                }
    
    return jsonify({
        'message': 'Update pushed successfully',
        'affected_devices': len(device_ids)
    }), 200
```

**推送流程：**
1. 获取目标设备列表
2. 验证固件文件存在
3. 如果设备列表为空，推送到所有设备
4. 将更新信息加入待处理队列
5. 返回推送结果

**推送逻辑：**
- 设备下次心跳时获取更新
- 不需要立即连接设备
- 异步推送机制

---

## 实践步骤

### 步骤 1：准备开发环境

#### 1.1 安装 STM32CubeIDE

1. 下载 STM32CubeIDE：https://www.st.com/zh/development-tools/stm32cubeide
2. 安装 STM32CubeIDE
3. 安装 STM32F4 固件包

#### 1.2 安装 Arduino IDE

1. 下载 Arduino IDE：https://www.arduino.cc/en/software
2. 安装 Arduino IDE
3. 安装 ESP8266 开发板支持

#### 1.3 安装 Python

1. 下载 Python：https://www.python.org/downloads/
2. 安装 Python 3.8+
3. 勾选 "Add Python to PATH"

### 步骤 2：编译 STM32 Bootloader

```bash
cd stm32_bootloader
pio run
```

**编译输出：**
```
Processing stm32_bootloader (platform: ststm32; board: nucleo_f403re)
...
Linking .pio\build\nucleo_f403re\firmware.elf
Building .pio\build\nucleo_f403re\firmware.hex
Building .pio\build\nucleo_f403re\firmware.bin
======================== [SUCCESS] Took 12.34 seconds ========================
```

### 步骤 3：烧录 STM32 Bootloader

```bash
cd stm32_bootloader
pio run --target upload
```

**烧录输出：**
```
Configuring upload protocol...
AVAILABLE: blackmagic, cmsis-dap, jlink, stlink
CURRENT: upload_protocol = stlink
Uploading .pio\build\nucleo_f403re\firmware.bin
...
======== [SUCCESS] Took 5.67 seconds =========
```

### 步骤 4：编译 STM32 应用程序

```bash
cd stm32_app
pio run
```

### 步骤 5：烧录 STM32 应用程序

```bash
cd stm32_app
pio run --target upload
```

**注意：**
- 应用程序地址：0x08010000
- 需要修改链接脚本

### 步骤 6：编译 ESP8266 固件

```bash
cd esp8266_firmware
pio run
```

### 步骤 7：烧录 ESP8266 固件

```bash
cd esp8266_firmware
pio run --target upload
```

### 步骤 8：启动服务器

```bash
cd server
pip install -r requirements.txt
python server.py
```

**服务器输出：**
```
 * Running on http://0.0.0.0:5000
```

### 步骤 9：上传测试固件

```bash
curl -X POST http://localhost:5000/firmware \
  -F "file=@test_firmware.bin" \
  -F "version=1.0.0" \
  -F "description=Test firmware"
```

### 步骤 10：测试升级流程

1. 连接 STM32 和 ESP8266
2. STM32 Bootloader 启动
3. ESP8266 下载固件
4. STM32 接收并写入 Flash
5. 验证升级结果

---

## 常见问题解答

### Q1: STM32F103 和 STM32F403 的 Flash 操作有什么区别？

**A:**
- F103 使用 PAGE（页）擦除，每页 1KB 或 2KB
- F403 使用 SECTOR（扇区）擦除，扇区 0-3 为 16KB，扇区 4+ 为 128KB
- F103 写入粒度：半字（2 字节）
- F403 写入粒度：字（4 字节）

### Q2: 为什么需要临时存储区？

**A:**
- 防止升级失败导致设备变砖
- 先下载到临时区，验证通过后再复制
- 提供回滚机制

### Q3: CRC32 校验的作用是什么？

**A:**
- 检测固件是否损坏
- 确保数据传输完整
- 防止写入错误

### Q4: 为什么使用大端序传输数据？

**A:**
- 网络协议常用大端序
- 便于跨平台通信
- STM32 自动转换

### Q5: ESP8266 为什么需要流式下载？

**A:**
- ESP8266 RAM 只有 80KB
- 固件可能 1MB
- 无法全部下载到 RAM

### Q6: Bootloader 如何跳转到应用程序？

**A:**
1. 关闭中断
2. 设置向量表偏移
3. 设置主栈指针
4. 跳转到应用程序入口

### Q7: 如何防止设备变砖？

**A:**
- 使用临时存储区
- CRC32 校验
- 验证应用程序有效性
- 保留 Bootloader

### Q8: 推送更新和主动更新的区别？

**A:**
- 主动更新：设备定期检查服务器
- 推送更新：服务器主动通知设备
- 推送更新需要心跳机制

---

## 学习建议

### 1. 循序渐进

```
第 1 周：学习 STM32F403 基础
  - Flash 操作
  - UART 通信
  - HAL 库使用

第 2 周：学习 Bootloader 原理
  - 启动流程
  - Flash 操作
  - 应用程序跳转

第 3 周：学习 OTA 协议
  - 数据包设计
  - 命令定义
  - 通信流程

第 4 周：学习 ESP8266 开发
  - WiFi 连接
  - HTTP 请求
  - UART 通信

第 5 周：学习 Flask 服务器
  - RESTful API
  - 文件上传
  - 设备管理

第 6 周：实践项目
  - 编译固件
  - 烧录测试
  - 调试问题
```

### 2. 动手实践

- 每个模块都要亲自编译和测试
- 使用串口调试输出
- 使用 LED 指示状态
- 逐步增加功能

### 3. 调试技巧

**串口调试：**
```c
printf("Debug: received_bytes = %lu\n", received_bytes);
```

**LED 指示：**
```c
HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
```

**断点调试：**
- 使用 STM32CubeIDE 调试器
- 设置断点
- 查看变量

### 4. 扩展功能

- 添加数字签名
- 实现 HTTPS
- 添加回滚机制
- 增加日志记录

---

## 总结

通过本学习指南，你已经掌握了：

✅ **STM32F403 基础**
- Flash 操作
- UART 通信
- HAL 库使用

✅ **Bootloader 原理**
- 启动流程
- 固件更新
- 应用程序跳转

✅ **OTA 协议设计**
- 数据包结构
- 命令定义
- 通信流程

✅ **ESP8266 开发**
- WiFi 连接
- HTTP 请求
- UART 转发

✅ **Flask 服务器**
- RESTful API
- 文件上传
- 设备管理

✅ **实践能力**
- 编译固件
- 烧录测试
- 调试问题

继续加油！你已经迈出了重要的一步！
