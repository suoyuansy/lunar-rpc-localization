# 本地 OpenCV 目录。
# 当前使用 OpenCV 4.12.0 的预编译 world 包。
set(RPC_OPENCV_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/opencv")
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
set(RPC_TIFF_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/libtiff/x64-windows")
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

# 统一接口目标，业务模块以后只链接 third_party_deps。
add_library(third_party_deps INTERFACE)
target_link_libraries(third_party_deps INTERFACE opencv_local tiff_local)

message(STATUS "使用本地 OpenCV: ${RPC_OPENCV_ROOT}")
message(STATUS "使用本地 libtiff: ${RPC_TIFF_ROOT}")
