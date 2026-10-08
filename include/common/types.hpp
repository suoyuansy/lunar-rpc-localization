#pragma once

#include <string>

namespace rpc_localization {

// 月面地理坐标：经度、纬度单位均为度，高程单位为米。
struct GeoPoint {
    double longitude_deg = 0.0;
    double latitude_deg = 0.0;
    double height_m = 0.0;
};

// 影像像点坐标：sample 是列方向坐标，line 是行方向坐标，均从 0 开始。
// sample、line 允许为小数，用来表示亚像素位置。
struct PixelPoint {
    double sample = 0.0;
    double line = 0.0;
};

// targets.csv 中的一行，描述一个反射器及其两景影像和分辨率。
struct TargetDefinition {
    std::string reflector_id;
    std::string truth_id;
    std::string image_1;
    std::string image_2;
    double resolution_1_mpp = 0.0;
    double resolution_2_mpp = 0.0;
};

// 根据影像名称查到的目标记录，同时带回该影像对应的分辨率。
struct ImageTargetMatch {
    TargetDefinition target;
    std::string image_name;
    double resolution_mpp = 0.0;
};

}  // namespace rpc_localization
