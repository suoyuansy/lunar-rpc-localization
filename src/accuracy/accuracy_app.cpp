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
    std::string method;
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
        << "  --method <方法>             必选：fixed_height 或 two_image\n"
        << "  --input <文件>              指定 RFM 结果 TXT，可重复\n"
        << "  --result-dir <目录>         指定结果目录，可重复\n"
        << "  --truth-file <文件>         覆盖真值文件\n"
        << "  --output <文件>             指定最终报告 TXT\n"
        << "  --output-dir <目录>         指定报告输出目录\n"
        << "  --help                      显示帮助\n\n"
        << "默认行为:\n"
        << "  未指定输入时只扫描所选方法对应的结果目录。\n"
        << "  默认报告保存为 output/accuracy/<method>_accuracy_report.txt。\n";
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
        } else if (argument == "--method") {
            options.method = next();
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
    const ProjectConfig& config,
    const std::string& method) {
    std::vector<std::filesystem::path> files = options.inputs;
    std::vector<std::filesystem::path> result_dirs = options.result_dirs;
    if (files.empty() && result_dirs.empty()) {
        result_dirs = {
            method == "fixed_height"
                ? config.fixed_height_dir
                : config.two_image_dir,
        };
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
    report << "| scope | count | mean_horizontal_m | rmse_horizontal_m "
              "| mean_3d_m | rmse_3d_m |\n";
    report << "|---|---:|---:|---:|---:|---:|\n";
    for (const auto& item : statistics) {
        report << "| " << item.scope
               << " | " << item.count
               << std::fixed << std::setprecision(4)
               << " | " << item.mean_horizontal_m
               << " | " << item.rmse_horizontal_m
               << " | " << item.mean_3d_m
               << " | " << item.rmse_3d_m << " |\n";
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
    if (options.method.empty()) {
        throw std::runtime_error(
            "evaluate 必须指定 --method fixed_height 或 --method two_image");
    }
    if (options.method != "fixed_height" &&
        options.method != "two_image") {
        throw std::runtime_error(
            "未知 --method: " + options.method +
            "，可选值为 fixed_height 或 two_image");
    }

    const ProjectConfig config = load_project_config(options.config_path);
    const auto result_files =
        collect_result_files(options, config, options.method);
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
        if (result.method != options.method) {
            throw std::runtime_error(
                "结果文件方法不匹配: " + file.u8string() +
                "，文件 method=" + result.method +
                "，评价 method=" + options.method);
        }
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

    std::map<std::string, std::vector<const AccuracyRecord*>> reflector_groups;
    std::vector<const AccuracyRecord*> all_records;
    for (const auto& record : records) {
        reflector_groups[record.result.reflector_id].push_back(&record);
        all_records.push_back(&record);
    }

    std::vector<Statistics> grouped_statistics;
    for (const auto& [reflector_id, group] : reflector_groups) {
        grouped_statistics.push_back(compute_statistics(
            "reflector=" + reflector_id,
            group));
    }
    grouped_statistics.push_back(
        compute_statistics("method=" + options.method, all_records));

    std::ostringstream report;
    report << "# RPC Localization Accuracy Report\n\n";
    report << "method=" << options.method << '\n';
    report << "moon_radius_m=" << std::fixed << std::setprecision(1)
           << kMoonRadiusM << "\n";
    report << "result_count=" << records.size() << "\n\n";

    report << "## Detailed Results\n\n";
    report << "| # | file | reflector_id | source_images "
              "| solved_lon(deg) | solved_lat(deg) | solved_h(m) "
              "| truth_lon(deg) | truth_lat(deg) | truth_h(m) "
              "| dlon(deg) | dlat(deg) | dh(m) "
              "| east(m) | north(m) | horizontal(m) | distance_3d(m) |\n";
    report << "|---:|---|---|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|\n";
    for (std::size_t i = 0; i < records.size(); ++i) {
        const auto& record = records[i];
        report << "| " << (i + 1)
               << " | " << record.result.path.filename().u8string()
               << " | " << record.result.reflector_id
               << " | " << record.result.source_images
               << " | " << std::fixed << std::setprecision(10)
               << record.result.solved.longitude_deg
               << " | " << record.result.solved.latitude_deg
               << " | " << std::setprecision(4)
               << record.result.solved.height_m
               << " | " << std::setprecision(10)
               << record.truth.longitude_deg
               << " | " << record.truth.latitude_deg
               << " | " << std::setprecision(4)
               << record.truth.height_m
               << " | " << std::setprecision(10)
               << record.delta_longitude_deg
               << " | " << record.delta_latitude_deg
               << " | " << std::setprecision(4)
               << record.delta_height_m
               << " | " << record.east_error_m
               << " | " << record.north_error_m
               << " | " << record.horizontal_distance_m
               << " | " << record.distance_3d_m << " |\n";
    }

    report << "\n## Group Statistics\n\n";
    write_statistics_table(report, grouped_statistics);

    report << "\n## Metric Definitions\n\n";
    report << "- `dlon` / `dlat`: solved minus truth longitude/latitude, degree.\n";
    report << "- `dh`: solved minus truth height, meter.\n";
    report << "- `east`: east-west lunar surface error, meter.\n";
    report << "- `north`: north-south lunar surface error, meter.\n";
    report << "- `horizontal`: sqrt(east^2 + north^2), meter.\n";
    report << "- `distance_3d`: sqrt(horizontal^2 + dh^2), meter.\n";
    report << "- Mean/RMSE statistics are computed over all evaluated files.\n";

    const std::string content = report.str();
    std::cout << content;

    const auto output_path = options.output.empty()
                                 ? (options.output_dir.empty()
                                        ? config.accuracy_dir /
                                              (options.method +
                                               "_accuracy_report.txt")
                                        : options.output_dir /
                                              (options.method +
                                               "_accuracy_report.txt"))
                                 : options.output;
    write_report(output_path, content);
    std::cout << "\n精度报告已保存: " << output_path.u8string() << '\n';
    return 0;
}

}  // namespace rpc_localization
