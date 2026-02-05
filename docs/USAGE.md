# STM32F403 + ESP8266 远程升级系统 - 使用指南

## 快速开始

### 1. 硬件连接

```
STM32F403          ESP8266
--------          -------
PA9 (TX)  ------>  RX (GPIO5)
PA10 (RX) <------  TX (GPIO4)
GND       <------  GND
3.3V      ------>  VCC
```

### 2. 编译和烧录

#### STM32 Bootloader
1. 使用 STM32CubeIDE 打开 `stm32_bootloader` 项目
2. 配置链接脚本为 `STM32F403VCTx_FLASH.ld`
3. 编译并烧录到 STM32F403

#### STM32 应用程序
1. 使用 STM32CubeIDE 打开 `stm32_app` 项目
2. 配置链接脚本为 `STM32F403VCTx_FLASH.ld`
3. 编译并烧录到 STM32F403 (地址 0x08010000)

#### ESP8266 固件
1. 使用 Arduino IDE 或 PlatformIO
2. 修改 `esp8266_ota.ino` 中的 WiFi 配置
3. 上传到 ESP8266

### 3. 启动服务器

```bash
cd server
pip install -r requirements.txt
python server.py
```

服务器将在 `http://localhost:8080` 启动

### 4. 上传固件

```bash
curl -X POST -F "file=@firmware.bin" http://localhost:8080/firmware/upload
```

### 5. 触发远程升级

1. STM32 Bootloader 启动时会查询 ESP8266
2. ESP8266 从服务器下载固件
3. ESP8266 将固件传输给 STM32
4. STM32 验证并写入 Flash
5. STM32 跳转到新固件

## API 接口

### 列出所有固件
```
GET /firmware
```

### 下载固件
```
GET /firmware/<filename>
```

### 上传固件
```
POST /firmware/upload
Content-Type: multipart/form-data
Body: file=<firmware.bin>
```

### 删除固件
```
DELETE /firmware/<filename>
```

### 获取固件信息
```
GET /firmware/<filename>/info
```

## 故障排除

### Bootloader 无法启动
- 检查链接脚本地址配置
- 确认 Bootloader 烧录到 0x08000000

### ESP8266 无法连接 WiFi
- 检查 WiFi SSID 和密码配置
- 确认 ESP8266 供电稳定

### 固件下载失败
- 检查服务器 URL 配置
- 确认网络连接正常
- 检查服务器日志

### CRC 校验失败
- 确认固件文件完整
- 检查传输过程中的数据完整性

## 安全建议

1. 使用 HTTPS 传输固件
2. 对固件进行数字签名
3. 实现固件版本回滚机制
4. 添加升级超时保护
5. 记录升级日志
