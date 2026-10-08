#include "accuracy/accuracy_app.hpp"

#include "common/config.hpp"
#include "common/file_io.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace rpc_localization {
namespace {

constexpr double kMoonRadiusM = 1737400.0;
constexpr double kPi = 3.14159265358979323846;

struct Options {
    std::filesystem::path config_path;
    std::vector<std::filesystem::path> inputs;
    std::vector<std::filesystem::path> result_dirs;
    std::filesystem::path truth_file;
    std::filesystem::path output;
    std::filesystem::path output_dir;
    bool show_help = false;
};

struct ResultRecord {
    std::filesystem::path path;
    std::string reflector_id;
    std::string method;
    std::string source_images;
    GeoPoint solved;
};

struct AccuracyRecord {
    ResultRecord result;
    GeoPoint truth;
    double delta_longitude_deg = 0.0;
    double delta_latitude_deg = 0.0;
    double delta_height_m = 0.0;
    double east_error_m = 0.0;
    double north_error_m = 0.0;
    double horizontal_distance_m = 0.0;
    double distance_3d_m = 0.0;
};

struct Statistics {
    std::string scope;
    std::size_t count = 0;
    double mean_horizontal_m = 0.0;
    double rmse_horizontal_m = 0.0;
    double mean_3d_m = 0.0;
    double rmse_3d_m = 0.0;
};

std::string trim(const std::string& value) {
    const auto begin = value.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) {
        return {};
    }
    const auto end = value.find_last_not_of(" \t\r\n");
    return value.substr(begin, end - begin + 1);
}

double parse_double(const std::string& text, const std::string& context) {
    try {
        std::size_t consumed = 0;
        const double value = std::stod(text, &consumed);
        if (consumed != text.size()) {
            throw std::invalid_argument("trailing characters");
        }
        return value;
    } catch (const std::exception&) {
        throw std::runtime_error("无法解析数值 " + context + ": " + text);
    }
}

void print_help() {
    std::cout
        << "用法: lunar_rpc_tool evaluate [选项]\n\n"
        << "选项:\n"
        << "  --config <文件>             指定 rpc_project.ini\n"
        << "  --input <文件>              指定 RFM 结果 TXT，可重复\n"
        << "  --result-dir <目录>         指定结果目录，可重复\n"
        << "  --truth-file <文件>         覆盖真值文件\n"
        << "  --output <文件>             指定最终报告 TXT\n"
        << "  --output-dir <目录>         指定报告输出目录\n"
        << "  --help                      显示帮助\n\n"
        << "默认行为:\n"
        << "  未指定输入时扫描 output/rfm/fixed_height 和 output/rfm/two_image。\n"
        << "  默认报告保存到 output/accuracy/accuracy_report.txt。\n";
}

Options parse_options(int argc, char** argv) {
    Options options;
    for (int i = 1; i < argc; ++i) {
        const std::string argument = argv[i];
        auto next = [&]() -> std::string {
            if (i + 1 >= argc) {
                throw std::runtime_error("参数缺少值: " + argument);
            }
            return argv[++i];
        };

        if (argument == "--config") {
            options.config_path = next();
        } else if (argument == "--input") {
            options.inputs.emplace_back(next());
        } else if (argument == "--result-dir") {
            options.result_dirs.emplace_back(next());
        } else if (argument == "--truth-file") {
            options.truth_file = next();
        } else if (argument == "--output") {
            options.output = next();
        } else if (argument == "--output-dir") {
            options.output_dir = next();
        } else if (argument == "--help" || argument == "-h") {
            options.show_help = true;
        } else {
            throw std::runtime_error("未知参数: " + argument);
        }
    }
    return options;
}

ResultRecord read_result(const std::filesystem::path& path) {
    std::ifstream input = open_input_file(path);
    if (!input) {
        throw std::runtime_error("无法打开 RFM 结果文件: " + path.u8string());
    }

    std::unordered_map<std::string, std::string> values;
    std::string line;
    while (std::getline(input, line)) {
        const auto separator = line.find('=');
        if (separator == std::string::npos) {
            continue;
        }
        values[trim(line.substr(0, separator))] =
            trim(line.substr(separator + 1));
    }

    auto require = [&](const std::string& key) -> const std::string& {
        const auto found = values.find(key);
        if (found == values.end()) {
            throw std::runtime_error(
                "结果文件缺少字段 " + key + ": " + path.u8string());
        }
        return found->second;
    };

    ResultRecord result;
    result.path = path;
    result.reflector_id = require("reflector_id");
    result.method = require("method");
    result.source_images = require("source_images");
    result.solved.longitude_deg =
        parse_double(require("longitude"), path.u8string() + " longitude");
    result.solved.latitude_deg =
        parse_double(require("latitude"), path.u8string() + " latitude");
    result.solved.height_m =
        parse_double(require("height_m"), path.u8string() + " height_m");
    return result;
}

std::vector<std::filesystem::path> txt_files_in_directory(
    const std::filesystem::path& directory) {
    if (!std::filesystem::exists(directory)) {
        throw std::runtime_error(
            "结果目录不存在: " + directory.u8string());
    }
    if (!std::filesystem::is_directory(directory)) {
        throw std::runtime_error(
            "结果路径不是目录: " + directory.u8string());
    }

    std::vector<std::filesystem::path> files;
    for (const auto& entry : std::filesystem::directory_iterator(directory)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        const std::string extension = entry.path().extension().string();
        if (extension == ".txt" || extension == ".TXT") {
            files.push_back(entry.path());
        }
    }
    std::sort(
        files.begin(),
        files.end(),
        [](const auto& left, const auto& right) {
            return left.filename().u8string() < right.filename().u8string();
        });
    return files;
}

std::vector<std::filesystem::path> collect_result_files(
    const Options& options,
    const ProjectConfig& config) {
    std::vector<std::filesystem::path> files = options.inputs;
    std::vector<std::filesystem::path> result_dirs = options.result_dirs;
    if (files.empty() && result_dirs.empty()) {
        result_dirs = {config.fixed_height_dir, config.two_image_dir};
    }

    for (const auto& directory : result_dirs) {
        const auto found = txt_files_in_directory(directory);
        if (found.empty()) {
            throw std::runtime_error(
                "结果目录中没有 TXT 文件: " + directory.u8string());
        }
        files.insert(files.end(), found.begin(), found.end());
    }

    std::set<std::string> seen;
    std::vector<std::filesystem::path> unique_files;
    for (const auto& file : files) {
        if (!std::filesystem::exists(file)) {
            throw std::runtime_error(
                "结果文件不存在: " + file.u8string());
        }
        const std::string key = std::filesystem::absolute(file).u8string();
        if (seen.insert(key).second) {
            unique_files.push_back(file);
        }
    }

    if (unique_files.empty()) {
        throw std::runtime_error("没有找到可评价的 RFM 结果文件");
    }
    return unique_files;
}

AccuracyRecord evaluate_record(
    const ResultRecord& result,
    const GeoPoint& truth) {
    AccuracyRecord record;
    record.result = result;
    record.truth = truth;
    record.delta_longitude_deg =
        result.solved.longitude_deg - truth.longitude_deg;
    record.delta_latitude_deg =
        result.solved.latitude_deg - truth.latitude_deg;
    record.delta_height_m = result.solved.height_m - truth.height_m;

    const double mean_latitude_rad =
        0.5 * (result.solved.latitude_deg + truth.latitude_deg) *
        kPi / 180.0;
    const double mean_height_m =
        0.5 * (result.solved.height_m + truth.height_m);
    const double radius = kMoonRadiusM + mean_height_m;
    record.east_error_m =
        radius * std::cos(mean_latitude_rad) *
        record.delta_longitude_deg * kPi / 180.0;
    record.north_error_m =
        radius * record.delta_latitude_deg * kPi / 180.0;
    record.horizontal_distance_m = std::hypot(
        record.east_error_m,
        record.north_error_m);
    record.distance_3d_m = std::hypot(
        record.horizontal_distance_m,
        record.delta_height_m);
    return record;
}

Statistics compute_statistics(
    const std::string& scope,
    const std::vector<const AccuracyRecord*>& records) {
    Statistics statistics;
    statistics.scope = scope;
    statistics.count = records.size();
    if (records.empty()) {
        return statistics;
    }

    double sum_horizontal = 0.0;
    double sum_horizontal_squared = 0.0;
    double sum_3d = 0.0;
    double sum_3d_squared = 0.0;
    for (const auto* record : records) {
        sum_horizontal += record->horizontal_distance_m;
        sum_horizontal_squared +=
            record->horizontal_distance_m * record->horizontal_distance_m;
        sum_3d += record->distance_3d_m;
        sum_3d_squared += record->distance_3d_m * record->distance_3d_m;
    }

    const double count = static_cast<double>(records.size());
    statistics.mean_horizontal_m = sum_horizontal / count;
    statistics.rmse_horizontal_m =
        std::sqrt(sum_horizontal_squared / count);
    statistics.mean_3d_m = sum_3d / count;
    statistics.rmse_3d_m = std::sqrt(sum_3d_squared / count);
    return statistics;
}

void write_statistics_table(
    std::ostringstream& report,
    const std::vector<Statistics>& statistics) {
    report << "scope\tcount\tmean_horizontal_m\trmse_horizontal_m"
              "\tmean_3d_m\trmse_3d_m\n";
    for (const auto& item : statistics) {
        report << item.scope << '\t'
               << item.count << '\t'
               << std::fixed << std::setprecision(4)
               << item.mean_horizontal_m << '\t'
               << item.rmse_horizontal_m << '\t'
               << item.mean_3d_m << '\t'
               << item.rmse_3d_m << '\n';
    }
}

void write_report(
    const std::filesystem::path& path,
    const std::string& content) {
    ensure_parent_directory(path);
    std::ofstream output = open_output_file(path);
    if (!output) {
        throw std::runtime_error("无法写精度报告: " + path.u8string());
    }
    output << content;
}

}  // namespace

int run_accuracy_evaluation_app(int argc, char** argv) {
    const Options options = parse_options(argc, argv);
    if (options.show_help) {
        print_help();
        return 0;
    }

    const ProjectConfig config = load_project_config(options.config_path);
    const auto result_files = collect_result_files(options, config);
    const auto targets = load_target_table(config.target_table);

    std::unordered_map<std::string, std::string> truth_ids;
    for (const auto& target : targets) {
        truth_ids[target.reflector_id] = target.truth_id;
    }

    const auto truth_file = options.truth_file.empty()
                                ? config.truth_file
                                : options.truth_file;
    std::unordered_map<std::string, GeoPoint> truth_cache;
    std::vector<AccuracyRecord> records;
    records.reserve(result_files.size());

    for (const auto& file : result_files) {
        const ResultRecord result = read_result(file);
        const auto mapping = truth_ids.find(result.reflector_id);
        if (mapping == truth_ids.end()) {
            throw std::runtime_error(
                "目标表中找不到反射器标识: " + result.reflector_id);
        }

        auto truth = truth_cache.find(result.reflector_id);
        if (truth == truth_cache.end()) {
            const auto loaded = find_truth_by_id(truth_file, mapping->second);
            if (!loaded) {
                throw std::runtime_error(
                    "真值文件中找不到 " + mapping->second +
                    "，结果文件: " + file.u8string());
            }
            truth = truth_cache.emplace(result.reflector_id, *loaded).first;
        }
        records.push_back(evaluate_record(result, truth->second));
    }

    std::map<std::pair<std::string, std::string>,
             std::vector<const AccuracyRecord*>>
        reflector_method_groups;
    std::map<std::string, std::vector<const AccuracyRecord*>> method_groups;
    std::vector<const AccuracyRecord*> all_records;
    for (const auto& record : records) {
        reflector_method_groups[
            {record.result.reflector_id, record.result.method}]
            .push_back(&record);
        method_groups[record.result.method].push_back(&record);
        all_records.push_back(&record);
    }

    std::vector<Statistics> grouped_statistics;
    for (const auto& [key, group] : reflector_method_groups) {
        grouped_statistics.push_back(compute_statistics(
            key.first + "/" + key.second,
            group));
    }
    for (const auto& [method, group] : method_groups) {
        grouped_statistics.push_back(compute_statistics(
            "method=" + method,
            group));
    }
    grouped_statistics.push_back(
        compute_statistics("all", all_records));

    std::ostringstream report;
    report << "# RPC Localization Accuracy Report\n\n";
    report << "moon_radius_m=" << std::fixed << std::setprecision(1)
           << kMoonRadiusM << "\n";
    report << "result_count=" << records.size() << "\n\n";

    report << "## Detailed Results\n";
    report << "input\treflector_id\tmethod\tsource_images"
              "\tsolved_longitude\tsolved_latitude\tsolved_height_m"
              "\ttruth_longitude\ttruth_latitude\ttruth_height_m"
              "\tdelta_longitude_deg\tdelta_latitude_deg\tdelta_height_m"
              "\teast_error_m\tnorth_error_m"
              "\thorizontal_distance_m\tdistance_3d_m\n";
    for (const auto& record : records) {
        report << record.result.path.u8string() << '\t'
               << record.result.reflector_id << '\t'
               << record.result.method << '\t'
               << record.result.source_images << '\t'
               << std::fixed << std::setprecision(10)
               << record.result.solved.longitude_deg << '\t'
               << record.result.solved.latitude_deg << '\t'
               << std::setprecision(4)
               << record.result.solved.height_m << '\t'
               << record.truth.longitude_deg << '\t'
               << record.truth.latitude_deg << '\t'
               << record.truth.height_m << '\t'
               << std::setprecision(10)
               << record.delta_longitude_deg << '\t'
               << record.delta_latitude_deg << '\t'
               << std::setprecision(4)
               << record.delta_height_m << '\t'
               << record.east_error_m << '\t'
               << record.north_error_m << '\t'
               << record.horizontal_distance_m << '\t'
               << record.distance_3d_m << '\n';
    }

    report << "\n## Group Statistics\n";
    write_statistics_table(report, grouped_statistics);

    const std::string content = report.str();
    std::cout << content;

    const auto output_path = options.output.empty()
                                 ? (options.output_dir.empty()
                                        ? config.accuracy_dir /
                                              "accuracy_report.txt"
                                        : options.output_dir /
                                              "accuracy_report.txt")
                                 : options.output;
    write_report(output_path, content);
    std::cout << "\n精度报告已保存: " << output_path.u8string() << '\n';
    return 0;
}

}  // namespace rpc_localization
