# STM32F103C8T6 + ESP8266 OTA 设备

这是一个基于 STM32F103C8T6 和 ESP8266 的 OTA 设备示例，包含闪灯程序和 OTA 升级功能。

## 项目结构

```
stm32_esp8266_ota/
├── stm32/
│   ├── Core/
│   │   ├── Inc/
│   │   ├── Src/
│   │   └── startup/
│   ├── Drivers/
│   └── ota/
├── esp8266/
│   ├── src/
│   ├── lib/
│   └── platformio.ini
└── README.md
```

## 硬件连接

### STM32F103 与 ESP8266 连接

| STM32F103 | ESP8266 |
|-----------|---------|
| PA9 (TX)  | RX      |
| PA10 (RX) | TX      |
| 3.3V      | 3.3V    |
| GND       | GND     |

### 闪灯硬件

| STM32F103 | 硬件 |
|-----------|------|
| PC13      | LED  |

## 软件功能

1. **闪灯功能**：LED 每 1 秒闪烁一次
2. **网络连接**：通过 ESP8266 连接到 WiFi
3. **OTA 注册**：设备启动时向 OTA 服务器注册
4. **心跳包**：定期发送心跳包保持在线状态
5. **固件更新**：接收并应用 OTA 服务器推送的固件更新

## 配置

### ESP8266 配置

在 `esp8266/src/main.cpp` 中配置 WiFi 信息：

```cpp
const char* ssid = "your_ssid";
const char* password = "your_password";
const char* ota_server = "http://192.168.60.71:5000";
```

### STM32 配置

在 `stm32/ota/ota_config.h` 中配置设备信息：

```c
#define DEVICE_ID "STM32F103_001"
#define DEVICE_TYPE "stm32f103"
#define CURRENT_VERSION "1.0.0"
```

## 编译和烧录

### ESP8266

使用 PlatformIO 编译并烧录：

```bash
cd esp8266
platformio run -t upload
```

### STM32F103

使用 Keil MDK 或 STM32CubeIDE 编译并烧录。

## OTA 系统接入

1. 启动 OTA 服务器
2. 设备上电后会自动连接 WiFi 并注册到 OTA 系统
3. 在 OTA 系统的 "设备管理" 页面可以看到设备
4. 上传新固件后，可以向设备推送更新

## 固件更新流程

1. 设备定期检查 OTA 服务器的更新指令
2. 收到更新指令后，通过 ESP8266 下载新固件
3. 验证固件完整性
4. 写入新固件到 Flash
5. 重启设备运行新固件

## 注意事项

1. 确保 ESP8266 和 STM32F103 的串口波特率一致（默认 115200）
2. 确保 OTA 服务器地址正确配置
3. 设备需要稳定的电源供应
4. 固件更新过程中不要断电

## 故障排查

- **设备不在线**：检查 WiFi 连接和 OTA 服务器地址
- **固件更新失败**：检查网络连接和固件文件
- **闪灯不正常**：检查硬件连接和代码逻辑

## 许可证

MIT License
