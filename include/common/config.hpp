#pragma once

#include "common/types.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace rpc_localization {

// 项目运行参数。
// 所有路径在读取配置后都会解析成相对项目根目录的绝对路径，程序内部
// 不再依赖调用者当前所在目录。
struct ProjectConfig {
    std::filesystem::path project_root;
    std::filesystem::path image_dir;
    std::filesystem::path rpc_dir;
    std::filesystem::path measurement_dir;
    std::filesystem::path fixed_height_dir;
    std::filesystem::path two_image_dir;
    std::filesystem::path accuracy_dir;
    std::filesystem::path target_table;
    std::filesystem::path truth_file;
    double roi_size_m = 500.0;
};

// 从给定目录开始向上查找项目根目录。
// 判定条件是同时存在 CMakeLists.txt 和 config/rpc_project.ini。
std::filesystem::path find_project_root(const std::filesystem::path& start);

// 读取 rpc_project.ini；未指定路径时自动使用项目默认配置文件。
ProjectConfig load_project_config(const std::filesystem::path& config_path = {});

// 读取 targets.csv，保存反射器、真值编号、影像对和影像分辨率。
std::vector<TargetDefinition> load_target_table(const std::filesystem::path& path);

// 根据影像名称查目标记录；找不到时返回空值。
std::optional<ImageTargetMatch> find_target_by_image(
    const std::vector<TargetDefinition>& targets,
    const std::string& image_name);

// 读取真值文件中的所有点。当前真值文件主要用于按目标编号查询。
std::vector<GeoPoint> load_truth_points(const std::filesystem::path& path);

// 根据真值编号查询经纬度高程；找不到时返回空值。
std::optional<GeoPoint> find_truth_by_id(
    const std::filesystem::path& path,
    const std::string& truth_id);

// 写文件前创建父目录，避免因为输出目录不存在而写盘失败。
void ensure_parent_directory(const std::filesystem::path& path);

}  // namespace rpc_localization
