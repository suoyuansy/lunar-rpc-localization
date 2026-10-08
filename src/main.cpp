#include "point_measurement/point_measurement_app.hpp"
#include "accuracy/accuracy_app.hpp"
#include "rfm/rfm_localization_app.hpp"

#include <opencv2/core/utils/logger.hpp>

#include <exception>
#include <iostream>
#include <string>

#ifdef _WIN32
#include <windows.h>
#endif

namespace {

// 顶层帮助信息只说明有哪些子命令；各子命令的参数由各自的 --help 输出。
void print_help() {
    std::cout
        << "用法: lunar_rpc_tool <命令> [选项]\n\n"
        << "命令:\n"
        << "  measure     LRRR 像点坐标量测\n"
        << "  localize    RFM 经纬度高程反算\n"
        << "  evaluate    定位精度评定\n\n"
        << "示例:\n"
        << "  lunar_rpc_tool measure --image-name M175124932RE\n"
        << "  lunar_rpc_tool measure --help\n";
}

}  // namespace

int main(int argc, char** argv) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
    cv::utils::logging::setLogLevel(cv::utils::logging::LOG_LEVEL_WARNING);

    try {
        if (argc < 2) {
            print_help();
            return 0;
        }

        const std::string command = argv[1];
        if (command == "--help" || command == "-h" || command == "help") {
            print_help();
            return 0;
        }

        if (command == "measure") {
            // 把 measure 后面的参数原样交给量测模块处理。
            return rpc_localization::run_point_measurement_app(
                argc - 1,
                argv + 1);
        }

        if (command == "localize") {
            // localize 内部再根据 --method 选择方案一或方案二。
            return rpc_localization::run_rfm_localization_app(
                argc - 1,
                argv + 1);
        }

        if (command == "evaluate") {
            // evaluate 必须指定方法，一次只评价一种反算方法。
            return rpc_localization::run_accuracy_evaluation_app(
                argc - 1,
                argv + 1);
        }

        std::cerr << "错误: 未知子命令: " << command << "\n\n";
        print_help();
        return 1;
    } catch (const std::exception& error) {
        std::cerr << "错误: " << error.what() << '\n';
        return 1;
    }
}
