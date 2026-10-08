# Visual Studio 2022 构建说明

## 1. 构建环境

项目优先使用 Visual Studio 2022 Developer Command Prompt。

不要优先使用 MSYS2 UCRT64。Developer Command Prompt 的 PATH 中可能同时存在 MSYS2，因此构建前应确认实际使用的是：

- Visual Studio 自带 MSVC 编译器。
- Visual Studio 自带 CMake。
- Visual Studio 自带 Ninja。
- Visual Studio 自带 vcpkg。

## 2. 打开正确终端

打开“Developer Command Prompt for VS 2022”或“x64 Native Tools Command Prompt for VS 2022”。

执行：

```bat
cd /d C:\Users\SuoYuan\Desktop\行星遥感\课程作业1\RPC_localization
set "VCPKG_ROOT=%VSINSTALLDIR%VC\vcpkg"
set "PATH=%VSINSTALLDIR%Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin;%VSINSTALLDIR%Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja;%PATH%"
```

说明：

- `VCPKG_ROOT` 告诉 CMake 使用 Visual Studio 自带的 vcpkg。
- PATH 最前面加入 Visual Studio 的 CMake 和 Ninja，避免误用 MSYS2 工具。
- 这些设置只对当前命令行窗口有效。

## 3. 检查工具

```bat
where cl
where cmake
where ninja
where vcpkg
cmake --version
ninja --version
vcpkg version
```

检查结果中，`cmake`、`ninja` 和 `vcpkg` 应位于 Visual Studio 安装目录中。

## 4. 文件功能

- `CMakeLists.txt`：总构建入口。
- `CMakePresets.json`：VS2022 Debug/Release 构建预设。
- `third_party/CMakeLists.txt`：查找 OpenCV 和 libtiff。
- `third_party/vcpkg.json`：声明和固定第三方依赖版本。
- `src/CMakeLists.txt`：后续业务模块的编译目标。
- `tests/CMakeLists.txt`：CTest 测试入口。

## 5. 下载第三方库

项目采用 vcpkg manifest 模式。`third_party/vcpkg.json` 声明：

- OpenCV 4。
- libtiff。

第一次执行 CMake 配置时，CMake 会调用 vcpkg：

1. 读取 `third_party/vcpkg.json`。
2. 检查本机是否已经安装对应依赖。
3. 缺少依赖时自动下载并编译。
4. 将依赖安装到 `third_party/vcpkg_installed/`。

下载目录和编译目录不会提交到 Git。

## 6. 配置项目

```bat
cmake --preset vs2022-x64-debug
```

含义：

- 使用 `vs2022-x64-debug` 预设。
- 使用 MSVC x64 编译器。
- 使用 Visual Studio 自带 vcpkg。
- 使用 `x64-windows` triplet。

第一次配置可能耗时较长，因为需要下载和编译 OpenCV。

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
