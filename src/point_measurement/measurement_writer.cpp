#include "point_measurement/measurement_writer.hpp"

#include "common/config.hpp"
#include "common/file_io.hpp"

#include <cmath>
#include <fstream>
#include <stdexcept>

namespace rpc_localization {

void write_measurement_txt(
    const std::filesystem::path& path,
    const PixelPoint& point) {
    ensure_parent_directory(path);

    std::ofstream output = open_output_file(path);
    if (!output) {
        throw std::runtime_error("无法写量测文件: " + path.u8string());
    }

    output << "sample=" << static_cast<long long>(std::llround(point.sample))
           << '\n';
    output << "line=" << static_cast<long long>(std::llround(point.line))
           << '\n';
}

}  // namespace rpc_localization
