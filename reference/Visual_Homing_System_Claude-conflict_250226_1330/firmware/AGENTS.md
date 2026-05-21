# AGENTS.md — Налаштування AI агентів для Visual Homing

> Цей файл містить конфігурацію та інструкції для AI агентів, що працюють над проектом.
> Використовується для збереження контексту між сесіями.

---

## 🤖 Основний агент (Claude / GPT / інші)

### Роль
Повноцінний розробник для системи Visual Homing на Raspberry Pi.

### Мова спілкування
**Українська** — завжди відповідай українською мовою.

### Контекстні файли
Перед початком роботи прочитай:
1. `CLAUDE.md` — технічний контекст проекту
2. `README.md` — загальний опис

### Ключові обмеження
```yaml
репо: "https://github.com/iigar/Visual_Homing_System_Claude"

порти:
  pi_web: 5000
  mavlink_uart: /dev/serial0
  
бібліотеки:
  camera_pi: picamera2 (не cv2.VideoCapture!)
  camera_usb: cv2.VideoCapture
  mavlink: pymavlink
  web: flask + flask-socketio
  vision: opencv-python
```

---

## 📝 Робочий процес

### 1. Початок сесії
```
1. Прочитати CLAUDE.md
2. Зрозуміти поточну проблему
3. Запитати користувача про деталі
```

### 2. Перед змінами коду
```
1. Переглянути файл перед редагуванням
2. Зрозуміти існуючу логіку
3. Мінімізувати зміни
```

### 3. Після змін
```
1. Запропонувати команду для оновлення на Pi
2. Перевірити логи: journalctl -u visual-homing -f
3. Протестувати API: curl http://visual-homing.local:5000/api/status
```

---

## 🔑 Специфіка проекту

### Raspberry Pi
```yaml
os: Debian 12/13 (Bookworm/Trixie)
model: Zero 2W / 4B / 5
camera: USB EasyCap або Pi Camera
uart: /dev/serial0 (GPIO 14/15)
service: visual-homing.service
```

### ArduPilot
```yaml
fc: Matek H743-Slim (або інший)
firmware: ArduCopter V4.3+
mavlink_version: 2.0
baudrate: 115200
key_params:
  - VISO_TYPE=1
  - EK3_SRC1_POSXY=6
  - EK3_SRC1_VELXY=6
  - SERIAL3_PROTOCOL=2
  - SERIAL3_BAUD=115
```

### MAVLink повідомлення
```yaml
відправляємо:
  - VISION_POSITION_ESTIMATE (102)
  - VISION_SPEED_ESTIMATE (103)
  
отримуємо:
  - HEARTBEAT (0)
  - ATTITUDE (30)
  - GLOBAL_POSITION_INT (33)
  - LOCAL_POSITION_NED (32)
```

---

## ⚡ Швидкі команди для Pi

### Оновити прошивку
```bash
cd ~/visual-homing && ./update.sh
```

### Перевірити статус
```bash
curl http://visual-homing.local:5000/api/status
```

### Діагностика MAVLink
```bash
python ~/visual_homing/diagnostics/mavlink_diag.py
```

### Логи сервісу
```bash
journalctl -u visual-homing -f
```

### Перезапустити сервіс
```bash
sudo systemctl restart visual-homing
```

---

## 🐛 Відомі проблеми та рішення

### VisOdom: not healthy
```
Причина: FC не отримує дані від Pi
Рішення:
1. Перевірити проводку UART (TX→RX, RX→TX, GND→GND)
2. Перевірити SERIAL3_PROTOCOL=2
3. Перевірити VISO_TYPE=1
4. Запустити mavlink_diag.py
```

### Камера не працює
```
Pi Camera:
  sudo apt install python3-picamera2
  libcamera-hello --list-cameras
  
USB Camera:
  ls /dev/video*
  sudo apt install v4l-utils
  v4l2-ctl --list-devices
```

### Дрейф позиції
```
Причина: Шум камери або вібрації
Рішення: Фільтр дрейфу 5мм (вже в коді)
Альтернатива: Збільшити поріг в config.py
```

### WebSocket не оновлюється
```
Причина: Firewall або мережа
Рішення: Перевірити порт 5000, перезапустити сервіс
```

---

## 📂 Структура файлів

```
visual_homing/
├── python/
│   ├── main.py              # Точка входу
│   ├── config.py            # Конфігурація
│   ├── camera/
│   │   ├── pi_camera.py     # picamera2
│   │   └── usb_capture.py   # OpenCV
│   ├── vision/
│   │   └── visual_odometry.py  # ORB + optical flow
│   ├── mavlink/
│   │   └── ardupilot.py     # MAVLink протокол
│   ├── navigation/
│   │   ├── smart_rtl.py     # Алгоритм повернення
│   │   └── route_recorder.py
│   ├── web/
│   │   ├── server.py        # Flask сервер
│   │   └── templates/
│   └── diagnostics/
│       └── mavlink_diag.py
├── config/
│   └── visual_homing.param
├── CLAUDE.md
└── AGENTS.md
```

---

## 🎯 Пріоритети при розробці

1. **Надійність** — система має працювати стабільно
2. **Безпека** — краще не летіти, ніж втратити дрон
3. **Простота** — мінімум залежностей
4. **Документація** — код має бути зрозумілим

---

## 📞 Контекст користувача

- **Мова:** Українська
- **Рівень:** Технічний (може читати код, працює з Linux)
- **Середовище:** Windows для розробки, Raspberry Pi для польотів
- **Основна ціль:** Надійна система RTL без GPS для FPV дронів

---

*Останнє оновлення: 03.03.2026*
