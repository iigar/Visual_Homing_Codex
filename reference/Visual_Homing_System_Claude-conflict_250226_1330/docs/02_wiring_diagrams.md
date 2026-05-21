# Схеми підключення Visual Homing

## 1. Загальна схема системи (оновлена)

```
+---------------------------------------------------------------------+
|                         МУЛЬТИКОПТЕР                                 |
|                                                                      |
|   +-----------+      UART3       +--------------+                    |
|   | Matek FC  |<--------------->| Raspberry Pi  |                    |
|   | H743-Slim |    (MAVLink)    |  Zero 2 W     |                    |
|   | ArduCopter|                 |               |                    |
|   |  4.5.7    |                 +-------+-------+                    |
|   +-----+-----+                        |                             |
|         |                              | USB                          |
|    UART4| UART5                        v                              |
|    +----+---+  +--------+      +--------------+                      |
|    | MATEK  |  |TF-Luna |      |   EasyCap    |                      |
|    | 3901-  |  | LiDAR  |      | USB Capture  |                      |
|    | L0X    |  | 0.2-8m |      +------+-------+                      |
|    +--------+  +--------+             | Analog                        |
|    OptFlow+LiDAR                      v                               |
|                                +--------------+                       |
|                                |Caddx Thermal |                       |
|                                |    256       |                       |
|                                | (8-14um IR)  |                       |
|                                +--------------+                       |
+----------------------------------------------------------------------+
```

## 2. Розпіновка Raspberry Pi Zero 2 W

### Зображення пінауту

![Raspberry Pi Zero 2 W Pinout](https://customer-assets.emergentagent.com/job_a3acf624-7531-4b1f-ac69-62b1ab2e0ff6/artifacts/fywuct00_Raspberry%20Pi%20Zero%20W%202%20pinout.jpg)

### GPIO Header (40-pin)

```
                    +-----------------------------+
                    |      Pi Zero 2 W (зверху)   |
                    |                             |
   3.3V  (1) *-----+-----------------------------+-----* (2)  5V
  GPIO2  (3) o-----+-----------------------------+-----o (4)  5V
  GPIO3  (5) o-----+-----------------------------+-----o (6)  GND  <-- до FC GND
  GPIO4  (7) o-----+-----------------------------+-----* (8)  TXD  <-- UART TX (до FC RX3)
    GND  (9) o-----+-----------------------------+-----* (10) RXD  <-- UART RX (до FC TX3)
 GPIO17 (11) o-----+-----------------------------+-----o (12) GPIO18
 GPIO27 (13) o-----+-----------------------------+-----o (14) GND
 GPIO22 (15) o-----+-----------------------------+-----o (16) GPIO23
   3.3V (17) o-----+-----------------------------+-----o (18) GPIO24
 GPIO10 (19) o-----+-----------------------------+-----o (20) GND
  GPIO9 (21) o-----+-----------------------------+-----o (22) GPIO25
 GPIO11 (23) o-----+-----------------------------+-----o (24) GPIO8
    GND (25) o-----+-----------------------------+-----o (26) GPIO7
                    +-----------------------------+

* = використовується для MAVLink
o = вільні для інших потреб
```

### Підключення до Matek FC (MAVLink UART)

```
      Pi Zero 2 W                    Matek H743-Slim V3
    +--------------+                +------------------+
    |              |                |                  |
    |  Pin 8  TXD |--------------->|  RX3 (UART3)    |
    |              |                |                  |
    | Pin 10  RXD |<---------------|  TX3 (UART3)    |
    |              |                |                  |
    |  Pin 6  GND |--------------->|  GND            |
    |              |                |                  |
    +--------------+                +------------------+

    УВАГА: TX-->RX, RX-->TX (перехресне підключення!)
    НЕ підключати 5V від FC до Pi - живлення окремо!
```

## 3. Розпіновка Mateksys H743-Slim V3

### Зображення пінауту

![Mateksys H743-Slim V3 Pinout](https://customer-assets.emergentagent.com/job_a3acf624-7531-4b1f-ac69-62b1ab2e0ff6/artifacts/2saekv7e_poletnyj-kontroller-matek-h743-slim-v3-3.jpg)

### Основні порти

| Порт | UART | ArduPilot Serial | Призначення |
|------|------|------------------|-------------|
| TX3/RX3 | UART3 | SERIAL3 | MAVLink до Pi Zero 2 W |
| TX4/RX4 | UART4 | SERIAL4 | MATEK 3901-L0X (MSP) |
| TX5/RX5 | UART5 | SERIAL5 | TF-Luna LiDAR |
| TX1/RX1 | UART1 | SERIAL1 | GPS (за потреби) |
| TX7/RX7 | UART7 | SERIAL7 | Телеметрія (запас) |

### Характеристики

- **Розмір**: 30x30mm (mounting 25.5x25.5mm)
- **MCU**: STM32H743
- **BEC**: 5V/2A (макс 3A)
- **Вхідна напруга**: 6-36V (2-8S LiPo)
- **UART**: 7 портів з інверсією
- **I2C**: 2 порти
- **PWM**: 13 виходів
- **OSD**: AT7456E (SPI2)
- **USB**: Type-C

## 4. Підключення MATEK 3901-L0X (Optical Flow + LiDAR)

### Зображення підключення

![MATEK 3901-L0X Optical Flow + LiDAR](https://customer-assets.emergentagent.com/job_a3acf624-7531-4b1f-ac69-62b1ab2e0ff6/artifacts/6j4mtw68_opticheskij-sensor-lidar-matek-3901-l0x-1.jpg)

### Характеристики MATEK 3901-L0X

- **Optical Flow**: PMW3901 (ефективний діапазон 0.08m - нескінченність)
- **Rangefinder**: VL53L0X (0.02 - 2.0m)
- **Протокол**: MSP V2 (UART 115200)
- **Живлення**: 4.5-5.5V, 40mA
- **Роз'єм**: 4-pin (TX, RX, 5V, GND)

### Схема підключення до Matek H743-Slim V3

```
    MATEK 3901-L0X                 Matek H743-Slim V3
    +--------------+                +------------------+
    |              |                |                  |
    |     TX  o----|--------------->|  RX4 (UART4)    |
    |              |                |                  |
    |     RX  o----|<---------------|  TX4 (UART4)    |
    |              |                |                  |
    |     5V  o----|<---------------|  5V (BEC)       |
    |              |                |                  |
    |    GND  o----|--------------->|  GND            |
    |              |                |                  |
    +--------------+                +------------------+

    УВАГА: TX-->RX, RX-->TX (перехресне підключення!)
```

### Монтаж сенсора

```
           Напрямок польоту (вперед)
                   ^
                   |
    +------------------------------+
    |       ДРОН (вид знизу)       |
    |                              |
    |        +----------+          |
    |        |  MATEK   |          |
    |        | 3901-L0X |          |
    |        |  [стрілка вперед]   |
    |        |  [лінза вниз]       |
    |        +----------+          |
    |                              |
    +------------------------------+
    
    - Встановити НА НИЖНІЙ частині дрона
    - Лінза камери дивиться ВНИЗ
    - Маленька стрілка біля лінзи вказує ВПЕРЕД
    - Лінза повинна бути >2см від землі при старті
```

### Параметри ArduPilot для MATEK 3901-L0X

```
SERIAL4_PROTOCOL = 32    # MSP
SERIAL4_BAUD = 115       # 115200

FLOW_TYPE = 7            # MSP Optical Flow
FLOW_FXSCALER = -800     # Масштабування X
FLOW_FYSCALER = -800     # Масштабування Y

RNGFND2_TYPE = 32        # MSP Rangefinder (VL53L0X)
RNGFND2_MIN_CM = 2       # 0.02m мінімум
RNGFND2_MAX_CM = 200     # 2.0m максимум
```

## 5. Підключення TF-Luna LiDAR

### Зображення підключення

![TF-Luna LiDAR Sensor](https://customer-assets.emergentagent.com/job_a3acf624-7531-4b1f-ac69-62b1ab2e0ff6/artifacts/29geo9h7_TF-LUNA.webp)

![TF-Luna Pinout](https://customer-assets.emergentagent.com/job_a3acf624-7531-4b1f-ac69-62b1ab2e0ff6/artifacts/0ufh5hha_TF-LUNA2.avif)

### Характеристики TF-Luna

- **Діапазон**: 0.2 - 8.0m
- **Точність**: +/-6cm (0.2-3m), +/-2% (3-8m)
- **Частота**: 1-250Hz (за замовчуванням 100Hz)
- **Протокол**: UART 115200
- **Живлення**: 3.7-5.2V, 70mA
- **Лазер**: 850nm (Class 1, безпечний для очей)

### Розпіновка TF-Luna (6-pin роз'єм)

```
    +---+---+---+---+---+---+
    | 1 | 2 | 3 | 4 | 5 | 6 |
    |VCC|RXD|TXD|GND|CFG|MUX|
    +---+---+---+---+---+---+
      |   |   |   |   |   |
      |   |   |   |   |   +-- MUX: не підключати (UART режим)
      |   |   |   |   +------ CFG: не підключати або 3.3V (UART)
      |   |   |   +---------- GND: до FC GND
      |   |   +-------------- TXD: до FC RX5
      |   +------------------ RXD: до FC TX5
      +---------------------- VCC: 5V від FC BEC
```

### Схема підключення до Matek H743-Slim V3

```
    TF-Luna LiDAR                  Matek H743-Slim V3
    +--------------+                +------------------+
    |              |                |                  |
    | Pin1 VCC o---|<---------------|  5V (BEC)       |
    |              |                |                  |
    | Pin2 RXD o---|<---------------|  TX5 (UART5)    |
    |              |                |                  |
    | Pin3 TXD o---|--------------->|  RX5 (UART5)    |
    |              |                |                  |
    | Pin4 GND o---|--------------->|  GND            |
    |              |                |                  |
    | Pin5 CFG o---| (не підключено або 3.3V)         |
    |              |                |                  |
    | Pin6 MUX o---| (не підключено)                  |
    |              |                |                  |
    +--------------+                +------------------+

    УВАГА: TXD-->RX, RXD-->TX (перехресне підключення!)
    CFG = не підключено для UART режиму
```

### Параметри ArduPilot для TF-Luna

```
SERIAL5_PROTOCOL = 9     # Rangefinder
SERIAL5_BAUD = 115       # 115200

RNGFND1_TYPE = 20        # Benewake-Serial
RNGFND1_MIN_CM = 20      # 0.2m мінімум
RNGFND1_MAX_CM = 800     # 8.0m максимум
RNGFND1_ORIENT = 25      # Вниз
RNGFND1_GNDCLEAR = 10    # 10cm від землі при посадці
```

### Монтаж TF-Luna

```
    +------------------------------+
    |       ДРОН (вид збоку)       |
    |                              |
    |    =====РАМА=====            |
    |         |                    |
    |    +---------+               |
    |    | TF-Luna |               |
    |    | [лазер] | --> вниз      |
    |    +---------+               |
    |                              |
    +------------------------------+
    
    - Лазер повинен дивитися СТРОГО ВНИЗ
    - Захистити від пропвошу
    - Не перекривати оптику
```

## 6. Підключення Caddx Thermal 256

### Розпіновка Caddx Thermal 256

```
      +---------------------+
      |  Caddx Thermal 256  |
      |     (вид ззаду)     |
      |                     |
      |  [VIN] [GND] [VIDEO]|
      |   |     |      |    |
      +---+-----+------+----+
          |     |      |
          |     |      +--------> Жовтий (VIDEO) --> EasyCap жовтий RCA
          |     +---------------> Чорний (GND)   --> EasyCap GND
          +---------------------> Червоний (VIN) --> 5V BEC від PDB
```

### Підключення через EasyCap USB Capture

```
   Caddx Thermal 256              EasyCap                Pi Zero 2 W
  +--------------+           +--------------+          +-------------+
  |              |           |              |          |             |
  | VIDEO (жовт) |--> RCA -->| VIDEO IN     |          |             |
  |              |           |   (жовтий)   |          |             |
  | GND (чорний) |--> GND -->|              |   USB    |             |
  |              |           |         USB -|--------->| USB Port    |
  | VIN (черв.)  |           |              |          |             |
  +------+-------+           +--------------+          +-------------+
         |
         | 5V від BEC
         v
    +---------+
    |   PDB   | (Power Distribution Board)
    +---------+
```

## 7. Альтернатива: Pi Camera Module

### Підключення через CSI

```
      Pi Zero 2 W                    Pi Camera v2/v3
    +-------------+                 +---------------+
    |             |                 |               |
    | +--------+  |   CSI шлейф    | +---------+   |
    | |  CSI   |<-|---------------->| |  CSI    |   |
    | | роз'єм |  |  (15-pin FFC)   | | роз'єм  |   |
    | +--------+  |                 | +---------+   |
    |             |                 |               |
    +-------------+                 +---------------+

    УВАГА: Pi Zero 2 W використовує МIНI CSI роз'єм!
    Потрібен перехідний шлейф 22-pin --> 15-pin
    Рекомендований: Adafruit #1648 (15см)
```

## 8. Повна схема з'єднань (всі компоненти)

```
                    ЖИВЛЕННЯ
    +----------------------------------------------------+
    |                                                     |
    |    LiPo -----> PDB ------+-------> ESC ----> Мотори|
    |              (5V BEC)    |                          |
    |                          |                          |
    |                          +-------> Pi Zero 2 W     |
    |                          |         (через 5V)      |
    |                          |                          |
    |                          +-------> Caddx Thermal   |
    |                          |         (5V-20V)        |
    |                          |                          |
    |                          +-------> TF-Luna         |
    |                                    (5V від FC BEC) |
    +----------------------------------------------------+


                    СИГНАЛИ
    +----------------------------------------------------+
    |                                                     |
    |  Caddx --VIDEO--> EasyCap --USB--> Pi Zero 2 W     |
    |                                         |           |
    |                                         | UART3     |
    |                                         v           |
    |                                    Matek H743-Slim  |
    |                                    |    |    |      |
    |                              UART4 |    |    | UART5|
    |                                    v    v    v      |
    |                              MATEK  GPS   TF-Luna   |
    |                              3901-  (опц) LiDAR     |
    |                              L0X                    |
    +----------------------------------------------------+
```

## 9. Фінальна перевірка

### Чек-лист підключень

- [ ] Pi Zero 2 W отримує стабільні 5V
- [ ] UART3 TX-->RX, RX-->TX правильно підключені (Pi <--> FC)
- [ ] GND Pi з'єднаний з GND Matek
- [ ] EasyCap підключений до USB Pi
- [ ] Caddx VIDEO підключений до EasyCap
- [ ] Caddx отримує живлення 5-20V
- [ ] **MATEK 3901-L0X підключений до UART4 (TX-->RX, RX-->TX)**
- [ ] **MATEK 3901-L0X встановлений лінзою ВНИЗ, стрілкою ВПЕРЕД**
- [ ] **TF-Luna підключений до UART5 (TXD-->RX, RXD-->TX)**
- [ ] **TF-Luna лазер дивиться ВНИЗ**
- [ ] **TF-Luna Pin5 (CFG) не підключений (UART режим)**
- [ ] Всі з'єднання закріплені від вібрацій

### Тест підключень

```bash
# На Pi Zero 2 W

# Перевірити USB capture
lsusb | grep -i video

# Перевірити UART
ls -la /dev/serial0

# Тест MAVLink з'єднання
python3 -c "
from pymavlink import mavutil
m = mavutil.mavlink_connection('/dev/serial0', baud=115200)
m.wait_heartbeat()
print(f'Connected to system {m.target_system}')
"

# Тест MATEK 3901-L0X
python3 -c "
import serial
ser = serial.Serial('/dev/serial1', 115200, timeout=1)
data = ser.read(64)
print(f'Received {len(data)} bytes from 3901-L0X')
ser.close()
"

# Тест TF-Luna
python3 -c "
import serial
ser = serial.Serial('/dev/serial2', 115200, timeout=1)
data = ser.read(18)  # Read 2 frames
if len(data) >= 9 and data[0] == 0x59 and data[1] == 0x59:
    dist = data[2] | (data[3] << 8)
    print(f'TF-Luna distance: {dist}cm ({dist/100:.2f}m)')
else:
    print('No valid TF-Luna data')
ser.close()
"
```
