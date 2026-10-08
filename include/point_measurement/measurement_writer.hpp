#pragma once

#include "common/types.hpp"

#include <filesystem>

namespace rpc_localization {

void write_measurement_txt(
    const std::filesystem::path& path,
    const PixelPoint& point);

}  // namespace rpc_localization

