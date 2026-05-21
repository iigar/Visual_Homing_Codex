"""
Smart RTL (Return-to-Launch) Module
Гібридна навігація: IMU/Baro на великій висоті + Optical Flow/Visual на малій висоті

Flight Strategy for 5km at 200m altitude:
1. PHASE_HIGH_ALT: RTL initiated -> fly toward home using ArduPilot IMU/Baro (>50m)
2. PHASE_DESCENT: After 50% return path covered, begin gradual descent
3. PHASE_LOW_ALT: Below 50m -> switch to Optical Flow + Visual navigation
4. PHASE_PRECISION_LAND: Below 5m -> precision landing using Optical Flow + LiDAR
"""
import time
import math
import logging
from typing import Optional
from dataclasses import dataclass
from enum import Enum

logger = logging.getLogger(__name__)


class SmartRTLPhase(Enum):
    IDLE = "idle"
    HIGH_ALT = "high_alt"           # >50m: IMU/Baro navigation (ArduPilot handles)
    DESCENT = "descent"              # Gradual descent phase
    LOW_ALT = "low_alt"             # <50m: Optical Flow + Visual
    PRECISION_LAND = "precision_land"  # <5m: Final landing approach
    COMPLETED = "completed"
    ERROR = "error"


@dataclass
class SmartRTLConfig:
    """Configuration for Smart RTL"""
    # Altitude thresholds (meters)
    high_alt_threshold: float = 50.0      # Switch to visual/flow below this
    precision_land_threshold: float = 5.0  # Switch to precision landing
    
    # Descent strategy
    descent_start_pct: float = 0.5        # Start descending after 50% return path
    descent_rate: float = 2.0             # m/s descent rate during descent phase
    max_descent_rate: float = 3.0         # Maximum descent rate
    
    # Speed settings
    high_alt_speed: float = 10.0          # m/s at high altitude
    low_alt_speed: float = 3.0            # m/s at low altitude
    precision_speed: float = 0.5          # m/s during precision landing
    
    # Navigation source trust
    flow_min_quality: int = 50            # Minimum Optical Flow quality
    visual_min_confidence: float = 0.3    # Minimum Visual Homing confidence
    
    # Safety
    min_altitude: float = 0.3             # Minimum altitude before disarm
    max_horizontal_error: float = 2.0     # Max error for landing (meters)
    failsafe_alt: float = 15.0           # Climb to this if nav lost


@dataclass
class SmartRTLState:
    """Current state of Smart RTL"""
    phase: SmartRTLPhase = SmartRTLPhase.IDLE
    current_altitude: float = 0.0
    home_distance: float = 0.0           # Horizontal distance to home (m)
    total_return_distance: float = 0.0   # Total return path length
    distance_covered: float = 0.0        # Distance already covered
    return_progress: float = 0.0         # 0.0 - 1.0
    nav_source: str = "imu"              # Current navigation source
    flow_quality: int = 0
    visual_confidence: float = 0.0
    lidar_distance: float = 0.0
    target_altitude: float = 0.0
    timestamp: float = 0.0


class SmartRTL:
    """
    Smart Return-to-Launch controller.
    
    Implements hybrid navigation strategy:
    - HIGH ALT (>50m): ArduPilot's built-in IMU/Baro navigation
    - DESCENT: Gradual altitude reduction after 50% path covered
    - LOW ALT (<50m): Optical Flow + Visual Homing for precision
    - PRECISION LAND (<5m): LiDAR-aided landing
    """
    
    def __init__(self, config: SmartRTLConfig = None):
        self.config = config or SmartRTLConfig()
        self._state = SmartRTLState()
        self._start_position = None  # (lat, lon, alt) at RTL initiation
        self._home_position = None   # (lat, lon, alt) home point
        self._active = False
        self._start_time: float = 0.0
    
    def initiate_rtl(
        self,
        current_altitude: float,
        home_distance: float,
        home_position: tuple = (0.0, 0.0, 0.0),
        current_position: tuple = (0.0, 0.0, 0.0)
    ) -> SmartRTLPhase:
        """
        Initiate Smart RTL.
        
        Args:
            current_altitude: Current altitude in meters
            home_distance: Horizontal distance to home in meters
            home_position: Home (lat, lon, alt)
            current_position: Current (lat, lon, alt)
            
        Returns:
            Initial phase
        """
        self._home_position = home_position
        self._start_position = current_position
        self._active = True
        self._start_time = time.time()
        
        self._state.current_altitude = current_altitude
        self._state.home_distance = home_distance
        self._state.total_return_distance = home_distance
        self._state.distance_covered = 0.0
        self._state.return_progress = 0.0
        self._state.timestamp = time.time()
        
        # Determine initial phase
        if current_altitude > self.config.high_alt_threshold:
            self._state.phase = SmartRTLPhase.HIGH_ALT
            self._state.nav_source = "imu_baro"
            self._state.target_altitude = current_altitude
            logger.info(f"Smart RTL initiated: HIGH_ALT phase at {current_altitude:.1f}m, "
                       f"distance to home: {home_distance:.0f}m")
        else:
            self._state.phase = SmartRTLPhase.LOW_ALT
            self._state.nav_source = "optical_flow_visual"
            self._state.target_altitude = current_altitude
            logger.info(f"Smart RTL initiated: LOW_ALT phase at {current_altitude:.1f}m")
        
        return self._state.phase
    
    def update(
        self,
        altitude: float,
        home_distance: float,
        flow_quality: int = 0,
        visual_confidence: float = 0.0,
        lidar_distance: float = 0.0,
        groundspeed: float = 0.0
    ) -> SmartRTLState:
        """
        Update Smart RTL state with current telemetry.
        
        Args:
            altitude: Current altitude (meters)
            home_distance: Horizontal distance to home (meters)
            flow_quality: Optical Flow quality (0-255)
            visual_confidence: Visual Homing confidence (0-1)
            lidar_distance: LiDAR ground distance (meters)
            groundspeed: Current ground speed (m/s)
            
        Returns:
            Updated state
        """
        if not self._active:
            return self._state
        
        prev_distance = self._state.home_distance
        self._state.current_altitude = altitude
        self._state.home_distance = home_distance
        self._state.flow_quality = flow_quality
        self._state.visual_confidence = visual_confidence
        self._state.lidar_distance = lidar_distance
        self._state.timestamp = time.time()
        
        # Calculate return progress
        if self._state.total_return_distance > 0:
            distance_covered = self._state.total_return_distance - home_distance
            self._state.distance_covered = max(0, distance_covered)
            self._state.return_progress = min(1.0, self._state.distance_covered / self._state.total_return_distance)
        
        # Phase transitions
        self._update_phase(altitude, home_distance, flow_quality, visual_confidence)
        
        return self._state
    
    def _update_phase(
        self,
        altitude: float,
        home_distance: float,
        flow_quality: int,
        visual_confidence: float
    ):
        """Update phase based on current conditions"""
        phase = self._state.phase
        
        if phase == SmartRTLPhase.HIGH_ALT:
            # Check if we should start descending
            if self._state.return_progress >= self.config.descent_start_pct:
                self._state.phase = SmartRTLPhase.DESCENT
                self._state.nav_source = "imu_baro"
                logger.info(f"Smart RTL: DESCENT phase started at {self._state.return_progress*100:.0f}% return")
            
        elif phase == SmartRTLPhase.DESCENT:
            # Calculate target altitude based on remaining distance
            remaining_pct = 1.0 - self._state.return_progress
            if remaining_pct > 0:
                # Linear descent from current to threshold
                target_alt = self.config.high_alt_threshold * (remaining_pct / (1.0 - self.config.descent_start_pct))
                self._state.target_altitude = max(self.config.high_alt_threshold * 0.5, target_alt)
            
            # Switch to LOW_ALT when below threshold
            if altitude <= self.config.high_alt_threshold:
                self._state.phase = SmartRTLPhase.LOW_ALT
                self._state.nav_source = "optical_flow_visual"
                logger.info(f"Smart RTL: LOW_ALT phase at {altitude:.1f}m")
        
        elif phase == SmartRTLPhase.LOW_ALT:
            # Use optical flow + visual navigation
            has_flow = flow_quality >= self.config.flow_min_quality
            has_visual = visual_confidence >= self.config.visual_min_confidence
            
            if has_flow:
                self._state.nav_source = "optical_flow" + ("_visual" if has_visual else "")
            elif has_visual:
                self._state.nav_source = "visual_only"
            else:
                self._state.nav_source = "imu_baro_fallback"
                logger.warning("Smart RTL: No optical/visual nav available, using IMU fallback")
            
            # Switch to precision landing
            if altitude <= self.config.precision_land_threshold and home_distance < 10.0:
                self._state.phase = SmartRTLPhase.PRECISION_LAND
                self._state.nav_source = "optical_flow_lidar"
                logger.info(f"Smart RTL: PRECISION_LAND at {altitude:.1f}m, {home_distance:.1f}m from home")
        
        elif phase == SmartRTLPhase.PRECISION_LAND:
            # Final approach - use LiDAR for height
            if altitude <= self.config.min_altitude:
                self._state.phase = SmartRTLPhase.COMPLETED
                logger.info("Smart RTL: COMPLETED - touchdown")
    
    def get_velocity_command(self) -> dict:
        """
        Get velocity command based on current phase.
        
        Returns:
            dict with vx, vy, vz, yaw_rate for flight controller
        """
        if not self._active:
            return {"vx": 0, "vy": 0, "vz": 0, "yaw_rate": 0}
        
        phase = self._state.phase
        
        if phase == SmartRTLPhase.HIGH_ALT:
            # ArduPilot handles navigation, we just monitor
            return {"vx": 0, "vy": 0, "vz": 0, "yaw_rate": 0, "autopilot_control": True}
        
        elif phase == SmartRTLPhase.DESCENT:
            # ArduPilot navigates horizontally, we command descent
            target_vz = self.config.descent_rate
            if self._state.current_altitude - self._state.target_altitude < 5:
                target_vz *= 0.5  # Slow down near target
            
            return {
                "vx": 0, "vy": 0,
                "vz": min(target_vz, self.config.max_descent_rate),
                "yaw_rate": 0,
                "autopilot_control": True,  # AP still handles XY
                "target_altitude": self._state.target_altitude
            }
        
        elif phase == SmartRTLPhase.LOW_ALT:
            # We control navigation using Optical Flow + Visual
            speed = self.config.low_alt_speed
            return {
                "vx": 0, "vy": 0,  # Set by route_follower
                "vz": 0.5,  # Gentle descent
                "yaw_rate": 0,
                "autopilot_control": False,
                "max_speed": speed
            }
        
        elif phase == SmartRTLPhase.PRECISION_LAND:
            descent_rate = 0.3 if self._state.lidar_distance > 1.0 else 0.15
            return {
                "vx": 0, "vy": 0,
                "vz": descent_rate,
                "yaw_rate": 0,
                "autopilot_control": False,
                "max_speed": self.config.precision_speed
            }
        
        return {"vx": 0, "vy": 0, "vz": 0, "yaw_rate": 0}
    
    def abort(self):
        """Abort Smart RTL and enter failsafe"""
        logger.warning("Smart RTL ABORTED")
        self._state.phase = SmartRTLPhase.ERROR
        self._active = False
    
    @property
    def is_active(self) -> bool:
        return self._active
    
    @property
    def state(self) -> SmartRTLState:
        return self._state
    
    @property
    def phase(self) -> SmartRTLPhase:
        return self._state.phase
