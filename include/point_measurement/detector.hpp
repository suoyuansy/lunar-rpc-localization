#pragma once

#include <opencv2/core.hpp>

namespace rpc_localization {

// 自动检测结果。
// point 使用局部 ROI 坐标，score 越大表示候选越可信。
struct DetectionResult {
    cv::Point2d point;
    double score = 0.0;
    bool low_confidence = true;
};

// 在局部 float32 影像中寻找一个最可能的反射器候选点。
// expected_center 是 RPC 正算得到的理论像点，仅用于引导候选搜索，
// 不会直接作为最终量测点。
DetectionResult detect_single_candidate(
    const cv::Mat_<float>& image,
    const cv::Point2d& expected_center);

}  // namespace rpc_localization
