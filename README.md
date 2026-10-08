# Lunar RPC Localization

月面反射器 RPC 直接定位项目。项目面向 LROC NAC 月球影像，根据 RPC 有理函数模型和反射器真值坐标，完成像点量测、RFM 反算和定位精度评定。

当前支持的目标：

| reflector_id | truth_id | 影像 1 | 影像 2 |
|---|---|---|---|
| Apollo11 | A11 | M104362199RE | M175124932RE |
| Apollo14 | A14 | M1096608496LE | M190715993LE |
| LK1 | LK1 | M1243906884RE | M181402751LE |

## 1. 任务描述

本项目的核心任务是从月球轨道影像中定位激光反射器，并评价定位精度。整体任务包括：

1. 读取 RPC 文件、目标表和真值坐标。
2. 使用 RPC 正算得到理论像点。
3. 在理论像点附近的局部影像窗口中量测反射器像点。
4. 使用固定高程 LM 反算经纬度。
5. 使用双影像 LM 联合反算经纬度和高程。
6. 将解算坐标与真值坐标比较，计算水平和三维误差。
7. 输出逐景结果、精度报告和诊断信息。

## 2. 数据目录

### 2.1 原始影像

原始 TIFF 影像放到：

```text
data/images/
```

当前项目使用的文件：

```text
M104362199RE.tif
M175124932RE.tif
M1096608496LE.tif
M190715993LE.tif
M1243906884RE.tif
M181402751LE.tif
```

程序要求 TIFF 满足：

- 单波段；
- 32 位 IEEE 浮点；
- sample format 为 `SAMPLEFORMAT_IEEEFP`；
- 条带式存储，不支持 tiled TIFF。

其中：

- `SAMPLEFORMAT_IEEEFP` 是 TIFF 文件中的一个元数据标记，表示像素按 IEEE 754 浮点数存储。当前项目读取的是 `float32`，因此像素值可以是小数，例如 `0.0123`，不是普通 8 位整数灰度。
- 条带式存储表示 TIFF 按一行或多行一条带保存，程序可以逐行读取局部窗口；tiled TIFF 表示影像被切成矩形瓦片保存，当前读取器没有实现瓦片读取，因此会报错而不是强行加载整幅影像。

RPC 正算和自动检测使用原始 float32，不先降成 8 位。

### 2.2 RPC 文件

RPC 文件放到：

```text
data/rpc/
```

文件命名应和影像名称对应：

```text
M104362199RE_rpc.txt
M175124932RE_rpc.txt
M1096608496LE_rpc.txt
M190715993LE_rpc.txt
M1243906884RE_rpc.txt
M181402751LE_rpc.txt
```

RPC 文件采用常见的 `KEY: VALUE` 格式，包含：

- `LINE_OFF`、`LINE_SCALE`
- `SAMP_OFF`、`SAMP_SCALE`
- `LAT_OFF`、`LAT_SCALE`
- `LONG_OFF`、`LONG_SCALE`
- `HEIGHT_OFF`、`HEIGHT_SCALE`
- `LINE_NUM_COEFF_1` 到 `LINE_NUM_COEFF_20`
- `LINE_DEN_COEFF_1` 到 `LINE_DEN_COEFF_20`
- `SAMP_NUM_COEFF_1` 到 `SAMP_NUM_COEFF_20`
- `SAMP_DEN_COEFF_1` 到 `SAMP_DEN_COEFF_20`

### 2.3 真值坐标

真值文件放到：

```text
data/truth/真值坐标.txt
```

当前文件格式为制表符分隔：

```text
棱镜点  经度  纬度  高程(m)
A11     23.47307       0.67345      -1927.65
A14     -17.47865      -3.64417     -1064.27
LK1     -35.00798      38.31517     -2471.41
```

程序通过 `targets.csv` 中的 `truth_id` 将反射器标识映射到真值行。

### 2.4 目标表

目标表文件：

```text
config/targets.csv
```

格式：

```csv
reflector_id,truth_id,image_1,image_2,resolution_1_mpp,resolution_2_mpp
Apollo11,A11,M104362199RE,M175124932RE,1.178557,0.399358
Apollo14,A14,M1096608496LE,M190715993LE,1.026185,0.789796
LK1,LK1,M1243906884RE,M181402751LE,1.197122,1.523278
```

字段含义：

| 字段 | 含义 |
|---|---|
| `reflector_id` | 程序内部使用的反射器统一标识 |
| `truth_id` | 真值文件中的标识 |
| `image_1` | 第一景影像名称，不带扩展名 |
| `image_2` | 第二景影像名称，不带扩展名 |
| `resolution_1_mpp` | 第一景影像地面分辨率，单位 m/pixel |
| `resolution_2_mpp` | 第二景影像地面分辨率，单位 m/pixel |

### 2.5 项目参数

项目参数文件：

```text
config/rpc_project.ini
```

当前默认内容：

```ini
image_dir=data/images
rpc_dir=data/rpc
measurement_dir=output/measurements
rfm_fixed_height_dir=output/rfm/fixed_height
rfm_two_image_dir=output/rfm/two_image
accuracy_dir=output/accuracy
target_table=config/targets.csv
truth_file=data/truth/真值坐标.txt
roi_size_m=500
```

路径相对于项目根目录。程序运行时会自动向上查找同时包含 `CMakeLists.txt` 和 `config/rpc_project.ini` 的项目根目录。

## 3. 第三方库和本地依赖

项目不再自动下载第三方库，使用本地预编译依赖。

### 3.1 OpenCV

默认路径：

```text
third_party/opencv/
```

当前使用 OpenCV 4.12.0 Windows x64 MSVC 预编译包，包含：

```text
third_party/opencv/include/
third_party/opencv/bin/
third_party/opencv/lib/
```

### 3.2 libtiff

默认路径：

```text
third_party/libtiff/x64-windows/
```

当前使用 libtiff 4.7.2，包含：

```text
third_party/libtiff/x64-windows/include/
third_party/libtiff/x64-windows/lib/
third_party/libtiff/x64-windows/debug/lib/
third_party/libtiff/x64-windows/bin/
third_party/libtiff/x64-windows/debug/bin/
```

`third_party/` 中的预编译二进制不提交到 Git，需要在本机准备好。

## 4. 构建环境

推荐环境：

- Windows；
- Visual Studio 2022 Developer Command Prompt；
- MSVC x64；
- CMake 3.21 或更高版本；
- Ninja；
- C++17；
- OpenCV；
- libtiff。

项目使用 CMake Presets，构建目录保留为：

```text
build/vs2022-x64-debug/
build/vs2022-x64-release/
```

可执行文件、DLL 和调试文件统一放在各自构建目录的 `bin/` 下：

```text
build/vs2022-x64-debug/bin/lunar_rpc_tool.exe
build/vs2022-x64-release/bin/lunar_rpc_tool.exe
```

### 4.1 Debug 构建

先进入项目根目录。下面的命令中的 `<项目根目录>` 替换成你实际下载得到的
`RPC_localization` 目录即可：

```bat
cd /d <项目根目录>
cmake --fresh --preset vs2022-x64-debug
cmake --build --preset vs2022-x64-debug
```

### 4.2 Release 构建

```bat
cd /d <项目根目录>
cmake --fresh --preset vs2022-x64-release
cmake --build --preset vs2022-x64-release
```

如果已经配置过，可以省略 `--fresh`：

```bat
cmake --preset vs2022-x64-debug
cmake --build --preset vs2022-x64-debug
```

## 5. 整体工作流程

完整流程如下：

1. 准备好 `data/images`、`data/rpc` 和 `data/truth`。
2. 使用 `measure` 逐景量测反射器像点。
3. 使用 `localize --method fixed_height` 执行方案一初值反算。
4. 使用 `localize --method two_image` 执行方案二联合反算。
5. 使用 `evaluate --method fixed_height` 评价方案一。
6. 使用 `evaluate --method two_image` 评价方案二。

推荐命令顺序：

```bat
build\vs2022-x64-debug\bin\lunar_rpc_tool.exe measure --image-dir data\images

build\vs2022-x64-debug\bin\lunar_rpc_tool.exe localize --method fixed_height --all
build\vs2022-x64-debug\bin\lunar_rpc_tool.exe localize --method two_image --all

build\vs2022-x64-debug\bin\lunar_rpc_tool.exe evaluate --method fixed_height
build\vs2022-x64-debug\bin\lunar_rpc_tool.exe evaluate --method two_image
```

## 6. 命令行程序

所有功能都由一个程序提供：

```text
build\vs2022-x64-debug\bin\lunar_rpc_tool.exe
```

查看总帮助：

```bat
lunar_rpc_tool.exe --help
```

查看某个子命令帮助：

```bat
lunar_rpc_tool.exe measure --help
lunar_rpc_tool.exe localize --help
lunar_rpc_tool.exe evaluate --help
```

### 6.1 measure：像点量测

单景量测：

```bat
build\vs2022-x64-debug\bin\lunar_rpc_tool.exe measure --image-name M175124932RE
```

目录批量量测：

```bat
build\vs2022-x64-debug\bin\lunar_rpc_tool.exe measure --image-dir data\images
```

只输出自动候选，不打开窗口，也不写量测文件：

```bat
build\vs2022-x64-debug\bin\lunar_rpc_tool.exe measure --image-dir data\images --auto-only
```

量测参数：

| 参数 | 含义 |
|---|---|
| `--config <文件>` | 指定 `rpc_project.ini` |
| `--image <文件>` | 直接指定 TIFF 文件 |
| `--image-name <名称>` | 根据配置目录查找 TIFF |
| `--image-dir <目录>` | 覆盖影像目录；单独使用时批量处理 |
| `--output-dir <目录>` | 覆盖量测输出目录 |
| `--auto-only` | 只打印自动候选，不打开窗口，不写文件 |
| `--help` | 显示帮助 |

交互操作：

| 操作 | 功能 |
|---|---|
| 鼠标左键 | 将判读点移动到点击位置 |
| 方向键 | 按 0.1 pixel 微调 |
| `q` | 确认并保存当前量测结果 |
| `Esc` | 取消当前量测 |

量测输出：

```text
output\measurements\<影像名称>_point_measurement.txt
```

内容：

```ini
sample=2486.977
line=23766.191
```

坐标是原影像全局 0 基像素中心坐标。

### 6.2 localize：RFM 经纬度高程反算

#### 方案一：固定高程

单景反算：

```bat
build\vs2022-x64-debug\bin\lunar_rpc_tool.exe localize ^
  --method fixed_height ^
  --image-name M175124932RE
```

批量反算全部影像：

```bat
build\vs2022-x64-debug\bin\lunar_rpc_tool.exe localize ^
  --method fixed_height ^
  --all
```

方案一需要：

- 影像对应的量测 TXT；
- 影像对应的 RPC；
- 目标表 `targets.csv`；
- 真值文件中的固定高程。

输出：

```text
output\rfm\fixed_height\<影像名称>_rfm_fixed_height.txt
```

#### 方案二：双影像联合反算

单目标反算：

```bat
build\vs2022-x64-debug\bin\lunar_rpc_tool.exe localize ^
  --method two_image ^
  --target Apollo11
```

批量反算全部目标：

```bat
build\vs2022-x64-debug\bin\lunar_rpc_tool.exe localize ^
  --method two_image ^
  --all
```

方案二需要：

- 两景影像的量测 TXT；
- 两景影像的 RPC；
- 两景影像的方案一结果作为初值。

输出：

```text
output\rfm\two_image\<目标名称>_rfm_two_image.txt
```

`localize` 参数：

| 参数 | 含义 |
|---|---|
| `--config <文件>` | 指定 `rpc_project.ini` |
| `--method <方法>` | 必选：`fixed_height` 或 `two_image` |
| `--target <目标名称>` | 指定目标，例如 `Apollo11` |
| `--image-name <影像名称>` | 可重复，指定一景或两景影像 |
| `--measurement-dir <目录>` | 覆盖量测目录 |
| `--rpc-dir <目录>` | 覆盖 RPC 目录 |
| `--output-dir <目录>` | 覆盖结果目录 |
| `--all` | 批量处理当前方法的全部影像或全部目标 |
| `--help` | 显示帮助 |

### 6.3 evaluate：精度评定

方案一评定：

```bat
build\vs2022-x64-debug\bin\lunar_rpc_tool.exe evaluate --method fixed_height
```

方案二评定：

```bat
build\vs2022-x64-debug\bin\lunar_rpc_tool.exe evaluate --method two_image
```

`evaluate` 参数：

| 参数 | 含义 |
|---|---|
| `--config <文件>` | 指定 `rpc_project.ini` |
| `--method <方法>` | 必选：`fixed_height` 或 `two_image` |
| `--input <文件>` | 指定一个结果 TXT，可重复 |
| `--result-dir <目录>` | 指定结果目录，可重复 |
| `--truth-file <文件>` | 覆盖真值文件 |
| `--output <文件>` | 指定最终报告路径 |
| `--output-dir <目录>` | 指定报告输出目录 |
| `--help` | 显示帮助 |

默认输出：

```text
output\accuracy\fixed_height_accuracy_report.txt
output\accuracy\two_image_accuracy_report.txt
```

报告文件名后缀是 `.txt`，内容使用 Markdown 标题和表格，便于直接查看或渲染。

## 7. RFM 模型和 LM 反算

### 7.1 RPC 20 项基函数

设归一化变量为：

```text
L = (longitude - LONG_OFF) / LONG_SCALE
P = (latitude  - LAT_OFF) / LAT_SCALE
H = (height    - HEIGHT_OFF) / HEIGHT_SCALE
```

20 项基函数顺序：

```text
1, L, P, H,
L*P, L^2, P^2, H^2, P*H, L*H,
L^3, L*P^2, L*H^2, L^2*P, P^3, P*H^2,
L^2*H, P^2*H, H^3, L*P*H
```

sample 和 line 均由分子、分母两个 20 项多项式相除得到。

### 7.2 方案一：固定高程 LM

未知量：

```text
[longitude, latitude]
```

残差：

```text
[sample_pred - sample_obs,
 line_pred   - line_obs]
```

LM 方程：

```text
(J^T W J + mu D) dx = -J^T W r
```

当前项目使用等权 `W = I`。

### 7.3 方案二：双影像 LM

未知量：

```text
[longitude, latitude, height]
```

残差包含两景影像的 sample、line 共 4 项。初值来自两景方案一结果的平均值，高程初值来自两景 RPC 的 `HEIGHT_OFF` 平均值。

### 7.4 收敛和诊断

当前代码中 `converged=true` 的条件是：

- LM 成功接受一次参数更新；
- 且残差 RMS 小于 `1e-6 pixel`，或本次参数增量范数小于 `1e-10`；
- 如果无法接受更新，或达到最多 `100` 次迭代仍不满足条件，则为 `false`。

输出中的：

| 字段 | 含义 |
|---|---|
| `iterations` | 实际迭代次数 |
| `residual_rms_px` | 像点残差 RMS，单位 pixel |
| `condition_number` | 雅可比矩阵条件数 |
| `solver=LM` | 表示使用 Levenberg-Marquardt |

条件数接近 1 表示问题良态；条件数很大时，不同参数可能互相补偿，高程和经纬度不容易分开。

## 8. 自动像点检测

自动检测只处理理论像点附近的局部 ROI，主要步骤：

1. 读取原始 float32 ROI。
2. 用中位数填充 NoData 和无效值。
3. 在 `sigma = 1, 2, 4` 三个尺度上估计高斯背景。
4. 计算影像减背景的高通响应。
5. 用 MAD 估计噪声并归一化。
6. 多尺度响应取最大值。
7. 搜索局部极大值。
8. 根据对比度、形状、尺度稳定性和与理论中心距离综合评分。
9. 对最高分点做抛物线插值，得到亚像素候选。

自动候选只作为人工判读初值，最终结果由人工确认。

## 9. 可视化显示

显示使用单通道 8 位 OpenCV 窗口，不改变自动检测使用的 float32 数据。

显示流程：

1. 计算 ROI 有效像素的 `0.1%` 和 `99.9%` 分位值。
2. 将分位区间线性映射到 `0-255`。
3. 高于 99.9% 的值截为 255，低于 0.1% 的值截为 0。
4. 根据 ROI 像素尺寸动态缩放，目标显示边长约 900 pixel。
5. 显示细十字标记、反射器、影像名称和当前像点坐标。

窗口左上角显示：

```text
reflector=Apollo11
image=M104362199RE
sample=1073.127 line=30150.108
```

## 10. 输出格式

### 10.1 像点量测文件

```text
output\measurements\<影像名称>_point_measurement.txt
```

```ini
sample=2486.977
line=23766.191
```

### 10.2 方案一结果

```text
output\rfm\fixed_height\<影像名称>_rfm_fixed_height.txt
```

```ini
reflector_id=Apollo11
method=fixed_height
solver=LM
source_images=M104362199RE
longitude=23.4727374211
latitude=0.6730814940
height_m=-1927.6500
```

### 10.3 方案二结果

```text
output\rfm\two_image\<目标名称>_rfm_two_image.txt
```

```ini
reflector_id=Apollo11
method=two_image
solver=LM
source_images=M104362199RE,M175124932RE
longitude=23.4730990777
latitude=0.6735221763
height_m=-1840.5657
```

### 10.4 精度报告

```text
output\accuracy\fixed_height_accuracy_report.txt
output\accuracy\two_image_accuracy_report.txt
```

报告包含：

- 每条结果的解算坐标和真值坐标；
- 经度差、纬度差和高程差；
- 东西、南北方向误差；
- 水平距离和三维距离；
- 按反射器分组的平均误差和 RMSE；
- 当前方法的总体平均误差和 RMSE。

## 11. 项目结构

```text
RPC_localization/
├─ CMakeLists.txt
├─ CMakePresets.json
├─ README.md
├─ PROGRESS.md
├─ TECHNICAL_PLAN.md
├─ config/
│  ├─ rpc_project.ini
│  └─ targets.csv
├─ data/
│  ├─ images/
│  ├─ rpc/
│  └─ truth/
├─ include/
│  ├─ accuracy/
│  ├─ common/
│  ├─ point_measurement/
│  └─ rfm/
├─ src/
│  ├─ accuracy/
│  ├─ common/
│  ├─ point_measurement/
│  └─ rfm/
├─ third_party/
│  ├─ opencv/
│  └─ libtiff/
└─ output/
   ├─ measurements/
   ├─ rfm/
   │  ├─ fixed_height/
   │  └─ two_image/
   └─ accuracy/
```

## 12. 常见问题

### 12.1 找不到可执行文件

程序生成在：

```text
build\vs2022-x64-debug\bin\lunar_rpc_tool.exe
```

不是项目根目录，也不是 `src` 目录。

### 12.2 找不到 TIFF 或 RPC

检查：

- 文件名是否和 `targets.csv` 完全一致；
- TIFF 是否在 `data/images/`；
- RPC 是否在 `data/rpc/`；
- RPC 文件名是否为 `<影像名称>_rpc.txt`。

### 12.3 提示“只支持单波段 float32 TIFF”

当前读取器不会自动把其他位深转换为 8 位。需要输入单波段、32 位 IEEE 浮点、条带式 TIFF。

### 12.4 方案二未收敛或残差很大

常见原因：

- 两景量测点不是同一个物理位置；
- 自动候选点锁到了亮岩石或其他地物；
- 两景观测几何条件接近，高程不容易分辨；
- 手动量测仍有误差。

程序会根据 LM 数值停止条件输出 `converged`，但 `converged=true` 不代表绝对定位精度一定合格，必须同时查看 `residual_rms_px` 和 `condition_number`。

### 12.5 报告内容乱码

程序和报告使用 UTF-8。建议使用 VS Code、Notepad++ 或 GitHub 查看。用支持 Markdown 的编辑器打开 `.txt` 报告时，标题和表格可以正常渲染。

## 13. Git 和本地文件

通常不提交到 Git 的内容：

- `data/images/*.tif`
- `data/images/*.tiff`
- `third_party/opencv/`
- `third_party/libtiff/`
- `build/`
- `out/`
- `output/` 下的运行结果

项目代码、配置、RPC 文件和真值文件按仓库当前规则保存。

## 14. 参考文档

- `TECHNICAL_PLAN.md`：技术方案、公式、模块接口和修订记录。
- `PROGRESS.md`：项目进度、当前状态和后续工作。
