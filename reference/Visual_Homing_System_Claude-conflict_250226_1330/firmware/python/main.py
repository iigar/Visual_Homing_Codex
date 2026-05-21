"""
Visual Homing - Main Entry Point
Головний файл запуску системи візуальної навігації
"""
import argparse
import logging
import signal
import sys
import time
import os
from pathlib import Path
from threading import Event

# Add current directory to path for imports
sys.path.insert(0, str(Path(__file__).parent))

from config import Config, CameraType, SystemState
from camera import USBCapture, PiCamera
from vision import VisualOdometry, Pose
from navigation import RouteRecorder, RouteFollower
from mavlink import ArduPilotInterface

# Configure logging
log_dir = Path('/home/pi/visual_homing/logs')
log_dir.mkdir(parents=True, exist_ok=True)
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(name)s - %(levelname)s - %(message)s',
    handlers=[
        logging.StreamHandler(sys.stdout),
        logging.FileHandler(log_dir / 'visual_homing.log')
    ]
)
logger = logging.getLogger('visual_homing')


class VisualHomingSystem:
    """
    Main Visual Homing System
    Integrates all components for visual navigation
    """
    
    def __init__(self, config: Config):
        self.config = config
        self.state = SystemState.IDLE
        self._shutdown_event = Event()
        
        # Initialize components
        self._init_camera()
        self._init_vision()
        self._init_navigation()
        self._init_mavlink()
        
        # Current pose from visual odometry
        self._current_pose = Pose()
        self._current_altitude = 0.1
        self._last_valid_pose_time: float = 0.0
        self._prev_vo_healthy: bool = False
        # Last position actually sent to EKF — used for resync on VO recovery
        self._last_sent_x: float = 0.0
        self._last_sent_y: float = 0.0
    
    def _init_camera(self):
        """Initialize camera based on config"""
        if self.config.camera.type == CameraType.USB_CAPTURE:
            self.camera = USBCapture(
                device=self.config.camera.device,
                width=self.config.camera.width,
                height=self.config.camera.height,
                fps=self.config.camera.fps
            )
        else:
            self.camera = PiCamera(
                width=self.config.camera.pi_width,
                height=self.config.camera.pi_height,
                fps=self.config.camera.pi_fps
            )
        logger.info(f"Camera initialized: {self.config.camera.type}")
    
    def _init_vision(self):
        """Initialize visual odometry"""
        self.vo = VisualOdometry(
            n_features=self.config.vision.orb_features,
            min_displacement=self.config.vision.vo_min_displacement
        )
        logger.info("Visual odometry initialized")
    
    def _init_navigation(self):
        """Initialize navigation components"""
        self.route_recorder = RouteRecorder(
            route_dir=self.config.route_dir,
            keyframe_distance=self.config.vision.keyframe_distance,
            keyframe_angle=self.config.vision.keyframe_angle,
            min_features=self.config.vision.keyframe_min_features
        )
        
        self.route_follower = RouteFollower(
            route_dir=self.config.route_dir,
            return_speed=self.config.navigation.return_speed,
            approach_threshold=self.config.navigation.approach_threshold
        )
        logger.info("Navigation components initialized")
    
    def _init_mavlink(self):
        """Initialize MAVLink interface"""
        self.mavlink = ArduPilotInterface(
            serial_port=self.config.mavlink.serial_port,
            baudrate=self.config.mavlink.baudrate,
            source_system=self.config.mavlink.source_system,
            source_component=self.config.mavlink.source_component
        )
        logger.info("MAVLink interface initialized")
    
    def start(self):
        """Start the visual homing system"""
        logger.info("Starting Visual Homing System...")
        
        # Start camera
        if not self.camera.start():
            logger.error("Failed to start camera")
            return False
        
        # Connect to MAVLink (optional in test mode)
        if not self.config.test_mode:
            if not self.mavlink.connect(timeout=15.0):
                logger.warning("MAVLink connection failed - running without FC")
        
        # Register frame callback
        self.camera.register_callback(self._on_frame)
        
        logger.info("System started successfully")
        return True
    
    def stop(self):
        """Stop the system"""
        logger.info("Stopping Visual Homing System...")
        
        self._shutdown_event.set()
        
        # Stop recording if active
        if self.route_recorder.is_recording:
            self.route_recorder.stop_recording()
        
        # Stop following if active
        if self.route_follower.is_active:
            self.route_follower.stop_following()
        
        # Stop components
        self.camera.stop()
        self.mavlink.disconnect()
        
        logger.info("System stopped")
    
    def _on_frame(self, frame, frame_info):
        """
        Callback for each camera frame
        Main processing loop
        """
        # Update altitude from MAVLink
        if self.mavlink.is_connected:
            self._current_altitude = self.mavlink.altitude
            if self._current_altitude < 0.1:
                self._current_altitude = 0.1
        
        # Update visual odometry
        self.vo.set_altitude(self._current_altitude)
        pose, velocity = self.vo.process_frame(frame, frame_info.timestamp)
        
        if pose:
            self._current_pose = pose
            self._last_valid_pose_time = time.time()

        # State machine
        if self.state == SystemState.RECORDING:
            self._handle_recording(frame)

        elif self.state == SystemState.RETURNING:
            self._handle_returning(frame)

        # Send visual data to ArduPilot only when tracking is fresh AND stable
        # Stability gate prevents brief 1-frame recoveries during reset cascade
        # from sending noisy position jumps to the EKF
        vo_healthy = (
            (time.time() - self._last_valid_pose_time) < 2.0
            and self.vo.is_tracking_stable
        )

        # On VO recovery: sync VO position to last position sent to EKF.
        # Prevents jump between what EKF knows and what VO accumulated during failure.
        # Uses own sent-position tracking — no dependency on LOCAL_POSITION_NED.
        if not self._prev_vo_healthy and vo_healthy and self.mavlink.is_connected:
            self.vo.set_position(self._last_sent_x, self._last_sent_y)
            logger.info(f"VO recovery: resynced to last sent x={self._last_sent_x:.2f}, y={self._last_sent_y:.2f}")
        self._prev_vo_healthy = vo_healthy

        if self.mavlink.is_connected:
            if vo_healthy:
                self.mavlink.send_vision_position(
                    x=self._current_pose.x,
                    y=self._current_pose.y,
                    z=-self._current_altitude,
                    yaw=self._current_pose.yaw,
                    confidence=self._current_pose.confidence if hasattr(self._current_pose, 'confidence') else 0.95
                )
                self._last_sent_x = self._current_pose.x
                self._last_sent_y = self._current_pose.y

                if velocity:
                    self.mavlink.send_vision_speed(
                        vx=velocity.vx,
                        vy=velocity.vy,
                        vz=velocity.vz
                    )

            # Debug log every 5 seconds
            if int(time.time()) % 5 == 0 and not hasattr(self, '_last_debug_time'):
                logger.info(f"Sending VisOdom: x={self._current_pose.x:.2f}, y={self._current_pose.y:.2f}, z={self._current_altitude:.2f}, yaw={self._current_pose.yaw:.2f}, connected={self.mavlink.is_connected}, vo_healthy={vo_healthy}")
                self._last_debug_time = time.time()
            elif int(time.time()) % 5 != 0:
                if hasattr(self, '_last_debug_time'):
                    delattr(self, '_last_debug_time')
        else:
            # Log connection issue
            if int(time.time()) % 10 == 0 and not hasattr(self, '_last_warn_time'):
                logger.warning("MAVLink not connected - cannot send VisOdom data")
                self._last_warn_time = time.time()
            elif int(time.time()) % 10 != 0:
                if hasattr(self, '_last_warn_time'):
                    delattr(self, '_last_warn_time')
    
    def _handle_recording(self, frame):
        """Handle recording state"""
        self.route_recorder.add_keyframe(
            frame=frame,
            pose=self._current_pose,
            altitude=self._current_altitude
        )
    
    def _handle_returning(self, frame):
        """Handle returning state"""
        command = self.route_follower.process_frame(
            frame=frame,
            current_altitude=self._current_altitude
        )
        
        if command and self.mavlink.is_connected:
            # Send velocity command to ArduPilot
            self.mavlink.send_velocity_command(
                vx=command.vx,
                vy=command.vy,
                vz=0,  # maintain altitude
                yaw_rate=command.yaw_rate
            )
        
        # Check if completed
        if self.route_follower.is_completed:
            self.state = SystemState.IDLE
            logger.info("Return to home completed!")
    
    # Control methods
    def start_recording(self, route_name: str = None) -> str:
        """Start recording route"""
        if self.state != SystemState.IDLE:
            logger.warning(f"Cannot start recording in state: {self.state}")
            return None
        
        self.vo.reset()
        route_id = self.route_recorder.start_recording(route_name)
        self.state = SystemState.RECORDING
        logger.info(f"Started recording: {route_id}")
        return route_id
    
    def stop_recording(self):
        """Stop recording route"""
        if self.state != SystemState.RECORDING:
            return None
        
        route = self.route_recorder.stop_recording()
        self.state = SystemState.IDLE
        logger.info(f"Stopped recording: {route.id if route else 'no route'}")
        return route
    
    def start_return(self, route_id: str = None):
        """Start return to home"""
        if self.state != SystemState.IDLE:
            logger.warning(f"Cannot start return in state: {self.state}")
            return False
        
        # Load latest route if not specified
        if route_id is None:
            routes = self.route_recorder.list_routes()
            if not routes:
                logger.error("No routes available")
                return False
            route_id = routes[-1]['id']
        
        # Load route
        route = self.route_recorder.load_route(route_id)
        if route is None:
            logger.error(f"Failed to load route: {route_id}")
            return False
        
        # Start following
        if self.route_follower.start_following(route):
            self.state = SystemState.RETURNING
            logger.info(f"Started return on route: {route_id}")
            return True
        
        return False
    
    def stop_return(self):
        """Stop return to home"""
        self.route_follower.stop_following()
        self.state = SystemState.IDLE
        logger.info("Return stopped")
    
    def run(self):
        """Main run loop"""
        if not self.start():
            return
        
        try:
            while not self._shutdown_event.is_set():
                time.sleep(0.1)
        except KeyboardInterrupt:
            logger.info("Interrupted by user")
        finally:
            self.stop()


def main():
    parser = argparse.ArgumentParser(description='Visual Homing System')
    parser.add_argument('--config', type=str, help='Config file path')
    parser.add_argument('--autostart', action='store_true', help='Auto-start recording')
    parser.add_argument('--test-mode', action='store_true', help='Test mode (no MAVLink)')
    parser.add_argument('--web', action='store_true', help='Enable web interface')
    parser.add_argument('--camera', choices=['usb', 'picamera'], default='usb')
    args = parser.parse_args()
    
    # Load config
    config = Config.from_env()
    config.test_mode = args.test_mode
    config.autostart = args.autostart
    
    if args.camera == 'picamera':
        config.camera.type = CameraType.PI_CAMERA
    
    # Handle signals
    system = VisualHomingSystem(config)
    
    def signal_handler(sig, frame):
        logger.info("Shutdown signal received")
        system.stop()
        sys.exit(0)
    
    signal.signal(signal.SIGINT, signal_handler)
    signal.signal(signal.SIGTERM, signal_handler)
    
    # Start web interface if requested
    if args.web:
        from web.server import start_web_server
        start_web_server(system, config.web)
    
    # Run system
    system.run()


if __name__ == '__main__':
    main()
