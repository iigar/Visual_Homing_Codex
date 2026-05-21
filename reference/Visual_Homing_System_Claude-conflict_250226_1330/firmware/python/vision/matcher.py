"""
Feature Matcher Module
Матчування фічей між кадрами
"""
import cv2
import numpy as np
import logging
from typing import List, Tuple, Optional
from dataclasses import dataclass

from .feature_detector import Features

logger = logging.getLogger(__name__)


@dataclass
class MatchResult:
    """Result of feature matching"""
    matches: List[cv2.DMatch]
    src_points: np.ndarray
    dst_points: np.ndarray
    inlier_mask: Optional[np.ndarray] = None
    homography: Optional[np.ndarray] = None
    
    @property
    def count(self) -> int:
        return len(self.matches)
    
    @property
    def inlier_count(self) -> int:
        if self.inlier_mask is not None:
            return int(np.sum(self.inlier_mask))
        return self.count


class FeatureMatcher:
    """
    Feature matcher for visual navigation
    Uses BFMatcher with ratio test for robust matching
    """
    
    def __init__(
        self,
        match_threshold: float = 60.0,
        ratio_threshold: float = 0.75,
        min_matches: int = 10,
        use_flann: bool = False
    ):
        self.match_threshold = match_threshold
        self.ratio_threshold = ratio_threshold
        self.min_matches = min_matches
        self.use_flann = use_flann
        
        self._matcher = self._create_matcher()
    
    def _create_matcher(self):
        """Create feature matcher"""
        if self.use_flann:
            index_params = dict(algorithm=6, table_number=6,
                                key_size=12, multi_probe_level=1)
            search_params = dict(checks=50)
            return cv2.FlannBasedMatcher(index_params, search_params)
        else:
            # crossCheck=True: mutual consistency — no ratio test needed,
            # robust on repetitive/uniform surfaces
            return cv2.BFMatcher(cv2.NORM_HAMMING, crossCheck=True)
    
    def match(
        self,
        features1: Features,
        features2: Features,
        compute_homography: bool = True
    ) -> Optional[MatchResult]:
        """
        Match features between two images
        
        Args:
            features1: Features from first image
            features2: Features from second image
            compute_homography: Whether to compute homography matrix
        
        Returns:
            MatchResult or None if matching failed
        """
        try:
            if features1 is None or features2 is None:
                return None
            
            if features1.descriptors is None or features2.descriptors is None:
                return None
            
            # crossCheck matcher: use match() not knnMatch()
            raw_matches = self._matcher.match(
                features1.descriptors,
                features2.descriptors
            )

            # Sort by distance, apply absolute threshold
            good_matches = sorted(
                [m for m in raw_matches if m.distance < self.match_threshold],
                key=lambda m: m.distance
            )
            
            if len(good_matches) < self.min_matches:
                logger.debug(f"Not enough matches: {len(good_matches)} < {self.min_matches}")
                return None
            
            # Extract matched point coordinates
            src_pts = np.float32([features1.keypoints[m.queryIdx].pt for m in good_matches])
            dst_pts = np.float32([features2.keypoints[m.trainIdx].pt for m in good_matches])
            
            result = MatchResult(
                matches=good_matches,
                src_points=src_pts,
                dst_points=dst_pts
            )
            
            # Compute homography with RANSAC
            if compute_homography and len(good_matches) >= 4:
                H, mask = cv2.findHomography(
                    src_pts.reshape(-1, 1, 2),
                    dst_pts.reshape(-1, 1, 2),
                    cv2.RANSAC,
                    5.0
                )
                result.homography = H
                result.inlier_mask = mask.ravel() if mask is not None else None
            
            return result
            
        except Exception as e:
            logger.error(f"Feature matching error: {e}")
            return None
    
    def draw_matches(
        self,
        img1: np.ndarray,
        features1: Features,
        img2: np.ndarray,
        features2: Features,
        match_result: MatchResult
    ) -> np.ndarray:
        """
        Draw matches between two images
        """
        if match_result.inlier_mask is not None:
            # Draw only inliers
            inlier_matches = [m for i, m in enumerate(match_result.matches)
                            if match_result.inlier_mask[i]]
        else:
            inlier_matches = match_result.matches
        
        return cv2.drawMatches(
            img1, features1.keypoints,
            img2, features2.keypoints,
            inlier_matches, None,
            matchColor=(0, 255, 0),
            flags=cv2.DrawMatchesFlags_NOT_DRAW_SINGLE_POINTS
        )
