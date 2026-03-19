#include <ESP8266WiFi.h>      // ESP8266 WiFi库
#include <ESP8266HTTPClient.h> // ESP8266 HTTP客户端库
#include <ArduinoJson.h>       // JSON解析库

// WiFi 配置
const char* ssid = "your_ssid";     // WiFi名称
const char* password = "your_password"; // WiFi密码

// OTA 服务器配置
const char* ota_server = "http://192.168.60.71:5000"; // OTA服务器地址

// 串口配置
#define BAUD_RATE 115200 // 串口波特率

// 设备信息（将从STM32获取）
String device_id = "STM32F103C8T6_001"; // 设备唯一标识符
String device_type = "stm32f103c8t6";   // 设备类型
String current_version = "1.0.0";       // 当前固件版本
String ip_address = "";                // 设备IP地址

// 状态标志
bool wifi_connected = false;    // WiFi是否连接
bool device_registered = false; // 设备是否已注册到OTA服务器

// 定时器
unsigned long last_heartbeat_time = 0;         // 上次心跳包发送时间
const unsigned long heartbeat_interval = 30000; // 心跳包间隔（30秒）

/**
  * @brief 初始化函数
  * @retval None
  */
void setup() {
  // 初始化串口
  Serial.begin(BAUD_RATE);
  Serial.println("ESP8266 初始化中...");

  // 连接 WiFi
  connectWiFi();

  // 获取IP地址
  ip_address = WiFi.localIP().toString();
  Serial.print("IP地址: ");
  Serial.println(ip_address);
}

/**
  * @brief 主循环函数
  * @retval None
  */
void loop() {
  // 检查WiFi连接
  if (!WiFi.isConnected()) {
    connectWiFi();
  }

  // 处理串口通信
  handleSerialCommunication();

  // 发送心跳包
  if (millis() - last_heartbeat_time > heartbeat_interval) {
    sendHeartbeat();
    last_heartbeat_time = millis();
  }
}

/**
  * @brief 连接WiFi
  * @retval None
  */
void connectWiFi() {
  Serial.print("连接 WiFi '" + String(ssid) + "'...");
  WiFi.begin(ssid, password);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println(" 成功!");
    wifi_connected = true;
  } else {
    Serial.println(" 失败!");
    wifi_connected = false;
  }
}

/**
  * @brief 处理串口通信
  * @retval None
  */
void handleSerialCommunication() {
  if (Serial.available() > 0) {
    String data = Serial.readStringUntil('\n');
    data.trim();
    
    // 处理STM32发送的数据
    if (data.startsWith("REGISTER:")) {
      // 解析设备信息
      int comma1 = data.indexOf(',');
      int comma2 = data.indexOf(',', comma1 + 1);
      
      if (comma1 > 0 && comma2 > 0) {
        device_id = data.substring(9, comma1);
        device_type = data.substring(comma1 + 1, comma2);
        current_version = data.substring(comma2 + 1);
        
        Serial.println("设备信息更新:");
        Serial.print("ID: ");
        Serial.println(device_id);
        Serial.print("类型: ");
        Serial.println(device_type);
        Serial.print("版本: ");
        Serial.println(current_version);
        
        // 注册设备到OTA服务器
        registerDevice();
      }
    } else if (data.startsWith("HEARTBEAT")) {
      // 发送心跳包
      sendHeartbeat();
    } else if (data.startsWith("CHECK_UPDATE")) {
      // 检查更新
      checkForUpdate();
    }
  }
}

/**
  * @brief 注册设备到OTA服务器
  * @retval None
  */
void registerDevice() {
  if (!wifi_connected) {
    Serial.println("WiFi未连接，无法注册设备");
    return;
  }
  
  Serial.println("注册设备到OTA服务器...");
  
  HTTPClient http;
  String url = String(ota_server) + "/device/register";
  
  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  
  // 创建JSON数据
  StaticJsonDocument<256> doc;
  doc["device_id"] = device_id;
  doc["device_type"] = device_type;
  doc["current_version"] = current_version;
  
  String jsonString;
  serializeJson(doc, jsonString);
  
  // 发送POST请求
  int httpCode = http.POST(jsonString);
  
  if (httpCode > 0) {
    String response = http.getString();
    Serial.print("注册响应 (" + String(httpCode) + "): ");
    Serial.println(response);
    
    if (httpCode == 200) {
      device_registered = true;
      Serial.println("设备注册成功!");
    }
  } else {
    Serial.print("注册失败: ");
    Serial.println(http.errorToString(httpCode));
  }
  
  http.end();
}

/**
  * @brief 发送心跳包
  * @retval None
  */
void sendHeartbeat() {
  if (!wifi_connected || !device_registered) {
    return;
  }
  
  Serial.println("发送心跳包...");
  
  HTTPClient http;
  String url = String(ota_server) + "/device/heartbeat";
  
  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  
  // 创建JSON数据
  StaticJsonDocument<256> doc;
  doc["device_id"] = device_id;
  doc["ip_address"] = ip_address;
  
  String jsonString;
  serializeJson(doc, jsonString);
  
  // 发送POST请求
  int httpCode = http.POST(jsonString);
  
  if (httpCode > 0) {
    String response = http.getString();
    Serial.print("心跳响应 (" + String(httpCode) + "): ");
    Serial.println(response);
  } else {
    Serial.print("心跳失败: ");
    Serial.println(http.errorToString(httpCode));
  }
  
  http.end();
}

/**
  * @brief 检查固件更新
  * @retval None
  */
void checkForUpdate() {
  if (!wifi_connected || !device_registered) {
    return;
  }
  
  Serial.println("检查固件更新...");
  
  HTTPClient http;
  String url = String(ota_server) + "/device/check-update";
  
  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  
  // 创建JSON数据
  StaticJsonDocument<256> doc;
  doc["device_id"] = device_id;
  doc["current_version"] = current_version;
  
  String jsonString;
  serializeJson(doc, jsonString);
  
  // 发送POST请求
  int httpCode = http.POST(jsonString);
  
  if (httpCode > 0) {
    String response = http.getString();
    Serial.print("更新检查响应 (" + String(httpCode) + "): ");
    Serial.println(response);
    
    // 解析响应
    StaticJsonDocument<512> responseDoc;
    DeserializationError error = deserializeJson(responseDoc, response);
    
    if (!error) {
      if (responseDoc.containsKey("update_available") && responseDoc["update_available"] == true) {
        // 有更新可用
        String firmware_url = responseDoc["firmware_url"];
        String new_version = responseDoc["version"];
        
        Serial.print("发现新版本: ");
        Serial.println(new_version);
        Serial.print("固件URL: ");
        Serial.println(firmware_url);
        
        // 通知STM32有更新（使用标志位）
        Serial.println("UPDATE_AVAILABLE");
        
        // 模拟固件下载过程
        downloadFirmware(firmware_url);
      }
    }
  } else {
    Serial.print("更新检查失败: ");
    Serial.println(http.errorToString(httpCode));
  }
  
  http.end();
}

/**
  * @brief 下载固件并传输到STM32
  * @param firmware_url: 固件下载URL
  * @retval None
  */
void downloadFirmware(String firmware_url) {
  Serial.println("开始下载固件...");
  
  HTTPClient http;
  http.begin(firmware_url);
  
  // 发送GET请求
  int httpCode = http.GET();
  
  if (httpCode > 0) {
    if (httpCode == HTTP_CODE_OK) {
      // 获取固件大小
      size_t firmwareSize = http.getSize();
      Serial.print("固件大小: " + String(firmwareSize) + " 字节\n");
      
      // 发送固件大小给STM32
      Serial.print("FIRMWARE_SIZE,");
      Serial.println(firmwareSize);
      
      // 等待STM32确认
      delay(1000);
      
      // 读取固件数据并传输
      WiFiClient* stream = http.getStreamPtr();
      size_t bytesRead = 0;
      size_t chunkSize = 64; // 每次传输64字节
      uint8_t buffer[chunkSize];
      
      while (http.connected() && (bytesRead < firmwareSize)) {
        size_t bytesAvailable = stream->available();
        if (bytesAvailable) {
          size_t toRead = min(bytesAvailable, chunkSize);
          stream->readBytes(buffer, toRead);
          
          // 发送数据到STM32
          Serial.write(buffer, toRead);
          
          // 等待STM32确认
          while (!Serial.available()) {
            delay(10);
          }
          Serial.read(); // 读取确认字符
          
          bytesRead += toRead;
          
          // 显示进度
          int progress = (bytesRead * 100) / firmwareSize;
          Serial.print("下载进度: " + String(progress) + "%\r");
        }
        delay(10);
      }
      
      // 下载完成
      Serial.println("\n固件下载完成!");
      Serial.println("DOWNLOAD_COMPLETE");
    } else {
      Serial.print("下载失败，HTTP代码: " + String(httpCode) + "\n");
      Serial.println("DOWNLOAD_FAILED");
    }
  } else {
    Serial.print("连接失败: " + http.errorToString(httpCode) + "\n");
    Serial.println("DOWNLOAD_FAILED");
  }
  
  http.end();
}
