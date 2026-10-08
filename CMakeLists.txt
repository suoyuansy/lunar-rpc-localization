cmake_minimum_required(VERSION 3.21)

# 项目名称、版本和编译语言
project(lunar_rpc_localization VERSION 0.1.0 LANGUAGES CXX)

# 全项目统一使用 C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 禁止在源码目录直接构建，避免生成文件污染源码
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR "请在独立 build 目录中构建项目")
endif()

# 第三方依赖入口
add_subdirectory(third_party)

# 三个业务模块的编译入口
add_subdirectory(src)

# 测试入口
include(CTest)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()