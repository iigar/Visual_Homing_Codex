"""
ArduPilot MAVLink Interface
MAVLink комунікація з політним контролером ArduPilot
"""
import threading
import time
import logging
from typing import Optional, Callable, Dict
from dataclasses import dataclass
import struct

# Try to import pymavlink
try:
    from pymavlink import mavutil
    MAVLINK_AVAILABLE = True
except ImportError:
    MAVLINK_AVAILABLE = False

logger = logging.getLogger(__name__)


@dataclass
class VehicleState:
    """Current state from flight controller"""
    armed: bool = False
    mode: str = "UNKNOWN"
    altitude: float = 0.0        # meters (barometer)
    altitude_rel: float = 0.0    # meters above home
    heading: float = 0.0         # degrees
    groundspeed: float = 0.0     # m/s
    lat: float = 0.0
    lon: float = 0.0
    gps_fix: int = 0
    battery_voltage: float = 0.0
    battery_remaining: int = 100
    timestamp: float = 0.0


class ArduPilotInterface:
    """
    MAVLink interface for ArduPilot/ArduCopter
    Sends visual navigation data and receives telemetry
    """
    
    def __init__(
        self,
        serial_port: str = "/dev/serial0",
        baudrate: int = 115200,
        source_system: int = 1,
        source_component: int = 191
    ):
        self.serial_port = serial_port
        self.baudrate = baudrate
        self.source_system = source_system
        self.source_component = source_component
        
        self._connection = None
        self._connected = False
        self._running = False
        self._recv_thread: Optional[threading.Thread] = None
        self._heartbeat_thread: Optional[threading.Thread] = None
        
        self._vehicle_state = VehicleState()
        self._state_lock = threading.Lock()
        self._callbacks: Dict[str, list] = {}

        # Store attitude separately for web interface
        self._attitude = {'roll': 0.0, 'pitch': 0.0, 'yaw': 0.0}
        self._gps_satellites = 0

        self._baro_press: float = 0.0
        self._baro_at_arm: float = 0.0
        self._was_armed: bool = False
        # EKF-fused position from LOCAL_POSITION_NED
        self._ned_z: float = 0.0
        self._ned_x: float = 0.0
        self._ned_y: float = 0.0
        self._ned_z_valid: bool = False
        self._ned_xy_valid: bool = False
        self._ned_z_last_update: float = 0.0
        self._NED_Z_TIMEOUT: float = 2.0  # seconds before fallback to baro
    
    def connect(self, timeout: float = 10.0) -> bool:
        """
        Connect to flight controller via MAVLink
        
        Returns:
            True if connection successful
        """
        if not MAVLINK_AVAILABLE:
            logger.error("pymavlink not available")
            return False
        
        try:
            logger.info(f"Connecting to {self.serial_port} at {self.baudrate}")
            
            self._connection = mavutil.mavlink_connection(
                self.serial_port,
                baud=self.baudrate,
                source_system=self.source_system,
                source_component=self.source_component
            )
            
            # Wait for heartbeat
            logger.info("Waiting for heartbeat...")
            self._connection.wait_heartbeat(timeout=timeout)
            
            logger.info(f"Connected to system {self._connection.target_system}")
            
            self._connected = True
            self._running = True
            
            # Request data streams from FC
            self._request_data_streams()
            
            # Start receive thread
            self._recv_thread = threading.Thread(
                target=self._receive_loop,
                daemon=True
            )
            self._recv_thread.start()
            
            # Start heartbeat thread
            self._heartbeat_thread = threading.Thread(
                target=self._heartbeat_loop,
                daemon=True
            )
            self._heartbeat_thread.start()
            
            return True
            
        except Exception as e:
            logger.error(f"Connection failed: {e}")
            self._connected = False
            return False
    
    def _request_data_streams(self):
        """Request data streams from flight controller"""
        try:
            # Request ATTITUDE stream (roll, pitch, yaw)
            self._connection.mav.request_data_stream_send(
                self._connection.target_system,
                self._connection.target_component,
                mavutil.mavlink.MAV_DATA_STREAM_EXTRA1,  # Attitude
                10,  # 10 Hz
                1    # Start
            )
            
            # Request POSITION stream (altitude, etc)
            self._connection.mav.request_data_stream_send(
                self._connection.target_system,
                self._connection.target_component,
                mavutil.mavlink.MAV_DATA_STREAM_POSITION,
                5,   # 5 Hz
                1    # Start
            )
            
            # Request EXTRA2 stream (VFR_HUD)
            self._connection.mav.request_data_stream_send(
                self._connection.target_system,
                self._connection.target_component,
                mavutil.mavlink.MAV_DATA_STREAM_EXTRA2,
                5,   # 5 Hz
                1    # Start
            )

            # Request RAW_SENSORS stream (SCALED_PRESSURE — raw baro, EKF-independent)
            self._connection.mav.request_data_stream_send(
                self._connection.target_system,
                self._connection.target_component,
                mavutil.mavlink.MAV_DATA_STREAM_RAW_SENSORS,
                2,   # 2 Hz sufficient for altitude
                1    # Start
            )

            # LOCAL_POSITION_NED (msg_id=32) is not in any MAV_DATA_STREAM group.
            # Must be requested explicitly via SET_MESSAGE_INTERVAL.
            self._connection.mav.command_long_send(
                self._connection.target_system,
                self._connection.target_component,
                mavutil.mavlink.MAV_CMD_SET_MESSAGE_INTERVAL,
                0,
                32,       # LOCAL_POSITION_NED
                200000,   # 5 Hz = 200000 μs
                0, 0, 0, 0, 0
            )

            logger.info("Requested data streams from FC")
        except Exception as e:
            logger.warning(f"Failed to request data streams: {e}")
    
    def disconnect(self):
        """Disconnect from flight controller"""
        self._running = False
        self._connected = False
        
        if self._recv_thread:
            self._recv_thread.join(timeout=2.0)
        
        if self._heartbeat_thread:
            self._heartbeat_thread.join(timeout=2.0)
        
        if self._connection:
            self._connection.close()
            self._connection = None
        
        logger.info("Disconnected from MAVLink")
    
    def _receive_loop(self):
        """Receive messages from flight controller"""
        while self._running:
            try:
                msg = self._connection.recv_match(blocking=True, timeout=1.0)
                
                if msg is not None:
                    self._process_message(msg)
                    
            except Exception as e:
                if not hasattr(self, '_recv_err_count'):
                    self._recv_err_count = 0
                self._recv_err_count += 1
                if self._recv_err_count <= 3 or self._recv_err_count % 100 == 0:
                    logger.error(f"Receive error #{self._recv_err_count}: {e}")
                time.sleep(0.1)
    
    def _heartbeat_loop(self):
        """Send periodic heartbeats"""
        while self._running:
            try:
                self._connection.mav.heartbeat_send(
                    mavutil.mavlink.MAV_TYPE_ONBOARD_CONTROLLER,
                    mavutil.mavlink.MAV_AUTOPILOT_INVALID,
                    0, 0, 0
                )
                time.sleep(1.0)
            except Exception as e:
                logger.error(f"Heartbeat error: {e}")
                time.sleep(1.0)
    
    def _process_message(self, msg):
        """Process received MAVLink message"""
        msg_type = msg.get_type()
        
        with self._state_lock:
            if msg_type == 'HEARTBEAT':
                is_armed = bool(msg.base_mode & mavutil.mavlink.MAV_MODE_FLAG_SAFETY_ARMED)
                if is_armed and not self._was_armed:
                    self._baro_at_arm = self._baro_press
                    logger.info(f"Armed: baro pressure = {self._baro_at_arm:.2f} hPa")
                elif not is_armed:
                    self._baro_at_arm = 0.0
                self._was_armed = is_armed
                self._vehicle_state.armed = is_armed
                self._vehicle_state.mode = mavutil.mode_string_v10(msg)

            elif msg_type == 'SCALED_PRESSURE':
                raw = msg.press_abs
                # EMA filter — reduces prop-wash noise without lag at 2 Hz update rate
                self._baro_press = 0.3 * raw + 0.7 * self._baro_press if self._baro_press > 0 else raw

            elif msg_type == 'LOCAL_POSITION_NED':
                if not self._ned_xy_valid:
                    logger.info(f"LOCAL_POSITION_NED first received: x={msg.x:.2f}, y={msg.y:.2f}, z={msg.z:.2f}")
                self._ned_z = msg.z      # NED: down is positive → height = -z
                self._ned_x = msg.x      # NED north
                self._ned_y = msg.y      # NED east
                self._ned_z_valid = True
                self._ned_xy_valid = True
                self._ned_z_last_update = time.time()
                
            elif msg_type == 'GLOBAL_POSITION_INT':
                self._vehicle_state.lat = msg.lat / 1e7
                self._vehicle_state.lon = msg.lon / 1e7
                # altitude and altitude_rel NOT updated here — GLOBAL_POSITION_INT
                # reports MSL altitude (~91m at ground) when there is no GPS fix.
                # VFR_HUD.alt is used instead (altitude above home, correct for VO).
                self._vehicle_state.heading = msg.hdg / 100.0
                self._vehicle_state.timestamp = time.time()
                
            elif msg_type == 'VFR_HUD':
                self._vehicle_state.groundspeed = msg.groundspeed
                self._vehicle_state.altitude = msg.alt
                
            elif msg_type == 'GPS_RAW_INT':
                self._vehicle_state.gps_fix = msg.fix_type
                self._gps_satellites = msg.satellites_visible if hasattr(msg, 'satellites_visible') else 0
                
            elif msg_type == 'ATTITUDE':
                # Store attitude for web interface
                self._attitude = {
                    'roll': msg.roll,
                    'pitch': msg.pitch,
                    'yaw': msg.yaw
                }
                
            elif msg_type == 'BATTERY_STATUS':
                if msg.voltages[0] != 65535:
                    self._vehicle_state.battery_voltage = msg.voltages[0] / 1000.0
                self._vehicle_state.battery_remaining = msg.battery_remaining
        
        # Call registered callbacks
        if msg_type in self._callbacks:
            for callback in self._callbacks[msg_type]:
                try:
                    callback(msg)
                except Exception as e:
                    logger.error(f"Callback error: {e}")
    
    def send_vision_position(
        self,
        x: float,
        y: float,
        z: float,
        roll: float = 0.0,
        pitch: float = 0.0,
        yaw: float = 0.0,
        confidence: float = 0.95
    ):
        """
        Send vision position estimate to ArduPilot
        Uses VISION_POSITION_ESTIMATE message
        
        Args:
            x, y, z: Position in NED frame (meters)
            roll, pitch, yaw: Attitude (radians)
            confidence: Confidence level (0-1)
        """
        if not self._connected:
            return
        
        try:
            # Standard MAVLink VISION_POSITION_ESTIMATE message
            # Parameters: usec, x, y, z, roll, pitch, yaw
            # Note: covariance and reset_counter are optional in MAVLink v2
            # and may not be supported by all pymavlink versions
            self._connection.mav.vision_position_estimate_send(
                int(time.time() * 1e6),  # usec timestamp
                x, y, z,
                roll, pitch, yaw
            )
            
        except Exception as e:
            logger.error(f"Send vision position error: {e}")
    
    def send_vision_speed(
        self,
        vx: float,
        vy: float,
        vz: float,
        confidence: float = 0.95
    ):
        """
        Send vision speed estimate to ArduPilot
        Uses VISION_SPEED_ESTIMATE message
        """
        if not self._connected:
            return
        
        try:
            # Standard MAVLink VISION_SPEED_ESTIMATE message
            # Parameters: usec, vx, vy, vz
            # Note: covariance and reset_counter are optional in MAVLink v2
            self._connection.mav.vision_speed_estimate_send(
                int(time.time() * 1e6),
                vx, vy, vz
            )
        except Exception as e:
            logger.error(f"Send vision speed error: {e}")
    
    def send_velocity_command(
        self,
        vx: float,
        vy: float,
        vz: float,
        yaw_rate: float = 0.0
    ):
        """
        Send velocity command to ArduPilot (guided mode)
        Uses SET_POSITION_TARGET_LOCAL_NED
        """
        if not self._connected:
            return
        
        try:
            # Type mask: ignore position, use velocity
            type_mask = (
                mavutil.mavlink.POSITION_TARGET_TYPEMASK_X_IGNORE |
                mavutil.mavlink.POSITION_TARGET_TYPEMASK_Y_IGNORE |
                mavutil.mavlink.POSITION_TARGET_TYPEMASK_Z_IGNORE |
                mavutil.mavlink.POSITION_TARGET_TYPEMASK_AX_IGNORE |
                mavutil.mavlink.POSITION_TARGET_TYPEMASK_AY_IGNORE |
                mavutil.mavlink.POSITION_TARGET_TYPEMASK_AZ_IGNORE |
                mavutil.mavlink.POSITION_TARGET_TYPEMASK_YAW_IGNORE
            )
            
            self._connection.mav.set_position_target_local_ned_send(
                0,  # time_boot_ms
                self._connection.target_system,
                self._connection.target_component,
                mavutil.mavlink.MAV_FRAME_BODY_NED,
                type_mask,
                0, 0, 0,      # position (ignored)
                vx, vy, vz,   # velocity
                0, 0, 0,      # acceleration (ignored)
                0,            # yaw (ignored)
                yaw_rate      # yaw_rate
            )
        except Exception as e:
            logger.error(f"Send velocity command error: {e}")
    
    def register_callback(self, msg_type: str, callback: Callable):
        """Register callback for specific message type"""
        if msg_type not in self._callbacks:
            self._callbacks[msg_type] = []
        self._callbacks[msg_type].append(callback)
    
    @property
    def vehicle_state(self) -> VehicleState:
        """Get current vehicle state"""
        with self._state_lock:
            return self._vehicle_state
    
    @property
    def is_connected(self) -> bool:
        return self._connected
    
    @property
    def ned_x(self) -> Optional[float]:
        """EKF north position (m). None until first LOCAL_POSITION_NED received."""
        with self._state_lock:
            return self._ned_x if self._ned_xy_valid else None

    @property
    def ned_y(self) -> Optional[float]:
        """EKF east position (m). None until first LOCAL_POSITION_NED received."""
        with self._state_lock:
            return self._ned_y if self._ned_xy_valid else None

    @property
    def altitude(self) -> float:
        """AGL altitude: LOCAL_POSITION_NED.z (EKF-fused, motor-wash filtered).
        Falls back to raw baro if EKF height not yet available."""
        with self._state_lock:
            ned_fresh = self._ned_z_valid and (time.time() - self._ned_z_last_update) < self._NED_Z_TIMEOUT
            if ned_fresh:
                return max(0.0, -self._ned_z)  # NED z down → height = -z
            if self._baro_at_arm > 0 and self._baro_press > 0:
                return (self._baro_at_arm - self._baro_press) / 0.12
            return 0.0
