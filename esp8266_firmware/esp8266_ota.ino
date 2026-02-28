#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include <ArduinoJson.h>
#include "ota_protocol.h"

#define WIFI_SSID           "YourWiFiSSID"
#define WIFI_PASSWORD       "YourWiFiPassword"
#define SERVER_BASE_URL     "http://your-server.com"
#define SERVER_PORT         5000

#define RX_PIN              5
#define TX_PIN              4
#define LED_PIN             2

#define PACKET_SIZE         512
#define MAX_RETRIES         3
#define HEARTBEAT_INTERVAL  60000      // 延长心跳间隔至60秒
#define CHECK_UPDATE_INTERVAL 300000   // 5分钟检查一次更新
#define DEEP_SLEEP_DURATION 60000000   // 深度睡眠60秒 (单位: 微秒)
#define WAKEUP_PIN          16         // RST引脚，用于唤醒

#define DEVICE_ID_PREFIX    "STM32_"

WiFiClient client;
HTTPClient http;

uint8_t firmware_buffer[PACKET_SIZE];
uint32_t firmware_size = 0;
uint32_t total_received = 0;

char device_id[32];
char current_firmware_version[16] = "1.0.0";
char firmware_url[256] = {0};

unsigned long last_heartbeat = 0;
unsigned long last_update_check = 0;
uint8_t update_available = 0;
uint8_t update_in_progress = 0;

void setup() {
    Serial.begin(115200);
    Serial.swap();
    pinMode(LED_PIN, OUTPUT);
    pinMode(WAKEUP_PIN, INPUT_PULLUP);

    generateDeviceID();
    // 按需连接WiFi，不自动连接
    // 首次启动时注册设备
    connectWiFi();
    registerDevice();
    WiFi.disconnect();
    WiFi.mode(WIFI_OFF);
}

void wakeupSTM32(void) {
    // 唤醒STM32，通过串口发送唤醒信号
    Serial.write(0xAA); // 唤醒命令
    delay(100); // 等待STM32唤醒
}

void loop() {
    unsigned long current_time = millis();
    uint8_t need_sleep = 1;

    // 检查是否需要发送心跳
    if (current_time - last_heartbeat >= HEARTBEAT_INTERVAL) {
        digitalWrite(LED_PIN, HIGH);
        wakeupSTM32(); // 唤醒STM32
        connectWiFi();
        sendHeartbeat();
        last_heartbeat = current_time;
        need_sleep = 0;
        WiFi.disconnect();
        WiFi.mode(WIFI_OFF);
        digitalWrite(LED_PIN, LOW);
    }

    // 检查是否需要检查更新
    if (!update_in_progress && current_time - last_update_check >= CHECK_UPDATE_INTERVAL) {
        digitalWrite(LED_PIN, HIGH);
        wakeupSTM32(); // 唤醒STM32
        connectWiFi();
        checkForUpdate();
        last_update_check = current_time;
        need_sleep = 0;
        WiFi.disconnect();
        WiFi.mode(WIFI_OFF);
        digitalWrite(LED_PIN, LOW);
    }

    // 检查是否有可用更新
    if (update_available && !update_in_progress) {
        digitalWrite(LED_PIN, HIGH);
        wakeupSTM32(); // 唤醒STM32
        connectWiFi();
        startPushedUpdate();
        need_sleep = 0;
        digitalWrite(LED_PIN, LOW);
    }

    // 检查是否有串口命令
    if (Serial.available()) {
        digitalWrite(LED_PIN, HIGH);
        uint8_t cmd = Serial.read();
        if (cmd == 0xAA) {
            // 忽略唤醒信号
        } else {
            processCommand(cmd);
        }
        need_sleep = 0;
        digitalWrite(LED_PIN, LOW);
    }

    // 进入深度睡眠
    if (need_sleep && !update_in_progress) {
        digitalWrite(LED_PIN, LOW);
        WiFi.disconnect();
        WiFi.mode(WIFI_OFF);
        ESP.deepSleep(DEEP_SLEEP_DURATION);
    }
}

void generateDeviceID() {
    uint8_t mac[6];
    WiFi.macAddress(mac);
    sprintf(device_id, "%s%02X%02X%02X%02X%02X%02X", 
            DEVICE_ID_PREFIX, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

void connectWiFi() {
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    while (WiFi.status() != WL_CONNECTED) {
        digitalWrite(LED_PIN, !digitalRead(LED_PIN));
        delay(500);
    }

    digitalWrite(LED_PIN, HIGH);
}

void registerDevice() {
    String url = String(SERVER_BASE_URL) + ":" + String(SERVER_PORT) + "/device/register";
    
    http.begin(url);
    http.addHeader("Content-Type", "application/json");

    String payload = "{\"device_id\":\"" + String(device_id) + 
                     "\",\"device_type\":\"stm32f403\"," +
                     "\"current_version\":\"" + String(current_firmware_version) + "\"}";

    int httpCode = http.POST(payload);
    http.end();
}

void sendHeartbeat() {
    // 按需连接WiFi
    if (WiFi.status() != WL_CONNECTED) {
        connectWiFi();
    }

    String url = String(SERVER_BASE_URL) + ":" + String(SERVER_PORT) + "/device/heartbeat";
    
    http.begin(url);
    http.addHeader("Content-Type", "application/json");

    String payload = "{\"device_id\":\"" + String(device_id) + 
                     "\",\"current_version\":\"" + String(current_firmware_version) + "\"}";

    int httpCode = http.POST(payload);

    if (httpCode == HTTP_CODE_OK) {
        String response = http.getString();
        DynamicJsonDocument doc(1024);
        deserializeJson(doc, response);

        if (doc.containsKey("update_required") && doc["update_required"] == true) {
            update_available = 1;
            if (doc.containsKey("update") && doc["update"].containsKey("firmware_url")) {
                strncpy(firmware_url, doc["update"]["firmware_url"], sizeof(firmware_url) - 1);
            }
        }
    }

    http.end();

    // 通信完成后断开WiFi
    if (!update_in_progress) {
        WiFi.disconnect();
        WiFi.mode(WIFI_OFF);
    }
}

void checkForUpdate() {
    // 按需连接WiFi
    if (WiFi.status() != WL_CONNECTED) {
        connectWiFi();
    }

    String url = String(SERVER_BASE_URL) + ":" + String(SERVER_PORT) + "/device/check-update";
    
    http.begin(url);
    http.addHeader("Content-Type", "application/json");

    String payload = "{\"device_id\":\"" + String(device_id) + 
                     "\",\"current_version\":\"" + String(current_firmware_version) + "\"}";

    int httpCode = http.POST(payload);

    if (httpCode == HTTP_CODE_OK) {
        String response = http.getString();
        DynamicJsonDocument doc(1024);
        deserializeJson(doc, response);

        if (doc.containsKey("update_available") && doc["update_available"] == true) {
            update_available = 1;
            if (doc.containsKey("latest_firmware") && doc["latest_firmware"].containsKey("url")) {
                strncpy(firmware_url, doc["latest_firmware"]["url"], sizeof(firmware_url) - 1);
            }
        }
    }

    http.end();

    // 通信完成后断开WiFi
    if (!update_in_progress) {
        WiFi.disconnect();
        WiFi.mode(WIFI_OFF);
    }
}

void startPushedUpdate() {
    update_in_progress = 1;
    update_available = 0;
    
    OTA_Packet_t packet;
    packet.cmd = OTA_CMD_START;
    packet.offset = 0;
    packet.length = 0;
    sendPacket(&packet);
}

void acknowledgeUpdate(uint8_t success) {
    String url = String(SERVER_BASE_URL) + ":" + String(SERVER_PORT) + "/push/acknowledge";
    
    http.begin(url);
    http.addHeader("Content-Type", "application/json");

    String payload = "{\"device_id\":\"" + String(device_id) + 
                     "\",\"firmware_filename\":\"" + String(current_firmware_version) + 
                     "\",\"success\":" + String(success ? "true" : "false") + "}";

    http.POST(payload);
    http.end();
}

void processCommand(uint8_t cmd) {
    switch (cmd) {
        case OTA_CMD_QUERY_VERSION:
            sendVersionResponse();
            break;

        case OTA_CMD_START:
            handleStartCommand();
            break;

        case OTA_CMD_DATA:
            handleDataAck();
            break;

        case OTA_CMD_END:
            handleEndCommand();
            break;

        case OTA_CMD_ACK:
        case OTA_CMD_NACK:
            handleAckNack(cmd);
            break;

        default:
            break;
    }
}

void sendVersionResponse() {
    OTA_Packet_t packet;
    packet.cmd = OTA_CMD_VERSION_RESP;
    packet.offset = 0x0001;
    packet.length = 0;
    sendPacket(&packet);
}

void handleStartCommand() {
    OTA_Packet_t packet;

    if (downloadFirmwareInfo()) {
        packet.cmd = OTA_CMD_ACK;
        packet.offset = firmware_size;
        packet.length = 0;
        sendPacket(&packet);
        total_received = 0;
    } else {
        packet.cmd = OTA_CMD_NACK;
        packet.offset = 0;
        packet.length = 0;
        sendPacket(&packet);
        update_in_progress = 0;
    }
}

uint8_t downloadFirmwareInfo() {
    String url;
    if (strlen(firmware_url) > 0) {
        url = String(SERVER_BASE_URL) + ":" + String(SERVER_PORT) + firmware_url;
    } else {
        url = String(SERVER_BASE_URL) + ":" + String(SERVER_PORT) + "/firmware/latest";
    }

    http.begin(url);
    int httpCode = http.GET();

    if (httpCode == HTTP_CODE_OK) {
        String response = http.getString();
        DynamicJsonDocument doc(1024);
        deserializeJson(doc, response);

        if (doc.containsKey("size")) {
            firmware_size = doc["size"];
            http.end();
            return 1;
        } else if (http.getSize() > 0) {
            firmware_size = http.getSize();
            http.end();
            return 1;
        }
    }

    http.end();
    return 0;
}

void handleDataAck() {
    if (total_received < firmware_size) {
        sendNextPacket();
    }
}

void sendNextPacket() {
    OTA_Packet_t packet;
    uint16_t chunk_size;

    chunk_size = (firmware_size - total_received) > PACKET_SIZE ?
                 PACKET_SIZE : (firmware_size - total_received);

    if (downloadFirmwareChunk(total_received, chunk_size)) {
        packet.cmd = OTA_CMD_DATA;
        packet.offset = total_received;
        packet.length = chunk_size;
        memcpy(packet.data, firmware_buffer, chunk_size);
        sendPacket(&packet);
    } else {
        packet.cmd = OTA_CMD_NACK;
        packet.offset = total_received;
        packet.length = 0;
        sendPacket(&packet);
    }
}

uint8_t downloadFirmwareChunk(uint32_t offset, uint16_t size) {
    WiFiClient stream;
    String url = String(SERVER_URL);

    if (http.begin(url)) {
        http.addHeader("Range", String("bytes=") + String(offset) + "-" + String(offset + size - 1));
        int httpCode = http.GET();

        if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_PARTIAL_CONTENT) {
            WiFiClient *client = http.getStreamPtr();
            size_t available = client->available();

            if (available >= size) {
                client->readBytes(firmware_buffer, size);
                http.end();
                return 1;
            }
        }
        http.end();
    }

    return 0;
}

void handleAckNack(uint8_t cmd) {
    if (cmd == OTA_CMD_ACK) {
        total_received += PACKET_SIZE;
        if (total_received < firmware_size) {
            sendNextPacket();
        }
    } else {
        sendNextPacket();
    }
}

void handleEndCommand() {
    OTA_Packet_t packet;
    uint32_t crc = calculateCRC32();

    packet.cmd = OTA_CMD_ACK;
    packet.offset = 0;
    packet.length = 4;
    memcpy(packet.data, &crc, 4);
    sendPacket(&packet);
}

uint32_t calculateCRC32() {
    uint32_t crc = 0xFFFFFFFF;
    uint32_t i, j;

    for (i = 0; i < firmware_size; i++) {
        crc ^= firmware_buffer[i % PACKET_SIZE];
        for (j = 0; j < 8; j++) {
            if (crc & 1) {
                crc = (crc >> 1) ^ 0xEDB88320;
            } else {
                crc >>= 1;
            }
        }
    }

    return crc ^ 0xFFFFFFFF;
}

void sendPacket(OTA_Packet_t *packet) {
    uint8_t buffer[OTA_PACKET_HEADER_SIZE + OTA_MAX_PACKET_SIZE];
    uint16_t offset = 0;

    buffer[offset++] = packet->cmd;
    buffer[offset++] = (packet->offset >> 24) & 0xFF;
    buffer[offset++] = (packet->offset >> 16) & 0xFF;
    buffer[offset++] = (packet->offset >> 8) & 0xFF;
    buffer[offset++] = packet->offset & 0xFF;
    buffer[offset++] = (packet->length >> 8) & 0xFF;
    buffer[offset++] = packet->length & 0xFF;
    memcpy(&buffer[offset], packet->data, packet->length);

    Serial.write(buffer, offset + packet->length);
}
