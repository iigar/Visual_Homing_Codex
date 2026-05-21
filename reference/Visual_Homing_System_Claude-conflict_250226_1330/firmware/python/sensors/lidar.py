"""
Benewake TF-Luna LiDAR Sensor Module
Модуль LiDAR TF-Luna для вимірювання висоти
"""
import struct
import time
import logging
from typing import Optional
from dataclasses import dataclass
import threading

logger = logging.getLogger(__name__)

# TF-Luna UART protocol constants
TF_LUNA_HEADER = 0x59
TF_LUNA_FRAME_SIZE = 9


@dataclass
class LidarData:
    """TF-Luna LiDAR measurement data"""
    distance_cm: int = 0       # Distance in centimeters
    signal_strength: int = 0   # Signal strength (0-65535)
    temperature_raw: int = 0   # Temperature in 0.01 Celsius
    timestamp: float = 0.0
    
    @property
    def distance_m(self) -> float:
        return self.distance_cm / 100.0
    
    @property
    def temperature_c(self) -> float:
        return self.temperature_raw / 100.0
    
    @property
    def is_valid(self) -> bool:
        return (self.signal_strength > 100 and 
                20 <= self.distance_cm <= 800)  # 0.2m to 8m valid range


class LidarSensor:
    """
    Benewake TF-Luna LiDAR sensor interface.
    Communicates via UART.
    
    Sensor specs:
    - Range: 0.2 - 8.0 meters
    - Accuracy: +/- 6cm (0.2-3m), +/- 2% (3-8m)
    - Frame rate: 1-250Hz (default 100Hz)
    - Interface: UART 115200 baud
    - Power: 3.7-5.2V, 70mA typical
    - Wavelength: 850nm (Class 1 eye-safe)
    """
    
    def __init__(
        self,
        serial_port: str = "/dev/serial2",
        baudrate: int = 115200
    ):
        self.serial_port = serial_port
        self.baudrate = baudrate
        self._serial = None
        self._running = False
        self._read_thread: Optional[threading.Thread] = None
        self._last_data = LidarData()
        self._data_lock = threading.Lock()
        self._connected = False
    
    def connect(self) -> bool:
        """Connect to TF-Luna sensor"""
        try:
            import serial
            self._serial = serial.Serial(
                port=self.serial_port,
                baudrate=self.baudrate,
                timeout=1.0
            )
            self._connected = True
            self._running = True
            
            self._read_thread = threading.Thread(
                target=self._read_loop,
                daemon=True
            )
            self._read_thread.start()
            
            logger.info(f"TF-Luna connected on {self.serial_port}")
            return True
        except ImportError:
            logger.error("pyserial not installed")
            return False
        except Exception as e:
            logger.error(f"Failed to connect to TF-Luna: {e}")
            return False
    
    def disconnect(self):
        """Disconnect from sensor"""
        self._running = False
        if self._read_thread:
            self._read_thread.join(timeout=2.0)
        if self._serial:
            self._serial.close()
            self._serial = None
        self._connected = False
        logger.info("TF-Luna disconnected")
    
    def _read_loop(self):
        """Continuously read TF-Luna frames"""
        buffer = bytearray()
        
        while self._running and self._serial:
            try:
                data = self._serial.read(128)
                if data:
                    buffer.extend(data)
                    self._parse_buffer(buffer)
            except Exception as e:
                logger.error(f"TF-Luna read error: {e}")
                time.sleep(0.1)
    
    def _parse_buffer(self, buffer: bytearray):
        """Parse TF-Luna data frames from buffer"""
        while len(buffer) >= TF_LUNA_FRAME_SIZE:
            # Find frame header (two 0x59 bytes)
            header_found = False
            for i in range(len(buffer) - 1):
                if buffer[i] == TF_LUNA_HEADER and buffer[i + 1] == TF_LUNA_HEADER:
                    if i > 0:
                        del buffer[:i]
                    header_found = True
                    break
            
            if not header_found:
                buffer.clear()
                return
            
            if len(buffer) < TF_LUNA_FRAME_SIZE:
                return
            
            # Validate checksum
            checksum = sum(buffer[:8]) & 0xFF
            if checksum != buffer[8]:
                del buffer[:1]
                continue
            
            # Parse frame
            distance_cm = buffer[2] | (buffer[3] << 8)
            signal_strength = buffer[4] | (buffer[5] << 8)
            temp_raw = buffer[6] | (buffer[7] << 8)
            
            with self._data_lock:
                self._last_data.distance_cm = distance_cm
                self._last_data.signal_strength = signal_strength
                self._last_data.temperature_raw = temp_raw
                self._last_data.timestamp = time.time()
            
            del buffer[:TF_LUNA_FRAME_SIZE]
    
    def set_frame_rate(self, fps: int = 100) -> bool:
        """Set TF-Luna output frame rate (1-250 Hz)"""
        if not self._serial or not self._connected:
            return False
        
        try:
            fps = max(1, min(250, fps))
            # TF-Luna frame rate command
            cmd = bytearray([
                0x5A, 0x06, 0x03,
                fps & 0xFF, (fps >> 8) & 0xFF,
                0x00
            ])
            cmd[5] = sum(cmd[:5]) & 0xFF
            self._serial.write(cmd)
            logger.info(f"TF-Luna frame rate set to {fps}Hz")
            return True
        except Exception as e:
            logger.error(f"Failed to set frame rate: {e}")
            return False
    
    @property
    def latest_data(self) -> LidarData:
        """Get latest sensor data"""
        with self._data_lock:
            return LidarData(
                distance_cm=self._last_data.distance_cm,
                signal_strength=self._last_data.signal_strength,
                temperature_raw=self._last_data.temperature_raw,
                timestamp=self._last_data.timestamp
            )
    
    @property
    def is_connected(self) -> bool:
        return self._connected
    
    @property
    def is_healthy(self) -> bool:
        """Check if sensor is providing fresh data"""
        if not self._connected:
            return False
        with self._data_lock:
            age = time.time() - self._last_data.timestamp
            return age < 0.5  # Data must be < 500ms old
