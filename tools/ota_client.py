import serial
import time
import struct

SERIAL_PORT = "COM3"
BAUD_RATE = 115200
TIMEOUT = 5

class OTAClient:
    def __init__(self, port, baudrate=115200):
        self.ser = serial.Serial(port, baudrate, timeout=TIMEOUT)
        time.sleep(2)
    
    def close(self):
        self.ser.close()
    
    def send_packet(self, cmd, offset=0, length=0, data=b''):
        header = struct.pack('>BIH', cmd, offset, length)
        packet = header + data
        self.ser.write(packet)
    
    def receive_packet(self):
        header = self.ser.read(7)
        if len(header) != 7:
            return None
        
        cmd = header[0]
        offset = struct.unpack('>I', header[1:5])[0]
        length = struct.unpack('>H', header[5:7])[0]
        
        data = b''
        if length > 0:
            data = self.ser.read(length)
        
        return {
            'cmd': cmd,
            'offset': offset,
            'length': length,
            'data': data
        }
    
    def query_version(self):
        print("查询版本...")
        self.send_packet(0x06)
        response = self.receive_packet()
        if response:
            print(f"版本: 0x{response['offset']:04X}")
        return response
    
    def start_ota(self):
        print("开始 OTA 升级...")
        self.send_packet(0x01)
        response = self.receive_packet()
        if response and response['cmd'] == 0x04:
            print(f"固件大小: {response['offset']} 字节")
            return response['offset']
        return None
    
    def send_data(self, offset, data):
        self.send_packet(0x02, offset, len(data), data)
        response = self.receive_packet()
        return response and response['cmd'] == 0x04
    
    def end_ota(self, crc32):
        crc_data = struct.pack('<I', crc32)
        self.send_packet(0x03, 0, 4, crc_data)
        response = self.receive_packet()
        return response and response['cmd'] == 0x04

def calculate_crc32(data):
    crc = 0xFFFFFFFF
    for byte in data:
        crc ^= byte
        for _ in range(8):
            if crc & 1:
                crc = (crc >> 1) ^ 0xEDB88320
            else:
                crc >>= 1
    return crc ^ 0xFFFFFFFF

def perform_ota(client, firmware_path, packet_size=512):
    print(f"开始 OTA 升级: {firmware_path}")
    
    with open(firmware_path, 'rb') as f:
        firmware_data = f.read()
    
    firmware_size = len(firmware_data)
    crc32 = calculate_crc32(firmware_data)
    
    print(f"固件大小: {firmware_size} 字节")
    print(f"CRC32: 0x{crc32:08X}")
    
    if not client.start_ota():
        print("OTA 启动失败")
        return False
    
    offset = 0
    while offset < firmware_size:
        chunk_size = min(packet_size, firmware_size - offset)
        chunk = firmware_data[offset:offset + chunk_size]
        
        print(f"发送数据: {offset}/{firmware_size} ({offset*100//firmware_size}%)")
        
        if not client.send_data(offset, chunk):
            print(f"数据发送失败，偏移: {offset}")
            return False
        
        offset += chunk_size
        time.sleep(0.01)
    
    print("发送完成，等待 CRC 验证...")
    
    if client.end_ota(crc32):
        print("OTA 升级成功!")
        return True
    else:
        print("OTA 升级失败: CRC 验证失败")
        return False

if __name__ == "__main__":
    print("=" * 50)
    print("OTA 客户端测试脚本")
    print("=" * 50)
    print()
    
    try:
        client = OTAClient(SERIAL_PORT, BAUD_RATE)
        
        client.query_version()
        
        firmware_path = input("请输入固件文件路径 (或按 Enter 跳过): ")
        if firmware_path:
            perform_ota(client, firmware_path)
        
        client.close()
        
    except serial.SerialException as e:
        print(f"串口错误: {e}")
    except KeyboardInterrupt:
        print("\n用户中断")
    except Exception as e:
        print(f"错误: {e}")
