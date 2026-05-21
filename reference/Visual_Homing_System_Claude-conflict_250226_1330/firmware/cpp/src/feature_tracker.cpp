/**
 * Visual Homing System - Feature Tracker Implementation
 */

#include "feature_tracker.hpp"
#include <iostream>

namespace visual_homing {

FeatureTracker::FeatureTracker(int n_features, float match_threshold, int min_matches)
    : match_threshold_(match_threshold)
    , min_matches_(min_matches)
{
    orb_ = cv::ORB::create(n_features);
    matcher_ = cv::BFMatcher::create(cv::NORM_HAMMING, false);
}

std::optional<Features> FeatureTracker::detect(const cv::Mat& image) {
    cv::Mat gray;
    if (image.channels() == 3) {
        cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);
    } else {
        gray = image;
    }

    // Enhance contrast
    cv::equalizeHist(gray, gray);

    Features features;
    features.image_size = gray.size();

    orb_->detectAndCompute(gray, cv::noArray(), features.keypoints, features.descriptors);

    if (features.keypoints.empty()) {
        return std::nullopt;
    }

    return features;
}

std::optional<MatchResult> FeatureTracker::match(const Features& features1,
                                                   const Features& features2) {
    if (features1.descriptors.empty() || features2.descriptors.empty()) {
        return std::nullopt;
    }

    // KNN matching
    std::vector<std::vector<cv::DMatch>> knn_matches;
    matcher_->knnMatch(features1.descriptors, features2.descriptors, knn_matches, 2);

    // Ratio test
    std::vector<cv::DMatch> good_matches;
    for (const auto& match_pair : knn_matches) {
        if (match_pair.size() == 2) {
            if (match_pair[0].distance < 0.75f * match_pair[1].distance) {
                if (match_pair[0].distance < match_threshold_) {
                    good_matches.push_back(match_pair[0]);
                }
            }
        }
    }

    if (static_cast<int>(good_matches.size()) < min_matches_) {
        return std::nullopt;
    }

    MatchResult result;
    result.matches = good_matches;

    // Extract point coordinates
    for (const auto& m : good_matches) {
        result.src_points.push_back(features1.keypoints[m.queryIdx].pt);
        result.dst_points.push_back(features2.keypoints[m.trainIdx].pt);
    }

    // Compute homography
    if (good_matches.size() >= 4) {
        result.homography = cv::findHomography(
            result.src_points, result.dst_points,
            cv::RANSAC, 5.0, result.inlier_mask
        );
    }

    return result;
}

cv::Mat FeatureTracker::drawFeatures(const cv::Mat& image, const Features& features) {
    cv::Mat output;
    cv::drawKeypoints(image, features.keypoints, output,
                      cv::Scalar(0, 255, 0),
                      cv::DrawMatchesFlags::DRAW_RICH_KEYPOINTS);
    return output;
}

cv::Mat FeatureTracker::drawMatches(const cv::Mat& img1, const Features& f1,
                                     const cv::Mat& img2, const Features& f2,
                                     const MatchResult& result) {
    cv::Mat output;
    cv::drawMatches(img1, f1.keypoints, img2, f2.keypoints,
                    result.matches, output,
                    cv::Scalar(0, 255, 0), cv::Scalar::all(-1),
                    std::vector<char>(),
                    cv::DrawMatchesFlags::NOT_DRAW_SINGLE_POINTS);
    return output;
}

} // namespace visual_homing
