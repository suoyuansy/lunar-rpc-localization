#include "common/config.hpp"
#include "common/file_io.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <unordered_map>

namespace rpc_localization {
namespace {

std::string trim(const std::string& value) {
    const auto begin = value.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) {
        return {};
    }
    const auto end = value.find_last_not_of(" \t\r\n");
    return value.substr(begin, end - begin + 1);
}

std::vector<std::string> split_commas(const std::string& line) {
    std::vector<std::string> fields;
    std::stringstream stream(line);
    std::string field;
    while (std::getline(stream, field, ',')) {
        fields.push_back(trim(field));
    }
    return fields;
}

std::filesystem::path resolve_against_root(
    const std::filesystem::path& root,
    const std::string& value_text) {
    const auto value = std::filesystem::u8path(value_text);
    if (value.is_absolute()) {
        return value;
    }
    return root / value;
}

}  // namespace

std::filesystem::path find_project_root(const std::filesystem::path& start) {
    std::filesystem::path current = std::filesystem::absolute(start);
    if (!std::filesystem::is_directory(current)) {
        current = current.parent_path();
    }

    while (!current.empty()) {
        const bool has_build_file =
            std::filesystem::exists(current / "CMakeLists.txt");
        const bool has_config =
            std::filesystem::exists(current / "config" / "rpc_project.ini");
        if (has_build_file && has_config) {
            return current;
        }
        const auto parent = current.parent_path();
        if (parent == current) {
            break;
        }
        current = parent;
    }

    throw std::runtime_error(
        "无法从当前目录向上找到同时包含 CMakeLists.txt 和 config/rpc_project.ini 的项目根目录");
}

ProjectConfig load_project_config(const std::filesystem::path& config_path) {
    const auto root = find_project_root(
        config_path.empty() ? std::filesystem::current_path()
                            : config_path.parent_path());

    ProjectConfig config;
    config.project_root = root;
    config.image_dir = root / "data" / "images";
    config.rpc_dir = root / "data" / "rpc";
    config.measurement_dir = root / "output" / "measurements";
    config.fixed_height_dir = root / "output" / "rfm" / "fixed_height";
    config.two_image_dir = root / "output" / "rfm" / "two_image";
    config.target_table = root / "config" / "targets.csv";
    config.truth_file = root / "data" / "truth" / L"真值坐标.txt";

    const auto actual_config_path =
        config_path.empty() ? root / "config" / "rpc_project.ini" : config_path;
    std::ifstream input = open_input_file(actual_config_path);
    if (!input) {
        throw std::runtime_error(
            "无法打开参数文件: " + actual_config_path.u8string());
    }

    std::string line;
    while (std::getline(input, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#' || line[0] == ';') {
            continue;
        }
        const auto separator = line.find('=');
        if (separator == std::string::npos) {
            continue;
        }
        const std::string key = trim(line.substr(0, separator));
        const std::string value = trim(line.substr(separator + 1));

        if (key == "image_dir") {
            config.image_dir = resolve_against_root(root, value);
        } else if (key == "rpc_dir") {
            config.rpc_dir = resolve_against_root(root, value);
        } else if (key == "measurement_dir") {
            config.measurement_dir = resolve_against_root(root, value);
        } else if (key == "rfm_fixed_height_dir") {
            config.fixed_height_dir = resolve_against_root(root, value);
        } else if (key == "rfm_two_image_dir") {
            config.two_image_dir = resolve_against_root(root, value);
        } else if (key == "target_table") {
            config.target_table = resolve_against_root(root, value);
        } else if (key == "truth_file") {
            config.truth_file = resolve_against_root(root, value);
        } else if (key == "roi_size_m") {
            config.roi_size_m = std::stod(value);
        }
    }

    return config;
}

std::vector<TargetDefinition> load_target_table(
    const std::filesystem::path& path) {
    std::ifstream input = open_input_file(path);
    if (!input) {
        throw std::runtime_error("无法打开目标表: " + path.u8string());
    }

    std::vector<TargetDefinition> targets;
    std::string line;
    bool first_line = true;
    while (std::getline(input, line)) {
        if (first_line) {
            first_line = false;
            continue;
        }
        if (trim(line).empty()) {
            continue;
        }
        const auto fields = split_commas(line);
        if (fields.size() != 6) {
            throw std::runtime_error("目标表列数不正确: " + line);
        }
        TargetDefinition target;
        target.reflector_id = fields[0];
        target.truth_id = fields[1];
        target.image_1 = fields[2];
        target.image_2 = fields[3];
        target.resolution_1_mpp = std::stod(fields[4]);
        target.resolution_2_mpp = std::stod(fields[5]);
        targets.push_back(std::move(target));
    }
    return targets;
}

std::optional<ImageTargetMatch> find_target_by_image(
    const std::vector<TargetDefinition>& targets,
    const std::string& image_name) {
    for (const auto& target : targets) {
        if (target.image_1 == image_name) {
            return ImageTargetMatch{target, image_name, target.resolution_1_mpp};
        }
        if (target.image_2 == image_name) {
            return ImageTargetMatch{target, image_name, target.resolution_2_mpp};
        }
    }
    return std::nullopt;
}

std::vector<GeoPoint> load_truth_points(const std::filesystem::path& path) {
    std::ifstream input = open_input_file(path);
    if (!input) {
        throw std::runtime_error("无法打开真值文件: " + path.u8string());
    }

    std::vector<GeoPoint> points;
    std::string line;
    while (std::getline(input, line)) {
        std::stringstream stream(line);
        std::string id;
        GeoPoint point;
        if (stream >> id >> point.longitude_deg >> point.latitude_deg >>
            point.height_m) {
            points.push_back(point);
        }
    }
    return points;
}

std::optional<GeoPoint> find_truth_by_id(
    const std::filesystem::path& path,
    const std::string& truth_id) {
    std::ifstream input = open_input_file(path);
    if (!input) {
        throw std::runtime_error("无法打开真值文件: " + path.u8string());
    }

    std::string line;
    while (std::getline(input, line)) {
        std::stringstream stream(line);
        std::string id;
        GeoPoint point;
        if (stream >> id >> point.longitude_deg >> point.latitude_deg >>
            point.height_m) {
            if (id == truth_id) {
                return point;
            }
        }
    }
    return std::nullopt;
}

void ensure_parent_directory(const std::filesystem::path& path) {
    const auto parent = path.parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent);
    }
}

}  // namespace rpc_localization
