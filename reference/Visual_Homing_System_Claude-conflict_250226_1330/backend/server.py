from fastapi import FastAPI, APIRouter, WebSocket, WebSocketDisconnect
from fastapi.responses import FileResponse, StreamingResponse, PlainTextResponse
from dotenv import load_dotenv
from starlette.middleware.cors import CORSMiddleware
from motor.motor_asyncio import AsyncIOMotorClient
import os
import logging
import json
import asyncio
from pathlib import Path
from pydantic import BaseModel, Field, ConfigDict
from typing import List, Optional, Dict, Any
import uuid
from datetime import datetime, timezone
import markdown
import zipfile
import io


ROOT_DIR = Path(__file__).parent
DOCS_DIR = ROOT_DIR.parent / 'docs'
FIRMWARE_DIR = ROOT_DIR.parent / 'firmware'

# Load .env file but DON'T override existing environment variables (production priority)
load_dotenv(ROOT_DIR / '.env', override=False)

# MongoDB connection - lazy initialization for production compatibility
# In production, MONGO_URL is set by Emergent platform to Atlas URL
mongo_url = os.environ.get('MONGO_URL', 'mongodb://localhost:27017')
db_name = os.environ.get('DB_NAME', 'visual_homing')

client = None
db = None

def get_db():
    """Get MongoDB database instance with lazy initialization"""
    global client, db
    if client is None:
        client = AsyncIOMotorClient(mongo_url)
        db = client[db_name]
    return db

# Create the main app without a prefix
app = FastAPI(title="Visual Homing Documentation API")

@app.on_event("startup")
async def startup_db_client():
    """Initialize MongoDB connection on startup"""
    global client, db
    try:
        client = AsyncIOMotorClient(mongo_url, serverSelectionTimeoutMS=2000)
        db = client[db_name]
        # Test connection
        await client.admin.command('ping')
        logging.info(f"Connected to MongoDB: {db_name}")
    except Exception as e:
        logging.warning(f"MongoDB connection warning: {e}. Running in no-DB mode.")
        client = None
        db = None

# Create a router with the /api prefix
api_router = APIRouter(prefix="/api")


# Health check endpoint for Kubernetes (at /api/health for proper routing)
@api_router.get("/health")
async def health_check():
    """Health check endpoint for Kubernetes liveness/readiness probes"""
    return {"status": "healthy", "timestamp": datetime.now(timezone.utc).isoformat()}


# Also add at root level for direct health checks
@app.get("/health")
async def root_health_check():
    """Root health check endpoint"""
    return {"status": "healthy", "timestamp": datetime.now(timezone.utc).isoformat()}


# Define Models
class StatusCheck(BaseModel):
    model_config = ConfigDict(extra="ignore")
    
    id: str = Field(default_factory=lambda: str(uuid.uuid4()))
    client_name: str
    timestamp: datetime = Field(default_factory=lambda: datetime.now(timezone.utc))

class StatusCheckCreate(BaseModel):
    client_name: str

class DocFile(BaseModel):
    name: str
    path: str
    title: str

class DocContent(BaseModel):
    name: str
    title: str
    content: str
    html: Optional[str] = None


# Documentation endpoints
@api_router.get("/")
async def root():
    return {"message": "Visual Homing API", "version": "1.0"}

@api_router.get("/docs/list")
async def list_docs():
    """List all documentation files"""
    docs = []
    if DOCS_DIR.exists():
        for f in sorted(DOCS_DIR.glob("*.md")):
            # Extract title from first line
            with open(f, 'r', encoding='utf-8') as file:
                first_line = file.readline().strip()
                title = first_line.replace('#', '').strip() if first_line.startswith('#') else f.stem
            docs.append(DocFile(name=f.name, path=str(f), title=title))
    return docs

@api_router.get("/docs/{filename}")
async def get_doc(filename: str):
    """Get documentation file content"""
    filepath = DOCS_DIR / filename
    if not filepath.exists():
        return {"error": "Document not found"}
    
    with open(filepath, 'r', encoding='utf-8') as f:
        content = f.read()
    
    # Convert markdown to HTML
    html = markdown.markdown(content, extensions=['tables', 'fenced_code'])
    
    # Extract title
    first_line = content.split('\n')[0]
    title = first_line.replace('#', '').strip() if first_line.startswith('#') else filename
    
    return DocContent(name=filename, title=title, content=content, html=html)

@api_router.get("/firmware/structure")
async def get_firmware_structure():
    """Get firmware directory structure"""
    structure = {"python": [], "cpp": [], "scripts": [], "config": []}
    
    if FIRMWARE_DIR.exists():
        # Python files
        for f in (FIRMWARE_DIR / 'python').rglob("*.py"):
            structure["python"].append(str(f.relative_to(FIRMWARE_DIR)))
        
        # C++ files
        for ext in ['*.cpp', '*.hpp', '*.h']:
            for f in (FIRMWARE_DIR / 'cpp').rglob(ext):
                structure["cpp"].append(str(f.relative_to(FIRMWARE_DIR)))
        
        # Scripts
        for f in (FIRMWARE_DIR / 'scripts').glob("*.sh"):
            structure["scripts"].append(str(f.relative_to(FIRMWARE_DIR)))
        
        # Config files
        for f in (FIRMWARE_DIR / 'config').glob("*"):
            structure["config"].append(str(f.relative_to(FIRMWARE_DIR)))
    
    return structure

@api_router.get("/firmware/file/{filepath:path}")
async def get_firmware_file(filepath: str):
    """Get firmware file content"""
    full_path = FIRMWARE_DIR / filepath
    if not full_path.exists():
        return {"error": "File not found"}
    
    with open(full_path, 'r', encoding='utf-8') as f:
        content = f.read()
    
    return {"path": filepath, "content": content}


@api_router.get("/scripts/download/{script_name}")
async def download_script(script_name: str):
    """Download installation script as raw file"""
    # Check in multiple locations
    script_paths = [
        ROOT_DIR.parent / 'scripts' / script_name,
        FIRMWARE_DIR / 'scripts' / script_name,
    ]
    
    script_path = None
    for path in script_paths:
        if path.exists():
            script_path = path
            break
    
    if not script_path:
        return {"error": f"Script '{script_name}' not found"}
    
    return FileResponse(
        path=str(script_path),
        media_type="text/x-shellscript",
        filename=script_name,
        headers={"Content-Disposition": f"attachment; filename={script_name}"}
    )


@api_router.get("/firmware/download/zip")
async def download_firmware_zip():
    """Download all firmware Python files, config and android as ZIP archive"""
    python_dir = FIRMWARE_DIR / 'python'
    config_dir = FIRMWARE_DIR / 'config'
    android_dir = ROOT_DIR.parent / 'android'
    
    if not python_dir.exists():
        return {"error": "Firmware directory not found"}
    
    # Create ZIP in memory
    zip_buffer = io.BytesIO()
    with zipfile.ZipFile(zip_buffer, 'w', zipfile.ZIP_DEFLATED) as zf:
        # Add Python files
        for file_path in python_dir.rglob('*'):
            if file_path.is_file():
                arcname = file_path.relative_to(python_dir)
                zf.write(file_path, arcname)
        
        # Add config files
        if config_dir.exists():
            for file_path in config_dir.rglob('*'):
                if file_path.is_file():
                    arcname = Path('config') / file_path.relative_to(config_dir)
                    zf.write(file_path, arcname)
        
        # Add android files
        if android_dir.exists():
            for file_path in android_dir.rglob('*'):
                if file_path.is_file():
                    arcname = Path('android') / file_path.relative_to(android_dir)
                    zf.write(file_path, arcname)
    
    zip_buffer.seek(0)
    return StreamingResponse(
        zip_buffer,
        media_type="application/zip",
        headers={"Content-Disposition": "attachment; filename=visual_homing_firmware.zip"}
    )


@api_router.get("/firmware/download/script")
async def download_firmware_script():
    """Generate a shell script to download all firmware files"""
    python_dir = FIRMWARE_DIR / 'python'
    if not python_dir.exists():
        return {"error": "Firmware directory not found"}
    
    # Get base URL from environment variable
    backend_url = os.environ.get('BACKEND_URL', os.environ.get('REACT_APP_BACKEND_URL', ''))
    if not backend_url:
        backend_url = 'http://localhost:8001'
    base_url = f"{backend_url}/api"
    
    script_lines = [
        "#!/bin/bash",
        "# Download Visual Homing firmware files",
        "# Run this script in ~/visual_homing directory",
        "",
        "set -e",
        "BASE_URL=\"" + base_url + "\"",
        "DEST_DIR=\"${1:-.}\"",
        "",
        "echo 'Downloading Visual Homing firmware...'",
        "cd \"$DEST_DIR\"",
        "",
    ]
    
    # Collect all files
    for file_path in sorted(python_dir.rglob('*')):
        if file_path.is_file():
            rel_path = file_path.relative_to(python_dir)
            dir_part = rel_path.parent
            
            if str(dir_part) != '.':
                script_lines.append(f"mkdir -p \"{dir_part}\"")
            
            api_path = f"python/{rel_path}"
            script_lines.append(
                f"curl -s \"$BASE_URL/firmware/file/{api_path}\" | python3 -c \"import sys,json; print(json.load(sys.stdin)['content'])\" > \"{rel_path}\""
            )
    
    # Add config files
    config_dir = FIRMWARE_DIR / 'config'
    if config_dir.exists():
        script_lines.append("")
        script_lines.append("# Config files")
        script_lines.append("mkdir -p config")
        for file_path in sorted(config_dir.glob('*')):
            if file_path.is_file():
                rel_path = f"config/{file_path.name}"
                api_path = f"config/{file_path.name}"
                script_lines.append(
                    f"curl -s \"$BASE_URL/firmware/file/{api_path}\" | python3 -c \"import sys,json; print(json.load(sys.stdin)['content'])\" > \"{rel_path}\""
                )
    
    script_lines.extend([
        "",
        "echo ''",
        "echo 'Download complete!'",
        "echo 'Files saved to:' \"$DEST_DIR\"",
        "ls -la",
    ])
    
    script_content = "\n".join(script_lines)
    return PlainTextResponse(content=script_content, media_type="text/x-shellscript")


# Route/Flight data endpoints for 3D visualization
class RoutePoint(BaseModel):
    x: float
    y: float
    z: float
    yaw: float = 0.0
    timestamp: float = 0.0
    is_keyframe: bool = False

class FlightRoute(BaseModel):
    id: str
    name: str
    points: List[RoutePoint]
    keyframes: List[RoutePoint]
    total_distance: float
    created_at: str

class DronePosition(BaseModel):
    x: float
    y: float
    z: float
    yaw: float
    pitch: float = 0.0
    roll: float = 0.0
    speed: float = 0.0
    mode: str = "IDLE"


class SensorStatus(BaseModel):
    optical_flow_connected: bool = False
    optical_flow_quality: int = 0
    flow_x: float = 0.0
    flow_y: float = 0.0
    lidar_connected: bool = False
    lidar_distance_m: float = 0.0
    lidar_signal: int = 0


class SmartRTLStatus(BaseModel):
    active: bool = False
    phase: str = "idle"
    current_altitude: float = 0.0
    home_distance: float = 0.0
    return_progress: float = 0.0
    nav_source: str = "none"
    target_altitude: float = 0.0


# In-memory storage for demo (in real system - from Pi via WebSocket)
_demo_routes = {}
_in_memory_settings = None
_current_position = DronePosition(x=0, y=0, z=5, yaw=0, mode="IDLE")
_sensor_status = SensorStatus()
_smart_rtl_status = SmartRTLStatus()

@api_router.get("/routes")
async def list_routes():
    """List all saved routes"""
    if db is None:
        return list(_demo_routes.values())
    try:
        routes = await db.routes.find({}, {"_id": 0}).sort("created_at", -1).to_list(100)
        return routes
    except Exception as e:
        logging.warning(f"Failed to list routes: {e}")
        return []

@api_router.get("/routes/{route_id}")
async def get_route(route_id: str):
    """Get route by ID"""
    if db is None:
        if route_id in _demo_routes:
            return _demo_routes[route_id]
        return {"error": "Route not found"}
    try:
        route = await db.routes.find_one({"id": route_id}, {"_id": 0})
        if route:
            return route
        return {"error": "Route not found"}
    except Exception as e:
        logging.warning(f"Failed to get route: {e}")
        return {"error": "Database error"}

@api_router.post("/routes")
async def create_route(route: FlightRoute):
    """Save a new route"""
    if db is None:
        _demo_routes[route.id] = route.model_dump()
        return {"success": True, "id": route.id}
    try:
        doc = route.model_dump()
        await db.routes.insert_one(doc)
        return {"success": True, "id": route.id}
    except Exception as e:
        logging.warning(f"Failed to create route: {e}")
        return {"success": False, "error": "Database error"}

@api_router.delete("/routes/{route_id}")
async def delete_route(route_id: str):
    """Delete a route by ID"""
    if db is None:
        if route_id in _demo_routes:
            del _demo_routes[route_id]
            return {"success": True, "message": "Route deleted"}
        return {"error": "Route not found"}
    try:
        result = await db.routes.delete_one({"id": route_id})
        if result.deleted_count > 0:
            return {"success": True, "message": "Route deleted"}
        return {"error": "Route not found"}
    except Exception as e:
        logging.warning(f"Failed to delete route: {e}")
        return {"error": "Database error"}

@api_router.get("/routes/demo/generate")
async def generate_demo_route():
    """Generate a demo route for testing 3D visualization"""
    import math
    import random
    
    # Generate spiral path
    points = []
    keyframes = []
    total_distance = 0.0
    
    num_points = 100
    for i in range(num_points):
        t = i / num_points * 4 * math.pi  # 2 full spirals
        radius = 20 + t * 2  # expanding spiral
        
        x = radius * math.cos(t)
        y = radius * math.sin(t)
        z = 5 + i * 0.2 + random.uniform(-0.5, 0.5)  # gradual climb with noise
        yaw = math.atan2(math.cos(t + 0.1) - math.cos(t), math.sin(t + 0.1) - math.sin(t))
        
        point = RoutePoint(
            x=x, y=y, z=z, yaw=yaw,
            timestamp=i * 0.5,
            is_keyframe=(i % 10 == 0)
        )
        points.append(point)
        
        if point.is_keyframe:
            keyframes.append(point)
        
        if i > 0:
            prev = points[i-1]
            total_distance += math.sqrt((x-prev.x)**2 + (y-prev.y)**2 + (z-prev.z)**2)
    
    route = FlightRoute(
        id="demo_route_001",
        name="Demo Spiral Route",
        points=[p.model_dump() for p in points],
        keyframes=[k.model_dump() for k in keyframes],
        total_distance=total_distance,
        created_at=datetime.now(timezone.utc).isoformat()
    )
    
    return route

@api_router.get("/position")
async def get_drone_position():
    """Get current drone position (simulated)"""
    return _current_position

@api_router.post("/position")
async def update_drone_position(pos: DronePosition):
    """Update drone position (from Pi or simulator)"""
    global _current_position
    _current_position = pos
    return {"success": True}

@api_router.get("/simulation/start/{route_id}")
async def start_simulation(route_id: str):
    """Start route simulation for demo"""
    return {"message": "Simulation would start here", "route_id": route_id}


# Sensor status endpoints
@api_router.get("/sensors/status")
async def get_sensor_status():
    """Get current sensor status"""
    return _sensor_status

@api_router.post("/sensors/status")
async def update_sensor_status(status: SensorStatus):
    """Update sensor status (from Pi)"""
    global _sensor_status
    _sensor_status = status
    return {"success": True}

@api_router.get("/smart-rtl/status")
async def get_smart_rtl_status():
    """Get Smart RTL status"""
    return _smart_rtl_status

@api_router.post("/smart-rtl/status")
async def update_smart_rtl_status(status: SmartRTLStatus):
    """Update Smart RTL status (from Pi)"""
    global _smart_rtl_status
    _smart_rtl_status = status
    return {"success": True}

@api_router.get("/smart-rtl/config")
async def get_smart_rtl_config():
    """Get Smart RTL configuration defaults"""
    return {
        "high_alt_threshold": 50.0,
        "precision_land_alt": 5.0,
        "descent_start_pct": 0.5,
        "descent_rate": 2.0,
        "high_alt_speed": 10.0,
        "low_alt_speed": 3.0,
        "precision_speed": 0.5,
        "flow_min_quality": 50,
        "visual_min_confidence": 0.3
    }


# ===== Return (Smart RTL) Control =====
class ReturnCommand(BaseModel):
    """Command to start/stop return flight"""
    action: str = "start"  # start, stop, pause

@api_router.post("/return/start")
async def start_return():
    """Start Smart RTL - return to home along recorded route"""
    global _smart_rtl_status
    _smart_rtl_status = SmartRTLStatus(
        active=True,
        phase="high_alt",
        current_altitude=_current_position.z,
        home_distance=100.0,  # Will be calculated from actual position
        return_progress=0.0,
        nav_source="imu_baro",
        target_altitude=50.0
    )
    await ws_manager.broadcast({
        "type": "rtl_started",
        "timestamp": datetime.now(timezone.utc).isoformat(),
        "status": _smart_rtl_status.model_dump()
    })
    return {"success": True, "message": "Smart RTL started", "status": _smart_rtl_status.model_dump()}

@api_router.post("/return/stop")
async def stop_return():
    """Stop Smart RTL"""
    global _smart_rtl_status
    _smart_rtl_status = SmartRTLStatus(active=False, phase="idle")
    await ws_manager.broadcast({
        "type": "rtl_stopped",
        "timestamp": datetime.now(timezone.utc).isoformat()
    })
    return {"success": True, "message": "Smart RTL stopped"}

@api_router.get("/return/status")
async def get_return_status():
    """Get current return flight status"""
    return _smart_rtl_status


# ===== Settings CRUD =====
class SystemSettings(BaseModel):
    camera_type: str = "usb_capture"
    camera_device: str = "/dev/video0"
    camera_resolution_w: int = 640
    camera_resolution_h: int = 480
    camera_fps: int = 30
    mavlink_port: str = "/dev/serial0"
    mavlink_baud: int = 115200
    flow_enabled: bool = True
    flow_port: str = "/dev/serial1"
    lidar_enabled: bool = True
    lidar_port: str = "/dev/serial2"
    rtl_high_alt: float = 50.0
    rtl_precision_alt: float = 5.0
    rtl_descent_pct: float = 0.5
    rtl_descent_rate: float = 2.0
    rtl_high_speed: float = 10.0
    rtl_low_speed: float = 3.0
    rtl_precision_speed: float = 0.5
    flow_min_quality: int = 50
    visual_min_confidence: float = 0.3
    web_port: int = 5000
    autostart: bool = True
    stream_enabled: bool = True
    stream_url: str = "http://192.168.213.234:5000/"

@api_router.get("/settings")
async def get_settings():
    """Get system settings from DB or return defaults"""
    if db is None:
        return _in_memory_settings if _in_memory_settings is not None else SystemSettings().model_dump()
    try:
        doc = await db.settings.find_one({"_id": "system"}, {"_id": 0})
        if doc:
            return doc
        return SystemSettings().model_dump()
    except Exception as e:
        logging.warning(f"Failed to get settings: {e}")
        return SystemSettings().model_dump()

@api_router.post("/settings")
async def save_settings(settings: SystemSettings):
    """Save system settings to DB"""
    global _in_memory_settings
    if db is None:
        _in_memory_settings = settings.model_dump()
        return {"success": True}
    try:
        doc = settings.model_dump()
        await db.settings.update_one(
            {"_id": "system"},
            {"$set": doc},
            upsert=True
        )
        return {"success": True}
    except Exception as e:
        logging.warning(f"Failed to save settings: {e}")
        return {"success": False, "error": "Database error"}

@api_router.post("/settings/reset")
async def reset_settings():
    """Reset settings to defaults"""
    global _in_memory_settings
    if db is None:
        _in_memory_settings = None
        return SystemSettings().model_dump()
    try:
        await db.settings.delete_one({"_id": "system"})
        return SystemSettings().model_dump()
    except Exception as e:
        logging.warning(f"Failed to reset settings: {e}")
        return SystemSettings().model_dump()


# ===== Route Export =====
@api_router.get("/routes/{route_id}/export/json")
async def export_route_json(route_id: str):
    """Export route as JSON file"""
    if db is None:
        if route_id in _demo_routes:
            return _demo_routes[route_id]
        return {"error": "Route not found"}
    try:
        route = await db.routes.find_one({"id": route_id}, {"_id": 0})
        if not route:
            return {"error": "Route not found"}
        return route
    except Exception as e:
        logging.warning(f"Failed to export route JSON: {e}")
        return {"error": "Database error"}

@api_router.get("/routes/{route_id}/export/kml")
async def export_route_kml(route_id: str):
    """Export route as KML for Google Earth"""
    route = None
    if db is None:
        if route_id in _demo_routes:
            route = _demo_routes[route_id]
        else:
            return {"error": "Route not found"}
    else:
        try:
            route = await db.routes.find_one({"id": route_id}, {"_id": 0})
        except Exception as e:
            logging.warning(f"Failed to export route KML: {e}")
            return {"error": "Database error"}
        if not route:
            return {"error": "Route not found"}

    name = route.get("name", "Route")
    points = route.get("points", [])

    coords_str = ""
    for p in points:
        lon = 30.5234 + p.get("x", 0) * 0.00001
        lat = 50.4501 + p.get("y", 0) * 0.00001
        alt = p.get("z", 0)
        coords_str += f"          {lon},{lat},{alt}\n"

    kml = f"""<?xml version="1.0" encoding="UTF-8"?>
<kml xmlns="http://www.opengis.net/kml/2.2">
  <Document>
    <name>{name}</name>
    <description>Visual Homing Route Export</description>
    <Style id="routeStyle">
      <LineStyle>
        <color>ff00ffff</color>
        <width>3</width>
      </LineStyle>
    </Style>
    <Placemark>
      <name>{name}</name>
      <styleUrl>#routeStyle</styleUrl>
      <LineString>
        <altitudeMode>relativeToGround</altitudeMode>
        <coordinates>
{coords_str}        </coordinates>
      </LineString>
    </Placemark>
  </Document>
</kml>"""

    return StreamingResponse(
        iter([kml]),
        media_type="application/vnd.google-earth.kml+xml",
        headers={"Content-Disposition": f"attachment; filename={name}.kml"}
    )


# ===== Video Stream =====
@api_router.get("/stream/status")
async def stream_status():
    """Check video stream availability and return configured URL"""
    stream_url = "/api/stream/video"
    stream_enabled = False
    if db is not None:
        try:
            doc = await db.settings.find_one({"_id": "system"}, {"_id": 0})
            if doc:
                stream_url = doc.get("stream_url", stream_url)
                stream_enabled = doc.get("stream_enabled", stream_enabled)
        except Exception as e:
            logging.warning(f"Failed to get stream status from DB: {e}")
    return {
        "available": stream_enabled,
        "url": stream_url,
        "type": "mjpeg",
        "message": "Stream available" if stream_enabled else "Stream not available"
    }


# ===== WebSocket =====
class ConnectionManager:
    def __init__(self):
        self.active_connections: list[WebSocket] = []

    async def connect(self, websocket: WebSocket):
        await websocket.accept()
        self.active_connections.append(websocket)

    def disconnect(self, websocket: WebSocket):
        # B2: safe remove — .remove() raises ValueError if already gone
        if websocket in self.active_connections:
            self.active_connections.remove(websocket)

    async def broadcast(self, data: dict):
        # B1: collect failed connections and remove after iteration
        dead = []
        for conn in self.active_connections:
            try:
                await conn.send_json(data)
            except Exception:
                dead.append(conn)
        for conn in dead:
            self.disconnect(conn)

ws_manager = ConnectionManager()

@app.websocket("/ws/telemetry")
async def websocket_telemetry(websocket: WebSocket):
    """WebSocket for real-time telemetry updates"""
    await ws_manager.connect(websocket)
    try:
        while True:
            # Send current state every 500ms
            data = {
                "type": "telemetry",
                "timestamp": datetime.now(timezone.utc).isoformat(),
                "sensors": _sensor_status.model_dump(),
                "smart_rtl": _smart_rtl_status.model_dump(),
                "position": _current_position.model_dump()
            }
            await websocket.send_json(data)
            
            # Also listen for incoming commands
            try:
                msg = await asyncio.wait_for(websocket.receive_text(), timeout=0.5)
                # B3: bad JSON must not disconnect the client
                try:
                    cmd = json.loads(msg)
                except json.JSONDecodeError:
                    continue
                if cmd.get("type") == "update_sensors":
                    _update_sensor_status(cmd.get("data", {}))
                elif cmd.get("type") == "update_rtl":
                    _update_rtl_status(cmd.get("data", {}))
            except asyncio.TimeoutError:
                pass
    except WebSocketDisconnect:
        ws_manager.disconnect(websocket)
    except Exception:
        ws_manager.disconnect(websocket)


def _update_sensor_status(data):
    # B4: ValidationError from bad Pi data must not crash the WS handler
    global _sensor_status
    try:
        _sensor_status = SensorStatus(**data)
    except Exception as e:
        logging.warning(f"Invalid sensor data from Pi: {e}")

def _update_rtl_status(data):
    global _smart_rtl_status
    try:
        _smart_rtl_status = SmartRTLStatus(**data)
    except Exception as e:
        logging.warning(f"Invalid RTL data from Pi: {e}")


# Status endpoints (original)
@api_router.post("/status", response_model=StatusCheck)
async def create_status_check(input: StatusCheckCreate):
    status_dict = input.model_dump()
    status_obj = StatusCheck(**status_dict)
    
    if db is not None:
        try:
            doc = status_obj.model_dump()
            doc['timestamp'] = doc['timestamp'].isoformat()
            _ = await db.status_checks.insert_one(doc)
        except Exception as e:
            logging.warning(f"Failed to save status check: {e}")
    
    return status_obj

@api_router.get("/status", response_model=List[StatusCheck])
async def get_status_checks():
    if db is None:
        return []
    try:
        status_checks = await db.status_checks.find({}, {"_id": 0}).to_list(1000)
        
        for check in status_checks:
            if isinstance(check['timestamp'], str):
                check['timestamp'] = datetime.fromisoformat(check['timestamp'])
        
        return status_checks
    except Exception as e:
        logging.warning(f"Failed to get status checks: {e}")
        return []

# Include the router in the main app
app.include_router(api_router)

app.add_middleware(
    CORSMiddleware,
    allow_credentials=True,
    allow_origins=os.environ.get('CORS_ORIGINS', '*').split(','),
    allow_methods=["*"],
    allow_headers=["*"],
)

# Configure logging
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(name)s - %(levelname)s - %(message)s'
)
logger = logging.getLogger(__name__)

@app.on_event("shutdown")
async def shutdown_db_client():
    if client is not None:
        client.close()