#include "point_measurement/detector.hpp"

#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <vector>

namespace rpc_localization {
namespace {

// 计算中位数。这里会复制输入，不影响调用者的数据。
double median(std::vector<float> values) {
    if (values.empty()) {
        return 0.0;
    }
    const auto middle = values.begin() + values.size() / 2;
    std::nth_element(values.begin(), middle, values.end());
    const double high = *middle;
    if (values.size() % 2 == 1) {
        return high;
    }
    const auto low = *std::max_element(values.begin(), middle);
    return 0.5 * (low + high);
}

// 计算 MAD（中位数绝对偏差）。
// MAD 对异常值不敏感，用来估计局部噪声强度。
double robust_mad(const cv::Mat_<float>& values) {
    std::vector<float> samples;
    samples.reserve(values.total());
    for (int row = 0; row < values.rows; ++row) {
        for (int column = 0; column < values.cols; ++column) {
            const float value = values(row, column);
            if (std::isfinite(value)) {
                samples.push_back(value);
            }
        }
    }
    const double center = median(samples);
    for (float& value : samples) {
        value = static_cast<float>(std::abs(value - center));
    }
    return median(samples);
}

// 候选点离理论中心越近，得分越高；距离太远时得分快速衰减。
double distance_score(double distance, int width, int height) {
    const double sigma = 0.25 * std::min(width, height);
    return std::exp(-0.5 * (distance * distance) / (sigma * sigma));
}

// 估计候选点的形状是否像一个紧凑的小亮斑。
// 统计 7×7 邻域内高于半峰值的像素数量，越接近 3 个像素得分越高。
double shape_score(const cv::Mat_<float>& response, int x, int y, double peak) {
    int area = 0;
    const int radius = 3;
    for (int dy = -radius; dy <= radius; ++dy) {
        for (int dx = -radius; dx <= radius; ++dx) {
            const int px = x + dx;
            const int py = y + dy;
            if (px < 0 || py < 0 || px >= response.cols || py >= response.rows) {
                continue;
            }
            if (response(py, px) >= 0.5 * peak) {
                ++area;
            }
        }
    }
    const double preferred_area = 3.0;
    const double sigma = 2.5;
    return std::exp(-0.5 * std::pow((area - preferred_area) / sigma, 2.0));
}

// 统计候选点在几个高斯尺度上都能达到显著响应。
int scale_hits(
    const std::vector<cv::Mat_<float>>& scale_scores,
    int x,
    int y) {
    int hits = 0;
    for (const auto& score : scale_scores) {
        if (score(y, x) >= 2.5) {
            ++hits;
        }
    }
    return hits;
}

// 用三点抛物线插值估计峰值相对整数位置的偏移，
// 从而把候选点从整数像素细化到亚像素。
double parabolic_offset(double left, double center, double right) {
    const double denominator = left - 2.0 * center + right;
    if (!std::isfinite(denominator) || std::abs(denominator) < 1e-12) {
        return 0.0;
    }

    const double offset = 0.5 * (left - right) / denominator;
    if (!std::isfinite(offset)) {
        return 0.0;
    }
    return std::clamp(offset, -0.5, 0.5);
}

}  // namespace

DetectionResult detect_single_candidate(
    const cv::Mat_<float>& image,
    const cv::Point2d& expected_center) {
    if (image.empty()) {
        throw std::runtime_error("检测输入影像为空");
    }

    // 复制一份数据，后续只对副本做 NoData 填充和滤波。
    cv::Mat_<float> clean = image.clone();
    std::vector<float> finite_values;
    finite_values.reserve(clean.total());
    for (int row = 0; row < clean.rows; ++row) {
        for (int column = 0; column < clean.cols; ++column) {
            const float value = clean(row, column);
            if (std::isfinite(value) && std::abs(value) < 1e30f) {
                finite_values.push_back(value);
            }
        }
    }
    if (finite_values.empty()) {
        // 没有任何有效像素时，只能返回理论中心并标记低置信度。
        return DetectionResult{cv::Point2d(expected_center),
                               0.0,
                               true};
    }
    const float fill_value = static_cast<float>(median(finite_values));
    // NoData 用中位数填充，避免极端值影响高斯背景估计。
    for (int row = 0; row < clean.rows; ++row) {
        for (int column = 0; column < clean.cols; ++column) {
            const float value = clean(row, column);
            if (!std::isfinite(value) || std::abs(value) >= 1e30f) {
                clean(row, column) = fill_value;
            }
        }
    }

    std::vector<cv::Mat_<float>> scale_scores;
    cv::Mat_<float> combined(clean.size(), 0.0f);

    // 分别在 1、2、4 像素尺度上估计背景，并计算高通响应。
    for (double sigma : {1.0, 2.0, 4.0}) {
        cv::Mat_<float> background;
        cv::GaussianBlur(clean, background, cv::Size(), sigma, sigma);

        cv::Mat_<float> response = clean - background;
        const double noise = 1.4826 * robust_mad(response) + 1e-12;
        cv::Mat_<float> normalized;
        response.convertTo(normalized, CV_32F, 1.0 / noise);
        scale_scores.push_back(normalized);
        for (int row = 0; row < combined.rows; ++row) {
            for (int column = 0; column < combined.cols; ++column) {
                combined(row, column) = std::max(
                    combined(row, column),
                    normalized(row, column));
            }
        }
    }

    cv::Mat_<float> local_maximum;
    // 5×5 膨胀用于寻找局部极大值，避免同一亮斑产生多个候选点。
    cv::dilate(
        combined,
        local_maximum,
        cv::getStructuringElement(cv::MORPH_RECT, cv::Size(5, 5)));

    const int border = 4;
    double best_score = -std::numeric_limits<double>::infinity();
    cv::Point2d best_point(expected_center);

    for (int row = border; row < combined.rows - border; ++row) {
        for (int column = border; column < combined.cols - border; ++column) {
            const double peak = combined(row, column);
            if (peak < 2.5 || peak < local_maximum(row, column) - 1e-6) {
                continue;
            }

            // 理论像点只作为先验中心，通过 proximity 项参与评分。
            const double distance = cv::norm(
                cv::Point2d(column, row) - expected_center);
            const double contrast = std::min(peak / 8.0, 1.0);
            const double shape = shape_score(combined, column, row, peak);
            const double scale = static_cast<double>(
                                     scale_hits(scale_scores, column, row)) /
                                 3.0;
            const double proximity = distance_score(
                distance, combined.cols, combined.rows);
            // 综合评分：峰值强度、形状、尺度稳定性和与理论中心的距离。
            const double total =
                0.45 * contrast + 0.20 * shape + 0.20 * scale + 0.15 * proximity;

            if (total > best_score) {
                best_score = total;
                best_point = cv::Point2d(column, row);
            }
        }
    }

    if (!std::isfinite(best_score)) {
        return DetectionResult{best_point, 0.0, true};
    }

    const int best_x = static_cast<int>(std::lround(best_point.x));
    const int best_y = static_cast<int>(std::lround(best_point.y));
    // 对最高分点做横向、纵向抛物线插值，得到亚像素坐标。
    if (best_x > 0 && best_x + 1 < combined.cols &&
        best_y > 0 && best_y + 1 < combined.rows) {
        best_point.x += parabolic_offset(
            combined(best_y, best_x - 1),
            combined(best_y, best_x),
            combined(best_y, best_x + 1));
        best_point.y += parabolic_offset(
            combined(best_y - 1, best_x),
            combined(best_y, best_x),
            combined(best_y + 1, best_x));
    }

    return DetectionResult{best_point, best_score, best_score < 0.35};
}

}  // namespace rpc_localization
