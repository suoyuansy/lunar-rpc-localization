#pragma once

namespace rpc_localization {

// 执行 measure 子命令。
// 单景模式处理一张影像；只给 --image-dir 时进入目录批量模式。
int run_point_measurement_app(int argc, char** argv);

}  // namespace rpc_localization
