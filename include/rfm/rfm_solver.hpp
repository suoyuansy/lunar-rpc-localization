#pragma once

#include "common/types.hpp"
#include "rfm/rpc_model.hpp"

namespace rpc_localization {

struct RfmSolution {
    GeoPoint point;
    double residual_rms_px = 0.0;
    double condition_number = 0.0;
    int iterations = 0;
    bool converged = false;
};

RfmSolution solve_fixed_height(
    const RpcModel& rpc,
    const PixelPoint& observed,
    double fixed_height_m);

RfmSolution solve_two_image(
    const RpcModel& rpc_1,
    const RpcModel& rpc_2,
    const PixelPoint& observed_1,
    const PixelPoint& observed_2,
    const GeoPoint& initial);

}  // namespace rpc_localization
