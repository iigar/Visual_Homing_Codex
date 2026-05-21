# Тестування та калібрування

## 1. Тест компонентів

### 1.1 Тест камери (USB Capture)

```bash
# Перевірити підключення
lsusb | grep -i video
v4l2-ctl --list-devices

# Тест захоплення
ffmpeg -f v4l2 -i /dev/video0 -frames:v 1 test.jpg

# Python тест
python3 -c "
import cv2
cap = cv2.VideoCapture(0)
ret, frame = cap.read()
print(f'Capture: {ret}, Shape: {frame.shape if ret else None}')
cap.release()
"
```

### 1.2 Тест MAVLink

```bash
# Перевірити UART
ls -la /dev/serial0

# Тест з'єднання
python3 -c "
from pymavlink import mavutil
import time

try:
    conn = mavutil.mavlink_connection('/dev/serial0', baud=115200)
    print('Waiting for heartbeat...')
    conn.wait_heartbeat(timeout=10)
    print(f'Connected to system {conn.target_system}, component {conn.target_component}')
except Exception as e:
    print(f'Error: {e}')
"
```

### 1.3 Тест MATEK 3901-L0X (Optical Flow)

```bash
cd ~/visual_homing
python3 -c "
import serial
import struct
import time

ser = serial.Serial('/dev/serial1', 115200, timeout=2)
time.sleep(1)

print('Reading MATEK 3901-L0X data...')
for i in range(5):
    data = ser.read(64)
    if len(data) > 0:
        # Look for MSP header
        for j in range(len(data) - 1):
            if data[j] == ord('$') and data[j+1] == ord('X'):
                print(f'  MSP packet found at offset {j}, size: {len(data)}')
                break
    time.sleep(0.2)

ser.close()
print('Test complete')
"
```

### 1.4 Тест TF-Luna LiDAR

```bash
cd ~/visual_homing
python3 -c "
import serial
import time

ser = serial.Serial('/dev/serial2', 115200, timeout=2)
time.sleep(1)

print('Reading TF-Luna data...')
for i in range(10):
    data = ser.read(9)
    if len(data) >= 9 and data[0] == 0x59 and data[1] == 0x59:
        dist = data[2] | (data[3] << 8)
        strength = data[4] | (data[5] << 8)
        temp = (data[6] | (data[7] << 8)) / 100.0
        print(f'  Distance: {dist}cm ({dist/100:.2f}m), Signal: {strength}, Temp: {temp:.1f}C')
    time.sleep(0.1)

ser.close()
print('Test complete')
"
```

### 1.5 Тест Feature Detection

```bash
cd ~/visual_homing
python3 -c "
import cv2
import numpy as np

# Створити ORB детектор
orb = cv2.ORB_create(nfeatures=500)

# Захопити кадр
cap = cv2.VideoCapture(0)
ret, frame = cap.read()
cap.release()

if ret:
    gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
    kp, des = orb.detectAndCompute(gray, None)
    print(f'Detected {len(kp)} features')
    print(f'Descriptor shape: {des.shape if des is not None else None}')
else:
    print('Failed to capture frame')
"
```

## 2. Тестування на столі (без польоту)

### 2.1 Тест запису маршруту

1. Запустити Visual Homing:
```bash
python main.py --test-mode
```

2. Відкрити веб-інтерфейс: `http://visual-homing.local:5000`

3. Натиснути "Start Recording"

4. Повільно рухати камеру над текстурою (ковер, газета)

5. Перевірити:
   - Кількість keyframes збільшується
   - Фічі відображаються на відео

### 2.2 Тест повернення

1. Після запису - натиснути "Stop"

2. Повернути камеру назад вручну

3. Натиснути "Start Return"

4. Перевірити:
   - Система розпізнає keyframes
   - Показує напрямок до наступного keyframe

## 3. Польотні тести

### 3.1 Перший польот (безпечний)

**Умови:**
- Відкритий простір без перешкод
- Висота 5-10м
- Безвітряна погода
- GPS увімкнено (для резерву)

**Процедура:**
1. Зліт в режимі Stabilize
2. Перехід в Alt Hold
3. Запуск запису (через веб або RC перемикач)
4. Проліт 50м по прямій
5. Зупинка запису
6. Запуск повернення
7. **ГОТОВНІСТЬ ПЕРЕЙТИ НА РУЧНИЙ РЕЖИМ!**

### 3.2 Порівняння з GPS

1. Записати маршрут з GPS
2. Повернутись по Visual Homing
3. Порівняти траєкторії в логах

## 4. Калібрування

### 4.1 Камера

Якщо використовуєте ширококутний об'єктив:

```bash
python calibrate_camera.py --device /dev/video0
```

### 4.2 Параметри Visual Homing

Редагувати `config.py`:

```python
# Фічі
ORB_FEATURES = 500        # Кількість фічей
MATCH_THRESHOLD = 30      # Поріг матчінгу
MIN_MATCHES = 10          # Мінімум для розпізнавання

# Keyframes
KEYFRAME_DISTANCE = 2.0   # Метри між keyframes
KEYFRAME_ANGLE = 15       # Градуси повороту
```

### 4.3 Параметри ArduPilot

```ini
# Точність External Nav
VISO_POS_M_NSE = 0.1      # Зменшити = більше довіри
VISO_DELAY_MS = 80        # Збільшити якщо лаг
```

## 5. Троублшутінг

### Мало фічей
- Перевірити фокус камери
- Збільшити ORB_FEATURES
- Поліпшити освітлення (IR для термальної)

### Нестабільне матчування
- Збільшити MIN_MATCHES
- Зменшити KEYFRAME_DISTANCE

### EKF не приймає дані
- Перевірити VISO_DELAY_MS
- Перевірити EK3_POS_I_GATE
- Дивитись EKF3.INN в логах

## 6. Безпека

### Чек-лист перед польотом

- [ ] GPS резерв увімкнено
- [ ] Battery failsafe налаштовано
- [ ] RC failsafe налаштовано
- [ ] Перемикач на ручний режим готовий
- [ ] Visual Homing запущено
- [ ] Веб-інтерфейс доступний
- [ ] Камера детектує фічі
- [ ] MAVLink з'єднання активне

### Аварійні дії

1. **Перша пріоритетна дія** - перейти на ручний режим (Stabilize)
2. Якщо Visual Homing збоїть - активується GPS RTL
3. Якщо все втрачено - Land на місці
