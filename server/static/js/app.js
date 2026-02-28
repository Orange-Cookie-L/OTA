const API_BASE = window.location.origin;

let devices = [];
let firmwareList = [];
let logs = [];

document.addEventListener('DOMContentLoaded', function() {
    initTabs();
    checkServerStatus();
    refreshDevices();
    refreshFirmware();
    loadLogs();
    loadTheme();
    setInterval(checkServerStatus, 30000);
});

function loadTheme() {
    const savedTheme = localStorage.getItem('ota_theme') || 'light-theme';
    document.body.className = savedTheme;
    updateThemeButton(savedTheme);
}

function toggleTheme() {
    const currentTheme = document.body.className;
    let newTheme;
    switch (currentTheme) {
        case 'light-theme':
            newTheme = 'modern-theme';
            break;
        case 'modern-theme':
            newTheme = 'dark-theme';
            break;
        case 'dark-theme':
            newTheme = 'blue-theme';
            break;
        default:
            newTheme = 'light-theme';
    }
    document.body.className = newTheme;
    localStorage.setItem('ota_theme', newTheme);
    updateThemeButton(newTheme);
}

function updateThemeButton(theme) {
    const button = document.getElementById('theme-toggle');
    switch (theme) {
        case 'light-theme':
            button.textContent = '切换现代主题';
            break;
        case 'modern-theme':
            button.textContent = '切换深色主题';
            break;
        case 'dark-theme':
            button.textContent = '切换蓝色主题';
            break;
        case 'blue-theme':
            button.textContent = '切换浅色主题';
            break;
    }
}

function initTabs() {
    const tabs = document.querySelectorAll('.tab-btn');
    tabs.forEach(tab => {
        tab.addEventListener('click', function() {
            const tabName = this.getAttribute('data-tab');
            switchTab(tabName);
        });
    });
}

function switchTab(tabName) {
    document.querySelectorAll('.tab-btn').forEach(btn => {
        btn.classList.remove('active');
    });
    document.querySelectorAll('.tab-content').forEach(content => {
        content.classList.remove('active');
    });
    
    document.querySelector(`[data-tab="${tabName}"]`).classList.add('active');
    document.getElementById(`${tabName}-tab`).classList.add('active');
}

async function checkServerStatus() {
    try {
        const response = await fetch(`${API_BASE}/health`);
        if (response.ok) {
            document.getElementById('server-status').textContent = '在线';
            document.getElementById('server-status').className = 'status-badge online';
        }
    } catch (error) {
        document.getElementById('server-status').textContent = '离线';
        document.getElementById('server-status').className = 'status-badge offline';
    }
}

async function refreshDevices() {
    try {
        const response = await fetch(`${API_BASE}/device/list`);
        const data = await response.json();
        devices = data.devices || [];
        renderDevices();
        updateDeviceCount();
        updateDeviceCheckboxes();
        addLog('info', '设备列表已刷新');
    } catch (error) {
        addLog('error', '获取设备列表失败: ' + error.message);
    }
}

function renderDevices() {
    const tbody = document.getElementById('device-list');
    if (devices.length === 0) {
        tbody.innerHTML = '<tr><td colspan="7" class="no-data">暂无设备</td></tr>';
        return;
    }
    
    tbody.innerHTML = devices.map(device => `
        <tr>
            <td>${device.device_id}</td>
            <td>${device.device_type}</td>
            <td>${device.current_version}</td>
            <td><span class="status-${device.status === 'online' ? 'online' : 'offline'}">${device.status === 'online' ? '在线' : '离线'}</span></td>
            <td>${device.ip_address}</td>
            <td>${formatTime(device.last_heartbeat)}</td>
            <td>
                <button class="btn btn-small btn-primary" onclick="viewDevice('${device.device_id}')">查看</button>
                <button class="btn btn-small btn-danger" onclick="deleteDevice('${device.device_id}')">删除</button>
            </td>
        </tr>
    `).join('');
}

function updateDeviceCount() {
    const onlineCount = devices.filter(d => d.status === 'online').length;
    const totalDevicesElement = document.getElementById('total-devices');
    const onlineDevicesElement = document.getElementById('online-devices');
    if (totalDevicesElement) {
        totalDevicesElement.textContent = devices.length;
    }
    if (onlineDevicesElement) {
        onlineDevicesElement.textContent = onlineCount;
    }
}

function updateDeviceCheckboxes() {
    const container = document.getElementById('device-checkboxes');
    container.innerHTML = devices.map(device => `
        <label>
            <input type="checkbox" class="device-checkbox" value="${device.device_id}">
            ${device.device_id} (${device.device_type})
        </label>
    `).join('');
}

function filterDevices() {
    const search = document.getElementById('device-search').value.toLowerCase();
    const filtered = devices.filter(device => 
        device.device_id.toLowerCase().includes(search) ||
        device.device_type.toLowerCase().includes(search) ||
        device.ip_address.toLowerCase().includes(search)
    );
    
    const tbody = document.getElementById('device-list');
    if (filtered.length === 0) {
        tbody.innerHTML = '<tr><td colspan="7" class="no-data">未找到匹配的设备</td></tr>';
    } else {
            tbody.innerHTML = filtered.map(device => `
            <tr>
                <td>${device.device_id}</td>
                <td>${device.device_type}</td>
                <td>${device.current_version}</td>
                <td><span class="status-${device.status === 'online' ? 'online' : 'offline'}">${device.status === 'online' ? '在线' : '离线'}</span></td>
                <td>${device.ip_address}</td>
                <td>${formatTime(device.last_heartbeat)}</td>
                <td>
                    <button class="btn btn-small btn-primary" onclick="viewDevice('${device.device_id}')">查看</button>
                    <button class="btn btn-small btn-danger" onclick="deleteDevice('${device.device_id}')">删除</button>
                </td>
            </tr>
        `).join('');
        }
}

async function refreshFirmware() {
    try {
        const response = await fetch(`${API_BASE}/firmware/info`);
        const data = await response.json();
        firmwareList = data.firmware_list || [];
        renderFirmware();
        updateFirmwareSelect();
        addLog('info', '固件列表已刷新');
    } catch (error) {
        addLog('error', '获取固件列表失败: ' + error.message);
    }
}

function renderFirmware() {
    const tbody = document.getElementById('firmware-list');
    if (firmwareList.length === 0) {
        tbody.innerHTML = '<tr><td colspan="6" class="no-data">暂无固件</td></tr>';
        return;
    }
    
    tbody.innerHTML = firmwareList.map(fw => `
        <tr>
            <td>${fw.filename}</td>
            <td>${fw.version || '未知'}</td>
            <td>${fw.size}</td>
            <td>${fw.crc32}</td>
            <td>${fw.upload_time || '未知'}</td>
            <td>
                <button class="btn btn-small btn-primary" onclick="downloadFirmware('${fw.filename}')">下载</button>
                <button class="btn btn-small btn-danger" onclick="deleteFirmware('${fw.filename}')">删除</button>
            </td>
        </tr>
    `).join('');
}

function updateFirmwareSelect() {
    const select = document.getElementById('firmware-select');
    select.innerHTML = '<option value="">请选择固件...</option>' + 
        firmwareList.map(fw => `<option value="${fw.filename}">${fw.filename}</option>`).join('');
}

function toggleDeviceSelection() {
    const allCheckbox = document.getElementById('push-all-devices');
    const deviceCheckboxes = document.querySelectorAll('.device-checkbox');
    
    deviceCheckboxes.forEach(cb => {
        cb.checked = allCheckbox.checked;
    });
}

async function pushUpdate(event) {
    event.preventDefault();
    
    const firmwareSelect = document.getElementById('firmware-select');
    const firmwareFilename = firmwareSelect.value;
    const forceUpdate = document.getElementById('force-update').checked;
    
    if (!firmwareFilename) {
        alert('请选择固件');
        return;
    }
    
    const selectedDevices = [];
    const allCheckbox = document.getElementById('push-all-devices');
    
    if (allCheckbox.checked) {
        selectedDevices.push(...devices.map(d => d.device_id));
    } else {
        document.querySelectorAll('.device-checkbox:checked').forEach(cb => {
            selectedDevices.push(cb.value);
        });
    }
    
    if (selectedDevices.length === 0) {
        alert('请选择至少一个设备');
        return;
    }
    
    try {
        const response = await fetch(`${API_BASE}/push/update`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({
                device_ids: selectedDevices,
                firmware_filename: firmwareFilename,
                force_update: forceUpdate
            })
        });
        
        const data = await response.json();
        
        if (response.ok) {
            addLog('success', `已推送更新到 ${selectedDevices.length} 个设备`);
            alert('更新推送成功！');
            refreshPushStatus();
        } else {
            addLog('error', '推送更新失败: ' + data.error);
            alert('推送更新失败: ' + data.error);
        }
    } catch (error) {
        addLog('error', '推送更新失败: ' + error.message);
        alert('推送更新失败: ' + error.message);
    }
}

async function refreshPushStatus() {
    const container = document.getElementById('push-status-list');
    container.innerHTML = '';
    
    for (const device of devices) {
        try {
            const response = await fetch(`${API_BASE}/push/status/${device.device_id}`);
            const data = await response.json();
            
            if (data.pending_updates > 0) {
                container.innerHTML += `
                    <div class="status-item pending">
                        <span>${device.device_id}</span>
                        <span>待更新 (${data.pending_updates})</span>
                    </div>
                `;
            }
        } catch (error) {
            console.error('获取推送状态失败:', error);
        }
    }
    
    if (container.innerHTML === '') {
        container.innerHTML = '<p style="color: #999; text-align: center;">暂无待处理的更新</p>';
    }
}

async function deleteDevice(deviceId) {
    if (!confirm(`确定要删除设备 ${deviceId} 吗？`)) {
        return;
    }
    
    try {
        const response = await fetch(`${API_BASE}/device/${deviceId}`, {
            method: 'DELETE'
        });
        
        if (response.ok) {
            addLog('success', `设备 ${deviceId} 已删除`);
            refreshDevices();
        } else {
            addLog('error', '删除设备失败');
        }
    } catch (error) {
        addLog('error', '删除设备失败: ' + error.message);
    }
}

async function deleteFirmware(filename) {
    if (!confirm(`确定要删除固件 ${filename} 吗？`)) {
        return;
    }
    
    try {
        const response = await fetch(`${API_BASE}/firmware/${filename}`, {
            method: 'DELETE'
        });
        
        if (response.ok) {
            addLog('success', `固件 ${filename} 已删除`);
            refreshFirmware();
        } else {
            addLog('error', '删除固件失败');
        }
    } catch (error) {
        addLog('error', '删除固件失败: ' + error.message);
    }
}

function downloadFirmware(filename) {
    window.open(`${API_BASE}/firmware/download/${filename}`, '_blank');
}

function showAddDeviceModal() {
    const modalBody = document.getElementById('modal-body');
    modalBody.innerHTML = `
        <h2>添加设备</h2>
        <form onsubmit="addDevice(event)">
            <div class="form-group">
                <label>设备 ID</label>
                <input type="text" id="new-device-id" required>
            </div>
            <div class="form-group">
                <label>设备类型</label>
                <input type="text" id="new-device-type" value="stm32f403">
            </div>
            <div class="form-group">
                <label>当前版本</label>
                <input type="text" id="new-device-version" value="1.0.0">
            </div>
            <button type="submit" class="btn btn-primary">添加</button>
        </form>
    `;
    showModal();
}

async function addDevice(event) {
    event.preventDefault();
    
    const deviceId = document.getElementById('new-device-id').value;
    const deviceType = document.getElementById('new-device-type').value;
    const currentVersion = document.getElementById('new-device-version').value;
    
    try {
        const response = await fetch(`${API_BASE}/device/register`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({
                device_id: deviceId,
                device_type: deviceType,
                current_version: currentVersion
            })
        });
        
        if (response.ok) {
            addLog('success', `设备 ${deviceId} 已添加`);
            closeModal();
            refreshDevices();
        } else {
            const data = await response.json();
            addLog('error', '添加设备失败: ' + data.error);
        }
    } catch (error) {
        addLog('error', '添加设备失败: ' + error.message);
    }
}

function showUploadModal() {
    const modalBody = document.getElementById('modal-body');
    modalBody.innerHTML = `
        <h2>上传固件</h2>
        <form onsubmit="uploadFirmware(event)">
            <div class="form-group">
                <label>选择文件</label>
                <input type="file" id="firmware-file" accept=".bin" required>
            </div>
            <div class="form-group">
                <label>版本号</label>
                <input type="text" id="firmware-version" required>
            </div>
            <div class="form-group">
                <label>描述</label>
                <input type="text" id="firmware-description">
            </div>
            <button type="submit" class="btn btn-primary">上传</button>
        </form>
    `;
    showModal();
}

async function uploadFirmware(event) {
    event.preventDefault();
    
    const fileInput = document.getElementById('firmware-file');
    const version = document.getElementById('firmware-version').value;
    const description = document.getElementById('firmware-description').value;
    
    if (!fileInput.files[0]) {
        alert('请选择文件');
        return;
    }
    
    const formData = new FormData();
    formData.append('file', fileInput.files[0]);
    formData.append('version', version);
    formData.append('description', description);
    
    try {
        const response = await fetch(`${API_BASE}/firmware`, {
            method: 'POST',
            body: formData
        });
        
        if (response.ok) {
            addLog('success', '固件上传成功');
            closeModal();
            refreshFirmware();
        } else {
            const data = await response.json();
            addLog('error', '上传固件失败: ' + data.error);
        }
    } catch (error) {
        addLog('error', '上传固件失败: ' + error.message);
    }
}

function viewDevice(deviceId) {
    const device = devices.find(d => d.device_id === deviceId);
    if (!device) return;
    
    const modalBody = document.getElementById('modal-body');
    modalBody.innerHTML = `
        <h2>设备详情</h2>
        <div class="device-info">
            <div class="info-item">
                <span class="info-label">设备 ID:</span>
                <span class="info-value">${device.device_id}</span>
            </div>
            <div class="info-item">
                <span class="info-label">设备类型:</span>
                <span class="info-value">${device.device_type}</span>
            </div>
            <div class="info-item">
                <span class="info-label">当前版本:</span>
                <span class="info-value">${device.current_version}</span>
            </div>
            <div class="info-item">
                <span class="info-label">状态:</span>
                <span class="status-${device.status === 'online' ? 'online' : 'offline'}">${device.status === 'online' ? '在线' : '离线'}</span>
            </div>
            <div class="info-item">
                <span class="info-label">IP 地址:</span>
                <span class="info-value">${device.ip_address}</span>
            </div>
            <div class="info-item">
                <span class="info-label">注册时间:</span>
                <span class="info-value">${formatTime(device.registered_at)}</span>
            </div>
            <div class="info-item">
                <span class="info-label">最后心跳:</span>
                <span class="info-value">${formatTime(device.last_heartbeat)}</span>
            </div>
        </div>
        <div class="modal-actions">
            <button class="btn btn-primary" onclick="closeModal()">关闭</button>
        </div>
        <style>
            .device-info {
                margin: 20px 0;
                background: var(--bg-secondary);
                border-radius: 8px;
                padding: 20px;
                border: 1px solid var(--border-secondary);
            }
            
            .info-item {
                display: flex;
                justify-content: space-between;
                align-items: center;
                padding: 10px 0;
                border-bottom: 1px solid var(--border-secondary);
            }
            
            .info-item:last-child {
                border-bottom: none;
            }
            
            .info-label {
                font-weight: 600;
                color: var(--text-secondary);
                min-width: 100px;
            }
            
            .info-value {
                color: var(--text-primary);
                font-weight: 500;
            }
            
            .modal-actions {
                display: flex;
                justify-content: flex-end;
                margin-top: 20px;
            }
        </style>
    `;
    showModal();
}

function showModal() {
    document.getElementById('modal').classList.add('show');
}

function closeModal() {
    document.getElementById('modal').classList.remove('show');
}

function addLog(type, message) {
    const timestamp = new Date().toLocaleString('zh-CN');
    logs.unshift({ type, message, timestamp });
    saveLogs();
    renderLogs();
}

function renderLogs() {
    const container = document.getElementById('log-container');
    container.innerHTML = logs.map(log => `
        <div class="log-entry ${log.type}">
            <span class="timestamp">[${log.timestamp}]</span>
            ${log.message}
        </div>
    `).join('');
}

function saveLogs() {
    localStorage.setItem('ota_logs', JSON.stringify(logs.slice(0, 100)));
}

function loadLogs() {
    const saved = localStorage.getItem('ota_logs');
    if (saved) {
        logs = JSON.parse(saved);
        renderLogs();
    }
}

function clearLogs() {
    if (!confirm('确定要清空日志吗？')) {
        return;
    }
    logs = [];
    localStorage.removeItem('ota_logs');
    renderLogs();
}

function escapeHtml(text) {
    if (!text) return '';
    const div = document.createElement('div');
    div.textContent = text;
    return div.innerHTML;
}

function formatTime(timeStr) {
    if (!timeStr) return '-';
    const date = new Date(timeStr);
    return date.toLocaleString('zh-CN');
}

function formatSize(bytes) {
    if (bytes === 0) return '0 B';
    const k = 1024;
    const sizes = ['B', 'KB', 'MB', 'GB'];
    const i = Math.floor(Math.log(bytes) / Math.log(k));
    return (bytes / Math.pow(k, i)).toFixed(2) + ' ' + sizes[i];
}

function extractVersion(filename) {
    const match = filename.match(/firmware_([\d.]+)_/);
    return match ? match[1] : '未知';
}

window.onclick = function(event) {
    const modal = document.getElementById('modal');
    if (event.target === modal) {
        closeModal();
    }
}