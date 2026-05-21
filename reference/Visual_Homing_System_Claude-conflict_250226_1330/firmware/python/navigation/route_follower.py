"""
Route Follower Module
Повернення по записаному маршруту (Visual Homing)
"""
import cv2
import numpy as np
import logging
import sys
from typing import Optional, Tuple, List
from dataclasses import dataclass
from pathlib import Path
import time

# Add parent directory to path for imports
sys.path.insert(0, str(Path(__file__).parent.parent))
from vision import FeatureDetector, FeatureMatcher, Features
from navigation.route_recorder import Route, Keyframe

logger = logging.getLogger(__name__)


@dataclass
class NavigationCommand:
    """Navigation command for flight controller"""
    vx: float = 0.0          # Forward velocity (m/s)
    vy: float = 0.0          # Right velocity (m/s)
    vz: float = 0.0          # Down velocity (m/s)
    yaw_rate: float = 0.0    # Yaw rate (rad/s)
    confidence: float = 0.0
    target_keyframe_id: int = -1
    distance_to_target: float = 0.0


class RouteFollower:
    """
    Follows recorded route in reverse (return to home)
    Uses visual feature matching for localization
    """
    
    def __init__(
        self,
        route_dir: str = "/home/pi/visual_homing/routes",
        return_speed: float = 2.0,
        approach_threshold: float = 1.0,
        heading_tolerance: float = 10.0
    ):
        self.route_dir = Path(route_dir)
        self.return_speed = return_speed
        self.approach_threshold = approach_threshold
        self.heading_tolerance = np.radians(heading_tolerance)
        
        self.detector = FeatureDetector(n_features=500)
        self.matcher = FeatureMatcher(min_matches=10)
        
        # State
        self._active = False
        self._route: Optional[Route] = None
        self._current_target_idx: int = -1
        self._keyframe_features: dict = {}  # id -> Features
        self._completed = False
    
    def start_following(self, route: Route) -> bool:
        """
        Start following route in reverse
        
        Args:
            route: Route to follow
        
        Returns:
            True if started successfully
        """
        if not route.keyframes:
            logger.error("Route has no keyframes")
            return False
        
        self._route = route
        self._active = True
        self._completed = False
        
        # Start from last keyframe (reverse order)
        self._current_target_idx = len(route.keyframes) - 1
        
        # Load keyframe features
        self._load_keyframe_features()
        
        logger.info(f"Started following route: {route.id}")
        return True
    
    def _load_keyframe_features(self):
        """Load features for all keyframes"""
        if self._route is None:
            return
        
        route_path = self.route_dir / self._route.id
        
        for kf in self._route.keyframes:
            try:
                # Load descriptors
                desc_file = route_path / f"kf_{kf.id}_desc.npy"
                kp_file = route_path / f"kf_{kf.id}_kp.npy"
                
                if desc_file.exists() and kp_file.exists():
                    descriptors = np.load(str(desc_file))
                    kp_data = np.load(str(kp_file))
                    
                    # Reconstruct keypoints
                    keypoints = []
                    for pt in kp_data:
                        kp = cv2.KeyPoint(
                            x=float(pt[0]),
                            y=float(pt[1]),
                            size=float(pt[2]),
                            angle=float(pt[3]),
                            response=float(pt[4])
                        )
                        keypoints.append(kp)
                    
                    self._keyframe_features[kf.id] = Features(
                        keypoints=keypoints,
                        descriptors=descriptors,
                        image_shape=(576, 720)  # Default size
                    )
                    
            except Exception as e:
                logger.error(f"Failed to load keyframe {kf.id}: {e}")
    
    def process_frame(
        self,
        frame: np.ndarray,
        current_altitude: float
    ) -> Optional[NavigationCommand]:
        """
        Process frame and generate navigation command
        
        Args:
            frame: Current camera frame
            current_altitude: Current altitude from barometer
        
        Returns:
            NavigationCommand or None
        """
        if not self._active or self._route is None:
            return None
        
        if self._completed:
            return NavigationCommand(confidence=1.0)  # Hold position
        
        # Detect features in current frame
        current_features = self.detector.detect(frame)
        
        if current_features is None or current_features.count < 10:
            return None
        
        # Find best matching keyframe
        best_match = self._find_best_keyframe_match(current_features)
        
        if best_match is None:
            logger.debug("No keyframe match found")
            return None
        
        kf_idx, match_result, matched_kf = best_match
        
        # Calculate navigation command
        command = self._calculate_command(
            current_features,
            matched_kf,
            match_result,
            current_altitude
        )
        
        # Check if we reached target keyframe
        if command.distance_to_target < self.approach_threshold:
            self._advance_to_next_keyframe()
        
        return command
    
    def _find_best_keyframe_match(
        self,
        current_features: Features
    ) -> Optional[Tuple[int, any, Keyframe]]:
        """
        Find best matching keyframe near current target
        """
        if self._current_target_idx < 0:
            return None
        
        best_score = 0
        best_match = None
        
        # Search window around current target
        search_range = range(
            max(0, self._current_target_idx - 2),
            min(len(self._route.keyframes), self._current_target_idx + 3)
        )
        
        for idx in search_range:
            kf = self._route.keyframes[idx]
            
            if kf.id not in self._keyframe_features:
                continue
            
            kf_features = self._keyframe_features[kf.id]
            match_result = self.matcher.match(current_features, kf_features)
            
            if match_result and match_result.inlier_count > best_score:
                best_score = match_result.inlier_count
                best_match = (idx, match_result, kf)
        
        return best_match
    
    def _calculate_command(
        self,
        current_features: Features,
        target_keyframe: Keyframe,
        match_result,
        current_altitude: float
    ) -> NavigationCommand:
        """
        Calculate navigation command to reach keyframe
        """
        # Use homography to estimate relative position
        if match_result.homography is None:
            return NavigationCommand()
        
        H = match_result.homography
        
        # Extract translation from homography
        # Negative because we're going back
        tx = -H[0, 2]
        ty = -H[1, 2]
        
        # Convert to metric using altitude
        fx = 500  # focal length estimate
        dx = tx * current_altitude / fx
        dy = ty * current_altitude / fx
        
        # Calculate distance
        distance = np.sqrt(dx*dx + dy*dy)
        
        # Calculate velocities (proportional control)
        kp = 0.5  # proportional gain
        max_speed = self.return_speed
        
        vx = np.clip(dx * kp, -max_speed, max_speed)
        vy = np.clip(dy * kp, -max_speed, max_speed)
        
        # Calculate yaw correction
        rotation = np.arctan2(H[1, 0], H[0, 0])
        yaw_rate = -rotation * 0.5  # proportional yaw control
        
        # Confidence based on match quality
        confidence = min(1.0, match_result.inlier_count / 50)
        
        return NavigationCommand(
            vx=vx,
            vy=vy,
            yaw_rate=yaw_rate,
            confidence=confidence,
            target_keyframe_id=target_keyframe.id,
            distance_to_target=distance
        )
    
    def _advance_to_next_keyframe(self):
        """Move to next keyframe in route (reverse order)"""
        self._current_target_idx -= 1
        
        if self._current_target_idx < 0:
            self._completed = True
            logger.info("Route following completed - reached start")
        else:
            logger.info(f"Advanced to keyframe {self._current_target_idx}")
    
    def stop_following(self):
        """Stop route following"""
        self._active = False
        self._route = None
        self._keyframe_features.clear()
        logger.info("Route following stopped")
    
    @property
    def is_active(self) -> bool:
        return self._active
    
    @property
    def is_completed(self) -> bool:
        return self._completed
    
    @property
    def current_target_idx(self) -> int:
        return self._current_target_idx
    
    @property
    def progress(self) -> float:
        """Return progress percentage (0-100)"""
        if self._route is None or not self._route.keyframes:
            return 0.0
        total = len(self._route.keyframes)
        remaining = self._current_target_idx + 1
        return 100.0 * (total - remaining) / total
