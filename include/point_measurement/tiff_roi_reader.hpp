#pragma once

#include <filesystem>

#include <opencv2/core.hpp>

namespace rpc_localization {

struct TiffInfo {
    int width = 0;
    int height = 0;
};

TiffInfo read_tiff_info(const std::filesystem::path& path);

cv::Mat_<float> read_tiff_roi(
    const std::filesystem::path& path,
    int x0,
    int y0,
    int width,
    int height);

}  // namespace rpc_localization

