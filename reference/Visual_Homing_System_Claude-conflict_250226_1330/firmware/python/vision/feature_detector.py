"""
Feature Detector Module
ORB/SIFT детекція фічей для Visual Homing
"""
import cv2
import numpy as np
import logging
from typing import Tuple, Optional, List
from dataclasses import dataclass
from enum import Enum

logger = logging.getLogger(__name__)


class DetectorType(Enum):
    ORB = "orb"      # Fast, good for real-time
    SIFT = "sift"    # More accurate, slower
    AKAZE = "akaze"  # Good balance


@dataclass
class Features:
    """Detected features container"""
    keypoints: List[cv2.KeyPoint]
    descriptors: np.ndarray
    image_shape: Tuple[int, int]
    
    @property
    def count(self) -> int:
        return len(self.keypoints)
    
    def get_points(self) -> np.ndarray:
        """Get keypoint coordinates as numpy array"""
        return np.array([kp.pt for kp in self.keypoints], dtype=np.float32)


class FeatureDetector:
    """
    Feature detector for visual navigation
    Supports ORB, SIFT, AKAZE
    """
    
    def __init__(
        self,
        detector_type: DetectorType = DetectorType.ORB,
        n_features: int = 500,
        scale_factor: float = 1.2,
        n_levels: int = 8
    ):
        self.detector_type = detector_type
        self.n_features = n_features
        self.scale_factor = scale_factor
        self.n_levels = n_levels
        
        self._detector = self._create_detector()
    
    def _create_detector(self):
        """Create feature detector based on type"""
        if self.detector_type == DetectorType.ORB:
            return cv2.ORB_create(
                nfeatures=self.n_features,
                scaleFactor=self.scale_factor,
                nlevels=self.n_levels
            )
        elif self.detector_type == DetectorType.SIFT:
            return cv2.SIFT_create(nfeatures=self.n_features)
        elif self.detector_type == DetectorType.AKAZE:
            return cv2.AKAZE_create()
        else:
            raise ValueError(f"Unknown detector type: {self.detector_type}")
    
    def detect(self, image: np.ndarray, mask: np.ndarray = None) -> Optional[Features]:
        """
        Detect features in image
        
        Args:
            image: Input image (BGR or grayscale)
            mask: Optional mask for detection region
        
        Returns:
            Features object or None if detection failed
        """
        try:
            # Convert to grayscale if needed
            if len(image.shape) == 3:
                gray = cv2.cvtColor(image, cv2.COLOR_BGR2GRAY)
            else:
                gray = image
            
            # Enhance contrast for thermal images
            gray = cv2.equalizeHist(gray)
            
            # Detect keypoints and compute descriptors
            keypoints, descriptors = self._detector.detectAndCompute(gray, mask)
            
            if keypoints is None or len(keypoints) == 0:
                logger.debug("No features detected")
                return None
            
            if descriptors is None:
                logger.debug("No descriptors computed")
                return None
            
            return Features(
                keypoints=keypoints,
                descriptors=descriptors,
                image_shape=gray.shape
            )
            
        except Exception as e:
            logger.error(f"Feature detection error: {e}")
            return None
    
    def draw_features(
        self,
        image: np.ndarray,
        features: Features,
        color: Tuple[int, int, int] = (0, 255, 0)
    ) -> np.ndarray:
        """
        Draw detected features on image
        """
        output = image.copy()
        return cv2.drawKeypoints(
            output,
            features.keypoints,
            output,
            color=color,
            flags=cv2.DRAW_MATCHES_FLAGS_DRAW_RICH_KEYPOINTS
        )
