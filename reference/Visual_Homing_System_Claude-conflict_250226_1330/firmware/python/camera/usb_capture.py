"""
USB Video Capture Module
Захоплення відео з аналогової камери через USB (EasyCap)
Для Caddx Thermal 256 та інших аналогових камер
"""
import cv2
import numpy as np
import threading
import time
import logging
from typing import Optional, Tuple, Callable
from dataclasses import dataclass

logger = logging.getLogger(__name__)


@dataclass
class FrameInfo:
    """Information about captured frame"""
    timestamp: float
    frame_number: int
    width: int
    height: int


class USBCapture:
    """
    USB Video Capture for analog cameras
    Supports EasyCap (UTV007) and similar devices
    """
    
    def __init__(
        self,
        device: str = "/dev/video0",
        width: int = 720,
        height: int = 576,
        fps: int = 25
    ):
        self.device = device
        self.width = width
        self.height = height
        self.fps = fps
        
        self._cap: Optional[cv2.VideoCapture] = None
        self._frame: Optional[np.ndarray] = None
        self._frame_info: Optional[FrameInfo] = None
        self._lock = threading.Lock()
        self._running = False
        self._thread: Optional[threading.Thread] = None
        self._frame_count = 0
        self._callbacks: list = []
    
    def start(self) -> bool:
        """Start video capture"""
        try:
            # Try to open device
            device_id = int(self.device.split('video')[-1]) if 'video' in self.device else self.device
            self._cap = cv2.VideoCapture(device_id, cv2.CAP_V4L2)
            
            if not self._cap.isOpened():
                logger.error(f"Failed to open camera device: {self.device}")
                return False
            
            # Configure capture
            self._cap.set(cv2.CAP_PROP_FRAME_WIDTH, self.width)
            self._cap.set(cv2.CAP_PROP_FRAME_HEIGHT, self.height)
            self._cap.set(cv2.CAP_PROP_FPS, self.fps)
            
            # Start capture thread
            self._running = True
            self._thread = threading.Thread(target=self._capture_loop, daemon=True)
            self._thread.start()
            
            logger.info(f"Camera started: {self.device} at {self.width}x{self.height}@{self.fps}fps")
            return True
            
        except Exception as e:
            logger.error(f"Camera start error: {e}")
            return False
    
    def stop(self):
        """Stop video capture"""
        self._running = False
        
        if self._thread:
            self._thread.join(timeout=2.0)
        
        if self._cap:
            self._cap.release()
            self._cap = None
        
        logger.info("Camera stopped")
    
    def _capture_loop(self):
        """Main capture loop in separate thread"""
        while self._running:
            try:
                ret, frame = self._cap.read()
                
                if ret and frame is not None:
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
                    
                    # Call registered callbacks
                    for callback in self._callbacks:
                        try:
                            callback(frame, frame_info)
                        except Exception as e:
                            logger.error(f"Callback error: {e}")
                else:
                    time.sleep(0.01)
                    
            except Exception as e:
                logger.error(f"Capture error: {e}")
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
        """Check if capture is running"""
        return self._running and self._cap is not None and self._cap.isOpened()
    
    @property
    def frame_count(self) -> int:
        """Get total captured frames count"""
        return self._frame_count
    
    def __enter__(self):
        self.start()
        return self
    
    def __exit__(self, exc_type, exc_val, exc_tb):
        self.stop()
