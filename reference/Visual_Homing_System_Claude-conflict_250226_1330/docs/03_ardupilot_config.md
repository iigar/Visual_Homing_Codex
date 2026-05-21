# Конфігурація ArduPilot для Visual Homing + Smart RTL

## 1. Базові налаштування Matek H743-Slim V3

### Підключення до Mission Planner/QGroundControl

1. Підключити Matek FC через USB Type-C
2. Відкрити Mission Planner -> Config -> Full Parameter List
3. Зберегти поточні параметри (Backup)

## 2. Налаштування UART портів

### Карта UART портів

| Порт | ArduPilot | Пристрій | Протокол | Baud |
|------|-----------|----------|----------|------|
| UART3 | SERIAL3 | Raspberry Pi Zero 2 W | MAVLink2 (2) | 115200 |
| UART4 | SERIAL4 | MATEK 3901-L0X | MSP (32) | 115200 |
| UART5 | SERIAL5 | TF-Luna LiDAR | Rangefinder (9) | 115200 |

### Параметри UART

```ini
# MAVLink до Raspberry Pi (UART3)
SERIAL3_PROTOCOL = 2        # MAVLink2
SERIAL3_BAUD = 115          # 115200 baud
SERIAL3_OPTIONS = 0

# MATEK 3901-L0X Optical Flow (UART4)
SERIAL4_PROTOCOL = 32       # MSP
SERIAL4_BAUD = 115          # 115200 baud

# TF-Luna LiDAR (UART5)
SERIAL5_PROTOCOL = 9        # Rangefinder
SERIAL5_BAUD = 115          # 115200 baud
```

## 3. Вимкнення компаса

```ini
# Повне вимкнення компаса
COMPASS_ENABLE = 0
COMPASS_USE = 0
COMPASS_USE2 = 0
COMPASS_USE3 = 0
```

## 4. Налаштування GPS

```ini
# GPS як резервне джерело (може бути вимкнено)
GPS_TYPE = 1                # Auto (0 = вимкнено)
```

## 5. External Navigation (Visual Homing від Pi)

```ini
# Увімкнити прийом Visual Position від Pi
VISO_TYPE = 1               # MAVLink (VISION_POSITION_ESTIMATE)
VISO_POS_M_NSE = 0.1        # Шум позиції (метри)
VISO_VEL_M_NSE = 0.1        # Шум швидкості (м/с)
VISO_YAW_M_NSE = 0.1        # Шум yaw (радіани)
VISO_DELAY_MS = 80          # Затримка обробки на Pi
```

## 6. Optical Flow (MATEK 3901-L0X)

```ini
# Увімкнити Optical Flow через MSP
FLOW_TYPE = 7               # MSP Optical Flow
FLOW_FXSCALER = -800        # Масштабування X (від'ємне = інвертований)
FLOW_FYSCALER = -800        # Масштабування Y
```

### Перевірка Optical Flow

В Mission Planner -> Status Tab:
- `opt_m_x` / `opt_m_y` - повинні змінюватись при русі
- `opt_qua` - якість (>50 = добре)

## 7. Rangefinder (TF-Luna + VL53L0X)

### TF-Luna (основний далекомір, до 8м)

```ini
RNGFND1_TYPE = 20           # Benewake-Serial
RNGFND1_MIN_CM = 20         # Мінімум 0.2м
RNGFND1_MAX_CM = 800        # Максимум 8.0м
RNGFND1_ORIENT = 25         # Вниз
RNGFND1_GNDCLEAR = 10       # Відстань від землі при посадці (см)
```

### VL53L0X (вбудований в MATEK 3901-L0X, до 2м)

```ini
RNGFND2_TYPE = 32           # MSP Rangefinder
RNGFND2_MIN_CM = 2          # Мінімум 0.02м
RNGFND2_MAX_CM = 200        # Максимум 2.0м
```

## 8. EKF Source Sets (ключові налаштування)

### Source Set 1: External Navigation (Visual Homing)

Використовується для Visual Homing режиму (дані від Pi):

```ini
EK3_SRC1_POSXY = 6          # ExternalNav
EK3_SRC1_VELXY = 6          # ExternalNav
EK3_SRC1_POSZ = 1           # Барометр
EK3_SRC1_YAW = 6            # ExternalNav (Visual Heading)
EK3_POS_I_GATE = 500        # Innovation gate (позиція)
EK3_VEL_I_GATE = 500        # Innovation gate (швидкість)
```

### Source Set 2: Optical Flow (низька висота)

Використовується для точної навігації на низькій висоті (<50м):

```ini
EK3_SRC2_POSXY = 0          # Немає (Optical Flow дає тільки швидкість)
EK3_SRC2_VELXY = 5          # OpticalFlow
EK3_SRC2_POSZ = 2           # RangeFinder
EK3_SRC2_YAW = 6            # ExternalNav
```

### Перемикання між Source Sets

Перемикання можна робити через RC канал або програмно (MAVLink):

```ini
# Призначити RC канал для перемикання
RC7_OPTION = 90             # EKF Pos Source (Low=Set1, Mid=Set2, High=Set3)
```

## 9. Smart RTL налаштування

### RTL параметри

```ini
# Висота RTL (см) - для дальніх польотів (5км на 200м)
RTL_ALT = 5000              # 50м (Smart RTL знижується поступово)
RTL_ALT_FINAL = 0           # Посадка після RTL
RTL_SPEED = 800             # 8м/с швидкість повернення
RTL_CLIMB_MIN = 0           # Не набирати додаткову висоту
RTL_LOIT_TIME = 5000        # 5с зависання перед посадкою
```

### Логіка Smart RTL (реалізована на Pi)

```
Політ 5км на 200м:

1. RTL активовано
   |
   v
2. PHASE: HIGH_ALT (>50m)
   - ArduPilot керує навігацією (IMU + Baro)
   - Pi моніторить та записує стан
   |
   v [після 50% зворотного шляху (~2.5км)]
3. PHASE: DESCENT
   - ArduPilot керує горизонтальним рухом
   - Pi командує поступове зниження
   - Target: від 200м до 50м
   |
   v [висота < 50м]
4. PHASE: LOW_ALT
   - Optical Flow (MATEK 3901-L0X) активний
   - Visual Homing для пошуку keyframes
   - TF-Luna для точної висоти
   |
   v [висота < 5м, відстань < 10м]
5. PHASE: PRECISION_LAND
   - Optical Flow для утримання позиції
   - TF-Luna для висоти
   - Повільне зниження 0.15-0.3 м/с
   |
   v
6. COMPLETED - посадка
```

## 10. Failsafe

```ini
# GPS failsafe вимкнено (Visual Homing працює без GPS)
FS_GPS_ENABLE = 0

# Battery failsafe
BATT_FS_LOW_ACT = 2         # RTL при низькому заряді
BATT_LOW_VOLT = 14.0        # Поріг для 4S (В)
BATT_CRT_VOLT = 13.2        # Критичний рівень
BATT_FS_CRT_ACT = 1         # Land при критичному
```

## 11. Streaming параметри

```ini
# Stream rates для SERIAL3 (до Pi)
SR3_POSITION = 10           # Позиція 10Hz
SR3_RAW_SENS = 2            # Raw sensors 2Hz
SR3_EXT_STAT = 2            # Extended status 2Hz
SR3_RC_CHAN = 5             # RC channels 5Hz
SR3_EXTRA1 = 10             # Attitude 10Hz
SR3_EXTRA2 = 5              # VFR HUD 5Hz
SR3_EXTRA3 = 2              # AHRS 2Hz
```

## 12. Повний файл параметрів

Файл: `firmware/config/visual_homing.param`

Завантаження:
1. Mission Planner -> Config -> Full Parameter List
2. Load from file -> Вибрати `visual_homing.param`
3. Write Params
4. Reboot FC

## 13. Тестування конфігурації

### Перевірка MAVLink з'єднання (Pi)

```bash
# На Pi - перевірити з'єднання
python3 -c "
from pymavlink import mavutil
m = mavutil.mavlink_connection('/dev/serial0', baud=115200)
m.wait_heartbeat()
print(f'Connected to system {m.target_system}')
print(f'Mode: {m.flightmode}')
"
```

### Перевірка Optical Flow

В Mission Planner -> Status Tab:
- `opt_m_x`, `opt_m_y` - оптичний потік
- `opt_qua` - якість (0-255)

### Перевірка Rangefinder

В Mission Planner -> Status Tab:
- `sonarrange` - відстань TF-Luna (м)
- `rangefinder1` - TF-Luna
- `rangefinder2` - VL53L0X (3901-L0X)

### Перевірка EKF

В Mission Planner:
1. Data Flash Logs -> Download
2. Log Browser -> EKF3.INN (має бути низьким)
3. Перевірити що `ekf_statusflags` не показує помилок
