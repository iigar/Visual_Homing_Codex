/**
 * Visual Homing System - Feature Tracker
 * ORB детекція та матчування фічей
 */

#ifndef FEATURE_TRACKER_HPP
#define FEATURE_TRACKER_HPP

#include <opencv2/opencv.hpp>
#include <opencv2/features2d.hpp>
#include <vector>
#include <optional>

namespace visual_homing {

struct Features {
    std::vector<cv::KeyPoint> keypoints;
    cv::Mat descriptors;
    cv::Size image_size;

    int count() const { return static_cast<int>(keypoints.size()); }
};

struct MatchResult {
    std::vector<cv::DMatch> matches;
    std::vector<cv::Point2f> src_points;
    std::vector<cv::Point2f> dst_points;
    cv::Mat homography;
    std::vector<uchar> inlier_mask;

    int inlierCount() const {
        int count = 0;
        for (uchar m : inlier_mask) {
            if (m) count++;
        }
        return count;
    }
};

/**
 * Feature tracker using ORB
 */
class FeatureTracker {
public:
    FeatureTracker(int n_features = 500,
                   float match_threshold = 30.0f,
                   int min_matches = 10);

    // Detect features in image
    std::optional<Features> detect(const cv::Mat& image);

    // Match features between two images
    std::optional<MatchResult> match(const Features& features1,
                                      const Features& features2);

    // Draw features on image
    cv::Mat drawFeatures(const cv::Mat& image, const Features& features);

    // Draw matches between two images
    cv::Mat drawMatches(const cv::Mat& img1, const Features& f1,
                        const cv::Mat& img2, const Features& f2,
                        const MatchResult& result);

private:
    cv::Ptr<cv::ORB> orb_;
    cv::Ptr<cv::BFMatcher> matcher_;

    float match_threshold_;
    int min_matches_;
};

} // namespace visual_homing

#endif // FEATURE_TRACKER_HPP
