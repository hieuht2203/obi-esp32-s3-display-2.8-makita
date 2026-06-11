/**
 * Embedded Web Interface for OBI ESP32
 *
 * FUNCTIONAL REQUIREMENTS:
 * 1. Serve single-page web application for battery diagnostics
 * 2. Provide REST API endpoints for battery operations
 * 3. Display real-time battery status via AJAX polling
 * 4. Provide Wi-Fi configuration portal for SoftAP connections
 *
 * AI-generated on 2026-06-11
 */

#ifndef WEB_INTERFACE_H
#define WEB_INTERFACE_H

const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>OBI - Open Battery Information</title>
    <style>
        * { box-sizing: border-box; margin: 0; padding: 0; }
        body {
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
            background: #0b0c10;
            color: #eee;
            min-height: 100vh;
            padding: 20px;
        }
        .header {
            text-align: center;
            margin-bottom: 20px;
            color: #00d4ff;
            text-transform: uppercase;
            letter-spacing: 2px;
            font-weight: 800;
            text-shadow: 0 0 10px rgba(0, 212, 255, 0.3);
        }
        
        /* Tab Navigation */
        .tabs {
            display: flex;
            justify-content: center;
            gap: 10px;
            margin-bottom: 25px;
            max-width: 600px;
            margin-left: auto;
            margin-right: auto;
        }
        .tab-btn {
            flex: 1;
            background: #1f2833;
            color: #888;
            border: 1px solid #333;
            padding: 14px 20px;
            font-size: 1em;
            font-weight: bold;
            cursor: pointer;
            border-radius: 4px;
            transition: all 0.25s cubic-bezier(0.4, 0, 0.2, 1);
            display: flex;
            align-items: center;
            justify-content: center;
            gap: 8px;
            text-transform: uppercase;
        }
        .tab-btn:hover {
            border-color: #00d4ff;
            color: #00d4ff;
            box-shadow: 0 0 10px rgba(0, 212, 255, 0.15);
        }
        .tab-btn.active {
            background: #0f3460;
            border-color: #00d4ff;
            color: #00d4ff;
            box-shadow: 0 0 15px rgba(0, 212, 255, 0.25);
        }
        
        /* Tab Content */
        .tab-content {
            display: none;
            animation: tabFadeIn 0.3s cubic-bezier(0.4, 0, 0.2, 1);
        }
        .tab-content.active-content {
            display: grid;
        }
        
        @keyframes tabFadeIn {
            from { opacity: 0; transform: translateY(8px); }
            to { opacity: 1; transform: translateY(0); }
        }

        .container {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(320px, 1fr));
            gap: 20px;
            max-width: 1600px;
            margin: 0 auto;
        }
        .card {
            background: #16213e;
            border: 1px solid #222;
            border-radius: 4px;
            padding: 20px;
            box-shadow: 0 4px 12px rgba(0,0,0,0.4);
            transition: all 0.3s ease;
        }
        .card:hover {
            border-color: #333;
            box-shadow: 0 6px 16px rgba(0,0,0,0.5);
        }
        .card-wide {
            grid-column: 1 / -1;
        }
        @media (min-width: 1000px) {
            .card-wide { grid-column: span 2; }
        }
        .card h2 {
            color: #00d4ff;
            margin-bottom: 15px;
            font-size: 1.15em;
            border-bottom: 1px solid #333;
            padding-bottom: 10px;
            text-transform: uppercase;
            letter-spacing: 1px;
        }
        .btn {
            background: #0f4c75;
            color: white;
            border: 1px solid transparent;
            padding: 12px 24px;
            border-radius: 4px;
            cursor: pointer;
            font-size: 0.95em;
            font-weight: bold;
            margin: 5px;
            transition: all 0.2s ease;
            text-transform: uppercase;
        }
        .btn:hover { 
            background: #1b6ca8; 
            border-color: #00d4ff;
            box-shadow: 0 0 10px rgba(0, 212, 255, 0.2);
        }
        .btn:active {
            transform: scale(0.97);
        }
        .btn:disabled { background: #333; border-color: transparent; cursor: not-allowed; color: #888; }
        .btn-danger { background: #c0392b; }
        .btn-danger:hover { background: #e74c3c; border-color: #ff6b6b; box-shadow: 0 0 10px rgba(231, 76, 60, 0.2); }
        .btn-success { background: #27ae60; }
        .btn-success:hover { background: #2ecc71; border-color: #39ff14; box-shadow: 0 0 10px rgba(46, 204, 113, 0.2); }
        
        .status {
            padding: 12px;
            border-radius: 4px;
            margin: 10px 0;
            font-family: monospace;
            font-size: 0.95em;
            border-left: 4px solid #555;
        }
        .status-ok { background: #1a361e; border-left-color: #2ecc71; color: #2ecc71; }
        .status-error { background: #3d1c1c; border-left-color: #e74c3c; color: #ff6b6b; }
        .status-waiting { background: #332f14; border-left-color: #f1c40f; color: #f1c40f; }
        
        .data-grid {
            display: grid;
            grid-template-columns: 1fr 1fr;
            gap: 10px;
        }
        .data-item {
            background: #0f3460;
            padding: 12px;
            border-radius: 4px;
            border: 1px solid #1a2a40;
        }
        .data-label {
            font-size: 0.8em;
            color: #888;
            margin-bottom: 4px;
            text-transform: uppercase;
        }
        .data-value {
            font-size: 1.15em;
            font-weight: bold;
            color: #00d4ff;
        }
        .cell-grid {
            display: grid;
            grid-template-columns: repeat(5, 1fr);
            gap: 8px;
            margin-top: 10px;
        }
        .cell {
            background: #0f3460;
            padding: 10px;
            border-radius: 4px;
            text-align: center;
            border: 1px solid #1a2a40;
        }
        .cell-voltage {
            font-size: 1.1em;
            font-weight: bold;
        }
        .cell-ok { color: #2ecc71; }
        .cell-warn { color: #f39c12; }
        .cell-bad { color: #e74c3c; }
        
        .debug {
            background: #05050a;
            padding: 12px;
            border-radius: 4px;
            border: 1px solid #222;
            font-family: monospace;
            font-size: 0.85em;
            max-height: 200px;
            overflow-y: auto;
            white-space: pre-wrap;
            color: #39ff14;
        }
        .buttons { text-align: center; margin: 15px 0; }
        
        /* WiFi Portal Specific Layout */
        .wifi-container {
            max-width: 900px;
            margin: 0 auto;
            display: grid;
            grid-template-columns: 1fr 1fr;
            gap: 20px;
        }
        @media (max-width: 800px) {
            .wifi-container { grid-template-columns: 1fr; }
        }
        .wifi-list-card {
            display: flex;
            flex-direction: column;
            max-height: 400px;
        }
        .wifi-list {
            margin-top: 10px;
            overflow-y: auto;
            border: 1px solid #333;
            border-radius: 4px;
            background: #05050a;
            flex-grow: 1;
        }
        .wifi-item {
            padding: 12px 15px;
            border-bottom: 1px solid #222;
            cursor: pointer;
            display: flex;
            justify-content: space-between;
            align-items: center;
            transition: all 0.2s ease;
        }
        .wifi-item:hover {
            background: #0f3460;
            color: #00d4ff;
        }
        .wifi-item:last-child {
            border-bottom: none;
        }
        .wifi-ssid-name {
            font-weight: bold;
            font-size: 0.95em;
        }
        .wifi-details {
            display: flex;
            align-items: center;
            gap: 10px;
            font-size: 0.85em;
            color: #888;
        }
        .wifi-rssi {
            color: #00d4ff;
        }
        
        .wifi-form {
            display: flex;
            flex-direction: column;
            gap: 15px;
        }
        .form-group {
            display: flex;
            flex-direction: column;
            gap: 6px;
        }
        .form-group label {
            font-size: 0.85em;
            color: #888;
            text-transform: uppercase;
            font-weight: bold;
        }
        .form-input {
            background: #05050a;
            border: 1px solid #333;
            color: #eee;
            padding: 12px;
            border-radius: 4px;
            font-size: 1em;
            outline: none;
            transition: border-color 0.25s;
        }
        .form-input:focus {
            border-color: #00d4ff;
            box-shadow: 0 0 5px rgba(0, 212, 255, 0.2);
        }
        
        /* Overlay & Modals */
        .overlay {
            position: fixed;
            top: 0;
            left: 0;
            width: 100vw;
            height: 100vh;
            background: rgba(5, 5, 10, 0.96);
            display: flex;
            flex-direction: column;
            justify-content: center;
            align-items: center;
            z-index: 1000;
            opacity: 0;
            pointer-events: none;
            transition: opacity 0.4s ease;
        }
        .overlay.show {
            opacity: 1;
            pointer-events: auto;
        }
        .overlay-card {
            background: #16213e;
            border: 1px solid #00d4ff;
            border-radius: 4px;
            padding: 30px;
            max-width: 460px;
            width: 90%;
            text-align: center;
            box-shadow: 0 0 25px rgba(0, 212, 255, 0.25);
            transform: scale(0.9);
            transition: transform 0.4s cubic-bezier(0.175, 0.885, 0.32, 1.15);
        }
        .overlay.show .overlay-card {
            transform: scale(1);
        }
        .overlay-title {
            color: #00d4ff;
            font-size: 1.4em;
            margin-bottom: 15px;
            font-weight: 800;
            text-transform: uppercase;
            letter-spacing: 1px;
        }
        .overlay-msg {
            margin-bottom: 20px;
            line-height: 1.6;
            color: #ccc;
        }
        .countdown {
            font-size: 3.5em;
            font-weight: 900;
            color: #2ecc71;
            margin: 15px 0;
            text-shadow: 0 0 10px rgba(46, 204, 113, 0.3);
        }
        
        .spinner {
            border: 3px solid rgba(255, 255, 255, 0.1);
            width: 32px;
            height: 32px;
            border-radius: 50%;
            border-left-color: #00d4ff;
            animation: spin 0.8s linear infinite;
            margin: 10px auto;
        }
        @keyframes spin {
            0% { transform: rotate(0deg); }
            100% { transform: rotate(360deg); }
        }

        /* 📱 Responsive Mobile Adjustments */
        @media (max-width: 600px) {
            body {
                padding: 10px;
            }
            .header {
                font-size: 1.5em;
                margin-bottom: 15px;
            }
            .tabs {
                margin-bottom: 15px;
                gap: 6px;
            }
            .tab-btn {
                padding: 10px 8px;
                font-size: 0.85em;
                gap: 4px;
            }
            .container {
                grid-template-columns: 1fr;
                gap: 12px;
            }
            .card {
                padding: 15px;
            }
            .card h2 {
                font-size: 1.05em;
                margin-bottom: 12px;
            }
            .data-grid {
                grid-template-columns: repeat(2, 1fr); /* Keep 2 cols for vertical compactness */
                gap: 8px;
            }
            .data-item {
                padding: 8px;
            }
            .data-label {
                font-size: 0.75em;
            }
            .data-value {
                font-size: 1em;
            }
            .cell-grid {
                grid-template-columns: repeat(5, 1fr); /* Fit all 5 cells in one row */
                gap: 4px;
            }
            .cell {
                padding: 8px 2px;
            }
            .cell .data-label {
                font-size: 0.7em;
                margin-bottom: 2px;
            }
            .cell-voltage {
                font-size: 0.88em;
            }
            
            /* Flow buttons cleanly side-by-side or stack */
            .buttons {
                display: flex;
                flex-wrap: wrap;
                justify-content: center;
                gap: 6px;
                margin: 10px 0;
            }
            .buttons .btn {
                margin: 0;
                flex: 1 1 120px;
                padding: 10px 12px;
                font-size: 0.82em;
            }
            
            /* WiFi form on mobile */
            .wifi-container {
                grid-template-columns: 1fr;
                gap: 12px;
            }
            .wifi-list-card {
                max-height: 320px;
            }
            
            .overlay-card {
                padding: 20px 15px;
            }
            .overlay-title {
                font-size: 1.25em;
            }
            .overlay-msg {
                font-size: 0.9em;
            }
        }
    </style>
</head>
<body>
    <h1 class="header">🔋 Open Battery Info</h1>
    
    <div class="tabs">
        <button id="btn-tab-battery" class="tab-btn active" onclick="switchTab('battery')">🔋 Quản lý Pin</button>
        <button id="btn-tab-wifi" class="tab-btn" onclick="switchTab('wifi')">📶 Cấu hình Wi-Fi</button>
    </div>

    <!-- TAB 1: BATTERY MONITOR -->
    <div id="battery-tab" class="tab-content active-content">
        <div class="container">
            <div class="card">
                <h2>Trạng thái kết nối</h2>
                <div id="connectionStatus" class="status status-waiting">
                    Đang chờ kết nối với pin...
                </div>
                <div class="buttons">
                    <button class="btn btn-success" onclick="readBattery()">Đọc Thông Tin Pin</button>
                    <button class="btn" onclick="readVoltages()">Đọc Điện Áp</button>
                </div>
            </div>

            <div class="card">
                <h2>Thông tin pin</h2>
                <div class="data-grid">
                    <div class="data-item">
                        <div class="data-label">Model</div>
                        <div class="data-value" id="model">--</div>
                    </div>
                    <div class="data-item">
                        <div class="data-label">Trạng thái</div>
                        <div class="data-value" id="status">--</div>
                    </div>
                    <div class="data-item">
                        <div class="data-label">Số lần sạc</div>
                        <div class="data-value" id="chargeCount">--</div>
                    </div>
                    <div class="data-item">
                        <div class="data-label">Ngày sản xuất</div>
                        <div class="data-value" id="mfgDate">--</div>
                    </div>
                    <div class="data-item">
                        <div class="data-label">Dung lượng thiết kế</div>
                        <div class="data-value" id="capacity">--</div>
                    </div>
                    <div class="data-item">
                        <div class="data-label">Tổng điện áp pack</div>
                        <div class="data-value" id="packVoltage">--</div>
                    </div>
                </div>
            </div>

            <div class="card">
                <h2>Điện áp từng Cell</h2>
                <div class="cell-grid" id="cellGrid">
                    <div class="cell"><div class="data-label">Cell 1</div><div class="cell-voltage" id="cell1">--</div></div>
                    <div class="cell"><div class="data-label">Cell 2</div><div class="cell-voltage" id="cell2">--</div></div>
                    <div class="cell"><div class="data-label">Cell 3</div><div class="cell-voltage" id="cell3">--</div></div>
                    <div class="cell"><div class="data-label">Cell 4</div><div class="cell-voltage" id="cell4">--</div></div>
                    <div class="cell"><div class="data-label">Cell 5</div><div class="cell-voltage" id="cell5">--</div></div>
                </div>
                <div class="data-grid" style="margin-top: 15px;">
                    <div class="data-item">
                        <div class="data-label">Độ lệch Cell</div>
                        <div class="data-value" id="cellDiff">--</div>
                    </div>
                    <div class="data-item">
                        <div class="data-label">Nhiệt độ cell</div>
                        <div class="data-value" id="tempCell">--</div>
                    </div>
                    <div class="data-item">
                        <div class="data-label">Nhiệt độ MOSFET</div>
                        <div class="data-value" id="tempMosfet">--</div>
                    </div>
                    <div class="data-item">
                        <div class="data-label">Mã lỗi (Error Code)</div>
                        <div class="data-value" id="errorCode">--</div>
                    </div>
                </div>
            </div>

            <div class="card">
                <h2>Điều khiển & Thao tác</h2>
                <div class="buttons">
                    <button class="btn" onclick="testLeds(true)">Bật LED Test</button>
                    <button class="btn" onclick="testLeds(false)">Tắt LED Test</button>
                    <button class="btn btn-danger" onclick="resetErrors()">Xóa Lỗi Pin</button>
                </div>
            </div>

            <div class="card card-wide">
                <h2>Nhật ký hệ thống (Debug)</h2>
                <div class="debug" id="debugLog">OBI ESP32 Web Interface Ready\n</div>
            </div>
        </div>
    </div>

    <!-- TAB 2: WI-FI SETTINGS -->
    <div id="wifi-tab" class="tab-content">
        <div class="wifi-container">
            <div class="card wifi-list-card">
                <h2>Mạng Wi-Fi xung quanh</h2>
                <div class="buttons" style="text-align: left; margin: 5px 0 10px 0;">
                    <button class="btn btn-success" style="padding: 8px 16px; font-size: 0.85em;" onclick="scanWifi()">Quét mạng Wi-Fi</button>
                </div>
                <div class="wifi-list" id="wifiList">
                    <div style="padding: 20px; text-align: center; color: #888;">
                        Nhấn nút phía trên để bắt đầu quét mạng Wi-Fi
                    </div>
                </div>
            </div>

            <div class="card">
                <h2>Cấu hình kết nối Wi-Fi</h2>
                <form class="wifi-form" onsubmit="saveWifi(event)">
                    <div class="form-group">
                        <label for="wifiSsid">Tên Wi-Fi (SSID)</label>
                        <input type="text" id="wifiSsid" class="form-input" placeholder="Chọn mạng từ danh sách hoặc tự nhập" required>
                    </div>
                    <div class="form-group">
                        <label for="wifiPass">Mật khẩu (Password)</label>
                        <input type="password" id="wifiPass" class="form-input" placeholder="Nhập mật khẩu Wi-Fi (nếu có)">
                    </div>
                    <div id="wifiStatus" class="status" style="display: none; margin-top: 10px;"></div>
                    <div class="buttons" style="text-align: left; margin: 10px 0 0 0;">
                        <button type="submit" id="wifiSubmitBtn" class="btn btn-success" style="width: 100%; margin: 0;">Lưu cấu hình & Kết nối</button>
                    </div>
                </form>
            </div>
        </div>
    </div>

    <!-- REBOOT OVERLAY -->
    <div class="overlay" id="rebootOverlay">
        <div class="overlay-card">
            <div class="overlay-title">Đang kết nối Wi-Fi...</div>
            <div class="overlay-msg">
                Thiết bị đang lưu cấu hình và khởi động lại để kết nối với mạng Wi-Fi mới.<br>
                Vui lòng kết nối lại điện thoại/máy tính của bạn vào mạng Wi-Fi nhà và truy cập lại thiết bị.
            </div>
            <div class="spinner"></div>
            <div class="countdown" id="countdownTimer">10</div>
        </div>
    </div>

    <script>
        function log(msg) {
            const el = document.getElementById('debugLog');
            const time = new Date().toLocaleTimeString();
            el.textContent += `[${time}] ${msg}\n`;
            el.scrollTop = el.scrollHeight;
        }

        function setStatus(msg, type) {
            const el = document.getElementById('connectionStatus');
            el.textContent = msg;
            el.className = 'status status-' + type;
        }

        function setCellClass(el, voltage) {
            el.classList.remove('cell-ok', 'cell-warn', 'cell-bad');
            if (voltage >= 3.5) el.classList.add('cell-ok');
            else if (voltage >= 3.0) el.classList.add('cell-warn');
            else el.classList.add('cell-bad');
        }

        async function apiCall(endpoint) {
            try {
                const response = await fetch('/api/' + endpoint);
                return await response.json();
            } catch (e) {
                log('Error: ' + e.message);
                return null;
            }
        }

        async function readBattery() {
            setStatus('Đang đọc thông tin pin...', 'waiting');
            log('Reading battery info...');

            const data = await apiCall('read');
            if (data && data.success) {
                document.getElementById('model').textContent = data.model || '--';
                document.getElementById('status').textContent = data.locked ? 'LOCKED (Khóa)' : 'OK (Hoạt động)';
                document.getElementById('chargeCount').textContent = data.chargeCount || '--';
                document.getElementById('mfgDate').textContent = data.mfgDate || '--';
                document.getElementById('capacity').textContent = data.capacity ? data.capacity + ' Ah' : '--';
                document.getElementById('errorCode').textContent = data.errorCode;
                document.getElementById('packVoltage').textContent = data.packVoltage.toFixed(2) + ' V';

                for (let i = 1; i <= 5; i++) {
                    const el = document.getElementById('cell' + i);
                    const v = data['cell' + i];
                    el.textContent = v.toFixed(3) + ' V';
                    setCellClass(el, v);
                }

                document.getElementById('cellDiff').textContent = data.cellDiff.toFixed(3) + ' V';
                document.getElementById('tempCell').textContent = data.tempCell.toFixed(1) + ' °C';
                document.getElementById('tempMosfet').textContent = data.tempMosfet ? data.tempMosfet.toFixed(1) + ' °C' : '--';

                setStatus('Đã kết nối pin: ' + (data.model || 'Unknown'), 'ok');
                log('Battery read successful: ' + data.model);
            } else {
                setStatus('Không thể kết nối hoặc đọc thông tin pin', 'error');
                log('Battery read failed');
            }
        }

        async function readVoltages() {
            log('Reading voltages...');

            const data = await apiCall('voltages');
            if (data && data.success) {
                document.getElementById('packVoltage').textContent = data.packVoltage.toFixed(2) + ' V';

                for (let i = 1; i <= 5; i++) {
                    const el = document.getElementById('cell' + i);
                    const v = data['cell' + i];
                    el.textContent = v.toFixed(3) + ' V';
                    setCellClass(el, v);
                }

                document.getElementById('cellDiff').textContent = data.cellDiff.toFixed(3) + ' V';
                document.getElementById('tempCell').textContent = data.tempCell.toFixed(1) + ' °C';
                document.getElementById('tempMosfet').textContent = data.tempMosfet ? data.tempMosfet.toFixed(1) + ' °C' : '--';
                log('Voltages read successful');
            } else {
                log('Voltage read failed');
            }
        }

        async function testLeds(on) {
            log('LEDs ' + (on ? 'ON' : 'OFF'));
            await apiCall('leds?state=' + (on ? '1' : '0'));
        }

        async function resetErrors() {
            log('Resetting errors...');
            const data = await apiCall('reset');
            if (data && data.success) {
                log('Errors reset successfully');
            } else {
                log('Error reset failed');
            }
        }
        
        // Tab switching logic
        function switchTab(tabId) {
            // Toggle active state of tab buttons
            document.getElementById('btn-tab-battery').classList.toggle('active', tabId === 'battery');
            document.getElementById('btn-tab-wifi').classList.toggle('active', tabId === 'wifi');
            
            // Toggle visibility of tab contents
            document.getElementById('battery-tab').classList.toggle('active-content', tabId === 'battery');
            document.getElementById('wifi-tab').classList.toggle('active-content', tabId === 'wifi');
            
            log('Switched to tab: ' + tabId);
            
            if (tabId === 'wifi') {
                scanWifi();
            }
        }
        
        // WiFi Scanner API call
        async function scanWifi() {
            const listEl = document.getElementById('wifiList');
            listEl.innerHTML = '<div style="padding: 20px; text-align: center;"><div class="spinner"></div> Đang tìm kiếm Wi-Fi...</div>';
            log('Scanning Wi-Fi networks...');
            
            const data = await apiCall('wifi/scan');
            if (data && Array.isArray(data)) {
                if (data.length === 0) {
                    listEl.innerHTML = '<div style="padding: 20px; text-align: center; color: #888;">Không quét được mạng Wi-Fi nào. Hãy thử quét lại.</div>';
                    log('WiFi networks list is empty');
                    return;
                }
                
                listEl.innerHTML = '';
                data.forEach(net => {
                    const item = document.createElement('div');
                    item.className = 'wifi-item';
                    
                    let strength = '📶';
                    if (net.rssi > -50) strength += ' 🟢';
                    else if (net.rssi > -70) strength += ' 🟡';
                    else strength += ' 🔴';
                    
                    const isSecured = net.encryption === 'secured';
                    const lockText = isSecured ? '🔒' : '🔓';
                    
                    item.innerHTML = `
                        <div class="wifi-ssid-name">${net.ssid}</div>
                        <div class="wifi-details">
                            <span class="wifi-rssi">${strength} ${net.rssi} dBm</span>
                            <span>${lockText}</span>
                        </div>
                    `;
                    item.onclick = () => {
                        document.getElementById('wifiSsid').value = net.ssid;
                        document.getElementById('wifiPass').focus();
                        log('Autofilled SSID: ' + net.ssid);
                    };
                    listEl.appendChild(item);
                });
                log('Found ' + data.length + ' Wi-Fi networks.');
            } else {
                listEl.innerHTML = '<div style="padding: 20px; text-align: center; color: #e74c3c;">Lỗi khi quét mạng Wi-Fi</div>';
                log('Failed to fetch Wi-Fi scan list.');
            }
        }
        
        // Save Wi-Fi config API call
        async function saveWifi(e) {
            e.preventDefault();
            
            const ssid = document.getElementById('wifiSsid').value.trim();
            const pass = document.getElementById('wifiPass').value;
            const statusEl = document.getElementById('wifiStatus');
            const submitBtn = document.getElementById('wifiSubmitBtn');
            
            if (!ssid) {
                statusEl.style.display = 'block';
                statusEl.className = 'status status-error';
                statusEl.textContent = 'Vui lòng nhập tên Wi-Fi (SSID)';
                return;
            }
            
            statusEl.style.display = 'block';
            statusEl.className = 'status status-waiting';
            statusEl.textContent = 'Đang gửi thông tin cấu hình...';
            submitBtn.disabled = true;
            
            log('Saving configuration for SSID: ' + ssid);
            
            try {
                const response = await fetch('/api/wifi/save', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({ ssid: ssid, pass: pass })
                });
                const data = await response.json();
                
                if (data && data.success) {
                    log('Credentials saved successfully. Rebooting device...');
                    statusEl.className = 'status status-ok';
                    statusEl.textContent = 'Lưu cấu hình thành công! Đang khởi động lại...';
                    
                    // Show Reboot Countdown Overlay
                    const overlay = document.getElementById('rebootOverlay');
                    overlay.classList.add('show');
                    
                    let countdown = 10;
                    const timerEl = document.getElementById('countdownTimer');
                    timerEl.textContent = countdown;
                    
                    const interval = setInterval(() => {
                        countdown--;
                        timerEl.textContent = countdown;
                        if (countdown <= 0) {
                            clearInterval(interval);
                            window.location.href = 'http://obi-esp32.local';
                        }
                    }, 1000);
                } else {
                    statusEl.className = 'status status-error';
                    statusEl.textContent = 'Lưu cấu hình thất bại: ' + (data.error || 'Lỗi không xác định');
                    submitBtn.disabled = false;
                    log('Failed to save Wi-Fi config');
                }
            } catch (err) {
                statusEl.className = 'status status-error';
                statusEl.textContent = 'Lỗi kết nối: ' + err.message;
                submitBtn.disabled = false;
                log('Error saving Wi-Fi: ' + err.message);
            }
        }

        // Initial check
        log('Checking connection...');
    </script>
</body>
</html>
)rawliteral";

#endif // WEB_INTERFACE_H
