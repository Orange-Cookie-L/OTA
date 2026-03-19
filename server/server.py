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
import uuid

app = Flask(__name__, template_folder='templates', static_folder='static')
app.secret_key = '9f8e7d6c5b4a3s2d1f0g9h8j7k6l5m4n3b2v1c0x'  # 用于加密session
app.permanent_session_lifetime = timedelta(seconds=0)  # 设置session过期时间为0秒
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
scheduled_tasks = []
update_lock = threading.Lock()
task_lock = threading.Lock()

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
            'role': 'admin',
            'status': 'active',
            'created_at': datetime.now().isoformat(),
            'last_login': None
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
            # 检查用户状态
            if users[username].get('status') == 'disabled':
                return render_template('login.html', error='账号已被禁用，请联系管理员')
            
            session['username'] = username
            session.permanent = True  # 设置session为永久会话，使用配置的过期时间
            
            # 更新最后登录时间
            users[username]['last_login'] = datetime.now().isoformat()
            save_users()
            
            print(f"Login successful for user: {username}")
            print(f"Session contents: {dict(session)}")
            print(f"Session permanent: {session.permanent}")
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
    """用户管理页面"""
    # 检查是否是管理员
    current_user = users.get(session['username'], {})
    if current_user.get('role') != 'admin':
        return redirect(url_for('index'))
    return render_template('users.html')

@app.route('/api/users', methods=['GET'])
@login_required
def get_users():
    """获取用户列表API"""
    # 检查是否是管理员
    current_user = users.get(session['username'], {})
    if current_user.get('role') != 'admin':
        return jsonify({'error': '权限不足'}), 403
    
    # 只返回必要的字段，不返回密码
    user_list = []
    for username, user_data in users.items():
        user_list.append({
            'username': username,
            'role': user_data.get('role', 'user'),
            'status': user_data.get('status', 'active'),
            'created_at': user_data.get('created_at', ''),
            'last_login': user_data.get('last_login', None)
        })
    return jsonify({'users': user_list})

@app.route('/api/users', methods=['POST'])
@login_required
def create_user():
    """创建新用户API"""
    # 检查权限
    current_user = users.get(session['username'], {})
    if current_user.get('role') != 'admin':
        return jsonify({'error': '只有管理员可以添加用户'}), 403
    
    data = request.json
    username = data.get('username', '').strip()
    password = data.get('password', '').strip()
    role = data.get('role', 'user')
    
    # 验证输入
    if not username or not password:
        return jsonify({'error': '用户名和密码不能为空'}), 400
    
    if len(username) < 3:
        return jsonify({'error': '用户名至少需要3个字符'}), 400
    
    if len(password) < 6:
        return jsonify({'error': '密码至少需要6个字符'}), 400
    
    if username in users:
        return jsonify({'error': '用户名已存在'}), 400
    
    if role not in ['admin', 'user']:
        return jsonify({'error': '无效的角色类型'}), 400
    
    # 创建用户
    users[username] = {
        'username': username,
        'password': hash_password(password),
        'role': role,
        'status': 'active',
        'created_at': datetime.now().isoformat(),
        'last_login': None
    }
    save_users()
    
    return jsonify({
        'message': '用户创建成功',
        'user': {
            'username': username,
            'role': role,
            'status': 'active',
            'created_at': users[username]['created_at']
        }
    })

@app.route('/api/users/<username>', methods=['DELETE'])
@login_required
def remove_user(username):
    """删除用户API"""
    # 检查权限
    current_user = users.get(session['username'], {})
    if current_user.get('role') != 'admin':
        return jsonify({'error': '只有管理员可以删除用户'}), 403
    
    # 不能删除自己
    if username == session['username']:
        return jsonify({'error': '不能删除当前登录的用户'}), 400
    
    # 不能删除admin账号
    if username == 'admin':
        return jsonify({'error': '不能删除管理员账号'}), 400
    
    if username not in users:
        return jsonify({'error': '用户不存在'}), 404
    
    del users[username]
    save_users()
    return jsonify({'message': '用户删除成功'})

@app.route('/api/users/<username>/password', methods=['PUT'])
@login_required
def change_password(username):
    """修改密码API"""
    # 检查是否是管理员
    current_user = users.get(session['username'], {})
    if current_user.get('role') != 'admin':
        return jsonify({'error': '权限不足'}), 403
    
    data = request.json
    new_password = data.get('new_password', '').strip()
    
    if username not in users:
        return jsonify({'error': '用户不存在'}), 404
    
    if not new_password:
        return jsonify({'error': '请输入新密码'}), 400
    
    if len(new_password) < 6:
        return jsonify({'error': '新密码至少需要6个字符'}), 400
    
    users[username]['password'] = hash_password(new_password)
    save_users()
    return jsonify({'message': '密码修改成功'})

@app.route('/api/users/<username>/status', methods=['PUT'])
@login_required
def toggle_user_status(username):
    """切换用户状态（启用/禁用）API"""
    current_user = users.get(session['username'], {})
    if current_user.get('role') != 'admin':
        return jsonify({'error': '只有管理员可以修改用户状态'}), 403
    
    if username == 'admin':
        return jsonify({'error': '不能禁用管理员账号'}), 400
    
    if username not in users:
        return jsonify({'error': '用户不存在'}), 404
    
    data = request.json
    new_status = data.get('status')
    
    if new_status not in ['active', 'disabled']:
        return jsonify({'error': '无效的状态值'}), 400
    
    users[username]['status'] = new_status
    save_users()
    
    status_text = '启用' if new_status == 'active' else '禁用'
    return jsonify({'message': f'用户已{status_text}'})

@app.route('/api/current-user', methods=['GET'])
@login_required
def get_current_user():
    """获取当前登录用户信息"""
    username = session.get('username')
    if username and username in users:
        user_data = users[username]
        return jsonify({
            'user': {
                'username': username,
                'role': user_data.get('role', 'user'),
                'status': user_data.get('status', 'active')
            }
        })
    return jsonify({'error': '未登录'}), 401

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

def check_scheduled_tasks():
    """检查并执行定时推送任务"""
    global scheduled_tasks
    while True:
        current_time = datetime.now()
        tasks_to_remove = []
        
        with task_lock:
            for task in scheduled_tasks:
                try:
                    scheduled_time = datetime.fromisoformat(task['scheduled_time'])
                    if current_time >= scheduled_time and task['status'] == 'pending':
                        # 执行推送
                        execute_scheduled_task(task)
                        task['status'] = 'completed'
                        task['executed_at'] = current_time.isoformat()
                        tasks_to_remove.append(task)
                except Exception as e:
                    print(f"执行定时任务失败: {e}")
                    task['status'] = 'failed'
                    task['error'] = str(e)
                    tasks_to_remove.append(task)
        
        # 移除已完成的任务
        with task_lock:
            for task in tasks_to_remove:
                if task in scheduled_tasks:
                    scheduled_tasks.remove(task)
        
        time.sleep(60)  # 每分钟检查一次

def execute_scheduled_task(task):
    """执行定时推送任务"""
    device_ids = task['device_ids']
    firmware_filename = task['firmware_filename']
    force_update = task['force_update']
    
    try:
        # 验证固件是否存在
        filepath = os.path.join(UPLOAD_FOLDER, firmware_filename)
        if not os.path.exists(filepath):
            print(f"固件不存在: {firmware_filename}")
            return
        
        # 计算固件信息
        file_size = os.path.getsize(filepath)
        crc32 = calculate_file_crc32(filepath)
        
        # 构建更新信息
        update_info = {
            'firmware_filename': firmware_filename,
            'firmware_url': f'/firmware/download/{firmware_filename}',
            'firmware_size': file_size,
            'firmware_crc32': hex(crc32),
            'pushed_at': datetime.now().isoformat(),
            'force_update': force_update
        }
        
        # 执行推送
        with update_lock:
            for device_id in device_ids:
                if device_id in devices:
                    pending_updates[device_id].append(update_info)
        
        save_pending_updates()
        print(f"定时推送执行成功: {firmware_filename} 到 {len(device_ids)} 个设备")
    except Exception as e:
        print(f"执行定时推送失败: {e}")

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

# 启动定时任务检查线程
task_checker_thread = threading.Thread(target=check_scheduled_tasks, daemon=True)
task_checker_thread.start()

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

@app.route('/push/schedule', methods=['POST'])
def schedule_push():
    """创建定时推送任务"""
    data = request.json
    device_ids = data.get('device_ids', [])
    firmware_filename = data.get('firmware_filename')
    scheduled_time = data.get('scheduled_time')
    force_update = data.get('force_update', False)
    
    if not firmware_filename:
        return jsonify({'error': 'firmware_filename is required'}), 400
    
    if not scheduled_time:
        return jsonify({'error': 'scheduled_time is required'}), 400
    
    if not device_ids:
        return jsonify({'error': 'device_ids is required'}), 400
    
    # 验证固件是否存在
    filepath = os.path.join(UPLOAD_FOLDER, firmware_filename)
    if not os.path.exists(filepath):
        return jsonify({'error': 'Firmware not found'}), 404
    
    # 创建任务
    task_id = str(uuid.uuid4())
    task = {
        'id': task_id,
        'device_ids': device_ids,
        'firmware_filename': firmware_filename,
        'scheduled_time': scheduled_time,
        'force_update': force_update,
        'status': 'pending',
        'created_at': datetime.now().isoformat()
    }
    
    with task_lock:
        scheduled_tasks.append(task)
    
    print(f"定时推送任务创建成功: {task_id}")
    return jsonify({'message': 'Scheduled push task created successfully', 'task_id': task_id})

@app.route('/push/scheduled-tasks', methods=['GET'])
def get_scheduled_tasks():
    """获取定时推送任务列表"""
    with task_lock:
        return jsonify({'tasks': scheduled_tasks})

@app.route('/push/schedule/<task_id>', methods=['DELETE'])
def cancel_scheduled_task(task_id):
    """取消定时推送任务"""
    global scheduled_tasks
    task = None
    
    with task_lock:
        for t in scheduled_tasks:
            if t['id'] == task_id:
                task = t
                break
    
    if not task:
        return jsonify({'error': 'Task not found'}), 404
    
    if task['status'] != 'pending':
        return jsonify({'error': 'Task cannot be cancelled'}), 400
    
    with task_lock:
        scheduled_tasks.remove(task)
    
    print(f"定时推送任务已取消: {task_id}")
    return jsonify({'message': 'Task cancelled successfully'})

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
