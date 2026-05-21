"""
MATEK 3901-L0X Optical Flow Sensor Module
Модуль оптичного потоку MATEK 3901-L0X (MSP протокол через UART)
"""
import struct
import time
import logging
from typing import Optional
from dataclasses import dataclass
import threading

logger = logging.getLogger(__name__)

# MSP V2 Protocol constants
MSP_HEADER = b'$X'
MSP_DIRECTION_REQUEST = b'<'
MSP_DIRECTION_RESPONSE = b'>'
MSP_SENSOR_RANGEFINDER = 0x1F01  # Rangefinder data
MSP_SENSOR_OPTICAL_FLOW = 0x1F02  # Optical flow data


@dataclass
class FlowData:
    """Optical flow measurement data"""
    flow_x: float = 0.0       # Flow in X axis (rad/s)
    flow_y: float = 0.0       # Flow in Y axis (rad/s)
    quality: int = 0           # Flow quality (0-255)
    distance_mm: int = 0       # Ground distance from LiDAR (mm)
    timestamp: float = 0.0
    
    @property
    def distance_m(self) -> float:
        return self.distance_mm / 1000.0
    
    @property
    def is_valid(self) -> bool:
        return self.quality > 50 and self.distance_mm > 20


class OpticalFlowSensor:
    """
    MATEK 3901-L0X Optical Flow + LiDAR (VL53L0X) sensor interface.
    Communicates via MSP V2 protocol over UART.
    
    Sensor specs:
    - Optical flow: PMW3901 (effective range 0.08-inf meters)
    - Rangefinder: VL53L0X (range 0.02-2.0 meters)
    - Interface: UART 115200 baud (MSP V2)
    - Power: 4.5-5.5V, 40mA
    """
    
    def __init__(
        self,
        serial_port: str = "/dev/serial1",
        baudrate: int = 115200
    ):
        self.serial_port = serial_port
        self.baudrate = baudrate
        self._serial = None
        self._running = False
        self._read_thread: Optional[threading.Thread] = None
        self._last_data = FlowData()
        self._data_lock = threading.Lock()
        self._connected = False
    
    def connect(self) -> bool:
        """Connect to the MATEK 3901-L0X sensor"""
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
            
            logger.info(f"MATEK 3901-L0X connected on {self.serial_port}")
            return True
        except ImportError:
            logger.error("pyserial not installed")
            return False
        except Exception as e:
            logger.error(f"Failed to connect to MATEK 3901-L0X: {e}")
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
        logger.info("MATEK 3901-L0X disconnected")
    
    def _read_loop(self):
        """Continuously read MSP packets from sensor"""
        buffer = bytearray()
        
        while self._running and self._serial:
            try:
                data = self._serial.read(256)
                if data:
                    buffer.extend(data)
                    self._parse_buffer(buffer)
            except Exception as e:
                logger.error(f"Read error: {e}")
                time.sleep(0.1)
    
    def _parse_buffer(self, buffer: bytearray):
        """Parse MSP V2 packets from buffer"""
        while len(buffer) >= 9:  # Minimum MSP V2 packet size
            # Find MSP header
            header_pos = buffer.find(b'$X')
            if header_pos < 0:
                buffer.clear()
                return
            
            if header_pos > 0:
                del buffer[:header_pos]
            
            if len(buffer) < 9:
                return
            
            # Parse MSP V2 frame
            direction = buffer[2]
            flag = buffer[3]
            func_id = struct.unpack('<H', buffer[4:6])[0]
            payload_size = struct.unpack('<H', buffer[6:8])[0]
            
            total_size = 8 + payload_size + 1  # header + payload + crc
            
            if len(buffer) < total_size:
                return
            
            payload = buffer[8:8 + payload_size]
            
            # Process based on function ID
            if func_id == MSP_SENSOR_OPTICAL_FLOW and payload_size >= 9:
                self._parse_optical_flow(payload)
            elif func_id == MSP_SENSOR_RANGEFINDER and payload_size >= 5:
                self._parse_rangefinder(payload)
            
            del buffer[:total_size]
    
    def _parse_optical_flow(self, payload: bytes):
        """Parse optical flow MSP payload"""
        try:
            quality = payload[0]
            flow_x = struct.unpack('<i', payload[1:5])[0]  # flow rate X (1/10 deg/s)
            flow_y = struct.unpack('<i', payload[5:9])[0]  # flow rate Y (1/10 deg/s)
            
            with self._data_lock:
                self._last_data.flow_x = flow_x / 10.0 * 0.0174533  # deg/s to rad/s
                self._last_data.flow_y = flow_y / 10.0 * 0.0174533
                self._last_data.quality = quality
                self._last_data.timestamp = time.time()
                
        except Exception as e:
            logger.error(f"Flow parse error: {e}")
    
    def _parse_rangefinder(self, payload: bytes):
        """Parse rangefinder MSP payload"""
        try:
            quality = payload[0]
            distance = struct.unpack('<i', payload[1:5])[0]  # mm
            
            with self._data_lock:
                self._last_data.distance_mm = distance
                
        except Exception as e:
            logger.error(f"Rangefinder parse error: {e}")
    
    @property
    def latest_data(self) -> FlowData:
        """Get latest sensor data"""
        with self._data_lock:
            return FlowData(
                flow_x=self._last_data.flow_x,
                flow_y=self._last_data.flow_y,
                quality=self._last_data.quality,
                distance_mm=self._last_data.distance_mm,
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
            return age < 1.0  # Data must be less than 1 second old
