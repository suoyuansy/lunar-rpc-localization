cmake_minimum_required(VERSION 3.21)

# 项目名称、版本和编译语言
project(lunar_rpc_localization VERSION 0.1.0 LANGUAGES CXX)

# 全项目统一使用 C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 源文件包含中文注释和错误信息，MSVC 统一按 UTF-8 读取源文件。
if(MSVC)
    add_compile_options(/utf-8)
endif()

# 禁止在源码目录直接构建，避免生成文件污染源码
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR "请在独立 build 目录中构建项目")
endif()

# 本地 OpenCV 目录。
# 当前使用 OpenCV 4.12.0 的预编译 world 包。
set(RPC_OPENCV_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/third_party/opencv")
set(RPC_OPENCV_INCLUDE_DIR "${RPC_OPENCV_ROOT}/include")

if(NOT EXISTS "${RPC_OPENCV_INCLUDE_DIR}/opencv2/core.hpp")
    message(FATAL_ERROR "未找到本地 OpenCV: ${RPC_OPENCV_ROOT}")
endif()

# 定义本地 OpenCV 导入目标。
add_library(opencv_local SHARED IMPORTED GLOBAL)
set_target_properties(
    opencv_local
    PROPERTIES
        IMPORTED_CONFIGURATIONS "DEBUG;RELEASE"
        IMPORTED_LOCATION_DEBUG "${RPC_OPENCV_ROOT}/bin/opencv_world4120d.dll"
        IMPORTED_IMPLIB_DEBUG "${RPC_OPENCV_ROOT}/lib/opencv_world4120d.lib"
        IMPORTED_LOCATION_RELEASE "${RPC_OPENCV_ROOT}/bin/opencv_world4120.dll"
        IMPORTED_IMPLIB_RELEASE "${RPC_OPENCV_ROOT}/lib/opencv_world4120.lib"
        INTERFACE_INCLUDE_DIRECTORIES "${RPC_OPENCV_INCLUDE_DIR}"
)

# 本地 libtiff 目录。
# 当前只安装核心功能，不依赖 JPEG、ZLIB 等额外第三方 DLL。
set(RPC_TIFF_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/third_party/libtiff/x64-windows")
set(RPC_TIFF_INCLUDE_DIR "${RPC_TIFF_ROOT}/include")

if(NOT EXISTS "${RPC_TIFF_INCLUDE_DIR}/tiff.h")
    message(FATAL_ERROR "未找到本地 libtiff: ${RPC_TIFF_ROOT}")
endif()

# 定义本地 libtiff 导入目标。
add_library(tiff_local SHARED IMPORTED GLOBAL)
set_target_properties(
    tiff_local
    PROPERTIES
        IMPORTED_CONFIGURATIONS "DEBUG;RELEASE"
        IMPORTED_LOCATION_DEBUG "${RPC_TIFF_ROOT}/debug/bin/tiffd.dll"
        IMPORTED_IMPLIB_DEBUG "${RPC_TIFF_ROOT}/debug/lib/tiffd.lib"
        IMPORTED_LOCATION_RELEASE "${RPC_TIFF_ROOT}/bin/tiff.dll"
        IMPORTED_IMPLIB_RELEASE "${RPC_TIFF_ROOT}/lib/tiff.lib"
        INTERFACE_INCLUDE_DIRECTORIES "${RPC_TIFF_INCLUDE_DIR}"
)

# 统一依赖目标，业务模块只链接 third_party_deps。
add_library(third_party_deps INTERFACE)
target_link_libraries(third_party_deps INTERFACE opencv_local tiff_local)

# 将本地 DLL 复制到可执行程序目录，避免运行时找不到动态库。
function(rpc_copy_third_party_runtime target_name)
    add_custom_command(
        TARGET ${target_name}
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:opencv_local>
            $<TARGET_FILE_DIR:${target_name}>
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:tiff_local>
            $<TARGET_FILE_DIR:${target_name}>
        COMMENT "Copy OpenCV and libtiff runtime DLLs"
    )
endfunction()

# 公共配置和数据结构模块。
add_library(rpc_common STATIC
    src/common/config.cpp
)
target_include_directories(rpc_common PUBLIC "${PROJECT_SOURCE_DIR}/include")
target_link_libraries(rpc_common PUBLIC third_party_deps)

# RPC 模型与正算模块。
add_library(rpc_rfm STATIC
    src/rfm/rpc_model.cpp
)
target_include_directories(rpc_rfm PUBLIC "${PROJECT_SOURCE_DIR}/include")
target_link_libraries(rpc_rfm PUBLIC rpc_common)

# 像点量测模块。
add_library(rpc_measurement STATIC
    src/point_measurement/detector.cpp
    src/point_measurement/measurement_writer.cpp
    src/point_measurement/point_measurement_app.cpp
    src/point_measurement/tiff_roi_reader.cpp
)
target_include_directories(rpc_measurement PUBLIC "${PROJECT_SOURCE_DIR}/include")
target_link_libraries(
    rpc_measurement
    PUBLIC
        rpc_common
        rpc_rfm
        third_party_deps
)

# 像点量测命令行程序。
add_executable(point_measure_tool
    src/point_measurement/main.cpp
)
target_link_libraries(point_measure_tool PRIVATE rpc_measurement)
rpc_copy_third_party_runtime(point_measure_tool)
