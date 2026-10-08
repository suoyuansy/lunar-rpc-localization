# Visual Studio 2022 构建说明

## 1. 构建环境

项目优先使用 Visual Studio 2022 Developer Command Prompt。

不要优先使用 MSYS2 UCRT64。Developer Command Prompt 的 PATH 中可能同时存在 MSYS2，因此构建前应确认实际使用的是：

- Visual Studio 自带 MSVC 编译器。
- Visual Studio 自带 CMake。
- Visual Studio 自带 Ninja。

## 2. 打开正确终端

打开“Developer Command Prompt for VS 2022”或“x64 Native Tools Command Prompt for VS 2022”。

执行：

```bat
cd /d C:\Users\SuoYuan\Desktop\行星遥感\课程作业1\RPC_localization
set "PATH=%VSINSTALLDIR%Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin;%VSINSTALLDIR%Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja;%PATH%"
```

说明：

- PATH 最前面加入 Visual Studio 的 CMake 和 Ninja，避免误用 MSYS2 工具。
- 这些设置只对当前命令行窗口有效。

## 3. 检查工具

```bat
where cl
where cmake
where ninja
cmake --version
ninja --version
```

检查结果中，`cmake` 和 `ninja` 应位于 Visual Studio 安装目录中。

## 4. 文件功能

- `CMakeLists.txt`：总构建入口。
- `CMakePresets.json`：VS2022 Debug/Release 构建预设。
- `third_party/CMakeLists.txt`：定义本地 OpenCV 和 libtiff 导入目标。
- `src/CMakeLists.txt`：后续业务模块的编译目标。
- `tests/CMakeLists.txt`：CTest 测试入口。

## 5. 本地第三方库

项目不再自动下载 OpenCV 和 libtiff，直接使用以下本地目录：

- `third_party/opencv/`：OpenCV 4.12.0 预编译包。
- `third_party/libtiff/x64-windows/`：libtiff 4.7.2 x64 MSVC 包。

这两个目录体积较大，不提交到 Git。

## 6. 配置项目

```bat
cmake --preset vs2022-x64-debug
```

含义：

- 使用 `vs2022-x64-debug` 预设。
- 使用 MSVC x64 编译器。
- 使用 `third_party/opencv` 和 `third_party/libtiff` 中的本地依赖。

## 7. 编译项目

```bat
cmake --build --preset vs2022-x64-debug
```

## 8. 运行测试

```bat
ctest --preset vs2022-x64-debug
```

当前只包含构建脚手架测试，后续会逐步增加模块测试。

## 9. Release 构建

```bat
cmake --preset vs2022-x64-release
cmake --build --preset vs2022-x64-release
ctest --preset vs2022-x64-release
```
