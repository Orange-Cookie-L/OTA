import requests
import json
from datetime import datetime

SERVER_URL = "http://localhost:5000"

class DeviceManager:
    def __init__(self, server_url=SERVER_URL):
        self.server_url = server_url.rstrip('/')
    
    def register_device(self, device_id, device_type='stm32f403', current_version='1.0.0'):
        print(f"注册设备: {device_id}")
        data = {
            'device_id': device_id,
            'device_type': device_type,
            'current_version': current_version
        }
        response = requests.post(f"{self.server_url}/device/register", json=data)
        print(f"状态码: {response.status_code}")
        print(f"响应: {response.json()}")
        print()
        return response.json()
    
    def list_devices(self):
        print("获取设备列表...")
        response = requests.get(f"{self.server_url}/device/list")
        print(f"状态码: {response.status_code}")
        data = response.json()
        print(f"设备数量: {data['device_count']}")
        print("设备列表:")
        for device in data['devices']:
            print(f"  - ID: {device['device_id']}")
            print(f"    类型: {device['device_type']}")
            print(f"    版本: {device['current_version']}")
            print(f"    状态: {device['status']}")
            print(f"    IP: {device['ip_address']}")
            print(f"    注册时间: {device['registered_at']}")
            print()
        return data
    
    def get_device(self, device_id):
        print(f"获取设备信息: {device_id}")
        response = requests.get(f"{self.server_url}/device/{device_id}")
        print(f"状态码: {response.status_code}")
        if response.status_code == 200:
            print(f"设备信息: {response.json()}")
        print()
        return response.json()
    
    def delete_device(self, device_id):
        print(f"删除设备: {device_id}")
        response = requests.delete(f"{self.server_url}/device/{device_id}")
        print(f"状态码: {response.status_code}")
        print(f"响应: {response.json()}")
        print()
        return response.json()
    
    def push_update(self, device_ids, firmware_filename, force_update=False):
        print(f"推送更新到设备: {device_ids}")
        data = {
            'device_ids': device_ids,
            'firmware_filename': firmware_filename,
            'force_update': force_update
        }
        response = requests.post(f"{self.server_url}/push/update", json=data)
        print(f"状态码: {response.status_code}")
        print(f"响应: {response.json()}")
        print()
        return response.json()
    
    def push_update_to_all(self, firmware_filename, force_update=False):
        print(f"推送更新到所有设备")
        data = {
            'device_ids': [],
            'firmware_filename': firmware_filename,
            'force_update': force_update
        }
        response = requests.post(f"{self.server_url}/push/update", json=data)
        print(f"状态码: {response.status_code}")
        print(f"响应: {response.json()}")
        print()
        return response.json()
    
    def get_push_status(self, device_id):
        print(f"获取推送状态: {device_id}")
        response = requests.get(f"{self.server_url}/push/status/{device_id}")
        print(f"状态码: {response.status_code}")
        print(f"响应: {response.json()}")
        print()
        return response.json()
    
    def clear_pending_updates(self, device_id):
        print(f"清除待处理更新: {device_id}")
        response = requests.delete(f"{self.server_url}/push/clear/{device_id}")
        print(f"状态码: {response.status_code}")
        print(f"响应: {response.json()}")
        print()
        return response.json()
    
    def upload_firmware(self, file_path, version, description=''):
        print(f"上传固件: {file_path}")
        with open(file_path, 'rb') as f:
            files = {'file': f}
            data = {
                'version': version,
                'description': description
            }
            response = requests.post(f"{self.server_url}/firmware", files=files, data=data)
        print(f"状态码: {response.status_code}")
        print(f"响应: {response.json()}")
        print()
        return response.json()
    
    def get_firmware_info(self):
        print("获取固件信息...")
        response = requests.get(f"{self.server_url}/firmware/info")
        print(f"状态码: {response.status_code}")
        data = response.json()
        print(f"固件数量: {data['firmware_count']}")
        print("固件列表:")
        for fw in data['firmware_list']:
            print(f"  - 文件名: {fw['filename']}")
            print(f"    大小: {fw['size']} 字节")
            print(f"    CRC32: {fw['crc32']}")
            print()
        return data
    
    def get_latest_firmware(self):
        print("获取最新固件...")
        response = requests.get(f"{self.server_url}/firmware/latest")
        print(f"状态码: {response.status_code}")
        if response.status_code == 200:
            print(f"最新固件: {response.json()}")
        print()
        return response.json()

def print_menu():
    print("=" * 50)
    print("设备管理工具")
    print("=" * 50)
    print("1. 列出所有设备")
    print("2. 查看设备详情")
    print("3. 注册设备")
    print("4. 删除设备")
    print("5. 推送更新到指定设备")
    print("6. 推送更新到所有设备")
    print("7. 查看推送状态")
    print("8. 清除待处理更新")
    print("9. 上传固件")
    print("10. 查看固件信息")
    print("11. 查看最新固件")
    print("0. 退出")
    print("=" * 50)

if __name__ == "__main__":
    print("=" * 50)
    print("设备管理工具")
    print("=" * 50)
    print()
    
    try:
        manager = DeviceManager()
        
        while True:
            print_menu()
            choice = input("请选择操作: ")
            
            if choice == '1':
                manager.list_devices()
            elif choice == '2':
                device_id = input("请输入设备ID: ")
                manager.get_device(device_id)
            elif choice == '3':
                device_id = input("请输入设备ID: ")
                device_type = input("请输入设备类型 (默认: stm32f403): ") or 'stm32f403'
                current_version = input("请输入当前版本 (默认: 1.0.0): ") or '1.0.0'
                manager.register_device(device_id, device_type, current_version)
            elif choice == '4':
                device_id = input("请输入设备ID: ")
                manager.delete_device(device_id)
            elif choice == '5':
                device_ids = input("请输入设备ID (多个设备用逗号分隔): ").split(',')
                device_ids = [d.strip() for d in device_ids if d.strip()]
                firmware_filename = input("请输入固件文件名: ")
                force = input("强制更新? (y/n, 默认: n): ").lower() == 'y'
                manager.push_update(device_ids, firmware_filename, force)
            elif choice == '6':
                firmware_filename = input("请输入固件文件名: ")
                force = input("强制更新? (y/n, 默认: n): ").lower() == 'y'
                manager.push_update_to_all(firmware_filename, force)
            elif choice == '7':
                device_id = input("请输入设备ID: ")
                manager.get_push_status(device_id)
            elif choice == '8':
                device_id = input("请输入设备ID: ")
                manager.clear_pending_updates(device_id)
            elif choice == '9':
                file_path = input("请输入固件文件路径: ")
                version = input("请输入版本号: ")
                description = input("请输入描述 (可选): ")
                manager.upload_firmware(file_path, version, description)
            elif choice == '10':
                manager.get_firmware_info()
            elif choice == '11':
                manager.get_latest_firmware()
            elif choice == '0':
                print("退出程序")
                break
            else:
                print("无效选择，请重试")
            
            input("按 Enter 继续...")
            print()
        
    except requests.exceptions.ConnectionError:
        print("错误: 无法连接到服务器。请确保服务器正在运行。")
    except KeyboardInterrupt:
        print("\n用户中断")
    except Exception as e:
        print(f"错误: {e}")
