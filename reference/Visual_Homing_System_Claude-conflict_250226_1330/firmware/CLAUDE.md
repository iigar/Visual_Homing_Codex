# CLAUDE.md — Контекст проекту для AI агента

> Цей файл містить критичну інформацію про проект Visual Homing.
> Прочитай його на початку кожної сесії для розуміння контексту.

---

## 🎯 Про проект

**Visual Homing** — система оптичної навігації для мультикоптерів ArduPilot.
Працює на Raspberry Pi Zero 2W/4B/5 з камерою.
Забезпечує повернення додому (RTL) без GPS через Visual Odometry.

**Користувач:** Українськомовний, використовує Windows для розробки, Raspberry Pi Zero 2W для польотів.

---

## 📁 Структура проекту

```
visual_homing/
├── python/                 # Python версія (основна)
│   ├── main.py             # Точка входу
│   ├── config.py           # Конфігурація
│   ├── camera/             # Драйвери камер
│   │   ├── pi_camera.py    # Pi Camera через picamera2
│   │   └── usb_capture.py  # USB камера через OpenCV
│   ├── vision/             # Visual Odometry
│   │   └── visual_odometry.py
│   ├── mavlink/            # MAVLink інтерфейс
│   │   └── ardupilot.py
│   ├── navigation/         # Smart RTL, запис маршрутів
│   │   ├── smart_rtl.py
│   │   └── route_recorder.py
│   ├── web/                # Flask веб-сервер для Pi
│   │   ├── server.py
│   │   └── templates/
│   └── diagnostics/        # Діагностичні скрипти
│       └── mavlink_diag.py
├── cpp/                    # C++ версія (альтернативна)
├── config/
│   └── visual_homing.param # Параметри ArduPilot
├── scripts/
│   └── install_dependencies.sh
├── CLAUDE.md               # Цей файл
└── AGENTS.md               # Налаштування AI агентів
```

---

## ⚠️ КРИТИЧНІ ПРАВИЛА

### Репозиторій
```
https://github.com/iigar/Visual_Homing_System_Claude
```

### Raspberry Pi
- **Веб-інтерфейс Pi:** `http://visual-homing.local:5000`
- **UART порт:** `/dev/serial0` (GPIO 14/15)
- **Камера:** USB EasyCap або Pi Camera (picamera2)
- **Systemd сервіс:** `visual-homing.service`

### ArduPilot
- **FC:** Matek H743-Slim
- **Версія:** ArduCopter V4.3.6
- **UART:** TX3/RX3 (SERIAL3)
- **Ключові параметри:**
  - `VISO_TYPE=1`
  - `SERIAL3_PROTOCOL=2`
  - `SERIAL3_BAUD=115`
  - `EK3_SRC1_POSXY=6`
  - `EK3_SRC1_VELXY=6`

### MAVLink
- **Повідомлення:** `VISION_POSITION_ESTIMATE` (msg_id=102)
- **Baudrate:** 115200
- **Попередження:** `msg_id 271` — ігнорувати (різниця MAVLink v1/v2)

---

## 🔧 Часті проблеми та рішення

### 1. VisOdom: not healthy
```
Причина: FC не отримує VISION_POSITION_ESTIMATE
Рішення:
1. Перевірити SERIAL3_PROTOCOL=2
2. Перевірити VISO_TYPE=1
3. Перевірити EK3_SRC1_POSXY=6
4. Перезавантажити FC
```

### 2. Дрейф X/Y при нерухомості
```
Причина: Шум камери
Рішення: Фільтр дрейфу 5мм в visual_odometry.py (вже реалізовано)
```

### 3. Roll/Pitch/Yaw = 0 на Pi UI
```
Причина: FC не надсилає ATTITUDE
Рішення: _request_data_streams() в ardupilot.py
```

### 4. Камера не працює
```
Pi Camera: Перевірити libcamera-hello, встановити picamera2
USB Camera: Перевірити ls /dev/video*, встановити v4l-utils
```

---

## 📡 API Endpoints (Pi Web Server)

```
GET  /                      # HTML інтерфейс
GET  /video_feed            # MJPEG стрім
POST /api/recording/start   # Почати запис маршруту
POST /api/stop              # Зупинити запис/повернення
POST /api/return/start      # Почати повернення
GET  /api/status            # JSON статус системи
WebSocket /socket.io        # Real-time телеметрія
```

---

## 🧪 Команди для тестування

### Оновити прошивку
```bash
cd ~/visual-homing && ./update.sh
```

### Перевірити MAVLink з'єднання
```bash
python ~/visual_homing/diagnostics/mavlink_diag.py
```

### Перевірити статус
```bash
curl http://visual-homing.local:5000/api/status
```

### Логи сервісу
```bash
journalctl -u visual-homing -f
```

---

## 📦 Ключові файли

| Файл | Опис |
|------|------|
| `python/main.py` | Головна логіка, state machine |
| `python/mavlink/ardupilot.py` | MAVLink комунікація з FC |
| `python/vision/visual_odometry.py` | VO алгоритм з фільтром дрейфу |
| `python/web/server.py` | Flask веб-сервер |
| `python/camera/pi_camera.py` | Драйвер Pi Camera |
| `python/camera/usb_capture.py` | Драйвер USB камери |
| `config/visual_homing.param` | Параметри ArduPilot |

---

## 🚫 НЕ РОБИ

1. **НЕ змінюй** порт веб-сервера (5000)
2. **НЕ видаляй** фільтр дрейфу в visual_odometry.py
3. **НЕ використовуй** cv2.VideoCapture для Pi Camera (тільки picamera2)
4. **НЕ ігноруй** помилки MAVLink heartbeat

---

## ✅ РОБИ

1. **Завжди** перевіряй MAVLink з'єднання перед польотом
2. **Завжди** калібруй камеру при зміні об'єктива
3. **Завжди** перезапускай FC після зміни параметрів
4. **Завжди** тестуй на землі перед польотом

---

## 📊 Поточний статус системи

- ✅ Visual Odometry працює
- ✅ MAVLink підключення працює
- ✅ EKF3 використовує ExternalNav
- ✅ Веб-інтерфейс на Pi працює
- ✅ Відеострім працює
- ✅ Телеметрія (Roll/Pitch/Yaw) працює

---

## 🔗 Корисні посилання

- **Репозиторій:** https://github.com/iigar/Visual_Homing_System_Claude
- **Pi Interface:** http://visual-homing.local:5000
- **ArduPilot Wiki:** https://ardupilot.org/copter/docs/common-external-position-estimation.html

---

*Останнє оновлення: 03.03.2026*
