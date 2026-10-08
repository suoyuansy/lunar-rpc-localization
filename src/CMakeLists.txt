# 当前阶段只建立构建骨架。
#
# 后续三个模块分别创建静态库：
#   rpc_common：配置、路径、日志和公共数据结构
#   rpc_measurement：像点判读和量测
#   rpc_rfm：RPC 解析、正算和 LM 反算
#   rpc_accuracy：精度评定
#
# 在没有源文件之前不创建空编译目标，避免 CMake 报“没有源文件”。

add_custom_target(rpc_project_scaffold ALL
    COMMAND ${CMAKE_COMMAND} -E echo "RPC localization build scaffold is ready."
    VERBATIM
)
