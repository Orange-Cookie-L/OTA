from flask import Flask, request, jsonify, send_file, render_template, session, redirect, url_for
from flask_cors import CORS
import os
import hashlib
import json
import threading
import time
from datetime import datetime, timedelta
from collections import defaultdict
import functools

app = Flask(__name__, template_folder='templates', static_folder='static')
app.secret_key = '9f8e7d6c5b4a3s2d1f0g9h8j7k6l5m4n3b2v1c0x'  # 用于加密session
app.permanent_session_lifetime = timedelta(seconds=0)  # 设置session立即过期
CORS(app)

UPLOAD_FOLDER = 'firmware'
FIRMWARE_INFO_FILE = 'firmware_info.json'
DEVICES_FILE = 'devices.json'
PENDING_UPDATES_FILE = 'pending_updates.json'
USERS_FILE = 'users.json'

if not os.path.exists(UPLOAD_FOLDER):
    os.makedirs(UPLOAD_FOLDER)

devices = {}
pending_updates = defaultdict(list)
users = {}
update_lock = threading.Lock()

def calculate_file_crc32(file_path):
    crc = 0xFFFFFFFF
    with open(file_path, 'rb') as f:
        while True:
            chunk = f.read(8192)
            if not chunk:
                break
            for byte in chunk:
                crc ^= byte
                for _ in range(8):
                    if crc & 1:
                        crc = (crc >> 1) ^ 0xEDB88320
                    else:
                        crc >>= 1
    return crc ^ 0xFFFFFFFF

def hash_password(password):
    """加密密码"""
    return hashlib.sha256(password.encode()).hexdigest()

def load_users():
    """加载用户信息"""
    global users
    if os.path.exists(USERS_FILE):
        try:
            with open(USERS_FILE, 'r') as f:
                users = json.load(f)
        except:
            users = {}
    else:
        users = {}
    
    # 确保管理员账号存在
    if 'admin' not in users:
        users['admin'] = {
            'username': 'admin',
            'password': hash_password('admin'),
            'registered_at': datetime.now().isoformat()
        }
        save_users()

def save_users():
    """保存用户信息"""
    with open(USERS_FILE, 'w') as f:
        json.dump(users, f, indent=2)

def login_required(f):
    """登录状态检查装饰器"""
    @functools.wraps(f)
    def decorated_function(*args, **kwargs):
        print(f"Login required check, session contents: {dict(session)}")
        if 'username' not in session:
            print("User not in session, redirecting to login")
            return redirect(url_for('login'))
        print(f"User {session['username']} is logged in")
        return f(*args, **kwargs)
    return decorated_function

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

@app.route('/firmware/download/<filename>', methods=['GET'])
def download_firmware(filename):
    filepath = os.path.join(UPLOAD_FOLDER, filename)
    
    if not os.path.exists(filepath):
        return jsonify({'error': 'Firmware not found'}), 404

    return send_file(filepath, as_attachment=True)

@app.route('/firmware/info', methods=['GET'])
def get_firmware_info():
    firmware_files = []
    
    for filename in os.listdir(UPLOAD_FOLDER):
        if filename.endswith('.bin'):
            filepath = os.path.join(UPLOAD_FOLDER, filename)
            file_size = os.path.getsize(filepath)
            crc32 = calculate_file_crc32(filepath)
            
            firmware_files.append({
                'filename': filename,
                'size': file_size,
                'crc32': hex(crc32),
                'url': f'/firmware/download/{filename}'
            })

    return jsonify({
        'firmware_count': len(firmware_files),
        'firmware_list': firmware_files
    })

@app.route('/firmware/latest', methods=['GET'])
def get_latest_firmware():
    firmware_files = []
    
    for filename in os.listdir(UPLOAD_FOLDER):
        if filename.endswith('.bin'):
            filepath = os.path.join(UPLOAD_FOLDER, filename)
            file_size = os.path.getsize(filepath)
            crc32 = calculate_file_crc32(filepath)
            upload_time = datetime.fromtimestamp(os.path.getmtime(filepath))
            
            firmware_files.append({
                'filename': filename,
                'size': file_size,
                'crc32': hex(crc32),
                'upload_time': upload_time,
                'url': f'/firmware/download/{filename}'
            })

    if not firmware_files:
        return jsonify({'error': 'No firmware found'}), 404

    latest_firmware = max(firmware_files, key=lambda x: x['upload_time'])
    return jsonify(latest_firmware)

@app.route('/firmware/<filename>', methods=['DELETE'])
def delete_firmware(filename):
    filepath = os.path.join(UPLOAD_FOLDER, filename)
    
    if not os.path.exists(filepath):
        return jsonify({'error': 'Firmware not found'}), 404

    os.remove(filepath)
    return jsonify({'message': 'Firmware deleted successfully'})

@app.route('/health', methods=['GET'])
def health_check():
    return jsonify({'status': 'healthy', 'timestamp': datetime.now().isoformat()})

@app.route('/login', methods=['GET', 'POST'])
def login():
    if request.method == 'POST':
        username = request.form.get('username')
        password = request.form.get('password')
        
        if username in users and users[username]['password'] == hash_password(password):
            session['username'] = username
            # 不设置session.permanent，确保session不会持久化
            print(f"Login successful for user: {username}")
            print(f"Session contents: {dict(session)}")
            return redirect(url_for('index'))
        else:
            return render_template('login.html', error='用户名或密码错误')
    return render_template('login.html')

@app.route('/register', methods=['GET', 'POST'])
def register():
    """注册已禁用，重定向到登录页面"""
    return redirect(url_for('login'))



@app.route('/users', methods=['GET'])
@login_required
def users_page():
    return render_template('users.html', users=users)

@app.route('/user/add', methods=['POST'])
@login_required
def add_user():
    if session['username'] != 'admin':
        return jsonify({'error': '只有管理员可以添加用户'}), 403
    
    data = request.json
    username = data.get('username')
    password = data.get('password')
    
    if not username or not password:
        return jsonify({'error': '用户名和密码不能为空'}), 400
    
    if username in users:
        return jsonify({'error': '用户名已存在'}), 400
    
    users[username] = {
        'username': username,
        'password': hash_password(password),
        'registered_at': datetime.now().isoformat()
    }
    save_users()
    return jsonify({'message': '用户添加成功'})

@app.route('/user/delete/<username>', methods=['DELETE'])
@login_required
def delete_user(username):
    if session['username'] != 'admin':
        return jsonify({'error': '只有管理员可以删除用户'}), 403
    
    if username == 'admin':
        return jsonify({'error': '不能删除管理员账号'}), 400
    
    if username not in users:
        return jsonify({'error': '用户不存在'}), 404
    
    del users[username]
    save_users()
    return jsonify({'message': '用户删除成功'})

@app.route('/', methods=['GET'])
@login_required
def index():
    print(f"Accessing index page, session contents: {dict(session)}")
    print(f"Session permanent: {session.permanent}")
    return render_template('index.html')

def load_devices():
    global devices
    if os.path.exists(DEVICES_FILE):
        try:
            with open(DEVICES_FILE, 'r') as f:
                devices = json.load(f)
        except:
            devices = {}

def save_devices():
    with open(DEVICES_FILE, 'w') as f:
        json.dump(devices, f, indent=2)

def check_device_status():
    """检查设备状态，将超过5分钟未心跳的设备标记为离线"""
    global devices
    current_time = datetime.now()
    for device_id, device in devices.items():
        try:
            last_heartbeat = datetime.fromisoformat(device.get('last_heartbeat', ''))
            time_diff = (current_time - last_heartbeat).total_seconds()
            if time_diff > 300:  # 5分钟
                device['status'] = 'offline'
        except:
            device['status'] = 'offline'
    save_devices()

def load_pending_updates():
    global pending_updates
    if os.path.exists(PENDING_UPDATES_FILE):
        try:
            with open(PENDING_UPDATES_FILE, 'r') as f:
                data = json.load(f)
                pending_updates = defaultdict(list, data)
        except:
            pending_updates = defaultdict(list)

def save_pending_updates():
    with open(PENDING_UPDATES_FILE, 'w') as f:
        json.dump(dict(pending_updates), f, indent=2)

load_devices()
load_pending_updates()
load_users()

@app.route('/device/register', methods=['POST'])
def register_device():
    data = request.json
    device_id = data.get('device_id')
    device_type = data.get('device_type', 'stm32f403')
    current_version = data.get('current_version', '0.0.0')
    ip_address = request.remote_addr

    if not device_id:
        return jsonify({'error': 'device_id is required'}), 400

    devices[device_id] = {
        'device_id': device_id,
        'device_type': device_type,
        'current_version': current_version,
        'ip_address': ip_address,
        'registered_at': datetime.now().isoformat(),
        'last_heartbeat': datetime.now().isoformat(),
        'status': 'online'
    }

    save_devices()

    return jsonify({
        'message': 'Device registered successfully',
        'device': devices[device_id]
    }), 201

@app.route('/device/heartbeat', methods=['POST'])
def device_heartbeat():
    data = request.json
    device_id = data.get('device_id')
    current_version = data.get('current_version')

    if not device_id or device_id not in devices:
        return jsonify({'error': 'Device not found'}), 404

    devices[device_id]['last_heartbeat'] = datetime.now().isoformat()
    devices[device_id]['status'] = 'online'
    if current_version:
        devices[device_id]['current_version'] = current_version

    save_devices()

    with update_lock:
        if device_id in pending_updates and pending_updates[device_id]:
            update_info = pending_updates[device_id][0]
            return jsonify({
                'message': 'Update available',
                'update_required': True,
                'update': update_info
            })

    return jsonify({
        'message': 'Heartbeat received',
        'update_required': False
    })

@app.route('/device/check-update', methods=['POST'])
def check_update():
    data = request.json
    device_id = data.get('device_id')
    current_version = data.get('current_version')

    if not device_id:
        return jsonify({'error': 'device_id is required'}), 400

    latest_firmware = get_latest_firmware_data()

    if not latest_firmware:
        return jsonify({'error': 'No firmware available'}), 404

    update_available = False
    if current_version and current_version != latest_firmware.get('version', '1.0.0'):
        update_available = True

    return jsonify({
        'update_available': update_available,
        'latest_firmware': latest_firmware
    })

@app.route('/device/list', methods=['GET'])
def list_devices():
    check_device_status()  # 检查设备状态
    return jsonify({
        'device_count': len(devices),
        'devices': list(devices.values())
    })

@app.route('/device/<device_id>', methods=['GET'])
def get_device(device_id):
    check_device_status()  # 检查设备状态
    if device_id not in devices:
        return jsonify({'error': 'Device not found'}), 404

    return jsonify(devices[device_id])

@app.route('/device/<device_id>', methods=['DELETE'])
def delete_device(device_id):
    if device_id not in devices:
        return jsonify({'error': 'Device not found'}), 404

    del devices[device_id]
    save_devices()

    return jsonify({'message': 'Device deleted successfully'})

@app.route('/push/update', methods=['POST'])
def push_update():
    data = request.json
    device_ids = data.get('device_ids', [])
    firmware_filename = data.get('firmware_filename')
    force_update = data.get('force_update', False)

    if not firmware_filename:
        return jsonify({'error': 'firmware_filename is required'}), 400

    filepath = os.path.join(UPLOAD_FOLDER, firmware_filename)
    if not os.path.exists(filepath):
        return jsonify({'error': 'Firmware not found'}), 404

    file_size = os.path.getsize(filepath)
    crc32 = calculate_file_crc32(filepath)

    update_info = {
        'firmware_filename': firmware_filename,
        'firmware_url': f'/firmware/download/{firmware_filename}',
        'firmware_size': file_size,
        'firmware_crc32': hex(crc32),
        'pushed_at': datetime.now().isoformat(),
        'force_update': force_update
    }

    with update_lock:
        if not device_ids:
            device_ids = list(devices.keys())

        for device_id in device_ids:
            if device_id in devices:
                pending_updates[device_id].append(update_info)

    save_pending_updates()

    return jsonify({
        'message': 'Update pushed successfully',
        'target_devices': len(device_ids),
        'update': update_info
    })

@app.route('/push/status/<device_id>', methods=['GET'])
def get_push_status(device_id):
    with update_lock:
        if device_id not in pending_updates:
            return jsonify({'pending_updates': 0, 'updates': []})

        return jsonify({
            'pending_updates': len(pending_updates[device_id]),
            'updates': pending_updates[device_id]
        })

@app.route('/push/clear/<device_id>', methods=['DELETE'])
def clear_pending_updates(device_id):
    with update_lock:
        if device_id in pending_updates:
            del pending_updates[device_id]

    save_pending_updates()

    return jsonify({'message': 'Pending updates cleared'})

@app.route('/push/acknowledge', methods=['POST'])
def acknowledge_update():
    data = request.json
    device_id = data.get('device_id')
    firmware_filename = data.get('firmware_filename')
    success = data.get('success', False)

    if not device_id or not firmware_filename:
        return jsonify({'error': 'device_id and firmware_filename are required'}), 400

    with update_lock:
        if device_id in pending_updates:
            pending_updates[device_id] = [
                u for u in pending_updates[device_id] 
                if u['firmware_filename'] != firmware_filename
            ]

    save_pending_updates()

    if device_id in devices:
        if success:
            devices[device_id]['current_version'] = firmware_filename.split('_')[1]
        devices[device_id]['last_update'] = datetime.now().isoformat()
        save_devices()

    return jsonify({'message': 'Update acknowledged'})

def get_latest_firmware_data():
    firmware_files = []
    
    for filename in os.listdir(UPLOAD_FOLDER):
        if filename.endswith('.bin'):
            filepath = os.path.join(UPLOAD_FOLDER, filename)
            file_size = os.path.getsize(filepath)
            crc32 = calculate_file_crc32(filepath)
            upload_time = datetime.fromtimestamp(os.path.getmtime(filepath))
            
            version = filename.split('_')[1] if '_' in filename else '1.0.0'
            
            firmware_files.append({
                'filename': filename,
                'version': version,
                'size': file_size,
                'crc32': hex(crc32),
                'upload_time': upload_time.isoformat(),
                'url': f'/firmware/download/{filename}'
            })

    if not firmware_files:
        return None

    return max(firmware_files, key=lambda x: x['upload_time'])

def cleanup_offline_devices():
    while True:
        time.sleep(300)
        current_time = datetime.now()
        offline_threshold = 600

        for device_id, device in list(devices.items()):
            last_heartbeat = datetime.fromisoformat(device['last_heartbeat'])
            if (current_time - last_heartbeat).total_seconds() > offline_threshold:
                device['status'] = 'offline'
        
        save_devices()

cleanup_thread = threading.Thread(target=cleanup_offline_devices, daemon=True)
cleanup_thread.start()

if __name__ == '__main__':
    load_users()
    load_devices()
    app.run(host='0.0.0.0', port=5000, debug=False)
