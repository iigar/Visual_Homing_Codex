#!/usr/bin/env python3
"""
Visual Homing MAVLink Diagnostics
Діагностика підключення до ArduPilot
"""
import sys
import time

# Add parent directory to path
sys.path.insert(0, '/home/pi/visual_homing')

def check_serial():
    """Check serial port availability"""
    import os
    
    print("=" * 50)
    print("1. Перевірка серійних портів")
    print("=" * 50)
    
    ports = ['/dev/serial0', '/dev/ttyAMA0', '/dev/ttyS0', '/dev/ttyUSB0']
    
    for port in ports:
        if os.path.exists(port):
            print(f"  ✓ {port} - існує")
            # Check if readable
            try:
                with open(port, 'rb') as f:
                    print(f"    → доступний для читання")
            except PermissionError:
                print(f"    → ПОМИЛКА: немає прав (sudo usermod -a -G dialout pi)")
            except Exception as e:
                print(f"    → ПОМИЛКА: {e}")
        else:
            print(f"  ✗ {port} - не існує")
    
    # Check /boot/firmware/config.txt
    print("\n  Перевірка /boot/firmware/config.txt:")
    try:
        with open('/boot/firmware/config.txt', 'r') as f:
            content = f.read()
            if 'enable_uart=1' in content:
                print("    ✓ enable_uart=1 знайдено")
            else:
                print("    ✗ enable_uart=1 НЕ знайдено - UART вимкнений!")
            
            if 'dtoverlay=disable-bt' in content or 'dtoverlay=pi3-disable-bt' in content:
                print("    ✓ Bluetooth вимкнений (serial0 вільний)")
            else:
                print("    ⚠ Bluetooth може займати serial0")
    except Exception as e:
        print(f"    Помилка читання config.txt: {e}")


def check_pymavlink():
    """Check pymavlink installation"""
    print("\n" + "=" * 50)
    print("2. Перевірка pymavlink")
    print("=" * 50)
    
    try:
        from pymavlink import mavutil
        print(f"  ✓ pymavlink встановлений")
        print(f"    Версія: {mavutil.mavlink.__version__ if hasattr(mavutil.mavlink, '__version__') else 'unknown'}")
    except ImportError as e:
        print(f"  ✗ pymavlink НЕ встановлений: {e}")
        print("    Виправлення: pip install pymavlink")
        return False
    
    return True


def check_mavlink_connection():
    """Try to connect to MAVLink"""
    print("\n" + "=" * 50)
    print("3. Тест підключення MAVLink")
    print("=" * 50)
    
    from pymavlink import mavutil
    
    port = '/dev/serial0'
    baud = 115200
    
    print(f"  Підключення до {port} @ {baud}...")
    
    try:
        conn = mavutil.mavlink_connection(
            port,
            baud=baud,
            source_system=1,
            source_component=191
        )
        
        print("  ✓ Порт відкрито")
        
        # Wait for heartbeat
        print("  Очікування heartbeat від FC (15 сек)...")
        msg = conn.recv_match(type='HEARTBEAT', blocking=True, timeout=15)
        
        if msg:
            print(f"  ✓ HEARTBEAT отримано!")
            print(f"    Тип: {mavutil.mavlink.enums['MAV_TYPE'][msg.type].name}")
            print(f"    Autopilot: {mavutil.mavlink.enums['MAV_AUTOPILOT'][msg.autopilot].name}")
            print(f"    Mode: {mavutil.mode_string_v10(msg)}")
            
            # Send our heartbeat
            print("\n  Відправка нашого heartbeat...")
            conn.mav.heartbeat_send(
                mavutil.mavlink.MAV_TYPE_ONBOARD_CONTROLLER,
                mavutil.mavlink.MAV_AUTOPILOT_INVALID,
                0, 0, 0
            )
            print("  ✓ Heartbeat відправлено")
            
            # Try sending vision position
            print("\n  Відправка VISION_POSITION_ESTIMATE...")
            try:
                conn.mav.vision_position_estimate_send(
                    int(time.time() * 1e6),
                    0.0, 0.0, 0.0,  # x, y, z
                    0.0, 0.0, 0.0   # roll, pitch, yaw
                )
                print("  ✓ VISION_POSITION_ESTIMATE відправлено")
            except TypeError as e:
                print(f"  ✗ Помилка відправки: {e}")
                print("    Можливо потрібно інші аргументи")
            
            conn.close()
            return True
        else:
            print("  ✗ HEARTBEAT не отримано")
            print("    Перевірте:")
            print("    - Підключення TX/RX (можливо переплутані)")
            print("    - Baudrate на FC (має бути 115200)")
            print("    - SERIALx_PROTOCOL = 2 (MAVLink2)")
            conn.close()
            return False
            
    except Exception as e:
        print(f"  ✗ Помилка підключення: {e}")
        return False


def check_ardupilot_params():
    """Remind user about ArduPilot parameters"""
    print("\n" + "=" * 50)
    print("4. Необхідні параметри ArduPilot")
    print("=" * 50)
    print("""
  В Mission Planner встановіть:
  
  # Visual Odometry
  VISO_TYPE = 1            # MAVLink
  VISO_ORIENT = 0          # Forward
  VISO_DELAY_MS = 50
  
  # EKF3 Navigation Sources
  EK3_SRC1_POSXY = 6       # ExternalNav
  EK3_SRC1_VELXY = 6       # ExternalNav
  EK3_SRC1_POSZ = 1        # Baro
  EK3_SRC1_YAW = 1         # Compass
  
  # Serial Port (якщо TELEM2/SERIAL2)
  SERIAL2_PROTOCOL = 2     # MAVLink2
  SERIAL2_BAUD = 115200
  
  Після зміни - REBOOT FC!
""")


def main():
    print("\n" + "=" * 50)
    print("Visual Homing MAVLink Diagnostics")
    print("=" * 50 + "\n")
    
    check_serial()
    
    if check_pymavlink():
        check_mavlink_connection()
    
    check_ardupilot_params()
    
    print("\n" + "=" * 50)
    print("Діагностика завершена")
    print("=" * 50 + "\n")


if __name__ == '__main__':
    main()
