"""
Pi Camera Module
Захоплення відео з Raspberry Pi Camera (CSI)
"""
import cv2
import numpy as np
import threading
import time
import logging
from typing import Optional, Tuple, Callable
from dataclasses import dataclass

logger = logging.getLogger(__name__)

# Try to import picamera2 (Bookworm+)
try:
    from picamera2 import Picamera2
    PICAMERA2_AVAILABLE = True
except ImportError:
    PICAMERA2_AVAILABLE = False
    logger.warning("picamera2 not available, Pi Camera support limited")


@dataclass
class FrameInfo:
    """Information about captured frame"""
    timestamp: float
    frame_number: int
    width: int
    height: int


class PiCamera:
    """
    Raspberry Pi Camera (CSI) capture
    Uses picamera2 library for Bookworm+ compatibility
    """
    
    def __init__(
        self,
        width: int = 1280,
        height: int = 720,
        fps: int = 30
    ):
        self.width = width
        self.height = height
        self.fps = fps
        
        self._camera = None
        self._frame: Optional[np.ndarray] = None
        self._frame_info: Optional[FrameInfo] = None
        self._lock = threading.Lock()
        self._running = False
        self._thread: Optional[threading.Thread] = None
        self._frame_count = 0
        self._callbacks: list = []
    
    def start(self) -> bool:
        """Start camera capture"""
        if not PICAMERA2_AVAILABLE:
            logger.error("picamera2 not available")
            return False
        
        try:
            self._camera = Picamera2()
            
            # Configure camera
            config = self._camera.create_video_configuration(
                main={"size": (self.width, self.height), "format": "RGB888"},
                controls={"FrameRate": self.fps}
            )
            self._camera.configure(config)
            self._camera.start()
            
            # Start capture thread
            self._running = True
            self._thread = threading.Thread(target=self._capture_loop, daemon=True)
            self._thread.start()
            
            logger.info(f"Pi Camera started: {self.width}x{self.height}@{self.fps}fps")
            return True
            
        except Exception as e:
            logger.error(f"Pi Camera start error: {e}")
            return False
    
    def stop(self):
        """Stop camera capture"""
        self._running = False
        
        if self._thread:
            self._thread.join(timeout=2.0)
        
        if self._camera:
            self._camera.stop()
            self._camera.close()
            self._camera = None
        
        logger.info("Pi Camera stopped")
    
    def _capture_loop(self):
        """Main capture loop"""
        while self._running:
            try:
                # Capture frame from Pi Camera
                frame = self._camera.capture_array()
                
                if frame is not None:
                    # Convert RGB to BGR for OpenCV compatibility
                    frame = cv2.cvtColor(frame, cv2.COLOR_RGB2BGR)
                    
                    timestamp = time.time()
                    self._frame_count += 1
                    
                    frame_info = FrameInfo(
                        timestamp=timestamp,
                        frame_number=self._frame_count,
                        width=frame.shape[1],
                        height=frame.shape[0]
                    )
                    
                    with self._lock:
                        self._frame = frame
                        self._frame_info = frame_info
                    
                    # Call callbacks
                    for callback in self._callbacks:
                        try:
                            callback(frame, frame_info)
                        except Exception as e:
                            logger.error(f"Callback error: {e}")
                            
            except Exception as e:
                logger.error(f"Pi Camera capture error: {e}")
                time.sleep(0.1)
    
    def get_frame(self) -> Tuple[Optional[np.ndarray], Optional[FrameInfo]]:
        """Get latest captured frame"""
        with self._lock:
            if self._frame is not None:
                return self._frame.copy(), self._frame_info
        return None, None
    
    def register_callback(self, callback: Callable[[np.ndarray, FrameInfo], None]):
        """Register callback for new frames"""
        self._callbacks.append(callback)
    
    def is_running(self) -> bool:
        """Check if camera is running"""
        return self._running
    
    @property
    def frame_count(self) -> int:
        """Get total frame count"""
        return self._frame_count
    
    def __enter__(self):
        self.start()
        return self
    
    def __exit__(self, exc_type, exc_val, exc_tb):
        self.stop()
