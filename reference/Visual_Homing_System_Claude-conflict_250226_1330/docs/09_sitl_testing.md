# Тестування в SITL (Software In The Loop)

## Що таке SITL?

SITL дозволяє запускати ArduPilot на вашому комп'ютері без реального дрона.
Це безпечний спосіб тестувати навігаційну логіку, Smart RTL та інтеграцію сенсорів.

## 1. Встановлення SITL

### Ubuntu/Debian

```bash
# Клонувати ArduPilot
git clone --recurse-submodules https://github.com/ArduPilot/ardupilot.git
cd ardupilot

# Встановити залежності
Tools/environment_install/install-prereqs-ubuntu.sh -y
. ~/.profile

# Зібрати SITL для ArduCopter
./waf configure --board sitl
./waf copter
```

### macOS

```bash
brew install gcc-arm-none-eabi
git clone --recurse-submodules https://github.com/ArduPilot/ardupilot.git
cd ardupilot
Tools/environment_install/install-prereqs-mac.sh -y
./waf configure --board sitl
./waf copter
```

### Windows (WSL2)

```bash
# В WSL2 Ubuntu
sudo apt install python3-pip python3-dev
git clone --recurse-submodules https://github.com/ArduPilot/ardupilot.git
cd ardupilot
Tools/environment_install/install-prereqs-ubuntu.sh -y
./waf configure --board sitl
./waf copter
```

## 2. Запуск SITL

### Базовий запуск

```bash
cd ardupilot

# Запустити ArduCopter SITL
Tools/autotest/sim_vehicle.py -v ArduCopter --console --map

# З конкретною локацією (Київ)
Tools/autotest/sim_vehicle.py -v ArduCopter \
  --console --map \
  -L 50.4501,30.5234,200,0
```

### Параметри для Visual Homing

В консолі SITL (MAVProxy):

```
# Завантажити наші параметри
param load /path/to/firmware/config/visual_homing.param

# Або вручну встановити ключові параметри
param set SERIAL3_PROTOCOL 2
param set SERIAL3_BAUD 115
param set VISO_TYPE 1
param set FLOW_TYPE 7
param set RNGFND1_TYPE 20
param set RNGFND1_MAX_CM 800

# Увімкнути EKF Source Set 2 для Optical Flow
param set EK3_SRC2_VELXY 5
param set EK3_SRC2_POSZ 2
```

## 3. Підключення Visual Homing до SITL

### Через TCP (замість UART)

```python
# В config.py або через змінні середовища
# Замість /dev/serial0 використовуємо TCP
# SITL слухає на localhost:5762

# Запуск main.py з TCP
export MAVLINK_PORT="tcp:127.0.0.1:5762"
python3 main.py --test-mode
```

### Або через UDP

```bash
# SITL автоматично відправляє MAVLink на UDP 14550
export MAVLINK_PORT="udp:127.0.0.1:14550"
python3 main.py --test-mode
```

### Python тест підключення

```python
from pymavlink import mavutil

# TCP
m = mavutil.mavlink_connection('tcp:127.0.0.1:5762')
# або UDP
# m = mavutil.mavlink_connection('udp:127.0.0.1:14550')

m.wait_heartbeat()
print(f'SITL connected: system={m.target_system}, mode={m.flightmode}')

# Arm та Takeoff
m.mav.command_long_send(
    m.target_system, m.target_component,
    400, 0,  # MAV_CMD_COMPONENT_ARM_DISARM
    1, 0, 0, 0, 0, 0, 0  # Arm
)

m.mav.command_long_send(
    m.target_system, m.target_component,
    22, 0,  # MAV_CMD_NAV_TAKEOFF
    0, 0, 0, 0, 0, 0, 10  # 10m altitude
)
```

## 4. Тест Smart RTL в SITL

### Сценарій тесту

```python
"""
Тест Smart RTL в SITL:
1. Takeoff до 10м
2. Летіти 100м на північ
3. Набрати 50м
4. Активувати Smart RTL
5. Перевірити фази: HIGH_ALT -> DESCENT -> LOW_ALT -> PRECISION_LAND
"""

from pymavlink import mavutil
import time
import sys
sys.path.insert(0, '/path/to/visual_homing')

from navigation.smart_rtl import SmartRTL, SmartRTLConfig

# Connect
m = mavutil.mavlink_connection('tcp:127.0.0.1:5762')
m.wait_heartbeat()
print(f'Connected to SITL: {m.flightmode}')

# Initialize Smart RTL
rtl = SmartRTL(SmartRTLConfig(
    high_alt_threshold=30.0,  # Lower for SITL testing
    precision_land_threshold=3.0,
    descent_start_pct=0.5,
))

# Arm + Takeoff
m.mav.command_long_send(m.target_system, m.target_component,
    400, 0, 1, 0, 0, 0, 0, 0, 0)
time.sleep(2)

m.mav.command_long_send(m.target_system, m.target_component,
    22, 0, 0, 0, 0, 0, 0, 0, 50)  # Takeoff to 50m
time.sleep(10)

print('Flying to waypoint...')
# Fly 100m north in GUIDED mode
m.mav.set_position_target_local_ned_send(
    0, m.target_system, m.target_component,
    mavutil.mavlink.MAV_FRAME_LOCAL_NED,
    0b0000111111111000,
    100, 0, -50,  # 100m north, 50m altitude
    0, 0, 0, 0, 0, 0, 0, 0
)
time.sleep(15)

# Initiate Smart RTL
print('Starting Smart RTL...')
phase = rtl.initiate_rtl(
    current_altitude=50.0,
    home_distance=100.0
)
print(f'Initial phase: {phase}')

# Monitor phases
for i in range(100):
    msg = m.recv_match(type='LOCAL_POSITION_NED', blocking=True, timeout=1)
    if msg:
        alt = -msg.z  # NED: z is negative up
        dist = (msg.x**2 + msg.y**2)**0.5
        
        state = rtl.update(
            altitude=alt,
            home_distance=dist,
            flow_quality=80,
            visual_confidence=0.7,
            lidar_distance=min(alt, 8.0)
        )
        
        print(f'Phase: {state.phase.value}, Alt: {alt:.1f}m, '
              f'Dist: {dist:.1f}m, Progress: {state.return_progress*100:.0f}%')
        
        if state.phase.value == 'completed':
            print('Smart RTL COMPLETED!')
            break
    
    time.sleep(0.5)

print('Test finished')
```

## 5. Тест Optical Flow в SITL

SITL має вбудовану симуляцію Optical Flow:

```bash
# Запустити SITL з Optical Flow симуляцією
Tools/autotest/sim_vehicle.py -v ArduCopter \
  --console --map \
  -A "--uartD=sim:opticalflow"
```

## 6. Mission Planner для візуалізації

1. Запустити SITL
2. Відкрити Mission Planner
3. Підключитись: TCP, localhost:5760
4. Status Tab -> перевірити:
   - `opt_m_x`, `opt_m_y` (Optical Flow)
   - `sonarrange` (Rangefinder)
   - `ekf_statusflags`

## 7. Автоматизація тестів

### Pytest для SITL

```python
# tests/test_sitl.py
import pytest
from navigation.smart_rtl import SmartRTL, SmartRTLConfig, SmartRTLPhase

class TestSmartRTL:
    def test_initiate_high_alt(self):
        rtl = SmartRTL()
        phase = rtl.initiate_rtl(altitude=100, home_distance=5000)
        assert phase == SmartRTLPhase.HIGH_ALT
    
    def test_initiate_low_alt(self):
        rtl = SmartRTL()
        phase = rtl.initiate_rtl(altitude=30, home_distance=500)
        assert phase == SmartRTLPhase.LOW_ALT
    
    def test_descent_transition(self):
        rtl = SmartRTL()
        rtl.initiate_rtl(altitude=200, home_distance=5000)
        # Simulate 51% return
        rtl.update(altitude=200, home_distance=2450)
        assert rtl.phase == SmartRTLPhase.DESCENT
    
    def test_precision_land(self):
        rtl = SmartRTL(SmartRTLConfig(high_alt_threshold=50))
        rtl.initiate_rtl(altitude=3, home_distance=5)
        rtl.update(altitude=3, home_distance=5, flow_quality=80)
        assert rtl.phase == SmartRTLPhase.PRECISION_LAND
    
    def test_completion(self):
        rtl = SmartRTL()
        rtl.initiate_rtl(altitude=0.2, home_distance=1)
        rtl.update(altitude=0.2, home_distance=1, flow_quality=80)
        # Below min altitude
        rtl.update(altitude=0.1, home_distance=0.5)
        assert rtl.phase == SmartRTLPhase.COMPLETED
```

```bash
# Запуск тестів
cd ~/visual_homing
python3 -m pytest tests/test_sitl.py -v
```

## 8. Рекомендації

- Завжди тестуйте нову логіку в SITL перед реальним польотом
- Використовуйте `--speedup 5` для прискорення SITL
- Зберігайте логи: `dataflash.log` для аналізу
- Тестуйте failsafe сценарії (втрата зв'язку, низький заряд)
