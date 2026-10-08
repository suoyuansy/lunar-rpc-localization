#pragma once

#include "common/types.hpp"

#include <filesystem>

namespace rpc_localization {

// 将最终确认的像点写入量测 TXT。
// 文件只保存 sample 和 line 两行，坐标使用原影像 0 基像素坐标。
void write_measurement_txt(
    const std::filesystem::path& path,
    const PixelPoint& point);

}  // namespace rpc_localization
