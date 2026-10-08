#include "point_measurement/point_measurement_app.hpp"
#include "rfm/rfm_localization_app.hpp"

#include <opencv2/core/utils/logger.hpp>

#include <exception>
#include <iostream>
#include <string>

#ifdef _WIN32
#include <windows.h>
#endif

namespace {

void print_help() {
    std::cout
        << "用法: lunar_rpc_tool <命令> [选项]\n\n"
        << "命令:\n"
        << "  measure     LRRR 像点坐标量测\n"
        << "  localize    RFM 经纬度高程反算（待实现）\n"
        << "  evaluate    定位精度评定（待实现）\n\n"
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
            return rpc_localization::run_point_measurement_app(
                argc - 1,
                argv + 1);
        }

        if (command == "localize") {
            return rpc_localization::run_rfm_localization_app(
                argc - 1,
                argv + 1);
        }

        if (command == "evaluate") {
            std::cerr << "错误: 子命令 " << command << " 尚未实现。\n";
            return 1;
        }

        std::cerr << "错误: 未知子命令: " << command << "\n\n";
        print_help();
        return 1;
    } catch (const std::exception& error) {
        std::cerr << "错误: " << error.what() << '\n';
        return 1;
    }
}
