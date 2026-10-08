#pragma once

namespace rpc_localization {

// 执行 localize 子命令，支持固定高程和双影像两种 LM 反算。
int run_rfm_localization_app(int argc, char** argv);

}  // namespace rpc_localization
