# STM32F403 + ESP8266 远程升级系统

## 系统架构

```
┌─────────────────┐         ┌─────────────────┐         ┌─────────────────┐
│   服务器        │  HTTP   │   ESP8266       │  UART   │   STM32F403     │
│  (Flask API)    │◄────────►│  (WiFi模块)     │◄────────►│  (MCU)          │
│                 │         │                 │         │                 │
│ - 固件存储      │         │ - WiFi连接      │         │ - Bootloader    │
│ - CRC校验       │         │ - HTTP下载      │         │ - 应用程序      │
│ - 版本管理      │         │ - 数据转发      │         │ - Flash写入     │
└─────────────────┘         └─────────────────┘         └─────────────────┘
```

## 目录结构

```
项目/
├── esp8266_firmware/          # ESP8266 固件
│   ├── esp8266_ota.ino         # 主程序
│   ├── ota_protocol.h          # OTA 协议定义
│   ├── config.ini              # 配置文件
│   └── platformio.ini          # PlatformIO 配置
├── stm32f403_bootloader/       # STM32 Bootloader
│   ├── src/
│   │   ├── main.c              # Bootloader 主程序
│   │   └── flash.c             # Flash 操作
│   ├── inc/
│   │   ├── bootloader.h        # Bootloader 头文件
│   │   ├── flash.h             # Flash 操作头文件
│   │   └── ota_protocol.h      # OTA 协议定义
│   ├── STM32F403VCTx_BOOTLOADER.ld  # 链接脚本
│   └── platformio.ini          # PlatformIO 配置
├── stm32f403_application/      # STM32 应用程序
│   ├── src/
│   │   └── main.c              # 应用程序主程序
│   ├── STM32F403VCTx_APPLICATION.ld  # 链接脚本
│   └── platformio.ini          # PlatformIO 配置
├── server/                     # 服务器端
│   ├── templates/              # HTML 模板
│   │   └── index.html         # Web 管理界面
│   ├── static/                 # 静态资源
│   │   ├── css/
│   │   │   └── style.css      # 样式文件
│   │   └── js/
│   │       └── app.js         # 前端 JavaScript
│   ├── server.py               # Flask API 服务器
│   ├── requirements.txt        # Python 依赖
│   └── test_server.py          # 服务器测试脚本
├── tools/                      # 工具脚本
│   ├── ota_client.py           # OTA 客户端测试脚本
│   └── device_manager.py       # 设备管理工具
├── README.md                   # 项目文档
└── project_config.ini          # 项目配置文件
```

## 内存布局

### STM32F403 Flash 布局

| 地址范围 | 大小 | 用途 |
|---------|------|------|
| 0x08000000 - 0x0800FFFF | 64KB | Bootloader |
| 0x08010000 - 0x080FFFFF | 960KB | 应用程序 |
| 0x0800FFF0 | 4字节 | 应用程序有效标志 |
| 0x0800FFF8 | 4字节 | OTA 触发标志 |

## OTA 协议

### 数据包格式

```
| 字节 | 字段          | 说明                     |
|------|---------------|--------------------------|
| 1    | cmd           | 命令类型                 |
| 4    | offset        | 偏移地址                 |
| 2    | length        | 数据长度                 |
| N    | data          | 数据内容 (可选)          |
```

### 命令定义

| 命令值 | 名称              | 说明                     |
|--------|-------------------|--------------------------|
| 0x01   | OTA_CMD_START     | 开始升级                 |
| 0x02   | OTA_CMD_DATA      | 数据传输                 |
| 0x03   | OTA_CMD_END       | 升级结束                 |
| 0x04   | OTA_CMD_ACK       | 确认                     |
| 0x05   | OTA_CMD_NACK      | 否认                     |
| 0x06   | OTA_CMD_QUERY_VERSION | 查询版本            |
| 0x07   | OTA_CMD_VERSION_RESP | 版本响应             |

## 升级流程

### 主动升级流程（设备主动获取）

1. STM32 应用程序发送 OTA_CMD_START 到 ESP8266
2. ESP8266 从服务器下载固件信息
3. ESP8266 发送 ACK 给 STM32，包含固件大小
4. STM32 Bootloader 擦除应用程序区域
5. ESP8266 分包下载固件，通过 UART 发送给 STM32
6. STM32 将固件写入 Flash
7. 传输完成后，ESP8266 发送 CRC32 校验值
8. STM32 验证 CRC32，设置有效标志
9. STM32 跳转到新应用程序

### 推送升级流程（服务器推送）

1. 设备启动时自动注册到服务器
2. 设备定期发送心跳包（默认30秒）
3. 设备定期检查更新（默认5分钟）
4. 管理员通过服务器推送更新到指定设备或所有设备
5. 设备在下次心跳时收到更新通知
6. ESP8266 自动开始下载固件
7. ESP8266 通过 UART 发送 OTA_CMD_START 到 STM32
8. STM32 Bootloader 擦除应用程序区域
9. ESP8266 分包下载固件，通过 UART 发送给 STM32
10. STM32 将固件写入 Flash
11. 传输完成后，ESP8266 发送 CRC32 校验值
12. STM32 验证 CRC32，设置有效标志
13. STM32 跳转到新应用程序
14. ESP8266 向服务器发送升级确认

### 触发升级

应用程序可以通过以下方式触发升级：
- 通过 UART 接收 OTA_CMD_START 命令
- 写入 BOOTLOADER_MAGIC_VALUE 到 0x0800FFF8
- 执行系统复位
- 服务器推送更新通知

## 硬件连接

### STM32F403 与 ESP8266 连接

| STM32F403 | ESP8266 | 说明 |
|-----------|---------|------|
| PA9 (TX)  | RX      | STM32 发送数据 |
| PA10 (RX) | TX      | STM32 接收数据 |
| GND       | GND     | 共地 |
| 3.3V      | 3.3V    | 电源 |

## 通信配置与使用详解

### 硬件连接配置

#### 引脚连接
| ESP8266 引脚 | STM32 引脚 | 功能 |
|-------------|------------|------|
| **RX (GPIO5)** | **TX (PA9)** | 数据接收 |
| **TX (GPIO4)** | **RX (PA10)** | 数据发送 |
| **GND** | **GND** | 共地 |
| **VCC** | **3.3V** | 电源（注意：ESP8266 需要稳定的 3.3V 电源） |

#### 电源注意事项
- **ESP8266 工作电流**：启动时可达 300-400mA，稳定工作时约 70-80mA
- **STM32 输出能力**：PA9/PA10 作为 UART 引脚，输出电流有限
- **建议**：使用独立的 3.3V 电源模块为 ESP8266 供电，确保稳定运行

### 软件配置

#### ESP8266 配置

**1. 串口配置**
```cpp
// esp8266_ota.ino
// 初始化硬件串口，设置波特率为 115200
// 波特率选择理由：平衡传输速度和可靠性，115200 是常用的稳定波特率
Serial.begin(115200);  

// 交换硬件串口引脚
// 注意：ESP8266 默认使用 GPIO1 (TX) 和 GPIO3 (RX) 作为串口
// 通过 swap() 函数可以使用 GPIO4 (TX) 和 GPIO5 (RX)，更方便硬件连接
Serial.swap();         
```

**2. WiFi 配置**
```cpp
// esp8266_ota.ino
// WiFi 网络配置
#define WIFI_SSID           "YourWiFiSSID"       // WiFi 网络名称
#define WIFI_PASSWORD       "YourWiFiPassword"   // WiFi 密码

// 服务器配置
#define SERVER_BASE_URL     "http://your-server.com"  // 服务器基础地址
#define SERVER_PORT         5000                      // 服务器端口

// 连接 WiFi 网络的函数
void connectWiFi() {
    // 启动 WiFi 连接
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    
    // 等待 WiFi 连接成功
    // 连接过程中通过 LED 闪烁指示状态
    while (WiFi.status() != WL_CONNECTED) {
        digitalWrite(LED_PIN, !digitalRead(LED_PIN));  // 切换 LED 状态
        delay(500);  // 500ms 延迟，控制闪烁频率
    }
    // 连接成功后 LED 会保持当前状态
}
```

**3. 心跳机制**
```cpp
// 心跳间隔定义（单位：毫秒）
// 30秒是一个合理的间隔，平衡了实时性和网络开销
#define HEARTBEAT_INTERVAL  30000  // 30秒

// 上次发送心跳的时间戳
// 用于计算是否到达心跳间隔
unsigned long last_heartbeat = 0;

// 主循环函数
void loop() {
    // 获取当前时间戳
    unsigned long current_time = millis();
    
    // 检查是否到达心跳间隔
    if (current_time - last_heartbeat >= HEARTBEAT_INTERVAL) {
        // 发送心跳包到服务器
        // 心跳包包含设备状态信息，服务器会返回是否需要更新
        sendHeartbeat();
        
        // 更新上次心跳时间戳
        last_heartbeat = current_time;
    }
    
    // 其他任务处理...
}
```

#### STM32 配置

**1. UART 初始化**
```c
// uart_comm.c
// 初始化 UART1 串口，用于与 ESP8266 通信
void UART_Init(void)
{
    // 1. 使能 USART1 时钟
    // USART1 挂载在 APB2 总线上
    __HAL_RCC_USART1_CLK_ENABLE();
    
    // 2. 配置 USART1 实例
    huart1.Instance = USART1;  // 指定使用 USART1
    
    // 3. 配置串口参数
    huart1.Init.BaudRate = 115200;          // 波特率：115200
    huart1.Init.WordLength = UART_WORDLENGTH_8B;  // 数据位：8位
    huart1.Init.StopBits = UART_STOPBITS_1;      // 停止位：1位
    huart1.Init.Parity = UART_PARITY_NONE;       // 校验位：无
    huart1.Init.Mode = UART_MODE_TX_RX;          // 模式：收发模式
    huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE; // 硬件流控：无
    huart1.Init.OverSampling = UART_OVERSAMPLING_16; // 过采样：16倍
    
    // 4. 初始化 USART1
    HAL_UART_Init(&huart1);
}
```

**2. 时钟配置**
- **STM32F403 系统时钟**：168MHz
  - 这是 STM32F403 的最大系统时钟频率
  - 由 HSE（外部高速时钟）通过 PLL（锁相环）倍频得到

- **USART1 时钟**：APB2 总线时钟（84MHz）
  - USART1 挂载在 APB2 总线上
  - APB2 总线时钟是系统时钟的 1/2

- **波特率计算**：84MHz / 16 / 45.5 = 115200
  - 公式：波特率 = 总线时钟 / (16 * 分频系数)
  - 实际使用中，硬件会自动计算并配置最佳的分频系数
  - 115200 是一个稳定可靠的波特率选择

### 通信协议详解

#### OTA 协议结构

**1. 数据包格式**
```
┌─────────────────────────────────────────────────────────┐
│  字节 0     │  字节 1-4    │  字节 5-6  │  字节 7-N  │
│  命令类型    │  偏移地址     │  数据长度   │  数据内容   │
│  (1字节)    │  (4字节)      │  (2字节)   │  (可变)    │
└─────────────────────────────────────────────────────────┘
```

**字段说明**：
- **命令类型**：1字节，定义数据包的类型（如开始升级、数据传输等）
- **偏移地址**：4字节，固件在Flash中的偏移地址（大端序）
- **数据长度**：2字节，数据部分的长度（大端序）
- **数据内容**：可变长度，最大1024字节

**协议设计考虑**：
- 固定7字节头部，便于解析
- 偏移地址用于支持断点续传
- 数据长度字段限制单次传输大小，提高可靠性

**2. 命令类型**
| 命令 | 字节值 | 发送方 | 接收方 | 用途 |
|------|--------|--------|--------|------|
| **OTA_CMD_START** | 0x01 | STM32 | ESP8266 | 开始升级，请求下载固件 |
| **OTA_CMD_DATA** | 0x02 | ESP8266 | STM32 | 数据传输，包含固件数据块 |
| **OTA_CMD_END** | 0x03 | ESP8266 | STM32 | 升级结束，包含CRC32校验值 |
| **OTA_CMD_ACK** | 0x04 | 双向 | 双向 | 确认收到，无错误 |
| **OTA_CMD_NACK** | 0x05 | 双向 | 双向 | 数据错误，需要重传 |
| **OTA_CMD_QUERY_VERSION** | 0x06 | STM32 | ESP8266 | 查询版本，检查是否有更新 |
| **OTA_CMD_VERSION_RESP** | 0x07 | ESP8266 | STM32 | 版本响应，返回最新版本信息 |

**命令流程说明**：
- **主动升级**：STM32 发送 OTA_CMD_QUERY_VERSION → ESP8266 发送 OTA_CMD_VERSION_RESP → STM32 发送 OTA_CMD_START → 数据传输 → OTA_CMD_END
- **推送升级**：ESP8266 发送 OTA_CMD_START → 数据传输 → OTA_CMD_END
- **错误处理**：任何环节出错都发送 OTA_CMD_NACK，正确接收发送 OTA_CMD_ACK

**3. 协议特点**
- **固定头部**：7字节头部，包含命令、偏移和长度
  - 固定长度头部便于接收方快速解析
  - 统一的格式简化了通信逻辑

- **可变数据**：最大 1024 字节数据
  - 数据长度可根据实际情况调整
  - 1024 字节是平衡可靠性和效率的选择
  - 过小会增加通信开销，过大可能导致传输错误

- **大端序**：偏移地址和长度使用大端序传输
  - 网络通信标准字节序
  - 确保不同设备间的兼容性

- **支持断点续传**：通过偏移地址实现
  - 传输中断后可从断点处继续
  - 提高升级成功率，特别是在网络不稳定环境

- **可靠性设计**：
  - 每包数据都需要确认（ACK/NACK）
  - 支持重传机制
  - CRC32 校验确保数据完整性

### 通信流程

#### 主动升级流程

1. **STM32 Bootloader 启动**
   - 初始化硬件（包括 UART、GPIO 等）
   - 检查是否需要升级（通过检查标志位或用户触发）

2. **版本查询**
   ```c
   // STM32 发送版本查询命令
   UART_SendByte(OTA_CMD_QUERY_VERSION);
   
   // ESP8266 接收并检查服务器
   void processCommand(uint8_t cmd) {
       if (cmd == OTA_CMD_QUERY_VERSION) {
           // 向服务器请求最新固件版本信息
           // 比较本地版本与服务器版本
           sendVersionResponse();  // 返回版本比较结果
       }
   }
   ```

3. **开始升级**
   - STM32 发送 `OTA_CMD_START` 命令，请求开始升级
   - ESP8266 向服务器请求固件信息
   - ESP8266 回复 `OTA_CMD_ACK` 并包含固件大小等信息
   - STM32 准备接收固件（如擦除 Flash 区域）

4. **数据传输**
   - ESP8266 从服务器下载固件，分包发送（每包最大 1024 字节）
   - ESP8266 通过 `OTA_CMD_DATA` 命令发送数据块
   - STM32 接收数据并写入 Flash 对应位置
   - STM32 每收到一包数据后发送 `OTA_CMD_ACK` 确认
   - 如有错误，发送 `OTA_CMD_NACK` 请求重传

5. **升级完成**
   - ESP8266 发送 `OTA_CMD_END` 命令，包含固件的 CRC32 校验值
   - STM32 计算接收到的固件 CRC32 值
   - STM32 比较两个 CRC32 值，验证固件完整性
   - 验证通过后，设置应用程序有效标志
   - STM32 跳转到新的应用程序
   - 验证失败则保持在 Bootloader 中，等待重试

#### 推送升级流程

1. **服务器推送**
   - 管理员通过 Web 界面选择设备和固件
   - 服务器记录待处理更新，关联设备 ID 和固件信息
   - 服务器等待设备心跳包

2. **ESP8266 心跳**
   ```cpp
   void sendHeartbeat() {
       // 发送心跳到服务器，包含设备 ID 和当前版本
       // 服务器检查该设备是否有待处理的更新
       // 服务器返回是否需要更新的标志
       if (doc.containsKey("update_required") && doc["update_required"] == true) {
           // 设置更新标志，触发升级流程
           update_available = 1;
           // 记录服务器返回的固件 URL 等信息
           firmware_url = doc["firmware_url"].as<String>();
       }
   }
   ```

3. **开始推送更新**
   - ESP8266 检测到 `update_available` 标志
   - ESP8266 发送 `OTA_CMD_START` 命令到 STM32
   - STM32 进入升级模式，准备接收固件
   - 后续流程与主动升级相同（数据传输、校验、跳转）

### 代码实现细节

#### ESP8266 代码关键部分

**1. 设备注册**
```cpp
// 设备注册函数，用于将设备信息注册到服务器
void registerDevice() {
    // 构建注册 URL
    String url = String(SERVER_BASE_URL) + ":" + String(SERVER_PORT) + "/device/register";
    
    // 初始化 HTTP 客户端
    http.begin(url);
    
    // 设置请求头，指定 JSON 格式
    http.addHeader("Content-Type", "application/json");
    
    // 构建 JSON  payload，包含设备信息
    String payload = "{\"device_id\":\"" + String(device_id) + 
                     "\",\"device_type\":\"stm32f403\"," +
                     "\"current_version\":\"" + String(current_firmware_version) + "\"}";
    
    // 发送 POST 请求到服务器
    int httpCode = http.POST(payload);
    
    // 检查响应码
    // if (httpCode == HTTP_CODE_OK) {
    //     // 注册成功
    // } else {
    //     // 注册失败，可记录日志或重试
    // }
    
    // 结束 HTTP 连接
    http.end();
}
```

**2. 固件下载与发送**
```cpp
// 固件下载与发送函数
// 从服务器下载固件并通过 UART 发送给 STM32
void downloadFirmware() {
    // 构建固件下载 URL
    String url = String(SERVER_BASE_URL) + ":" + String(SERVER_PORT) + firmware_url;
    
    // 初始化 HTTP 客户端
    http.begin(url);
    
    // 发送 GET 请求下载固件
    int httpCode = http.GET();
    
    if (httpCode == HTTP_CODE_OK) {  // 下载成功
        // 获取固件大小
        int len = http.getSize();
        
        // 获取 WiFiClient 流指针，用于流式读取
        WiFiClient * stream = http.getStreamPtr();
        
        // 初始化偏移量，用于记录已发送的数据位置
        uint32_t offset = 0;
        
        // 循环读取并发送数据
        while (http.connected() && len > 0) {
            // 检查可读数据大小
            int size = stream->available();
            if (size) {
                // 读取数据到缓冲区，最大 PACKET_SIZE 字节
                int c = stream->readBytes(firmware_buffer, ((size > PACKET_SIZE) ? PACKET_SIZE : size));
                
                // 发送数据 packet 到 STM32
                sendDataPacket(offset, firmware_buffer, c);
                
                // 更新偏移量和剩余长度
                offset += c;
                len -= c;
            }
        }
    }
    
    // 结束 HTTP 连接
    http.end();
}
```

#### STM32 代码关键部分

**1. 接收固件**
```c
// 接收固件函数
// 从 ESP8266 接收固件数据并写入 Flash
uint8_t Bootloader_ReceiveFirmware(void)
{
    // 定义 OTA 数据包结构
    OTA_Packet_t packet;
    
    // 初始化写入地址（临时存储区起始地址）
    uint32_t write_addr = TEMP_ADDR;
    
    // 初始化 CRC32 校验值
    uint32_t crc = 0xFFFFFFFF;
    
    // 1. 擦除临时存储区
    // 为新固件准备空间
    Flash_EraseSector(TEMP_ADDR, firmware_size);
    
    // 2. 循环接收固件数据
    while (received_bytes < firmware_size) {
        // 从 UART 接收数据包
        if (UART_ReceivePacket(&packet, TIMEOUT_MS) == HAL_OK) {
            // 检查是否为数据命令
            if (packet.cmd == OTA_CMD_DATA) {
                // 写入 Flash
                if (Flash_Write(write_addr, packet.data, packet.length) == HAL_OK) {
                    // 计算 CRC32 校验
                    crc = CRC32_Calculate(packet.data, packet.length, crc);
                    
                    // 更新写入地址和已接收字节数
                    write_addr += packet.length;
                    received_bytes += packet.length;
                    
                    // 发送确认包
                    packet.cmd = OTA_CMD_ACK;
                    UART_SendPacket(&packet);
                } else {
                    // Flash 写入失败，发送否认包
                    packet.cmd = OTA_CMD_NACK;
                    UART_SendPacket(&packet);
                }
            }
        }
    }
    
    // 保存计算得到的 CRC32 值
    calculated_crc = crc;
    
    return 1;  // 接收成功
}
```

**2. 验证固件**
```c
// 验证固件函数
// 验证接收到的固件完整性
uint8_t Bootloader_VerifyFirmware(void)
{
    // 定义 OTA 数据包结构
    OTA_Packet_t packet;
    
    // 服务器 CRC32 值
    uint32_t server_crc;
    
    // 本地计算的 CRC32 值
    uint32_t local_crc;
    
    // 1. 发送结束命令，请求 CRC32 值
    packet.cmd = OTA_CMD_END;
    UART_SendPacket(&packet);
    
    // 2. 接收服务器返回的 CRC32 值
    if (UART_ReceivePacket(&packet, TIMEOUT_MS) == HAL_OK) {
        if (packet.cmd == OTA_CMD_ACK) {
            // 提取服务器 CRC32 值
            server_crc = *(uint32_t*)packet.data;
            
            // 3. 计算本地 Flash 中固件的 CRC32 值
            local_crc = CRC32_CalculateFlash(TEMP_ADDR, firmware_size);
            
            // 4. 比较两个 CRC32 值
            if (server_crc == local_crc) {
                return 1;  // 验证成功
            }
        }
    }
    
    return 0;  // 验证失败
}
```

### 可靠性机制

#### 错误处理与重传机制

**1. ESP8266 重传**
```cpp
// 最大重传次数
// 3次是一个合理的选择，平衡了可靠性和时间开销
#define MAX_RETRIES 3

// 带重传机制的数据包发送函数
uint8_t sendPacketWithRetry(OTA_Packet_t *packet) {
    // 初始化重试计数器
    uint8_t retry = 0;
    
    // 循环尝试发送
    while (retry < MAX_RETRIES) {
        // 发送数据包
        sendPacket(packet);
        
        // 等待接收方确认
        if (waitForAck()) {
            return 1;  // 发送成功
        }
        
        // 未收到确认，增加重试计数
        retry++;
        
        // 可选：添加延迟，避免连续重传
        // delay(100);
    }
    
    return 0;  // 重传失败
}
```

**2. STM32 重传**
```c
// 最大重试次数
#define MAX_RETRY 3

uint8_t Bootloader_ReceiveFirmware(void) {
    // ...
    while (received_bytes < firmware_size) {
        // 初始化重试计数器
        retry = 0;
        
        // 循环尝试接收
        while (retry < MAX_RETRY) {
            // 接收数据包
            if (UART_ReceivePacket(&packet, TIMEOUT_MS) == HAL_OK) {
                // 检查是否为数据命令
                if (packet.cmd == OTA_CMD_DATA) {
                    // 处理数据...
                    break;  // 接收成功，跳出重试循环
                }
            }
            
            // 接收失败或命令错误，增加重试计数
            retry++;
            
            // 发送否认包，请求重传
            packet.cmd = OTA_CMD_NACK;
            UART_SendPacket(&packet);
        }
        
        // 检查是否达到最大重试次数
        if (retry >= MAX_RETRY) {
            return 0;  // 重传失败，升级过程失败
        }
    }
    // ...
}
```

#### 数据完整性校验

**1. CRC32 计算**
```c
// CRC32 计算函数
// 用于计算数据的 CRC32 校验值
// 参数：
//   data - 数据指针
//   length - 数据长度
//   crc - 初始 CRC 值（首次调用时为 0xFFFFFFFF）
// 返回值：
//   计算得到的 CRC32 值
uint32_t CRC32_Calculate(uint8_t *data, uint16_t length, uint32_t crc)
{
    // 遍历每个字节
    for (uint16_t i = 0; i < length; i++) {
        // 与当前字节异或
        crc ^= data[i];
        
        // 处理每个位
        for (uint8_t j = 0; j < 8; j++) {
            // 检查最低位
            if (crc & 1) {
                // 最低位为 1，右移并异或多项式
                crc = (crc >> 1) ^ 0xEDB88320;
            } else {
                // 最低位为 0，仅右移
                crc >>= 1;
            }
        }
    }
    
    return crc;
}
```

**2. 固件验证**
- **服务器端**：
  - 上传固件时计算并存储 CRC32 值
  - 将 CRC32 值与固件信息关联
  
- **ESP8266 端**：
  - 下载固件时验证 CRC32
  - 确保从服务器下载的固件完整无误
  
- **STM32 端**：
  - 接收完成后再次验证 CRC32
  - 确保通过 UART 传输的数据完整无误
  - 验证通过后才设置应用程序有效标志

**验证流程**：
1. 服务器：固件上传 → 计算 CRC32 → 存储
2. ESP8266：下载固件 → 验证 CRC32 → 分包发送
3. STM32：接收数据 → 实时计算 CRC32 → 接收完成后最终验证
4. STM32：比较本地计算的 CRC32 与服务器提供的 CRC32
5. 验证通过：设置有效标志，跳转应用程序
6. 验证失败：保持在 Bootloader，等待重试

### 配置与使用注意事项

#### 常见问题与解决方案

| 问题 | 原因 | 解决方案 |
|------|------|----------|
| **通信失败** | 波特率不匹配 | 确保双方波特率均为 115200 |
| **数据丢失** | 缓冲区溢出 | 减小数据包大小，增加接收超时 |
| **升级失败** | Flash 写入错误 | 检查 Flash 擦除是否正确，使用正确的扇区 |
| **ESP8266 重启** | 电源不稳定 | 使用独立 3.3V 电源，增加电容滤波 |
| **STM32 无法启动** | 固件损坏 | 保留 Bootloader，使用临时存储区 |

#### 性能参数

| 参数 | 值 | 说明 |
|------|-----|------|
| **通信速率** | 115200 bps | 约 14.4 KB/s |
| **数据包大小** | 1024 字节 | 平衡速度和可靠性 |
| **1MB 固件传输时间** | ~70 秒 | 包含验证时间 |
| **心跳间隔** | 30 秒 | 平衡实时性和功耗 |
| **升级检查间隔** | 5 分钟 | 减少服务器负载 |

### 代码优化建议

#### 性能优化

1. **增加数据包大小**
   - 可将数据包大小从 1024 字节增加到 2048 字节
   - 减少通信开销，提高传输速度

2. **使用 DMA 传输**
   - STM32 配置 UART DMA
   - 减少 CPU 占用，提高数据吞吐量

3. **优化 Flash 操作**
   - 使用扇区级擦除，减少擦除次数
   - 批量写入，减少 Flash 写入操作

#### 安全性增强

1. **添加数据加密**
   - 对传输的固件数据进行加密
   - 防止固件被篡改

2. **设备认证**
   - 在注册时添加设备认证
   - 防止未授权设备接入

3. **固件签名**
   - 对固件进行数字签名
   - STM32 验证签名后再运行

### 实际应用示例

#### 智能家居场景

**功能需求**：
- 远程更新温湿度传感器固件
- 支持批量设备管理
- 低功耗运行

**配置方案**：
1. **ESP8266 配置**：
   - 使用深度睡眠模式
   - 每 30 分钟唤醒检查更新
   - 完成后立即睡眠

2. **STM32 配置**：
   - 使用低功耗模式
   - 仅在需要时唤醒
   - 优化传感器采样频率

**通信优化**：
- 减少心跳频率
- 使用 MQTT 协议替代 HTTP
- 实现差分升级，减少数据传输量

#### 工业控制场景

**功能需求**：
- 高可靠性升级
- 实时监控升级状态
- 支持远程诊断

**配置方案**：
1. **硬件冗余**：
   - 双 ESP8266 模块备份
   - 独立电源供应

2. **通信增强**：
   - 使用有线网络备份
   - 实现断点续传
   - 增加详细的日志记录

3. **安全措施**：
   - 端到端加密
   - 多级权限控制
   - 固件版本回滚机制

## 编译和烧录

### ESP8266 固件

```bash
cd esp8266_firmware
pio run
pio run --target upload
```

### STM32 Bootloader

```bash
cd stm32f403_bootloader
pio run
pio run --target upload
```

### STM32 应用程序

```bash
cd stm32f403_application
pio run
pio run --target upload
```

## 服务器部署

### 安装依赖

```bash
cd server
pip install -r requirements.txt
```

### 启动服务器

```bash
python server.py
```

服务器将在 `http://0.0.0.0:5000` 上运行。

### Web 管理界面

启动服务器后，在浏览器中访问 `http://localhost:5000` 即可打开 Web 管理界面。

#### 功能特性

- **设备管理**：查看、添加、删除设备，搜索设备，查看设备详情
- **固件管理**：上传、下载、删除固件，查看固件信息
- **推送更新**：选择固件和设备，批量推送更新，查看推送状态
- **操作日志**：实时显示操作记录，支持清空日志

#### 使用说明

1. **设备管理**
   - 点击"刷新设备列表"获取最新设备状态
   - 点击"添加设备"手动注册新设备
   - 在搜索框中输入关键词过滤设备
   - 点击"查看"查看设备详细信息
   - 点击"删除"移除设备

2. **固件管理**
   - 点击"刷新固件列表"获取最新固件信息
   - 点击"上传固件"上传新的固件文件
   - 点击"下载"下载固件文件
   - 点击"删除"移除固件

3. **推送更新**
   - 勾选"全部设备"或选择特定设备
   - 从下拉列表中选择要推送的固件
   - 可选择"强制更新"忽略版本检查
   - 点击"推送更新"开始推送
   - 在"推送状态"区域查看更新进度

4. **操作日志**
   - 所有操作都会记录在日志中
   - 日志保存在浏览器本地存储
   - 点击"清空日志"清除所有记录

#### 界面特性

- 响应式设计，支持桌面和移动设备
- 实时显示服务器连接状态
- 自动刷新设备列表和固件列表
- 本地日志存储，刷新页面不丢失
- 模态对话框操作，用户体验友好

### API 接口

#### 固件管理接口

| 方法 | 路径 | 说明 |
|------|------|------|
| POST | /firmware | 上传固件 |
| GET | /firmware/download/<filename> | 下载固件 |
| GET | /firmware/info | 获取所有固件信息 |
| GET | /firmware/latest | 获取最新固件 |
| DELETE | /firmware/<filename> | 删除固件 |

#### 设备管理接口

| 方法 | 路径 | 说明 |
|------|------|------|
| POST | /device/register | 注册设备 |
| POST | /device/heartbeat | 设备心跳 |
| POST | /device/check-update | 检查更新 |
| GET | /device/list | 获取设备列表 |
| GET | /device/<device_id> | 获取设备信息 |
| DELETE | /device/<device_id> | 删除设备 |

#### 推送更新接口

| 方法 | 路径 | 说明 |
|------|------|------|
| POST | /push/update | 推送更新到设备 |
| GET | /push/status/<device_id> | 获取推送状态 |
| DELETE | /push/clear/<device_id> | 清除待处理更新 |
| POST | /push/acknowledge | 确认更新完成 |

#### 系统接口

| 方法 | 路径 | 说明 |
|------|------|------|
| GET | /health | 健康检查 |

### 上传固件示例

```bash
curl -X POST http://localhost:5000/firmware \
  -F "file=@firmware.bin" \
  -F "version=1.0.0" \
  -F "description=Initial release"
```

### 推送更新示例

```bash
# 推送到指定设备
curl -X POST http://localhost:5000/push/update \
  -H "Content-Type: application/json" \
  -d '{
    "device_ids": ["STM32_AABBCCDDEEFF", "STM32_112233445566"],
    "firmware_filename": "firmware_1.0.0_20240101_120000.bin",
    "force_update": false
  }'

# 推送到所有设备
curl -X POST http://localhost:5000/push/update \
  -H "Content-Type: application/json" \
  -d '{
    "device_ids": [],
    "firmware_filename": "firmware_1.0.0_20240101_120000.bin",
    "force_update": false
  }'
```

### 设备管理工具

项目提供了设备管理工具 `tools/device_manager.py`，可以方便地管理设备和推送更新。

```bash
# 运行设备管理工具
python tools/device_manager.py
```

功能包括：
- 列出所有设备
- 查看设备详情
- 注册/删除设备
- 推送更新到指定设备或所有设备
- 查看推送状态
- 清除待处理更新
- 上传/查看固件

## 配置说明

### ESP8266 配置

修改 `esp8266_firmware/config.ini`：

```ini
[WiFi]
SSID=YourWiFiSSID
Password=YourWiFiPassword

[Server]
URL=http://your-server.com
Port=5000

[Communication]
BaudRate=115200
PacketSize=512
MaxRetries=3
Timeout=5000

[GPIO]
RX_Pin=5
TX_Pin=4
LED_Pin=2
```

或在 `esp8266_firmware/platformio.ini` 中修改：

```ini
build_flags =
    -DWIFI_SSID=\"YourWiFiSSID\"
    -DWIFI_PASSWORD=\"YourWiFiPassword\"
    -DSERVER_BASE_URL=\"http://your-server.com\"
    -DSERVER_PORT=5000
    -DPACKET_SIZE=512
```

### STM32 配置

修改 `platformio.ini` 中的编译选项和上传协议。

## 安全建议

1. 使用 HTTPS 加密固件传输
2. 固件添加数字签名验证
3. 实现回滚机制
4. 添加版本兼容性检查
5. 限制升级频率

## 故障排除

### Bootloader 无法启动应用程序

- 检查应用程序有效标志是否正确设置
- 验证应用程序向量表是否有效
- 检查 Flash 是否正确写入

### ESP8266 无法连接 WiFi

- 检查 SSID 和密码是否正确
- 确认 ESP8266 电源稳定
- 检查天线连接

### 固件下载失败

- 检查服务器 URL 是否正确
- 验证网络连接
- 检查固件文件大小是否超限

### 推送更新未生效

- 确认设备已成功注册到服务器
- 检查设备心跳是否正常发送
- 验证推送的固件文件是否存在
- 查看设备推送状态确认更新是否已排队
- 检查设备是否在线（状态应为 online）

### 设备无法注册

- 确认服务器正在运行
- 检查网络连接
- 验证服务器地址和端口配置
- 查看服务器日志获取详细错误信息

### 升级后设备无法启动

- 检查固件是否正确编译
- 验证固件版本兼容性
- 检查 Flash 是否正确写入
- 使用 ST-Link 重新烧录 Bootloader
