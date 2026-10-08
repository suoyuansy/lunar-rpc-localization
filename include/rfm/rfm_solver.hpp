#pragma once

#include "common/types.hpp"
#include "rfm/rpc_model.hpp"

namespace rpc_localization {

// RFM 反算结果。
// converged 表示 LM 满足数值停止条件，residual_rms_px 表示像点残差大小，
// condition_number 用于判断解算问题是否病态。
struct RfmSolution {
    GeoPoint point;
    double residual_rms_px = 0.0;
    double condition_number = 0.0;
    int iterations = 0;
    bool converged = false;
};

// 方案一：固定高程，使用 LM 反算单景影像对应的经纬度。
RfmSolution solve_fixed_height(
    const RpcModel& rpc,
    const PixelPoint& observed,
    double fixed_height_m);

// 方案二：使用两景影像的 RPC 和量测点联合反算经纬度和高程。
// initial 通常由两景方案一结果平均得到。
RfmSolution solve_two_image(
    const RpcModel& rpc_1,
    const RpcModel& rpc_2,
    const PixelPoint& observed_1,
    const PixelPoint& observed_2,
    const GeoPoint& initial);

}  // namespace rpc_localization
