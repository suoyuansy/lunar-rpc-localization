#include "point_measurement/detector.hpp"

#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <vector>

namespace rpc_localization {
namespace {

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

double distance_score(double distance, int width, int height) {
    const double sigma = 0.25 * std::min(width, height);
    return std::exp(-0.5 * (distance * distance) / (sigma * sigma));
}

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

}  // namespace

DetectionResult detect_single_candidate(
    const cv::Mat_<float>& image,
    const cv::Point2d& expected_center) {
    if (image.empty()) {
        throw std::runtime_error("检测输入影像为空");
    }

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
        return DetectionResult{cv::Point(
                                   static_cast<int>(std::lround(expected_center.x)),
                                   static_cast<int>(std::lround(expected_center.y))),
                               0.0,
                               true};
    }
    const float fill_value = static_cast<float>(median(finite_values));
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
    cv::dilate(
        combined,
        local_maximum,
        cv::getStructuringElement(cv::MORPH_RECT, cv::Size(5, 5)));

    const int border = 4;
    double best_score = -std::numeric_limits<double>::infinity();
    cv::Point best_point(
        static_cast<int>(std::lround(expected_center.x)),
        static_cast<int>(std::lround(expected_center.y)));

    for (int row = border; row < combined.rows - border; ++row) {
        for (int column = border; column < combined.cols - border; ++column) {
            const double peak = combined(row, column);
            if (peak < 2.5 || peak < local_maximum(row, column) - 1e-6) {
                continue;
            }

            const double distance = cv::norm(
                cv::Point2d(column, row) - expected_center);
            const double contrast = std::min(peak / 8.0, 1.0);
            const double shape = shape_score(combined, column, row, peak);
            const double scale = static_cast<double>(
                                     scale_hits(scale_scores, column, row)) /
                                 3.0;
            const double proximity = distance_score(
                distance, combined.cols, combined.rows);
            const double total =
                0.45 * contrast + 0.20 * shape + 0.20 * scale + 0.15 * proximity;

            if (total > best_score) {
                best_score = total;
                best_point = cv::Point(column, row);
            }
        }
    }

    if (!std::isfinite(best_score)) {
        return DetectionResult{best_point, 0.0, true};
    }

    return DetectionResult{best_point, best_score, best_score < 0.35};
}

}  // namespace rpc_localization
