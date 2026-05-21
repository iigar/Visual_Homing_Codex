"""
Flask Web Server for Visual Homing
Веб-інтерфейс моніторингу
"""
import cv2
import json
import time
import logging
import threading
from flask import Flask, Response, render_template_string, jsonify, request
from flask_socketio import SocketIO, emit
import numpy as np

logger = logging.getLogger(__name__)

app = Flask(__name__)
app.config['SECRET_KEY'] = 'visual_homing_secret'
socketio = SocketIO(app, cors_allowed_origins="*", async_mode='threading')

# Global reference to visual homing system
_system = None
_web_config = None


HTML_TEMPLATE = '''
<!DOCTYPE html>
<html lang="uk">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Visual Homing - Моніторинг</title>
    <style>
        * { margin: 0; padding: 0; box-sizing: border-box; }
        body {
            font-family: 'Segoe UI', sans-serif;
            background: #0a0a0f;
            color: #e0e0e0;
            min-height: 100vh;
        }
        .container {
            max-width: 1600px;
            margin: 0 auto;
            padding: 10px;
        }
        header {
            display: flex;
            justify-content: space-between;
            align-items: center;
            padding: 10px 0;
            border-bottom: 1px solid #2a2a35;
            margin-bottom: 15px;
        }
        h1 { color: #4ecdc4; font-size: 20px; font-weight: 600; }
        .status-badge {
            padding: 6px 14px;
            border-radius: 20px;
            font-size: 12px;
            font-weight: 600;
        }
        .status-idle { background: #2a2a35; color: #888; }
        .status-recording { background: #ff4444; color: white; animation: pulse 1s infinite; }
        .status-returning { background: #44ff44; color: black; animation: pulse 1s infinite; }
        
        .main-grid {
            display: grid;
            grid-template-columns: 1fr 1fr;
            gap: 15px;
        }
        
        .left-column { display: flex; flex-direction: column; gap: 15px; }
        .right-column { display: flex; flex-direction: column; gap: 15px; }
        
        /* Video Panel - Smaller */
        .video-panel {
            background: #12121a;
            border-radius: 10px;
            overflow: hidden;
            border: 1px solid #2a2a35;
        }
        .video-panel img { width: 100%; max-height: 280px; object-fit: contain; display: block; }
        .video-header {
            padding: 8px 12px;
            border-bottom: 1px solid #2a2a35;
            display: flex;
            justify-content: space-between;
            font-size: 12px;
        }
        .fps-display { color: #4ecdc4; font-weight: 600; }
        
        /* 3D Mini Map */
        .map-panel {
            background: #12121a;
            border-radius: 10px;
            border: 1px solid #2a2a35;
            flex: 1;
            min-height: 250px;
            position: relative;
        }
        .map-header {
            padding: 8px 12px;
            border-bottom: 1px solid #2a2a35;
            font-size: 12px;
            color: #4ecdc4;
        }
        .map-canvas {
            width: 100%;
            height: calc(100% - 35px);
            background: radial-gradient(circle at center, #1a1a25 0%, #0a0a0f 100%);
        }
        
        /* Controls - Compact */
        .controls-row {
            display: flex;
            gap: 10px;
        }
        .btn-small {
            flex: 1;
            padding: 8px 12px;
            border: none;
            border-radius: 6px;
            font-size: 11px;
            font-weight: 600;
            cursor: pointer;
            transition: all 0.2s;
        }
        .btn-record { background: linear-gradient(135deg, #ff4444, #cc0000); color: white; }
        .btn-stop { background: linear-gradient(135deg, #666, #444); color: white; }
        .btn-return {
            padding: 15px 20px;
            border: none;
            border-radius: 8px;
            font-size: 16px;
            font-weight: 600;
            cursor: pointer;
            background: linear-gradient(135deg, #44ff44, #00cc00);
            color: black;
            width: 100%;
        }
        .btn-small:hover, .btn-return:hover {
            transform: translateY(-1px);
            box-shadow: 0 3px 15px rgba(0,0,0,0.3);
        }
        
        /* Stats - Compact (60% smaller) */
        .stats-grid {
            display: grid;
            grid-template-columns: repeat(4, 1fr);
            gap: 8px;
        }
        .stat-box {
            background: #1a1a25;
            padding: 8px;
            border-radius: 6px;
            text-align: center;
            border: 1px solid #2a2a35;
        }
        .stat-value { font-size: 16px; font-weight: 600; color: #4ecdc4; }
        .stat-label { font-size: 9px; color: #666; text-transform: uppercase; margin-top: 2px; }
        
        /* Telemetry - Large (2x bigger) */
        .telemetry-section {
            background: #12121a;
            border-radius: 10px;
            padding: 15px;
            border: 1px solid #2a2a35;
        }
        .telemetry-title {
            color: #4ecdc4;
            font-size: 12px;
            text-transform: uppercase;
            letter-spacing: 1px;
            margin-bottom: 12px;
        }
        .telemetry-grid {
            display: grid;
            grid-template-columns: repeat(3, 1fr);
            gap: 10px;
        }
        .telem-box {
            background: #1a1a25;
            padding: 12px;
            border-radius: 8px;
            text-align: center;
            border: 1px solid #2a2a35;
        }
        .telem-value {
            font-size: 24px;
            font-weight: 700;
            font-family: 'Consolas', monospace;
        }
        .telem-label {
            font-size: 10px;
            color: #666;
            text-transform: uppercase;
            margin-top: 4px;
        }
        .telem-pos { color: #4ecdc4; }
        .telem-att { color: #ff9f43; }
        .telem-status { color: #44ff44; }
        .telem-warn { color: #ff4444; }
        
        /* Attitude indicators */
        .attitude-row {
            display: grid;
            grid-template-columns: repeat(3, 1fr);
            gap: 10px;
            margin-top: 10px;
        }
        
        /* Status row */
        .status-row {
            display: grid;
            grid-template-columns: repeat(2, 1fr);
            gap: 10px;
            margin-top: 10px;
        }
        
        @keyframes pulse {
            0%, 100% { opacity: 1; }
            50% { opacity: 0.7; }
        }
        
        /* Mobile responsive */
        @media (max-width: 900px) {
            .main-grid { grid-template-columns: 1fr; }
            .telemetry-grid { grid-template-columns: repeat(2, 1fr); }
            .attitude-row { grid-template-columns: repeat(3, 1fr); }
        }
    </style>
</head>
<body>
    <div class="container">
        <header>
            <h1>Visual Homing</h1>
            <span class="status-badge" id="statusBadge">IDLE</span>
        </header>
        
        <div class="main-grid">
            <!-- Left Column: Video + Map -->
            <div class="left-column">
                <div class="video-panel">
                    <div class="video-header">
                        <span>Камера</span>
                        <span class="fps-display" id="fps">-- FPS</span>
                    </div>
                    <img src="/video_feed" id="videoFeed" alt="Camera Feed">
                </div>
                
                <div class="map-panel">
                    <div class="map-header">3D Карта маршруту</div>
                    <canvas id="mapCanvas" class="map-canvas"></canvas>
                </div>
            </div>
            
            <!-- Right Column: Controls + Telemetry -->
            <div class="right-column">
                <!-- Main Return Button -->
                <button class="btn-return" onclick="startReturn()">
                    ↩ ПОВЕРНЕННЯ (Smart RTL)
                </button>
                
                <!-- Small Control Buttons -->
                <div class="controls-row">
                    <button class="btn-small btn-record" onclick="startRecording()">● ЗАПИС</button>
                    <button class="btn-small btn-stop" onclick="stopAll()">■ СТОП</button>
                </div>
                
                <!-- Stats - Compact -->
                <div class="stats-grid">
                    <div class="stat-box">
                        <div class="stat-value" id="keyframes">0</div>
                        <div class="stat-label">Keyframes</div>
                    </div>
                    <div class="stat-box">
                        <div class="stat-value" id="altitude">0.0m</div>
                        <div class="stat-label">Висота</div>
                    </div>
                    <div class="stat-box">
                        <div class="stat-value" id="features">0</div>
                        <div class="stat-label">Features</div>
                    </div>
                    <div class="stat-box">
                        <div class="stat-value" id="progress">0%</div>
                        <div class="stat-label">Прогрес</div>
                    </div>
                </div>
                
                <!-- Telemetry - Large -->
                <div class="telemetry-section">
                    <div class="telemetry-title">Телеметрія позиції</div>
                    <div class="telemetry-grid">
                        <div class="telem-box">
                            <div class="telem-value telem-pos" id="posX">0.00</div>
                            <div class="telem-label">Position X (m)</div>
                        </div>
                        <div class="telem-box">
                            <div class="telem-value telem-pos" id="posY">0.00</div>
                            <div class="telem-label">Position Y (m)</div>
                        </div>
                        <div class="telem-box">
                            <div class="telem-value telem-pos" id="posZ">0.00</div>
                            <div class="telem-label">Position Z (m)</div>
                        </div>
                    </div>
                    
                    <div class="telemetry-title" style="margin-top: 15px;">Орієнтація дрона</div>
                    <div class="attitude-row">
                        <div class="telem-box">
                            <div class="telem-value telem-att" id="roll">0.0°</div>
                            <div class="telem-label">Roll</div>
                        </div>
                        <div class="telem-box">
                            <div class="telem-value telem-att" id="pitch">0.0°</div>
                            <div class="telem-label">Pitch</div>
                        </div>
                        <div class="telem-box">
                            <div class="telem-value telem-att" id="yaw">0.0°</div>
                            <div class="telem-label">Yaw</div>
                        </div>
                    </div>
                    
                    <div class="telemetry-title" style="margin-top: 15px;">Статус системи</div>
                    <div class="status-row">
                        <div class="telem-box">
                            <div class="telem-value" id="mavlink" style="font-size: 16px;">--</div>
                            <div class="telem-label">MAVLink</div>
                        </div>
                        <div class="telem-box">
                            <div class="telem-value" id="gps" style="font-size: 16px;">--</div>
                            <div class="telem-label">GPS</div>
                        </div>
                    </div>
                </div>
            </div>
        </div>
    </div>
    
    <script src="https://cdnjs.cloudflare.com/ajax/libs/socket.io/4.0.1/socket.io.js"></script>
    <script>
        const socket = io();
        let routePoints = [];
        let currentPos = {x: 0, y: 0, z: 0};
        let lastFrameTime = Date.now();
        let frameCount = 0;
        
        // FPS calculation
        setInterval(() => {
            document.getElementById('fps').textContent = frameCount + ' FPS';
            frameCount = 0;
        }, 1000);
        
        // Count frames from video
        const videoImg = document.getElementById('videoFeed');
        videoImg.onload = () => { frameCount++; };
        
        socket.on('status', function(data) {
            // Update status badge
            const badge = document.getElementById('statusBadge');
            badge.textContent = data.state ? data.state.toUpperCase() : 'IDLE';
            badge.className = 'status-badge status-' + (data.state || 'idle');
            
            // Update stats
            document.getElementById('keyframes').textContent = data.keyframes || 0;
            document.getElementById('altitude').textContent = (data.altitude || 0).toFixed(1) + 'm';
            document.getElementById('features').textContent = data.features || 0;
            document.getElementById('progress').textContent = (data.progress || 0).toFixed(0) + '%';
            
            // Update position telemetry
            const posX = data.pose?.x || 0;
            const posY = data.pose?.y || 0;
            const posZ = data.altitude || 0;
            document.getElementById('posX').textContent = posX.toFixed(2);
            document.getElementById('posY').textContent = posY.toFixed(2);
            document.getElementById('posZ').textContent = posZ.toFixed(2);
            
            // Update attitude (roll, pitch, yaw) - from Flight Controller
            const roll = (data.attitude?.roll || 0) * 180 / Math.PI;
            const pitch = (data.attitude?.pitch || 0) * 180 / Math.PI;
            const yaw = (data.attitude?.yaw || 0) * 180 / Math.PI;  // Use FC yaw, not VO
            document.getElementById('roll').textContent = roll.toFixed(1) + '°';
            document.getElementById('pitch').textContent = pitch.toFixed(1) + '°';
            document.getElementById('yaw').textContent = yaw.toFixed(1) + '°';
            
            // Update status indicators
            const mavlinkEl = document.getElementById('mavlink');
            mavlinkEl.textContent = data.mavlink_connected ? 'OK' : 'OFFLINE';
            mavlinkEl.className = 'telem-value ' + (data.mavlink_connected ? 'telem-status' : 'telem-warn');
            
            const gpsEl = document.getElementById('gps');
            gpsEl.textContent = data.gps_fix ? data.gps_fix + ' SAT' : 'NO FIX';
            gpsEl.className = 'telem-value ' + (data.gps_fix ? 'telem-status' : 'telem-warn');
            
            // Store position for map
            currentPos = {x: posX, y: posY, z: posZ};
            if (data.state === 'recording' && data.keyframes > routePoints.length) {
                routePoints.push({...currentPos});
            }
            
            drawMap();
        });
        
        // 3D Mini Map drawing
        function drawMap() {
            const canvas = document.getElementById('mapCanvas');
            const ctx = canvas.getContext('2d');
            const rect = canvas.parentElement.getBoundingClientRect();
            canvas.width = rect.width;
            canvas.height = rect.height - 35;
            
            const cx = canvas.width / 2;
            const cy = canvas.height / 2;
            const scale = 5; // pixels per meter
            
            // Clear
            ctx.fillStyle = '#0a0a0f';
            ctx.fillRect(0, 0, canvas.width, canvas.height);
            
            // Draw grid
            ctx.strokeStyle = '#1a1a25';
            ctx.lineWidth = 1;
            for (let i = -10; i <= 10; i++) {
                // Vertical lines
                ctx.beginPath();
                ctx.moveTo(cx + i * scale * 10, 0);
                ctx.lineTo(cx + i * scale * 10, canvas.height);
                ctx.stroke();
                // Horizontal lines
                ctx.beginPath();
                ctx.moveTo(0, cy + i * scale * 10);
                ctx.lineTo(canvas.width, cy + i * scale * 10);
                ctx.stroke();
            }
            
            // Draw home point
            ctx.fillStyle = '#44ff44';
            ctx.beginPath();
            ctx.arc(cx, cy, 8, 0, Math.PI * 2);
            ctx.fill();
            ctx.fillStyle = '#000';
            ctx.font = '10px sans-serif';
            ctx.textAlign = 'center';
            ctx.fillText('H', cx, cy + 3);
            
            // Draw route
            if (routePoints.length > 1) {
                ctx.strokeStyle = '#4ecdc4';
                ctx.lineWidth = 2;
                ctx.beginPath();
                ctx.moveTo(cx + routePoints[0].x * scale, cy - routePoints[0].y * scale);
                for (let i = 1; i < routePoints.length; i++) {
                    ctx.lineTo(cx + routePoints[i].x * scale, cy - routePoints[i].y * scale);
                }
                ctx.stroke();
                
                // Draw keyframe points
                ctx.fillStyle = '#ff4444';
                routePoints.forEach(p => {
                    ctx.beginPath();
                    ctx.arc(cx + p.x * scale, cy - p.y * scale, 3, 0, Math.PI * 2);
                    ctx.fill();
                });
            }
            
            // Draw current position (drone)
            const droneX = cx + currentPos.x * scale;
            const droneY = cy - currentPos.y * scale;
            ctx.fillStyle = '#ffff00';
            ctx.beginPath();
            ctx.arc(droneX, droneY, 6, 0, Math.PI * 2);
            ctx.fill();
            
            // Distance from home
            const dist = Math.sqrt(currentPos.x**2 + currentPos.y**2);
            ctx.fillStyle = '#666';
            ctx.font = '10px sans-serif';
            ctx.textAlign = 'left';
            ctx.fillText('Dist: ' + dist.toFixed(1) + 'm', 10, canvas.height - 10);
        }
        
        function startRecording() {
            routePoints = [];
            fetch('/api/recording/start', {method: 'POST'})
                .then(r => r.json())
                .then(d => console.log('Recording:', d));
        }
        
        function startReturn() {
            fetch('/api/return/start', {method: 'POST'})
                .then(r => r.json())
                .then(d => console.log('Return:', d));
        }
        
        function stopAll() {
            fetch('/api/stop', {method: 'POST'})
                .then(r => r.json())
                .then(d => console.log('Stopped:', d));
        }
        
        // Initial map draw
        setTimeout(drawMap, 100);
        window.addEventListener('resize', drawMap);
    </script>
</body>
</html>
'''


@app.route('/')
def index():
    return render_template_string(HTML_TEMPLATE)


@app.route('/video_feed')
def video_feed():
    """MJPEG video stream"""
    def generate():
        while True:
            try:
                if _system and _system.camera:
                    frame, info = _system.camera.get_frame()
                    if frame is not None:
                        # Draw features if available (safe)
                        try:
                            if hasattr(_system, 'vo') and hasattr(_system.vo, '_prev_pts') and _system.vo._prev_pts is not None:
                                frame = _system.vo.detector.draw_features(
                                    frame, _system.vo._prev_pts
                                )
                        except Exception as e:
                            logger.debug(f"draw_features skipped: {e}")

                        quality = _web_config.video_quality if _web_config else 50
                        ret, jpeg = cv2.imencode('.jpg', frame,
                                                [cv2.IMWRITE_JPEG_QUALITY, quality])
                        if ret:
                            yield (b'--frame\r\n'
                                   b'Content-Type: image/jpeg\r\n\r\n' +
                                   jpeg.tobytes() + b'\r\n')
            except Exception as e:
                logger.error(f"video_feed error: {e}")
            time.sleep(0.05)  # ~20 FPS
    
    return Response(generate(), 
                   mimetype='multipart/x-mixed-replace; boundary=frame')


@app.route('/api/status')
def get_status():
    """Get current system status"""
    if not _system:
        return jsonify({'error': 'System not initialized'})
    
    return jsonify({
        'state': _system.state.value,
        'keyframes': _system.route_recorder.keyframe_count,
        'altitude': _system._current_altitude,
        'pose': {
            'x': _system._current_pose.x,
            'y': _system._current_pose.y,
            'z': _system._current_pose.z,
            'yaw': _system._current_pose.yaw
        },
        'mavlink_connected': _system.mavlink.is_connected if _system.mavlink else False,
        'progress': _system.route_follower.progress if _system.route_follower.is_active else 0
    })


@app.route('/api/recording/start', methods=['POST'])
def start_recording():
    """Start route recording"""
    if not _system:
        return jsonify({'error': 'System not initialized'}), 500
    
    route_id = _system.start_recording()
    if route_id:
        return jsonify({'success': True, 'route_id': route_id})
    return jsonify({'success': False})


@app.route('/api/recording/stop', methods=['POST'])
def stop_recording():
    """Stop route recording"""
    if not _system:
        return jsonify({'error': 'System not initialized'}), 500
    
    route = _system.stop_recording()
    if route:
        return jsonify({'success': True, 'route_id': route.id})
    return jsonify({'success': False})


@app.route('/api/return/start', methods=['POST'])
def start_return():
    """Start return to home"""
    if not _system:
        return jsonify({'error': 'System not initialized'}), 500
    
    data = request.get_json(silent=True) or {}
    route_id = data.get('route_id')
    
    if _system.start_return(route_id):
        return jsonify({'success': True})
    return jsonify({'success': False})


@app.route('/api/return/stop', methods=['POST'])
def stop_return():
    """Stop return to home"""
    if not _system:
        return jsonify({'error': 'System not initialized'}), 500
    
    _system.stop_return()
    return jsonify({'success': True})


@app.route('/api/stop', methods=['POST'])
def stop_all():
    """Stop all operations"""
    if _system:
        if _system.route_recorder.is_recording:
            _system.stop_recording()
        if _system.route_follower.is_active:
            _system.stop_return()
    return jsonify({'success': True})


@app.route('/api/routes')
def list_routes():
    """List saved routes"""
    if not _system:
        return jsonify({'error': 'System not initialized'}), 500
    
    routes = _system.route_recorder.list_routes()
    return jsonify({'routes': routes})


def status_broadcast_loop():
    """Broadcast status updates via WebSocket"""
    while True:
        if _system:
            try:
                # Get attitude from MAVLink if available
                attitude = {'roll': 0, 'pitch': 0, 'yaw': 0}
                if hasattr(_system, 'mavlink') and _system.mavlink.is_connected:
                    if hasattr(_system.mavlink, '_attitude'):
                        att = _system.mavlink._attitude
                        attitude = {
                            'roll': att.get('roll', 0),
                            'pitch': att.get('pitch', 0),
                            'yaw': att.get('yaw', 0)
                        }
                
                status = {
                    'state': _system.state.value,
                    'keyframes': _system.route_recorder.keyframe_count,
                    'altitude': _system._current_altitude,
                    'pose': {
                        'x': _system._current_pose.x,
                        'y': _system._current_pose.y,
                        'z': _system._current_pose.z,
                        'yaw': _system._current_pose.yaw
                    },
                    'attitude': attitude,
                    'mavlink_connected': _system.mavlink.is_connected,
                    'gps_fix': getattr(_system.mavlink, '_gps_satellites', 0) if _system.mavlink.is_connected else 0,
                    'progress': _system.route_follower.progress if _system.route_follower.is_active else 0,
                    'features': len(_system.vo._prev_pts) if _system.vo._prev_pts is not None else 0
                }
                socketio.emit('status', status)
            except Exception as e:
                logger.error(f"Status broadcast error: {e}")
        time.sleep(0.5)


def start_web_server(system, config):
    """Start web server in background thread"""
    global _system, _web_config
    _system = system
    _web_config = config
    
    # Start status broadcast thread
    broadcast_thread = threading.Thread(target=status_broadcast_loop, daemon=True)
    broadcast_thread.start()
    
    # Start Flask in background
    server_thread = threading.Thread(
        target=lambda: socketio.run(
            app, 
            host=config.host, 
            port=config.port, 
            debug=False,
            use_reloader=False,
            allow_unsafe_werkzeug=True
        ),
        daemon=True
    )
    server_thread.start()
    
    logger.info(f"Web server started on http://{config.host}:{config.port}")
