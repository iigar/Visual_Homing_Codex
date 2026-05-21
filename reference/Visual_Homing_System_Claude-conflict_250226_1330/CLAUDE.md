# CLAUDE.md — Контекст проекту для AI агента

> Прочитай на початку кожної сесії.

---

## Про проект

**Visual Homing** — система оптичної навігації для мультикоптерів ArduPilot.
Працює на Raspberry Pi Zero 2W/4B/5 з камерою.
Забезпечує повернення додому (RTL) без GPS через Visual Odometry (Teach & Repeat).

**Репо:** `https://github.com/iigar/Visual_Homing_System_Claude`
**Користувач:** Ігор (ЗСУ), Ukrainian, Windows для розробки, RPi Zero 2W на дроні.

---

## Структура проекту

```
firmware/
├── python/            # Python реалізація (основна)
│   ├── main.py
│   ├── config.py
│   ├── camera/        # Драйвери камер
│   ├── vision/        # Visual Odometry (ORB + Homography)
│   ├── mavlink/       # MAVLink 2.0 інтерфейс
│   ├── navigation/    # Smart RTL, Route Recorder/Follower
│   ├── sensors/       # Optical Flow (MATEK 3901-L0X), LiDAR (TF-Luna)
│   ├── diagnostics/   # Діагностика MAVLink
│   └── web/           # Flask веб-сервер на Pi (port 5000)
├── cpp/src/           # C++ реалізація (продуктивність)
└── config/
    └── visual_homing.param  # Параметри ArduPilot

backend/               # FastAPI (port 8001) — маршрути, документація, WebSocket
frontend/              # React + Three.js (port 3000) — UI
scripts/               # install.sh, setup_local_interface.sh
docs/                  # Документація (00_QUICK_START.md → 11_long_range.md)
android/               # Android додаток (Kotlin + Compose)
tests/                 # Playwright E2E (port 3000)
memory/PRD.md          # Product Requirements Document
```

---

## КРИТИЧНІ ПРАВИЛА

### URLs та порти
```
Backend API:   http://localhost:8001
Frontend UI:   http://localhost:3000
Pi Web:        http://visual-homing.local:5000
WebSocket:     ws://localhost:8001/ws/telemetry
```

### Тести — НІКОЛИ не хардкодити Emergent URL
```python
# conftest.py — ПРАВИЛЬНО:
BASE_URL = os.environ.get('REACT_APP_BACKEND_URL', 'http://localhost:8001')

# playwright.config.ts — ПРАВИЛЬНО:
baseURL: process.env.PLAYWRIGHT_BASE_URL || 'http://localhost:3000'
```

### Пакетні менеджери
- Frontend: `yarn` (не npm)
- Backend: `pip`

---

## Hardware

| Компонент | Деталь |
|-----------|--------|
| Комп'ютер | Raspberry Pi Zero 2W (4× A53 @ 1GHz, 512MB) |
| FC | Matek H743-Slim V3 + ArduCopter 4.5.7 |
| Камера | Pi Camera v2/v3 або USB EasyCap |
| Thermal | Caddx Thermal 256 (аналогова) |
| Optical Flow | MATEK 3901-L0X (MSP V2 UART, /dev/serial1) |
| LiDAR | Benewake TF-Luna (UART, /dev/serial2) |
| Зв'язок | UART /dev/serial0, MAVLink 2.0, 115200 baud |

---

## Smart RTL фази

```
IDLE → HIGH_ALT (>50m, ArduPilot IMU/Baro)
     → DESCENT (після 50% шляху)
     → LOW_ALT (<50m, Optical Flow + Visual)
     → PRECISION_LAND (<5m, Flow + LiDAR)
     → COMPLETED
```

---

## Запуск

```bash
# Backend
cd backend && pip install -r requirements.txt && uvicorn server:app --port 8001

# Frontend
cd frontend && yarn && yarn start

# Pi firmware
cd firmware && ./scripts/install.sh
python firmware/python/main.py
```
