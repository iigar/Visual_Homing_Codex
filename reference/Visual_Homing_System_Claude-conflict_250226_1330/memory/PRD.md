# Visual Homing System - PRD

## Оновлено: 01.03.2026

## Опис
Прошивка для Raspberry Pi (Zero 2 W, Pi 4B, Pi 5) для оптичної навігації дрона на базі ArduPilot.

## Підтримуване обладнання
- **Raspberry Pi:** Zero 2 W (512MB), Pi 4B (2-8GB), Pi 5 (4-8GB)
- **Політний контролер:** Matek H743-Slim V3
- **Сенсори:** MATEK 3901-L0X (Optical Flow), TF-Luna LiDAR
- **Камери:** Caddx Thermal 256, Pi Camera v2/v3, аналогова через EasyCap

---

## Реліз-ноти

### v2.2.3 (02.03.2026) — URL Consistency Fix
- **URL Fix:** Виправлено всі посилання на Emergent — замінено на GitHub репо
- **Affected files:** `docs/01_raspberry_pi_setup.md`, `docs/README.md`, `docs/00_QUICK_START.md`, `memory/PRD.md`, `scripts/setup_local_interface.sh`
- **API Verified:** Ендпоінти `/api/scripts/download/install.sh` та `/api/firmware/download/zip` працюють коректно

### v2.2.2 (01.03.2026) — MAVLink & API Fixes
- **MAVLink Fix:** Виправлено помилку `TypeError` у функціях `vision_position_estimate_send` та `vision_speed_estimate_send` - видалено зайві аргументи (covariance, reset_counter) для сумісності з pymavlink
- **API Fix:** Додано відсутні ендпоінти `/api/return/start`, `/api/return/stop`, `/api/return/status` для керування Smart RTL через веб-інтерфейс
- **WebSocket:** RTL події тепер транслюються через WebSocket (`rtl_started`, `rtl_stopped`)

### v2.2.1 (28.02.2026) — Multi-Pi Support
- **Multi-Pi Support:** Інсталятор тепер підтримує Raspberry Pi Zero 2 W, Pi 4B та Pi 5
- Автоматичне визначення моделі Pi під час інсталяції
- Специфічні налаштування UART для кожної моделі (Pi 5 використовує uart2-pi5, uart3-pi5)
- Оптимізація swap та gpu_mem для кожної моделі
- Оновлена документація з таблицями порівняння продуктивності

### v2.2 (28.02.2026) — Full Feature Release
- **Settings page**: конфігурація камери, MAVLink, сенсорів, Smart RTL через UI
- **Video stream**: плейсхолдер в Телеметрії (MJPEG на Pi)
- **WebSocket**: `/ws/telemetry` для real-time оновлень
- **Route export**: JSON + KML (Google Earth) з кнопками в Історії
- **Mobile responsive**: компактні таби, scrollable nav, touch targets
- **SITL документація**: повний гайд по тестуванню в симуляторі
- Тести: 100% (29 backend, 32 frontend)

### v2.1.1 — Bug Fix + C++ + Installer
- Fix: Smart RTL null crash, колір дрона #D3D3D3
- C++ v2.1 з сенсорами, Unified Installer

### v2.1 — Smart RTL Simulation
- Симуляція фаз, повзунок швидкості, HUD overlay

### v2.0 — Sensors + Telemetry
- MATEK 3901-L0X, TF-Luna, Smart RTL, телеметрія

### v1.0 — Foundation
- FastAPI + React + MongoDB, 3D карта, документація

---

## Тестування
- Backend: **29/29 (100%)**
- Frontend: **32/32 (100%)**
- Iterations: 10

## Backlog (все реалізовано)
- [x] Sensors (MATEK 3901-L0X, TF-Luna)
- [x] Smart RTL logic + simulation
- [x] Telemetry dashboard
- [x] Video stream integration
- [x] Settings page
- [x] WebSocket real-time
- [x] Route export (JSON/KML)
- [x] Mobile responsive
- [x] SITL documentation
- [x] C++ update
- [x] Unified installer
- [x] Multi-Pi Support (Zero 2 W, Pi 4B, Pi 5)

## Pending Tasks
- [ ] Pre-flight Checklist UI
- [ ] WebSocket real-time telemetry updates (infrastructure ready)
- [ ] Extended analytics dashboard
- [x] **Android App Concept** — концепція та базовий код створено

## Виправлені баги (v2.2.2+)
- [x] MAVLink TypeError: `vision_position_estimate_send() takes from 8 to 9 positional arguments but 10 were given`
- [x] HTTP 415 error on `/api/return/start` - ендпоінт не існував
- [x] MongoDB `load_dotenv(override=False)` для production Atlas
- [x] Health endpoint `/health` для Kubernetes

## Android App (NEW)
- **Документація:** `/app/docs/10_android_app.md`
- **Код:** `/app/android/`
- **Функції:**
  - Mobile Hotspot керування
  - Native UI з Jetpack Compose
  - 3D карта маршруту
  - Real-time телеметрія
  - mDNS discovery для Pi
- **Tech Stack:** Kotlin, Jetpack Compose, Material 3, Retrofit, Room

## Preview URLs (актуальні)
- **Web UI:** https://github.com/iigar/Visual_Homing_System_Claude
- **API:** https://github.com/iigar/Visual_Homing_System_Claude/api
- **Firmware Download:** https://github.com/iigar/Visual_Homing_System_Claude/api/firmware/download/zip
- **Install Script:** https://github.com/iigar/Visual_Homing_System_Claude/api/scripts/download/install.sh

## Команди запуску на Pi
```bash
cd ~/visual_homing
source ~/venv/bin/activate

# Pi Camera (рекомендовано):
python main.py --web --camera picamera

# USB камера:
python main.py --web --camera usb

# Тестовий режим:
python main.py --web --camera picamera --test-mode
```
