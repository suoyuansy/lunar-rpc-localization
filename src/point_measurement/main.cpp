#include "point_measurement/point_measurement_app.hpp"

#include <exception>
#include <iostream>

int main(int argc, char** argv) {
    try {
        return rpc_localization::run_point_measurement_app(argc, argv);
    } catch (const std::exception& error) {
        std::cerr << "错误: " << error.what() << '\n';
        return 1;
    }
}

