#include "point_measurement/point_measurement_app.hpp"

#include "common/config.hpp"
#include "point_measurement/detector.hpp"
#include "point_measurement/measurement_writer.hpp"
#include "point_measurement/tiff_roi_reader.hpp"
#include "rfm/rpc_model.hpp"

#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <limits>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace rpc_localization {
namespace {

struct Options {
    std::filesystem::path config_path;
    std::filesystem::path image_path;
    std::string image_name;
    std::filesystem::path image_dir;
    std::filesystem::path output_dir;
    bool auto_only = false;
    bool show_help = false;
};

struct UiState {
    cv::Mat display_base;
    cv::Mat_<float> image;
    cv::Point2d selected;
    double scale = 1.0;
    bool confirmed = false;
    bool cancelled = false;
};

void print_help() {
    std::cout
        << "用法: lunar_rpc_tool measure [选项]\n"
        << "  --config <文件>       指定 rpc_project.ini\n"
        << "  --image <文件>        直接指定原始 TIFF\n"
        << "  --image-name <名称>   用配置目录查找 TIFF\n"
        << "  --image-dir <目录>    覆盖目录；单独使用时批量处理所有 TIFF\n"
        << "  --output-dir <目录>   覆盖量测输出目录\n"
        << "  --auto-only           只打印自动候选，不打开窗口，不写结果\n"
        << "  --help                显示帮助\n\n"
        << "交互: 鼠标左键粗调，方向键微调 0.1 pixel，q 确认，Esc 取消。\n";
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
        } else if (argument == "--image") {
            options.image_path = next();
        } else if (argument == "--image-name") {
            options.image_name = next();
        } else if (argument == "--image-dir") {
            options.image_dir = next();
        } else if (argument == "--output-dir") {
            options.output_dir = next();
        } else if (argument == "--auto-only") {
            options.auto_only = true;
        } else if (argument == "--help" || argument == "-h") {
            options.show_help = true;
        } else {
            throw std::runtime_error("未知参数: " + argument);
        }
    }
    return options;
}

std::string stem_from_image_argument(const Options& options) {
    if (!options.image_path.empty()) {
        return options.image_path.stem().string();
    }
    return std::filesystem::path(options.image_name).stem().string();
}

std::filesystem::path resolve_image_path(
    const Options& options,
    const ProjectConfig& config,
    std::string& image_stem) {
    if (!options.image_path.empty()) {
        image_stem = options.image_path.stem().string();
        return options.image_path;
    }

    if (options.image_name.empty()) {
        throw std::runtime_error("必须指定 --image 或 --image-name");
    }

    const std::filesystem::path name(options.image_name);
    image_stem = name.stem().string();
    const auto directory =
        options.image_dir.empty() ? config.image_dir : options.image_dir;
    std::filesystem::path candidate = directory / name;
    if (candidate.extension().empty()) {
        candidate += ".tif";
    }
    return candidate;
}

std::pair<double, double> finite_range(const cv::Mat_<float>& image) {
    double minimum = std::numeric_limits<double>::infinity();
    double maximum = -std::numeric_limits<double>::infinity();
    for (int row = 0; row < image.rows; ++row) {
        for (int column = 0; column < image.cols; ++column) {
            const float value = image(row, column);
            if (std::isfinite(value) && std::abs(value) < 1e30f) {
                minimum = std::min(minimum, static_cast<double>(value));
                maximum = std::max(maximum, static_cast<double>(value));
            }
        }
    }
    return {minimum, maximum};
}

std::pair<double, double> percentile_range(
    const cv::Mat_<float>& image,
    double low_fraction,
    double high_fraction) {
    std::vector<float> samples;
    samples.reserve(image.total());
    for (int row = 0; row < image.rows; ++row) {
        for (int column = 0; column < image.cols; ++column) {
            const float value = image(row, column);
            if (std::isfinite(value) && std::abs(value) < 1e30f) {
                samples.push_back(value);
            }
        }
    }
    if (samples.empty()) {
        return {
            std::numeric_limits<double>::infinity(),
            -std::numeric_limits<double>::infinity(),
        };
    }

    const auto index_for = [&](double fraction) {
        const double scaled =
            fraction * static_cast<double>(samples.size() - 1);
        const auto index = static_cast<std::size_t>(
            std::clamp(scaled, 0.0, static_cast<double>(samples.size() - 1)));
        return index;
    };
    const std::size_t low_index = index_for(low_fraction);
    const std::size_t high_index = index_for(high_fraction);

    std::nth_element(
        samples.begin(),
        samples.begin() + static_cast<std::ptrdiff_t>(low_index),
        samples.end());
    const float low = samples[low_index];

    std::nth_element(
        samples.begin() + static_cast<std::ptrdiff_t>(low_index),
        samples.begin() + static_cast<std::ptrdiff_t>(high_index),
        samples.end());
    const float high = samples[high_index];
    return {low, high};
}

cv::Mat make_linear_display_image(
    const cv::Mat_<float>& image,
    double minimum,
    double maximum) {
    cv::Mat display(image.size(), CV_8U, cv::Scalar(0));
    if (!std::isfinite(minimum) || !std::isfinite(maximum) ||
        maximum <= minimum) {
        return display;
    }

    const double range = maximum - minimum;
    for (int row = 0; row < image.rows; ++row) {
        for (int column = 0; column < image.cols; ++column) {
            const float value = image(row, column);
            if (!std::isfinite(value) || std::abs(value) >= 1e30f) {
                continue;
            }

            const double normalized = (value - minimum) / range;
            display.at<std::uint8_t>(row, column) =
                cv::saturate_cast<std::uint8_t>(normalized * 255.0);
        }
    }
    return display;
}

void draw_outlined_text(
    cv::Mat& display,
    const std::string& text,
    const cv::Point& origin,
    double font_scale,
    int thickness) {
    int baseline = 0;
    const auto text_size = cv::getTextSize(
        text,
        cv::FONT_HERSHEY_SIMPLEX,
        font_scale,
        thickness,
        &baseline);
    const double available_width =
        std::max(1.0, display.cols - static_cast<double>(origin.x) - 10.0);
    const double fitted_scale =
        std::max(0.2, font_scale * std::min(
                                  1.0,
                                  available_width /
                                      std::max(1, text_size.width)));

    cv::putText(
        display,
        text,
        origin + cv::Point(1, 1),
        cv::FONT_HERSHEY_SIMPLEX,
        fitted_scale,
        cv::Scalar(0),
        thickness + 1);
    cv::putText(
        display,
        text,
        origin,
        cv::FONT_HERSHEY_SIMPLEX,
        fitted_scale,
        cv::Scalar(255),
        thickness);
}

void on_mouse(int event, int x, int y, int, void* userdata) {
    if (event != cv::EVENT_LBUTTONDOWN || userdata == nullptr) {
        return;
    }
    auto* state = static_cast<UiState*>(userdata);
    state->selected.x = std::clamp(
        static_cast<double>(x) / state->scale,
        0.0,
        state->image.cols - 1.0);
    state->selected.y = std::clamp(
        static_cast<double>(y) / state->scale,
        0.0,
        state->image.rows - 1.0);
}

bool is_left_key(int key) {
    return key == 81 || key == 2424832;
}

bool is_up_key(int key) {
    return key == 82 || key == 2490368;
}

bool is_right_key(int key) {
    return key == 83 || key == 2555904;
}

bool is_down_key(int key) {
    return key == 84 || key == 2621440;
}

std::vector<std::filesystem::path> list_tiff_files(
    const std::filesystem::path& directory) {
    if (!std::filesystem::exists(directory)) {
        throw std::runtime_error(
            "影像目录不存在: " + directory.u8string());
    }
    if (!std::filesystem::is_directory(directory)) {
        throw std::runtime_error(
            "影像目录不是文件夹: " + directory.u8string());
    }

    std::vector<std::filesystem::path> files;
    for (const auto& entry :
         std::filesystem::directory_iterator(directory)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        const std::string extension =
            entry.path().extension().string();
        if (extension == ".tif" || extension == ".tiff" ||
            extension == ".TIF" || extension == ".TIFF") {
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

}  // namespace

int run_point_measurement_app(int argc, char** argv) {
    const Options options = parse_options(argc, argv);
    if (options.show_help) {
        print_help();
        return 0;
    }

    std::cout << "读取项目参数..." << std::endl;
    const ProjectConfig config = load_project_config(options.config_path);

    if (options.image_path.empty() && options.image_name.empty() &&
        !options.image_dir.empty()) {
        const auto files = list_tiff_files(options.image_dir);
        if (files.empty()) {
            throw std::runtime_error(
                "影像目录中没有 TIFF 文件: " +
                options.image_dir.u8string());
        }

        std::cout << "批量模式: 共发现 " << files.size()
                  << " 景影像，按文件名顺序处理。" << std::endl;
        for (std::size_t i = 0; i < files.size(); ++i) {
            std::cout << "\n[" << (i + 1) << '/' << files.size() << "] "
                      << files[i].filename().u8string() << std::endl;

            std::vector<std::string> child_arguments = {
                "measure",
                "--image",
                files[i].u8string(),
            };
            if (!options.config_path.empty()) {
                child_arguments.push_back("--config");
                child_arguments.push_back(options.config_path.u8string());
            }
            if (!options.output_dir.empty()) {
                child_arguments.push_back("--output-dir");
                child_arguments.push_back(options.output_dir.u8string());
            }
            if (options.auto_only) {
                child_arguments.push_back("--auto-only");
            }

            std::vector<char*> child_argv;
            child_argv.reserve(child_arguments.size() + 1);
            for (std::string& argument : child_arguments) {
                child_argv.push_back(argument.data());
            }
            child_argv.push_back(nullptr);

            const int result = run_point_measurement_app(
                static_cast<int>(child_arguments.size()),
                child_argv.data());
            if (result != 0) {
                return result;
            }
        }
        std::cout << "\n批量量测完成，共处理 " << files.size()
                  << " 张影像。" << std::endl;
        return 0;
    }

    std::string image_stem;
    const auto image_path = resolve_image_path(options, config, image_stem);
    std::cout << "影像路径: " << image_path.u8string() << std::endl;
    if (!std::filesystem::exists(image_path)) {
        throw std::runtime_error("找不到影像: " + image_path.u8string());
    }

    std::cout << "读取目标表..." << std::endl;
    const auto targets = load_target_table(config.target_table);
    const auto target_match = find_target_by_image(targets, image_stem);
    if (!target_match.has_value()) {
        throw std::runtime_error("目标表中找不到影像: " + image_stem);
    }
    std::cout << "反射器: " << target_match->target.reflector_id
              << " (truth_id=" << target_match->target.truth_id << ")\n"
              << "影像对: " << target_match->target.image_1 << ", "
              << target_match->target.image_2 << std::endl;

    std::cout << "读取真值..." << std::endl;
    const auto truth =
        find_truth_by_id(config.truth_file, target_match->target.truth_id);
    if (!truth.has_value()) {
        throw std::runtime_error(
            "真值文件中找不到目标: " + target_match->target.truth_id);
    }
    std::cout << std::setprecision(10)
              << "真值: longitude=" << truth->longitude_deg
              << ", latitude=" << truth->latitude_deg
              << ", height_m=" << truth->height_m << std::endl;

    const auto rpc_path = config.rpc_dir / (image_stem + "_rpc.txt");
    if (!std::filesystem::exists(rpc_path)) {
        throw std::runtime_error("找不到 RPC 文件: " + rpc_path.u8string());
    }

    std::cout << "读取 RPC..." << std::endl;
    const RpcModel rpc = RpcModel::from_file(rpc_path);
    const PixelPoint expected = rpc.forward(*truth);
    std::cout << "读取 TIFF 元数据..." << std::endl;
    const TiffInfo info = read_tiff_info(image_path);

    const int roi_side = std::max(
        16,
        static_cast<int>(std::lround(config.roi_size_m /
                                     target_match->resolution_mpp)));
    const int roi_width = std::min(roi_side, info.width);
    const int roi_height = std::min(roi_side, info.height);
    const int center_x = static_cast<int>(std::lround(expected.sample));
    const int center_y = static_cast<int>(std::lround(expected.line));
    const int x0 = std::clamp(center_x - roi_width / 2, 0, info.width - roi_width);
    const int y0 =
        std::clamp(center_y - roi_height / 2, 0, info.height - roi_height);

    std::cout << "理论像点: sample=" << expected.sample
              << ", line=" << expected.line << '\n'
              << "ROI: " << x0 << ',' << y0 << " 尺寸 " << roi_width << 'x'
              << roi_height << '\n';

    std::cout << "读取 TIFF ROI..." << std::endl;
    const auto roi = read_tiff_roi(image_path, x0, y0, roi_width, roi_height);
    const cv::Point2d expected_local(
        expected.sample - static_cast<double>(x0),
        expected.line - static_cast<double>(y0));
    const DetectionResult detection =
        detect_single_candidate(roi, expected_local);

    const double candidate_global_sample = x0 + detection.point.x;
    const double candidate_global_line = y0 + detection.point.y;
    std::ostringstream candidate_text;
    candidate_text << std::fixed << std::setprecision(3)
                   << "自动候选: sample=" << candidate_global_sample
                   << ", line=" << candidate_global_line
                   << ", score=" << detection.score
                   << ", low_confidence="
                   << (detection.low_confidence ? "true" : "false");
    std::cout << candidate_text.str() << std::endl;

    const auto [minimum, maximum] = finite_range(roi);
    std::cout << "ROI 原始值范围: min=" << minimum
              << ", max=" << maximum << std::endl;
    const auto [display_low, display_high] =
        percentile_range(roi, 0.001, 0.999);
    std::cout << "显示拉伸范围: 0.1%=" << display_low
              << ", 99.9%=" << display_high << std::endl;
    if (options.auto_only) {
        return 0;
    }

    const std::string window_name =
        "LRRR point measurement - " + target_match->target.reflector_id +
        " / " + image_stem;
    UiState state;
    state.image = roi;
    state.selected = detection.point;
    constexpr double target_display_side = 900.0;
    const double source_side =
        static_cast<double>(std::max(roi_width, roi_height));
    state.scale = std::clamp(
        target_display_side / std::max(1.0, source_side),
        0.5,
        8.0);

    std::cout << "显示映射: ROI 有效值 [min, max] -> [0, 255]，单通道 8 位。"
              << std::endl;
    std::cout << "显示缩放: " << std::fixed << std::setprecision(2)
              << state.scale << "x，显示尺寸约 "
              << static_cast<int>(std::lround(roi_width * state.scale))
              << "x"
              << static_cast<int>(std::lround(roi_height * state.scale))
              << " pixel。" << std::endl;

    state.display_base =
        make_linear_display_image(state.image, display_low, display_high);
    cv::resize(
        state.display_base,
        state.display_base,
        cv::Size(),
        state.scale,
        state.scale,
        cv::INTER_NEAREST);

    cv::namedWindow(window_name, cv::WINDOW_AUTOSIZE);
    cv::setMouseCallback(window_name, on_mouse, &state);

    std::cout << "操作提示:\n"
                 "  鼠标左键: 将判读点移动到点击位置\n"
                 "  方向键: 按 0.1 pixel 微调\n"
                 "  q: 确认并保存量测结果\n"
                 "  Esc 或关闭窗口: 取消量测"
              << std::endl;

    while (true) {
        if (cv::getWindowProperty(window_name, cv::WND_PROP_VISIBLE) < 1.0) {
            state.cancelled = true;
            break;
        }

        cv::Mat display = state.display_base.clone();

        const cv::Point marker_position(
            static_cast<int>(std::lround(state.selected.x * state.scale)),
            static_cast<int>(std::lround(state.selected.y * state.scale)));
        cv::drawMarker(
            display,
            marker_position,
            cv::Scalar(0),
            cv::MARKER_CROSS,
            14,
            2);
        cv::drawMarker(
            display,
            marker_position,
            cv::Scalar(255),
            cv::MARKER_CROSS,
            10,
            1);

        std::ostringstream coordinate_text;
        coordinate_text << std::fixed << std::setprecision(3)
                        << "sample=" << x0 + state.selected.x
                        << " line=" << y0 + state.selected.y;
        const std::string coordinate = coordinate_text.str();
        const std::string reflector_text =
            "reflector=" + target_match->target.reflector_id;
        const std::string image_text = "image=" + image_stem;
        draw_outlined_text(
            display,
            reflector_text,
            cv::Point(10, 24),
            0.42,
            1);
        draw_outlined_text(
            display,
            image_text,
            cv::Point(10, 48),
            0.42,
            1);
        draw_outlined_text(
            display,
            coordinate,
            cv::Point(10, 72),
            0.65,
            2);

        const std::string help =
            "L-click: move  Arrows: 0.1 px  q: confirm  Esc: cancel";
        draw_outlined_text(
            display,
            help,
            cv::Point(10, display.rows - 12),
            0.45,
            1);
        cv::imshow(window_name, display);

        const int key = cv::waitKeyEx(10);
        if (key == 'q' || key == 'Q') {
            state.confirmed = true;
            break;
        }
        if (key == 27) {
            state.cancelled = true;
            break;
        }
        if (is_left_key(key)) {
            state.selected.x = std::max(0.0, state.selected.x - 0.1);
        } else if (is_right_key(key)) {
            state.selected.x =
                std::min(state.image.cols - 1.0, state.selected.x + 0.1);
        } else if (is_up_key(key)) {
            state.selected.y = std::max(0.0, state.selected.y - 0.1);
        } else if (is_down_key(key)) {
            state.selected.y =
                std::min(state.image.rows - 1.0, state.selected.y + 0.1);
        }
    }

    cv::destroyWindow(window_name);

    if (!state.confirmed || state.cancelled) {
        std::cout << "量测已取消，不写入文件。\n";
        return 0;
    }

    if (detection.low_confidence) {
        std::cout << "注意: 自动候选置信度较低，已采用人工确认结果。\n";
    }

    const auto output_dir = options.output_dir.empty()
                                ? config.measurement_dir
                                : options.output_dir;
    const auto output_path =
        output_dir / (image_stem + "_point_measurement.txt");
    const bool output_existed = std::filesystem::exists(output_path);

    write_measurement_txt(
        output_path,
        PixelPoint{
            static_cast<double>(x0 + state.selected.x),
            static_cast<double>(y0 + state.selected.y)});
    std::cout << (output_existed ? "量测结果已保存（已覆盖原文件）: "
                                 : "量测结果已保存: ")
              << output_path.u8string() << '\n';
    return 0;
}

}  // namespace rpc_localization
