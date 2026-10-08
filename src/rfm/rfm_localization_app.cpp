#include "rfm/rfm_localization_app.hpp"

#include "common/config.hpp"
#include "common/file_io.hpp"
#include "rfm/rfm_solver.hpp"
#include "rfm/rpc_model.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace rpc_localization {
namespace {

struct Options {
    std::filesystem::path config_path;
    std::string method;
    std::string target;
    std::vector<std::string> image_names;
    std::filesystem::path measurement_dir;
    std::filesystem::path rpc_dir;
    std::filesystem::path output_dir;
    bool all = false;
    bool show_help = false;
};

struct InitialPoint {
    double longitude_deg = 0.0;
    double latitude_deg = 0.0;
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

std::string image_stem(const std::string& image_name) {
    return std::filesystem::path(image_name).stem().string();
}

void print_help() {
    std::cout
        << "用法: lunar_rpc_tool localize --method <方法> [选项]\n\n"
        << "方法:\n"
        << "  fixed_height   已知高程，单景反算经纬度\n"
        << "  two_image      双影像联合反算经纬度和高程\n\n"
        << "选项:\n"
        << "  --config <文件>             指定 rpc_project.ini\n"
        << "  --target <目标名称>         指定目标，例如 Apollo11\n"
        << "  --image-name <影像名称>     可重复，指定一景或两景影像\n"
        << "  --measurement-dir <目录>    覆盖量测目录\n"
        << "  --rpc-dir <目录>            覆盖 RPC 目录\n"
        << "  --output-dir <目录>         覆盖结果输出目录\n"
        << "  --all                       批量处理该方法的全部目标或影像\n"
        << "  --help                      显示帮助\n\n"
        << "示例:\n"
        << "  lunar_rpc_tool localize --method fixed_height --image-name M175124932RE\n"
        << "  lunar_rpc_tool localize --method two_image --target Apollo11\n"
        << "  lunar_rpc_tool localize --method fixed_height --all\n"
        << "  lunar_rpc_tool localize --method two_image --all\n";
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
        } else if (argument == "--target") {
            options.target = next();
        } else if (argument == "--image-name") {
            options.image_names.push_back(next());
        } else if (argument == "--measurement-dir") {
            options.measurement_dir = next();
        } else if (argument == "--rpc-dir") {
            options.rpc_dir = next();
        } else if (argument == "--output-dir") {
            options.output_dir = next();
        } else if (argument == "--all") {
            options.all = true;
        } else if (argument == "--help" || argument == "-h") {
            options.show_help = true;
        } else {
            throw std::runtime_error("未知参数: " + argument);
        }
    }
    return options;
}

PixelPoint read_measurement(const std::filesystem::path& path) {
    std::ifstream input = open_input_file(path);
    if (!input) {
        throw std::runtime_error("无法打开量测文件: " + path.u8string());
    }

    bool has_sample = false;
    bool has_line = false;
    PixelPoint point;
    std::string line;
    while (std::getline(input, line)) {
        const auto separator = line.find('=');
        if (separator == std::string::npos) {
            continue;
        }
        const std::string key = trim(line.substr(0, separator));
        const std::string value = trim(line.substr(separator + 1));
        if (key == "sample") {
            point.sample = parse_double(value, path.u8string() + " sample");
            has_sample = true;
        } else if (key == "line") {
            point.line = parse_double(value, path.u8string() + " line");
            has_line = true;
        }
    }

    if (!has_sample || !has_line) {
        throw std::runtime_error(
            "量测文件缺少 sample 或 line 字段: " + path.u8string());
    }
    return point;
}

InitialPoint read_initial_point(const std::filesystem::path& path) {
    std::ifstream input = open_input_file(path);
    if (!input) {
        throw std::runtime_error(
            "无法打开方案一结果文件: " + path.u8string());
    }

    bool has_longitude = false;
    bool has_latitude = false;
    InitialPoint point;
    std::string line;
    while (std::getline(input, line)) {
        const auto separator = line.find('=');
        if (separator == std::string::npos) {
            continue;
        }
        const std::string key = trim(line.substr(0, separator));
        const std::string value = trim(line.substr(separator + 1));
        if (key == "longitude") {
            point.longitude_deg =
                parse_double(value, path.u8string() + " longitude");
            has_longitude = true;
        } else if (key == "latitude") {
            point.latitude_deg =
                parse_double(value, path.u8string() + " latitude");
            has_latitude = true;
        }
    }

    if (!has_longitude || !has_latitude) {
        throw std::runtime_error(
            "方案一结果文件缺少 longitude 或 latitude 字段: " +
            path.u8string());
    }
    return point;
}

void write_result(
    const std::filesystem::path& path,
    const std::string& reflector_id,
    const std::string& method,
    const std::vector<std::string>& source_images,
    const RfmSolution& solution) {
    ensure_parent_directory(path);
    std::ofstream output = open_output_file(path);
    if (!output) {
        throw std::runtime_error("无法写 RFM 结果文件: " + path.u8string());
    }

    std::string source_image_text;
    for (std::size_t i = 0; i < source_images.size(); ++i) {
        if (i != 0) {
            source_image_text += ",";
        }
        source_image_text += source_images[i];
    }

    output << "reflector_id=" << reflector_id << '\n'
           << "method=" << method << '\n'
           << "solver=LM\n"
           << "source_images=" << source_image_text << '\n'
           << std::fixed << std::setprecision(10)
           << "longitude=" << solution.point.longitude_deg << '\n'
           << "latitude=" << solution.point.latitude_deg << '\n'
           << std::setprecision(4)
           << "height_m=" << solution.point.height_m << '\n';
}

void print_solution(
    const std::string& label,
    const std::string& reflector_id,
    const RfmSolution& solution) {
    std::cout << label << " reflector_id=" << reflector_id
              << ", converged=" << (solution.converged ? "true" : "false")
              << ", iterations=" << solution.iterations
              << ", residual_rms_px=" << solution.residual_rms_px
              << ", condition_number=" << solution.condition_number
              << ", solver=LM"
              << std::fixed << std::setprecision(10)
              << ", longitude=" << solution.point.longitude_deg
              << ", latitude=" << solution.point.latitude_deg
              << std::setprecision(4)
              << ", height_m=" << solution.point.height_m
              << std::endl;
}

std::optional<TargetDefinition> find_target_by_id(
    const std::vector<TargetDefinition>& targets,
    const std::string& reflector_id) {
    for (const auto& target : targets) {
        if (target.reflector_id == reflector_id) {
            return target;
        }
    }
    return std::nullopt;
}

std::set<std::string> normalized_image_set(
    const std::vector<std::string>& image_names) {
    std::set<std::string> result;
    for (const auto& image_name : image_names) {
        result.insert(image_stem(image_name));
    }
    return result;
}

std::filesystem::path resolve_measurement_dir(
    const Options& options,
    const ProjectConfig& config) {
    return options.measurement_dir.empty() ? config.measurement_dir
                                           : options.measurement_dir;
}

std::filesystem::path resolve_rpc_dir(
    const Options& options,
    const ProjectConfig& config) {
    return options.rpc_dir.empty() ? config.rpc_dir : options.rpc_dir;
}

void run_fixed_height(
    const Options& options,
    const ProjectConfig& config,
    const std::vector<TargetDefinition>& targets) {
    if (options.image_names.size() != 1) {
        throw std::runtime_error(
            "fixed_height 必须且只能指定一处 --image-name");
    }

    const std::string image = image_stem(options.image_names.front());
    const auto match = find_target_by_image(targets, image);
    if (!match) {
        throw std::runtime_error(
            "目标表中找不到影像对应的记录: " + image);
    }
    if (!options.target.empty() &&
        match->target.reflector_id != options.target) {
        throw std::runtime_error(
            "影像 " + image + " 不属于目标 " + options.target);
    }

    const auto truth = find_truth_by_id(
        config.truth_file,
        match->target.truth_id);
    if (!truth) {
        throw std::runtime_error(
            "真值文件中找不到目标: " + match->target.truth_id);
    }

    const auto measurement_path =
        resolve_measurement_dir(options, config) /
        (image + "_point_measurement.txt");
    if (!std::filesystem::exists(measurement_path)) {
        throw std::runtime_error(
            "缺少像点量测文件: " + measurement_path.u8string());
    }

    const auto rpc_path = resolve_rpc_dir(options, config) /
                          (image + "_rpc.txt");
    if (!std::filesystem::exists(rpc_path)) {
        throw std::runtime_error("缺少 RPC 文件: " + rpc_path.u8string());
    }

    const RpcModel rpc = RpcModel::from_file(rpc_path);
    const PixelPoint observed = read_measurement(measurement_path);
    const RfmSolution solution = solve_fixed_height(
        rpc,
        observed,
        truth->height_m);
    print_solution("方案一:", match->target.reflector_id, solution);
    if (!solution.converged) {
        throw std::runtime_error("方案一 LM 未收敛，不写出结果文件");
    }

    const auto output_dir = options.output_dir.empty()
                                ? config.fixed_height_dir
                                : options.output_dir;
    const auto output_path =
        output_dir / (image + "_rfm_fixed_height.txt");
    write_result(
        output_path,
        match->target.reflector_id,
        "fixed_height",
        {image},
        solution);
    std::cout << "方案一结果已保存: " << output_path.u8string() << '\n';
}

void run_two_image(
    const Options& options,
    const ProjectConfig& config,
    const std::vector<TargetDefinition>& targets) {
    TargetDefinition target;
    bool has_target = false;

    if (!options.target.empty()) {
        const auto found = find_target_by_id(targets, options.target);
        if (!found) {
            throw std::runtime_error(
                "目标表中找不到目标: " + options.target);
        }
        target = *found;
        has_target = true;
    }

    std::vector<std::string> image_names;
    if (options.image_names.empty()) {
        if (!has_target) {
            throw std::runtime_error(
                "two_image 需要 --target 或两个 --image-name");
        }
        image_names = {target.image_1, target.image_2};
    } else {
        image_names = options.image_names;
    }

    if (image_names.size() != 2) {
        throw std::runtime_error(
            "two_image 必须恰好指定两景影像");
    }

    image_names[0] = image_stem(image_names[0]);
    image_names[1] = image_stem(image_names[1]);
    if (image_names[0] == image_names[1]) {
        throw std::runtime_error("two_image 的两景影像不能相同");
    }

    if (!has_target) {
        const auto match_1 = find_target_by_image(targets, image_names[0]);
        const auto match_2 = find_target_by_image(targets, image_names[1]);
        if (!match_1 || !match_2) {
            throw std::runtime_error(
                "目标表中找不到指定影像对应的记录");
        }
        if (match_1->target.reflector_id != match_2->target.reflector_id) {
            throw std::runtime_error(
                "两景影像不属于同一个目标");
        }
        target = match_1->target;
        has_target = true;
    }

    const std::set<std::string> requested_images =
        normalized_image_set(options.image_names);
    const std::set<std::string> target_images = {
        target.image_1,
        target.image_2,
    };
    if (!requested_images.empty() && requested_images != target_images) {
        throw std::runtime_error(
            "指定的影像与目标 " + target.reflector_id + " 不匹配");
    }

    const auto measurement_dir = resolve_measurement_dir(options, config);
    const auto rpc_dir = resolve_rpc_dir(options, config);
    std::vector<PixelPoint> observations;
    std::vector<RpcModel> rpc_models;
    std::vector<InitialPoint> initial_points;

    for (const auto& image : image_names) {
        const auto measurement_path =
            measurement_dir / (image + "_point_measurement.txt");
        if (!std::filesystem::exists(measurement_path)) {
            throw std::runtime_error(
                "缺少像点量测文件: " + measurement_path.u8string());
        }
        observations.push_back(read_measurement(measurement_path));

        const auto rpc_path = rpc_dir / (image + "_rpc.txt");
        if (!std::filesystem::exists(rpc_path)) {
            throw std::runtime_error("缺少 RPC 文件: " + rpc_path.u8string());
        }
        rpc_models.push_back(RpcModel::from_file(rpc_path));

        const auto fixed_path =
            config.fixed_height_dir /
            (image + "_rfm_fixed_height.txt");
        if (!std::filesystem::exists(fixed_path)) {
            throw std::runtime_error(
                "缺少方案一初值结果: " + fixed_path.u8string());
        }
        initial_points.push_back(read_initial_point(fixed_path));
    }

    GeoPoint initial;
    initial.longitude_deg =
        0.5 * (initial_points[0].longitude_deg +
               initial_points[1].longitude_deg);
    initial.latitude_deg =
        0.5 * (initial_points[0].latitude_deg +
               initial_points[1].latitude_deg);
    initial.height_m =
        0.5 * (rpc_models[0].height_offset() +
               rpc_models[1].height_offset());

    std::cout << "方案二初值: longitude=" << std::setprecision(10)
              << initial.longitude_deg
              << ", latitude=" << initial.latitude_deg
              << ", height_m=" << initial.height_m << std::endl;

    const RfmSolution solution = solve_two_image(
        rpc_models[0],
        rpc_models[1],
        observations[0],
        observations[1],
        initial);
    print_solution("方案二:", target.reflector_id, solution);
    if (!solution.converged) {
        throw std::runtime_error("方案二 LM 未收敛，不写出结果文件");
    }

    const auto output_dir = options.output_dir.empty()
                                ? config.two_image_dir
                                : options.output_dir;
    const auto output_path =
        output_dir / (target.reflector_id + "_rfm_two_image.txt");
    write_result(
        output_path,
        target.reflector_id,
        "two_image",
        image_names,
        solution);
    std::cout << "方案二结果已保存: " << output_path.u8string() << '\n';
}

void run_all_fixed_height(
    const Options& options,
    const ProjectConfig& config,
    const std::vector<TargetDefinition>& targets) {
    std::vector<std::pair<std::string, std::string>> images;
    images.reserve(targets.size() * 2);
    for (const auto& target : targets) {
        images.emplace_back(target.image_1, target.reflector_id);
        images.emplace_back(target.image_2, target.reflector_id);
    }

    std::size_t success_count = 0;
    std::size_t failure_count = 0;
    for (std::size_t i = 0; i < images.size(); ++i) {
        const auto& [image, reflector_id] = images[i];
        std::cout << "\n[fixed_height " << (i + 1) << '/' << images.size()
                  << "] image=" << image
                  << " reflector=" << reflector_id << std::endl;

        Options single_options = options;
        single_options.all = false;
        single_options.target = reflector_id;
        single_options.image_names = {image};
        try {
            run_fixed_height(single_options, config, targets);
            ++success_count;
        } catch (const std::exception& error) {
            ++failure_count;
            std::cerr << "错误: " << error.what() << '\n';
        }
    }

    std::cout << "\n批量 fixed_height 完成: 成功 " << success_count
              << "，失败 " << failure_count << "。" << std::endl;
    if (failure_count != 0) {
        throw std::runtime_error("批量 fixed_height 存在失败项");
    }
}

void run_all_two_image(
    const Options& options,
    const ProjectConfig& config,
    const std::vector<TargetDefinition>& targets) {
    std::size_t success_count = 0;
    std::size_t failure_count = 0;
    for (std::size_t i = 0; i < targets.size(); ++i) {
        const auto& target = targets[i];
        std::cout << "\n[two_image " << (i + 1) << '/' << targets.size()
                  << "] reflector=" << target.reflector_id
                  << " images=" << target.image_1 << ","
                  << target.image_2 << std::endl;

        Options single_options = options;
        single_options.all = false;
        single_options.target = target.reflector_id;
        single_options.image_names.clear();
        try {
            run_two_image(single_options, config, targets);
            ++success_count;
        } catch (const std::exception& error) {
            ++failure_count;
            std::cerr << "错误: " << error.what() << '\n';
        }
    }

    std::cout << "\n批量 two_image 完成: 成功 " << success_count
              << "，失败 " << failure_count << "。" << std::endl;
    if (failure_count != 0) {
        throw std::runtime_error("批量 two_image 存在失败项");
    }
}

}  // namespace

int run_rfm_localization_app(int argc, char** argv) {
    const Options options = parse_options(argc, argv);
    if (options.show_help) {
        print_help();
        return 0;
    }

    if (options.method.empty()) {
        throw std::runtime_error(
            "必须指定 --method fixed_height 或 --method two_image");
    }
    if (options.all &&
        (!options.target.empty() || !options.image_names.empty())) {
        throw std::runtime_error(
            "--all 不能与 --target 或 --image-name 同时使用");
    }

    const ProjectConfig config = load_project_config(options.config_path);
    const std::vector<TargetDefinition> targets =
        load_target_table(config.target_table);

    if (options.method == "fixed_height") {
        if (options.all) {
            run_all_fixed_height(options, config, targets);
        } else {
            run_fixed_height(options, config, targets);
        }
    } else if (options.method == "two_image") {
        if (options.all) {
            run_all_two_image(options, config, targets);
        } else {
            run_two_image(options, config, targets);
        }
    } else {
        throw std::runtime_error(
            "未知 --method: " + options.method +
            "，可选值为 fixed_height 或 two_image");
    }
    return 0;
}

}  // namespace rpc_localization
