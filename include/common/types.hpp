#pragma once

#include <string>

namespace rpc_localization {

struct GeoPoint {
    double longitude_deg = 0.0;
    double latitude_deg = 0.0;
    double height_m = 0.0;
};

struct PixelPoint {
    double sample = 0.0;
    double line = 0.0;
};

struct TargetDefinition {
    std::string reflector_id;
    std::string truth_id;
    std::string image_1;
    std::string image_2;
    double resolution_1_mpp = 0.0;
    double resolution_2_mpp = 0.0;
};

struct ImageTargetMatch {
    TargetDefinition target;
    std::string image_name;
    double resolution_mpp = 0.0;
};

}  // namespace rpc_localization

