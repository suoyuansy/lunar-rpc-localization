#pragma once

#include <filesystem>
#include <fstream>

namespace rpc_localization {

// 统一使用二进制方式打开输入文件。
// Windows 下使用宽字符路径，避免中文目录或中文文件名乱码。
inline std::ifstream open_input_file(const std::filesystem::path& path) {
#ifdef _WIN32
    return std::ifstream(path.wstring().c_str(), std::ios::binary);
#else
    return std::ifstream(path, std::ios::binary);
#endif
}

// 统一使用二进制方式打开输出文件。
// 同名文件会被截断覆盖，这是量测结果覆盖旧结果的预期行为。
inline std::ofstream open_output_file(const std::filesystem::path& path) {
#ifdef _WIN32
    return std::ofstream(path.wstring().c_str(), std::ios::binary);
#else
    return std::ofstream(path, std::ios::binary);
#endif
}

}  // namespace rpc_localization
