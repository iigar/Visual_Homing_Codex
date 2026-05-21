"""
Visual Homing Configuration
Конфігурація системи візуальної навігації
"""
import os
from dataclasses import dataclass
from enum import Enum


class CameraType(Enum):
    USB_CAPTURE = "usb"      # EasyCap USB (Caddx Thermal)
    PI_CAMERA = "picamera"   # Raspberry Pi Camera CSI


class SensorType(Enum):
    NONE = "none"
    MATEK_3901_L0X = "matek_3901_l0x"    # Optical Flow + VL53L0X LiDAR
    TF_LUNA = "tf_luna"                     # Benewake TF-Luna LiDAR


class SystemState(Enum):
    IDLE = "idle"
    RECORDING = "recording"
    RETURNING = "returning"
    SMART_RTL = "smart_rtl"
    ERROR = "error"


@dataclass
class CameraConfig:
    type: CameraType = CameraType.USB_CAPTURE
    device: str = "/dev/video0"
    width: int = 720
    height: int = 576
    fps: int = 25
    # Pi Camera specific
    pi_width: int = 640
    pi_height: int = 480
    pi_fps: int = 30


@dataclass
class VisionConfig:
    # ORB Feature Detector
    orb_features: int = 500
    orb_scale_factor: float = 1.2
    orb_nlevels: int = 8
    
    # Feature Matching
    match_threshold: float = 60.0
    min_matches: int = 10
    
    # Keyframe Settings
    keyframe_distance: float = 2.0    # meters
    keyframe_angle: float = 15.0      # degrees
    keyframe_min_features: int = 50
    
    # Visual Odometry
    vo_min_displacement: float = 0.1  # meters


@dataclass
class OpticalFlowConfig:
    enabled: bool = True
    sensor_type: str = "matek_3901_l0x"
    serial_port: str = "/dev/serial1"
    baudrate: int = 115200
    # ArduPilot parameters
    flow_type: int = 7                # MSP protocol
    flow_fxscaler: int = -800
    flow_fyscaler: int = -800


@dataclass
class LidarConfig:
    enabled: bool = True
    sensor_type: str = "tf_luna"
    serial_port: str = "/dev/serial2"
    baudrate: int = 115200
    # ArduPilot parameters
    rngfnd_type: int = 20             # Benewake-Serial
    rngfnd_min_cm: int = 20           # 0.2m
    rngfnd_max_cm: int = 800          # 8.0m


@dataclass
class SmartRTLConfig:
    # Altitude thresholds
    high_alt_threshold: float = 50.0  # meters - switch to visual below
    precision_land_alt: float = 5.0   # meters - precision landing
    # Descent strategy
    descent_start_pct: float = 0.5    # Start descent after 50% return
    descent_rate: float = 2.0         # m/s
    # Speed limits
    high_alt_speed: float = 10.0      # m/s
    low_alt_speed: float = 3.0        # m/s
    precision_speed: float = 0.5      # m/s
    # Navigation source trust
    flow_min_quality: int = 50
    visual_min_confidence: float = 0.3


@dataclass
class NavigationConfig:
    # Return Navigation
    return_speed: float = 2.0         # m/s
    approach_threshold: float = 1.0   # meters to keyframe
    heading_tolerance: float = 10.0   # degrees
    
    # Position Estimation
    position_smoothing: float = 0.8
    max_position_jump: float = 5.0    # meters
    
    # Smart RTL
    smart_rtl: SmartRTLConfig = None
    
    def __post_init__(self):
        if self.smart_rtl is None:
            self.smart_rtl = SmartRTLConfig()


@dataclass
class MAVLinkConfig:
    serial_port: str = "/dev/serial0"
    baudrate: int = 115200
    source_system: int = 1
    source_component: int = 191  # MAV_COMP_ID_VISUAL_INERTIAL_ODOMETRY
    
    # Message rates
    position_rate_hz: float = 20.0
    heartbeat_rate_hz: float = 1.0
    
    # External Navigation
    viso_delay_ms: int = 80


@dataclass
class WebConfig:
    host: str = "0.0.0.0"
    port: int = 5000
    debug: bool = False
    enable_video_stream: bool = True
    video_quality: int = 50  # JPEG quality


@dataclass
class Config:
    camera: CameraConfig = None
    vision: VisionConfig = None
    navigation: NavigationConfig = None
    mavlink: MAVLinkConfig = None
    web: WebConfig = None
    optical_flow: OpticalFlowConfig = None
    lidar: LidarConfig = None
    
    # Data paths
    data_dir: str = "/home/pi/visual_homing/data"
    route_dir: str = "/home/pi/visual_homing/routes"
    log_dir: str = "/home/pi/visual_homing/logs"
    
    # System
    autostart: bool = False
    test_mode: bool = False
    
    def __post_init__(self):
        if self.camera is None:
            self.camera = CameraConfig()
        if self.vision is None:
            self.vision = VisionConfig()
        if self.navigation is None:
            self.navigation = NavigationConfig()
        if self.mavlink is None:
            self.mavlink = MAVLinkConfig()
        if self.web is None:
            self.web = WebConfig()
        if self.optical_flow is None:
            self.optical_flow = OpticalFlowConfig()
        if self.lidar is None:
            self.lidar = LidarConfig()
    
    @classmethod
    def from_env(cls) -> 'Config':
        """Load configuration from environment variables"""
        config = cls()
        
        # Camera
        if os.environ.get('CAMERA_TYPE') == 'picamera':
            config.camera.type = CameraType.PI_CAMERA
        if os.environ.get('CAMERA_DEVICE'):
            config.camera.device = os.environ['CAMERA_DEVICE']
        
        # MAVLink
        if os.environ.get('MAVLINK_PORT'):
            config.mavlink.serial_port = os.environ['MAVLINK_PORT']
        if os.environ.get('MAVLINK_BAUD'):
            config.mavlink.baudrate = int(os.environ['MAVLINK_BAUD'])
        
        # Optical Flow
        if os.environ.get('FLOW_SERIAL'):
            config.optical_flow.serial_port = os.environ['FLOW_SERIAL']
        if os.environ.get('FLOW_ENABLED') == '0':
            config.optical_flow.enabled = False
        
        # LiDAR
        if os.environ.get('LIDAR_SERIAL'):
            config.lidar.serial_port = os.environ['LIDAR_SERIAL']
        if os.environ.get('LIDAR_ENABLED') == '0':
            config.lidar.enabled = False
        
        # Smart RTL thresholds
        if os.environ.get('SMART_RTL_ALT'):
            config.navigation.smart_rtl.high_alt_threshold = float(os.environ['SMART_RTL_ALT'])
        if os.environ.get('DESCENT_START_PCT'):
            config.navigation.smart_rtl.descent_start_pct = float(os.environ['DESCENT_START_PCT'])
        
        # Web
        if os.environ.get('WEB_PORT'):
            config.web.port = int(os.environ['WEB_PORT'])
        
        return config


# Default configuration instance
default_config = Config()
