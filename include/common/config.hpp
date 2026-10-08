#pragma once

#include "common/types.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace rpc_localization {

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

std::filesystem::path find_project_root(const std::filesystem::path& start);

ProjectConfig load_project_config(const std::filesystem::path& config_path = {});

std::vector<TargetDefinition> load_target_table(const std::filesystem::path& path);

std::optional<ImageTargetMatch> find_target_by_image(
    const std::vector<TargetDefinition>& targets,
    const std::string& image_name);

std::vector<GeoPoint> load_truth_points(const std::filesystem::path& path);

std::optional<GeoPoint> find_truth_by_id(
    const std::filesystem::path& path,
    const std::string& truth_id);

void ensure_parent_directory(const std::filesystem::path& path);

}  // namespace rpc_localization
