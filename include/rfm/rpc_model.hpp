#pragma once

#include "common/types.hpp"

#include <array>
#include <filesystem>

namespace rpc_localization {

class RpcModel {
public:
    static RpcModel from_file(const std::filesystem::path& path);

    PixelPoint forward(const GeoPoint& point) const;

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

