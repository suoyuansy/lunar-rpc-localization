#include "point_measurement/measurement_writer.hpp"

#include "common/config.hpp"
#include "common/file_io.hpp"

#include <cmath>
#include <fstream>
#include <iomanip>
#include <stdexcept>

namespace rpc_localization {

void write_measurement_txt(
    const std::filesystem::path& path,
    const PixelPoint& point) {
    ensure_parent_directory(path);

    // 使用截断模式写入：同名量测文件会被新结果直接覆盖。
    std::ofstream output = open_output_file(path);
    if (!output) {
        throw std::runtime_error("无法写量测文件: " + path.u8string());
    }

    // 像点允许亚像素，因此保留三位小数。
    output << std::fixed << std::setprecision(3)
           << "sample=" << point.sample << '\n';
    output << "line=" << point.line << '\n';
}

}  // namespace rpc_localization
