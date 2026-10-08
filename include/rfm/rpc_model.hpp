#pragma once

#include "common/types.hpp"

#include <array>
#include <filesystem>

namespace rpc_localization {

// RPC 有理函数模型。
// 该类负责读取 RPC 文件，并根据经纬度高程正算 sample、line 像点。
class RpcModel {
public:
    // 从 RPC 文本文件读取全部归一化参数和 80 个有理函数系数。
    static RpcModel from_file(const std::filesystem::path& path);

    // 正算：经纬度高程 -> 影像 sample、line。
    PixelPoint forward(const GeoPoint& point) const;

    // 暴露归一化偏移量，供初值选择和诊断使用。
    double longitude_offset() const {
        return long_off_;
    }

    double latitude_offset() const {
        return lat_off_;
    }

    double height_offset() const {
        return height_off_;
    }

private:
    std::array<double, 20> line_num_{};
    std::array<double, 20> line_den_{};
    std::array<double, 20> samp_num_{};
    std::array<double, 20> samp_den_{};
    double line_off_ = 0.0;
    double line_scale_ = 0.0;
    double samp_off_ = 0.0;
    double samp_scale_ = 0.0;
    double lat_off_ = 0.0;
    double lat_scale_ = 0.0;
    double long_off_ = 0.0;
    double long_scale_ = 0.0;
    double height_off_ = 0.0;
    double height_scale_ = 0.0;
};

}  // namespace rpc_localization
