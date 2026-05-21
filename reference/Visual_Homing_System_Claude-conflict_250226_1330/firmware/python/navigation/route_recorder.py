"""
Route Recorder Module
Запис маршруту та keyframes
"""
import cv2
import numpy as np
import json
import os
import sys
import time
import logging
from typing import List, Optional, Dict
from dataclasses import dataclass, asdict
from pathlib import Path

# Add parent directory to path for imports
sys.path.insert(0, str(Path(__file__).parent.parent))
from vision import FeatureDetector, Features, Pose

logger = logging.getLogger(__name__)


@dataclass
class Keyframe:
    """Single keyframe in route"""
    id: int
    timestamp: float
    pose: Dict  # x, y, z, yaw
    features_count: int
    altitude: float
    # Descriptors saved separately in binary format
    
    def to_dict(self) -> Dict:
        return asdict(self)
    
    @classmethod
    def from_dict(cls, data: Dict) -> 'Keyframe':
        return cls(**data)


@dataclass
class Route:
    """Complete route with keyframes"""
    id: str
    name: str
    created_at: float
    keyframes: List[Keyframe]
    start_position: Dict
    end_position: Dict
    total_distance: float
    
    def to_dict(self) -> Dict:
        return {
            'id': self.id,
            'name': self.name,
            'created_at': self.created_at,
            'keyframes': [kf.to_dict() for kf in self.keyframes],
            'start_position': self.start_position,
            'end_position': self.end_position,
            'total_distance': self.total_distance
        }
    
    @classmethod
    def from_dict(cls, data: Dict) -> 'Route':
        keyframes = [Keyframe.from_dict(kf) for kf in data.get('keyframes', [])]
        return cls(
            id=data['id'],
            name=data['name'],
            created_at=data['created_at'],
            keyframes=keyframes,
            start_position=data['start_position'],
            end_position=data['end_position'],
            total_distance=data['total_distance']
        )


class RouteRecorder:
    """
    Records flight route with visual keyframes
    for later playback (return to home)
    """
    
    def __init__(
        self,
        route_dir: str = "/home/pi/visual_homing/routes",
        keyframe_distance: float = 2.0,    # meters
        keyframe_angle: float = 15.0,      # degrees
        min_features: int = 50
    ):
        self.route_dir = Path(route_dir)
        self.route_dir.mkdir(parents=True, exist_ok=True)
        
        self.keyframe_distance = keyframe_distance
        self.keyframe_angle = keyframe_angle
        self.min_features = min_features
        
        self.detector = FeatureDetector(n_features=500)
        
        # Current recording state
        self._recording = False
        self._current_route: Optional[Route] = None
        self._current_route_path: Optional[Path] = None
        self._last_keyframe_pose: Optional[Pose] = None
        self._keyframe_counter = 0
    
    def start_recording(self, route_name: str = None) -> str:
        """
        Start recording new route
        
        Returns:
            Route ID
        """
        if self._recording:
            logger.warning("Already recording, stopping previous")
            self.stop_recording()
        
        # Generate route ID
        route_id = f"route_{int(time.time())}"
        if route_name is None:
            route_name = route_id
        
        # Create route directory
        self._current_route_path = self.route_dir / route_id
        self._current_route_path.mkdir(parents=True, exist_ok=True)
        
        # Initialize route
        self._current_route = Route(
            id=route_id,
            name=route_name,
            created_at=time.time(),
            keyframes=[],
            start_position={'x': 0, 'y': 0, 'z': 0},
            end_position={'x': 0, 'y': 0, 'z': 0},
            total_distance=0.0
        )
        
        self._recording = True
        self._keyframe_counter = 0
        self._last_keyframe_pose = None
        
        logger.info(f"Started recording route: {route_id}")
        return route_id
    
    def add_keyframe(
        self,
        frame: np.ndarray,
        pose: Pose,
        altitude: float,
        force: bool = False
    ) -> bool:
        """
        Try to add keyframe at current position
        
        Args:
            frame: Current camera frame
            pose: Current pose estimate
            altitude: Current altitude
            force: Force add even if conditions not met
        
        Returns:
            True if keyframe was added
        """
        if not self._recording:
            return False
        
        # Check if we should add keyframe
        if not force and self._last_keyframe_pose is not None:
            # Calculate distance from last keyframe
            dx = pose.x - self._last_keyframe_pose.x
            dy = pose.y - self._last_keyframe_pose.y
            distance = np.sqrt(dx*dx + dy*dy)
            
            # Calculate angle change
            angle_diff = abs(pose.yaw - self._last_keyframe_pose.yaw)
            angle_diff = min(angle_diff, 2*np.pi - angle_diff)  # shortest path
            angle_diff_deg = np.degrees(angle_diff)
            
            # Skip if not enough movement
            if distance < self.keyframe_distance and angle_diff_deg < self.keyframe_angle:
                return False
        
        # Detect features
        features = self.detector.detect(frame)
        
        if features is None or features.count < self.min_features:
            logger.debug(f"Not enough features for keyframe: {features.count if features else 0}")
            return False
        
        # Create keyframe
        kf_id = self._keyframe_counter
        self._keyframe_counter += 1
        
        keyframe = Keyframe(
            id=kf_id,
            timestamp=time.time(),
            pose={'x': pose.x, 'y': pose.y, 'z': pose.z, 'yaw': pose.yaw},
            features_count=features.count,
            altitude=altitude
        )
        
        # Save keyframe data
        self._save_keyframe_data(kf_id, frame, features)
        
        # Update route
        self._current_route.keyframes.append(keyframe)
        self._last_keyframe_pose = pose
        
        # Update distance
        if len(self._current_route.keyframes) > 1:
            prev = self._current_route.keyframes[-2]
            dx = pose.x - prev.pose['x']
            dy = pose.y - prev.pose['y']
            self._current_route.total_distance += np.sqrt(dx*dx + dy*dy)
        
        logger.info(f"Added keyframe {kf_id} at ({pose.x:.1f}, {pose.y:.1f})")
        return True
    
    def _save_keyframe_data(
        self,
        kf_id: int,
        frame: np.ndarray,
        features: Features
    ):
        """Save keyframe image and features to disk"""
        if self._current_route_path is None:
            return
        
        # Save thumbnail
        thumbnail = cv2.resize(frame, (160, 120))
        cv2.imwrite(
            str(self._current_route_path / f"kf_{kf_id}_thumb.jpg"),
            thumbnail,
            [cv2.IMWRITE_JPEG_QUALITY, 80]
        )
        
        # Save descriptors (binary)
        np.save(
            str(self._current_route_path / f"kf_{kf_id}_desc.npy"),
            features.descriptors
        )
        
        # Save keypoints
        kp_data = [(kp.pt[0], kp.pt[1], kp.size, kp.angle, kp.response)
                   for kp in features.keypoints]
        np.save(
            str(self._current_route_path / f"kf_{kf_id}_kp.npy"),
            np.array(kp_data)
        )
    
    def stop_recording(self) -> Optional[Route]:
        """Stop recording and save route"""
        if not self._recording:
            return None
        
        self._recording = False
        
        if self._current_route and len(self._current_route.keyframes) > 0:
            # Set end position
            last_kf = self._current_route.keyframes[-1]
            self._current_route.end_position = last_kf.pose
            
            # Set start position
            first_kf = self._current_route.keyframes[0]
            self._current_route.start_position = first_kf.pose
            
            # Save route metadata
            route_file = self._current_route_path / "route.json"
            with open(route_file, 'w') as f:
                json.dump(self._current_route.to_dict(), f, indent=2)
            
            logger.info(f"Route saved: {self._current_route.id} with {len(self._current_route.keyframes)} keyframes")
            
            return self._current_route
        
        return None
    
    def load_route(self, route_id: str) -> Optional[Route]:
        """Load route from disk"""
        route_path = self.route_dir / route_id
        route_file = route_path / "route.json"
        
        if not route_file.exists():
            logger.error(f"Route not found: {route_id}")
            return None
        
        try:
            with open(route_file, 'r') as f:
                data = json.load(f)
            return Route.from_dict(data)
        except Exception as e:
            logger.error(f"Failed to load route: {e}")
            return None
    
    def list_routes(self) -> List[Dict]:
        """List all saved routes"""
        routes = []
        for route_dir in self.route_dir.iterdir():
            if route_dir.is_dir():
                route_file = route_dir / "route.json"
                if route_file.exists():
                    try:
                        with open(route_file, 'r') as f:
                            data = json.load(f)
                        routes.append({
                            'id': data['id'],
                            'name': data['name'],
                            'created_at': data['created_at'],
                            'keyframes': len(data.get('keyframes', [])),
                            'distance': data.get('total_distance', 0)
                        })
                    except:
                        pass
        return routes
    
    @property
    def is_recording(self) -> bool:
        return self._recording
    
    @property
    def keyframe_count(self) -> int:
        if self._current_route:
            return len(self._current_route.keyframes)
        return 0
