#pragma once

#include <filesystem>

#include <opencv2/core.hpp>

namespace rpc_localization {

// TIFF 基本尺寸信息。
struct TiffInfo {
    int width = 0;
    int height = 0;
};

// 读取影像宽度和高度，不把整幅大影像加载进内存。
TiffInfo read_tiff_info(const std::filesystem::path& path);

// 从条带式单波段 float32 TIFF 中读取指定矩形区域。
// x0、y0 是原影像左上角 0 基坐标，width、height 是像素宽高。
cv::Mat_<float> read_tiff_roi(
    const std::filesystem::path& path,
    int x0,
    int y0,
    int width,
    int height);

}  // namespace rpc_localization
