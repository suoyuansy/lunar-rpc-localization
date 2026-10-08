# 公共配置和数据结构模块。
add_library(rpc_common STATIC
    common/config.cpp
)
target_include_directories(rpc_common PUBLIC "${PROJECT_SOURCE_DIR}/include")
target_link_libraries(rpc_common PUBLIC third_party_deps)

# RPC 模型与正算模块。
add_library(rpc_rfm STATIC
    rfm/rpc_model.cpp
)
target_include_directories(rpc_rfm PUBLIC "${PROJECT_SOURCE_DIR}/include")
target_link_libraries(rpc_rfm PUBLIC rpc_common)

# 像点量测模块。
add_library(rpc_measurement STATIC
    point_measurement/detector.cpp
    point_measurement/measurement_writer.cpp
    point_measurement/point_measurement_app.cpp
    point_measurement/tiff_roi_reader.cpp
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
    point_measurement/main.cpp
)
target_link_libraries(point_measure_tool PRIVATE rpc_measurement)
rpc_copy_third_party_runtime(point_measure_tool)
