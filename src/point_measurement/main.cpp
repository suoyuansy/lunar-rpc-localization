#include "point_measurement/point_measurement_app.hpp"

#include <opencv2/core/utils/logger.hpp>

#include <exception>
#include <iostream>

#ifdef _WIN32
#include <windows.h>
#endif

int main(int argc, char** argv) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
    cv::utils::logging::setLogLevel(cv::utils::logging::LOG_LEVEL_WARNING);

    try {
        return rpc_localization::run_point_measurement_app(argc, argv);
    } catch (const std::exception& error) {
        std::cerr << "错误: " << error.what() << '\n';
        return 1;
    }
}
