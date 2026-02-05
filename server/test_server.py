import requests
import time

SERVER_URL = "http://localhost:5000"

def test_health():
    print("测试健康检查...")
    response = requests.get(f"{SERVER_URL}/health")
    print(f"状态码: {response.status_code}")
    print(f"响应: {response.json()}")
    print()

def test_upload_firmware(file_path, version="1.0.0", description="Test firmware"):
    print(f"上传固件: {file_path}")
    with open(file_path, 'rb') as f:
        files = {'file': f}
        data = {
            'version': version,
            'description': description
        }
        response = requests.post(f"{SERVER_URL}/firmware", files=files, data=data)
    print(f"状态码: {response.status_code}")
    print(f"响应: {response.json()}")
    print()
    return response.json()

def test_get_firmware_info():
    print("获取固件信息...")
    response = requests.get(f"{SERVER_URL}/firmware/info")
    print(f"状态码: {response.status_code}")
    print(f"响应: {response.json()}")
    print()

def test_get_latest_firmware():
    print("获取最新固件...")
    response = requests.get(f"{SERVER_URL}/firmware/latest")
    print(f"状态码: {response.status_code}")
    print(f"响应: {response.json()}")
    print()

def test_download_firmware(filename):
    print(f"下载固件: {filename}")
    response = requests.get(f"{SERVER_URL}/firmware/download/{filename}")
    print(f"状态码: {response.status_code}")
    if response.status_code == 200:
        with open(f"downloaded_{filename}", 'wb') as f:
            f.write(response.content)
        print(f"固件已保存为: downloaded_{filename}")
    print()

def test_delete_firmware(filename):
    print(f"删除固件: {filename}")
    response = requests.delete(f"{SERVER_URL}/firmware/{filename}")
    print(f"状态码: {response.status_code}")
    print(f"响应: {response.json()}")
    print()

if __name__ == "__main__":
    print("=" * 50)
    print("OTA 服务器测试脚本")
    print("=" * 50)
    print()

    try:
        test_health()
        
        test_get_firmware_info()
        
        test_get_latest_firmware()
        
        print("测试完成!")
        
    except requests.exceptions.ConnectionError:
        print("错误: 无法连接到服务器。请确保服务器正在运行。")
    except Exception as e:
        print(f"错误: {e}")
