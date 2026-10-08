#pragma once

#include <filesystem>
#include <fstream>

namespace rpc_localization {

inline std::ifstream open_input_file(const std::filesystem::path& path) {
#ifdef _WIN32
    return std::ifstream(path.wstring().c_str(), std::ios::binary);
#else
    return std::ifstream(path, std::ios::binary);
#endif
}

inline std::ofstream open_output_file(const std::filesystem::path& path) {
#ifdef _WIN32
    return std::ofstream(path.wstring().c_str(), std::ios::binary);
#else
    return std::ofstream(path, std::ios::binary);
#endif
}

}  // namespace rpc_localization

