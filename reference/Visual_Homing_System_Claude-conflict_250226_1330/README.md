# Visual Homing System

Autonomous drone navigation system using **Teach & Repeat** visual homing — no GPS required.

The drone records visual landmarks during flight and returns along the same path using computer vision.

## How It Works

```
RECORD:  Fly manually → system saves keyframes (position + ORB features) every 2m
RETURN:  Trigger RTL → system matches current view to keyframes → follows path in reverse
```

## Architecture

```
┌──────────────┐   MAVLink/UART   ┌────────────────────┐
│  ArduPilot   │◄────────────────►│  Raspberry Pi       │
│  Matek H743  │  VISION_POSITION │  Zero 2W            │
│  ArduCopter  │  ESTIMATE        │                     │
│    4.5.7     │                  │  ┌───────────────┐  │
└──────────────┘                  │  │ Camera Module │  │
                                  │  │ Pi Cam / USB  │  │
                                  │  └───────┬───────┘  │
                                  │          │           │
                                  │  ┌───────▼───────┐  │
                                  │  │Visual Odometry│  │
                                  │  │ ORB + Homog.  │  │
                                  │  └───────┬───────┘  │
                                  │          │           │
                                  │  ┌───────▼───────┐  │
                                  │  │Route Recorder │  │
                                  │  │  / Follower   │  │
                                  │  └───────────────┘  │
                                  └──────────┬──────────┘
                                             │ HTTP/WebSocket
                                  ┌──────────▼──────────┐
                                  │    Web Interface     │
                                  │  React + Three.js    │
                                  │    (port 3000)       │
                                  └─────────────────────┘
```

## Repository Structure

```
firmware/
├── python/                 # Python implementation (primary)
│   ├── main.py
│   ├── config.py
│   ├── camera/             # Pi Camera + USB capture drivers
│   ├── vision/             # ORB detector, BF matcher, VO
│   ├── mavlink/            # MAVLink 2.0 interface
│   ├── navigation/         # Route recorder + follower
│   └── web/                # Local Flask server
├── cpp/src/                # C++ implementation (performance)
│   ├── camera.cpp/hpp
│   ├── feature_tracker.cpp/hpp
│   ├── visual_odometry.cpp/hpp
│   ├── route_memory.cpp/hpp
│   └── mavlink_interface.cpp/hpp
└── scripts/                # build.sh, install.sh, autostart.sh

backend/                    # FastAPI documentation/routes API (port 8001)
├── server.py               # Routes CRUD + doc viewer
├── requirements.txt
└── tests/

frontend/                   # React + Three.js dashboard (port 3000)
├── src/
│   ├── App.js              # Main app, route history, 3D map
│   └── components/
│       ├── SimpleMap3D.js  # Three.js 3D route visualization
│       └── RouteMap3D.js

docs/                       # 8 documentation files
tests/                      # Playwright E2E + pytest backend
```

## Hardware

| Component | Spec |
|-----------|------|
| Computer | Raspberry Pi Zero 2W (4× Cortex-A53 @ 1GHz, 512MB) |
| Camera | Pi Camera Module or USB camera |
| Thermal | Caddx Thermal 256 (analog capture) |
| Flight controller | Matek H743 with ArduCopter 4.5.7 |
| Connection | UART (MAVLink 2.0, 115200 baud) |

## Quick Start

```bash
# Install dependencies on Raspberry Pi
cd firmware && ./scripts/install_dependencies.sh

# Run Python version
cd firmware/python && python3 main.py

# Setup autostart
./scripts/setup_autostart.sh

# Build C++ version
./scripts/build_cpp.sh
```

## ArduPilot Parameters

```ini
VISO_TYPE = 1          # MAVLink visual odometry
EK3_SRC1_POSXY = 6    # ExternalNav for position XY
EK3_SRC1_VELXY = 6    # ExternalNav for velocity XY
EK3_SRC1_POSZ = 1     # Barometer for altitude
SERIAL3_PROTOCOL = 2   # MAVLink2 on UART3
SERIAL3_BAUD = 115     # 115200 baud
```

Full config: [`firmware/config/visual_homing.param`](firmware/config/visual_homing.param)

## API Endpoints (port 8001)

| Method | Endpoint | Description |
|--------|----------|-------------|
| GET | `/api/routes` | List saved routes |
| POST | `/api/routes` | Save route |
| DELETE | `/api/routes/{id}` | Delete route |
| GET | `/api/routes/demo/generate` | Generate demo route (mock) |
| GET | `/api/docs/list` | List documentation files |
| GET | `/api/docs/{filename}` | Get document content |
| GET | `/api/firmware/structure` | Firmware file tree |

## Testing

```bash
# Backend tests (runs against localhost:8001)
cd backend && pytest tests/

# E2E tests (runs against localhost:3000)
cd tests && npx playwright test
```

## Roadmap

- [ ] WebSocket real-time drone position
- [ ] Live camera stream (MJPEG)
- [ ] Android companion app ([spec](android/ANDROID_APP_SPEC.md))
- [ ] SITL testing
- [ ] Route export (JSON/KML)
- [ ] Settings page

## Related

- [JT-Zero](https://github.com/iigar) — real-time Visual Odometry for EKF3 stabilization (sister project)
