# 🚁 Visual Homing — Повний посібник

> **Версія:** 2.3.0  
> **Дата:** Березень 2026  
> **Для:** Raspberry Pi Zero 2W / 4B / 5 + ArduPilot

---

## 📋 Зміст

1. [Що це таке?](#-що-це-таке)
2. [Що потрібно?](#-що-потрібно)
3. [Крок 1: Підготовка SD карти](#-крок-1-підготовка-sd-карти)
4. [Крок 2: Перше підключення](#-крок-2-перше-підключення)
5. [Крок 3: Налаштування Pi](#-крок-3-налаштування-pi)
6. [Крок 4: Встановлення прошивки](#-крок-4-встановлення-прошивки)
7. [Крок 5: Підключення до ArduPilot](#-крок-5-підключення-до-ardupilot)
8. [Крок 6: Налаштування ArduPilot](#-крок-6-налаштування-ardupilot)
9. [Крок 7: Перший запуск](#-крок-7-перший-запуск)
10. [Крок 8: Автозапуск](#-крок-8-автозапуск)
11. [Усунення проблем](#-усунення-проблем)

---

## 🎯 Що це таке?

**Visual Homing** — система візуальної навігації для дронів без GPS.

### Як працює:
```
📹 Камера → 🧠 Pi обробляє зображення → 📡 Надсилає позицію → ✈️ ArduPilot керує дроном
```

### Можливості:
- ✅ Запис маршруту польоту
- ✅ Автоматичне повернення по записаному маршруту
- ✅ Робота без GPS (в приміщеннях, під мостами, тощо)
- ✅ Веб-інтерфейс для моніторингу

---

## 🛒 Що потрібно?

### Обов'язково:

| Компонент | Модель | Ціна* | Де купити |
|-----------|--------|-------|-----------|
| Комп'ютер | Raspberry Pi Zero 2W | ~$20 | raspberrypi.com |
| Камера | Pi Camera v2 | ~$25 | raspberrypi.com |
| SD карта | 16GB+ Class 10 | ~$10 | Будь-який магазин |
| Кабель живлення | USB Micro-B | ~$5 | Будь-який магазин |
| Польотний контролер | З ArduPilot | ~$50+ | ardupilot.org/copter |

### Опціонально (для кращої точності):

| Компонент | Модель | Для чого |
|-----------|--------|----------|
| Optical Flow | MATEK 3901-L0X | Точніше позиціонування |
| LiDAR | TF-Luna | Точна висота |

*Ціни орієнтовні на 2026 рік

---

## 💿 Крок 1: Підготовка SD карти

### Що робимо:
Записуємо операційну систему на SD карту.

### Інструкція:

**1.1** Завантажте **Raspberry Pi Imager**:
- Windows/Mac/Linux: https://www.raspberrypi.com/software/

**1.2** Вставте SD карту в комп'ютер

**1.3** Запустіть Raspberry Pi Imager:

```
┌─────────────────────────────────────────────┐
│  Raspberry Pi Imager                        │
├─────────────────────────────────────────────┤
│                                             │
│  [Choose Device]  →  Raspberry Pi Zero 2W   │
│                                             │
│  [Choose OS]      →  Raspberry Pi OS Lite   │
│                       (64-bit)              │
│                                             │
│  [Choose Storage] →  Ваша SD карта          │
│                                             │
└─────────────────────────────────────────────┘
```

**1.4** Натисніть ⚙️ (шестерінка) для налаштувань:

```
┌─────────────────────────────────────────────┐
│  OS Customisation                           │
├─────────────────────────────────────────────┤
│                                             │
│  ☑ Set hostname:   visual-homing            │
│                                             │
│  ☑ Enable SSH:     ● Use password           │
│                                             │
│  ☑ Set username:   pi                       │
│    Set password:   ваш_пароль               │
│                                             │
│  ☑ Configure WiFi:                          │
│    SSID:           назва_вашої_мережі       │
│    Password:       пароль_wifi              │
│    Country:        UA                       │
│                                             │
│  ☑ Set locale:                              │
│    Timezone:       Europe/Kyiv              │
│    Keyboard:       us                       │
│                                             │
└─────────────────────────────────────────────┘
```

**1.5** Натисніть **Write** та дочекайтесь завершення (~5 хвилин)

**1.6** Вийміть SD карту та вставте в Raspberry Pi

---

## 🔌 Крок 2: Перше підключення

### Що робимо:
Підключаємось до Pi через SSH.

### Інструкція:

**2.1** Увімкніть Pi (підключіть живлення)

**2.2** Зачекайте **2 хвилини** (перше завантаження довше)

**2.3** На комп'ютері відкрийте термінал:
- **Windows**: Натисніть `Win + R`, введіть `cmd`, Enter
- **Mac/Linux**: Відкрийте Terminal

**2.4** Підключіться по SSH:

```bash
ssh pi@visual-homing.local
```

> 💡 **Якщо не працює**, спробуйте IP адресу:
> - Подивіться в налаштуваннях роутера
> - Або підключіть монітор до Pi

**2.5** Введіть пароль (той що вказали в Imager)

**2.6** Ви побачите:
```
pi@visual-homing:~ $
```

✅ **Готово!** Ви підключені до Pi.

---

## ⚙️ Крок 3: Налаштування Pi

### Що робимо:
Налаштовуємо UART для зв'язку з ArduPilot та камеру.

### Інструкція:

**3.1** Відкрийте raspi-config:

```bash
sudo raspi-config
```

**3.2** Увімкніть камеру:
```
Interface Options → Camera → Yes
```

**3.3** Увімкніть UART:
```
Interface Options → Serial Port
  → Login shell: NO
  → Hardware enable: YES
```

**3.4** Збережіть та вийдіть

**3.5** Відредагуйте config.txt:

```bash
sudo nano /boot/firmware/config.txt
```

**3.6** Додайте в кінець файлу:

```ini
# Visual Homing Settings
enable_uart=1
dtoverlay=disable-bt
gpu_mem=128
start_x=1
```

**3.7** Збережіть: `Ctrl+O`, `Enter`, `Ctrl+X`

**3.8** Перезавантажте Pi:

```bash
sudo reboot
```

**3.9** Підключіться знову через 1 хвилину:

```bash
ssh pi@visual-homing.local
```

---

## 📦 Крок 4: Встановлення прошивки

### Що робимо:
Встановлюємо Python залежності та завантажуємо код.

### Інструкція:

**4.1** Встановіть системні пакети:

```bash
sudo apt update && sudo apt install -y \
  python3-pip python3-venv python3-opencv \
  python3-picamera2 git
```

**4.2** Створіть віртуальне середовище:

```bash
python3 -m venv ~/venv --system-site-packages
```

**4.3** Активуйте віртуальне середовище:

```bash
source ~/venv/bin/activate
```

> 💡 Ви побачите `(venv)` на початку рядка

**4.4** Встановіть Python пакети:

```bash
pip install pymavlink pyserial flask flask-cors flask-socketio
```

**4.5** Завантажте прошивку:

```bash
cd ~
git clone https://github.com/iigar/Visual_Homing_System_Claude.git ~/visual-homing && cd ~/visual-homing && ./setup.sh
unzip -o firmware.zip -d visual_homing
```

**4.6** Перевірте що все на місці:

```bash
ls ~/visual_homing/
```

Має показати:
```
camera/  config.py  diagnostics/  main.py  mavlink/  navigation/  sensors/  vision/  web/
```

✅ **Готово!** Прошивка встановлена.

---

## 🔗 Крок 5: Підключення до ArduPilot

### Що робимо:
Фізично з'єднуємо Pi з польотним контролером.

### Схема підключення:

```
┌─────────────────────┐         ┌─────────────────────┐
│   Raspberry Pi      │         │  Flight Controller  │
│                     │         │  (TELEM2 порт)      │
│   GPIO14 (TX) ●─────┼────────►│● RX                 │
│   GPIO15 (RX) ●◄────┼─────────│● TX                 │
│   GND         ●─────┼─────────│● GND                │
│                     │         │                     │
└─────────────────────┘         └─────────────────────┘
```

### Розташування GPIO на Pi:

```
          ┌─────────────────────────────┐
          │  ●  ●  ●  ●  ●  ●  ●  ●  ● │ ← GPIO
          │  1  3  5  7  9  11 13 15 17│
          │                            │
Pi Zero   │  ●  ●  ●  ●  ●  ●  ●  ●  ● │
2W        │  2  4  6  8  10 12 14 16 18│
          │                            │
          │        [USB]   [HDMI]      │
          └────────────────────────────┘

Pin 8  = GPIO14 (TX) → підключити до RX контролера
Pin 10 = GPIO15 (RX) → підключити до TX контролера  
Pin 6  = GND         → підключити до GND контролера
```

> ⚠️ **ВАЖЛИВО:** TX Pi → RX контролера, RX Pi → TX контролера (перехресне з'єднання!)

---

## 🎛️ Крок 6: Налаштування ArduPilot

### Що робимо:
Налаштовуємо ArduPilot для прийому даних Visual Odometry.

### Інструкція:

**6.1** Підключіть дрон до комп'ютера через USB

**6.2** Відкрийте **Mission Planner** → Connect

**6.3** Перейдіть: **Config → Full Parameter List**

**6.4** Знайдіть та встановіть параметри:

```
# ===== SERIAL порт для Pi (якщо TELEM2) =====
SERIAL2_PROTOCOL = 2        # MAVLink2
SERIAL2_BAUD = 115          # 115200 бод

# ===== Visual Odometry =====
VISO_TYPE = 1               # MAVLink
VISO_ORIENT = 0             # Forward
VISO_DELAY_MS = 50          # Затримка 50мс

# ===== EKF3 джерела навігації =====
EK3_SRC1_POSXY = 6          # ExternalNav
EK3_SRC1_VELXY = 6          # ExternalNav  
EK3_SRC1_POSZ = 1           # Barometer
EK3_SRC1_YAW = 1            # Compass
```

> 💡 Якщо Pi підключений до іншого порту (UART7 = SERIAL7), змініть SERIAL2 на SERIAL7

**6.5** Натисніть **Write Params**

**6.6** **Перезавантажте** польотний контролер:
- Від'єднайте USB
- Зачекайте 5 секунд
- Підключіть знову

✅ **Готово!** ArduPilot налаштований.

---

## 🚀 Крок 7: Перший запуск

### Що робимо:
Запускаємо Visual Homing та перевіряємо роботу.

### Інструкція:

**7.1** Підключіться до Pi:

```bash
ssh pi@visual-homing.local
```

**7.2** Активуйте віртуальне середовище:

```bash
source ~/venv/bin/activate
```

**7.3** Перейдіть в папку проекту:

```bash
cd ~/visual_homing
```

**7.4** Запустіть систему:

```bash
python main.py --web --camera picamera
```

**7.5** Ви побачите:

```
2026-03-02 12:00:00 - visual_homing - INFO - Camera initialized: CameraType.PI_CAMERA
2026-03-02 12:00:00 - visual_homing - INFO - Visual odometry initialized
2026-03-02 12:00:00 - visual_homing - INFO - MAVLink interface initialized
2026-03-02 12:00:00 - web.server - INFO - Web server started on http://0.0.0.0:5000
2026-03-02 12:00:00 - visual_homing - INFO - Starting Visual Homing System...
```

**7.6** Відкрийте в браузері (на телефоні або комп'ютері):

```
http://visual-homing.local:5000
```

**7.7** Ви побачите інтерфейс:

```
┌─────────────────────────────────────────────────┐
│  Visual Homing                           IDLE   │
├─────────────────────────────────────────────────┤
│  ┌─────────────────┐  ┌─────────────────────┐  │
│  │                 │  │ ↩ ПОВЕРНЕННЯ        │  │
│  │   📹 Камера     │  ├─────────────────────┤  │
│  │   (live)        │  │ [● ЗАПИС] [■ СТОП]  │  │
│  │                 │  ├─────────────────────┤  │
│  └─────────────────┘  │ Position X: 0.00    │  │
│  ┌─────────────────┐  │ Position Y: 0.00    │  │
│  │   🗺️ 3D Карта   │  │ Position Z: 0.00    │  │
│  │                 │  │ Roll: 0.0°          │  │
│  │                 │  │ Pitch: 0.0°         │  │
│  └─────────────────┘  │ Yaw: 0.0°           │  │
│                       │ MAVLink: OK ✓       │  │
└───────────────────────┴─────────────────────────┘
```

✅ **Готово!** Система працює.

---

## 🔄 Крок 8: Автозапуск

### Що робимо:
Налаштовуємо автоматичний запуск при включенні Pi.

### Інструкція:

**8.1** Створіть файл сервісу:

```bash
sudo nano /etc/systemd/system/visual-homing.service
```

**8.2** Вставте цей вміст:

```ini
[Unit]
Description=Visual Homing Navigation System
After=network.target

[Service]
Type=simple
User=pi
WorkingDirectory=/home/pi/visual_homing
Environment=PATH=/home/pi/venv/bin:/usr/bin:/bin
ExecStartPre=/bin/sleep 10
ExecStart=/home/pi/venv/bin/python main.py --web --camera picamera
Restart=on-failure
RestartSec=5

[Install]
WantedBy=multi-user.target
```

**8.3** Збережіть: `Ctrl+O`, `Enter`, `Ctrl+X`

**8.4** Активуйте автозапуск:

```bash
sudo systemctl daemon-reload
sudo systemctl enable visual-homing
sudo systemctl start visual-homing
```

**8.5** Перевірте статус:

```bash
sudo systemctl status visual-homing
```

Має показати: `Active: active (running)`

**8.6** Тепер при кожному включенні Pi система запуститься автоматично!

### Корисні команди:

| Дія | Команда |
|-----|---------|
| Статус | `sudo systemctl status visual-homing` |
| Перезапустити | `sudo systemctl restart visual-homing` |
| Зупинити | `sudo systemctl stop visual-homing` |
| Логи | `journalctl -u visual-homing -f` |

---

## 🔧 Усунення проблем

### Проблема: "HEARTBEAT не отримано"

**Причина:** Pi не бачить польотний контролер

**Рішення:**
1. Перевірте підключення проводів (TX↔RX можуть бути переплутані)
2. Перевірте параметр `SERIAL2_PROTOCOL = 2` в ArduPilot
3. Перевірте `SERIAL2_BAUD = 115`

Запустіть діагностику:
```bash
cd ~/visual_homing
source ~/venv/bin/activate
python diagnostics/mavlink_diag.py
```

---

### Проблема: "VisOdom: not healthy" в ArduPilot

**Причина:** ArduPilot не отримує дані від Pi

**Рішення:**
1. Переконайтесь що Visual Homing запущений
2. Перевірте параметри:
   - `VISO_TYPE = 1`
   - `EK3_SRC1_POSXY = 6`
   - `EK3_SRC1_VELXY = 6`
3. Перезавантажте польотний контролер

---

### Проблема: "Camera not found"

**Причина:** Камера не підключена або не налаштована

**Рішення:**
```bash
# Перевірте камеру
rpicam-hello --timeout 2000

# Якщо помилка, перевірте:
# 1. Шлейф камери підключений правильно
# 2. В /boot/firmware/config.txt є: gpu_mem=128
```

---

### Проблема: "Port 5000 is in use"

**Причина:** Попередній процес ще працює

**Рішення:**
```bash
sudo fuser -k 5000/tcp
```

---

## 📞 Підтримка

- **Документація:** https://github.com/iigar/Visual_Homing_System_Claude

---

© 2026 Visual Homing Project
