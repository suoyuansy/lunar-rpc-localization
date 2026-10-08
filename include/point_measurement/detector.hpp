#pragma once

#include <opencv2/core.hpp>

namespace rpc_localization {

struct DetectionResult {
    cv::Point point;
    double score = 0.0;
    bool low_confidence = true;
};

DetectionResult detect_single_candidate(
    const cv::Mat_<float>& image,
    const cv::Point2d& expected_center);

}  // namespace rpc_localization

