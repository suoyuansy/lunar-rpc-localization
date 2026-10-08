# RPC 直接定位技术方案

> **版本：** v0.34（草案）
> **状态：** 核心模块实施中
> **更新日期：** 2026-10-08

## 1. 已确认的技术决策

- 编程语言：C++17。
- 项目构建：CMake。
- 默认 Windows 构建环境：Visual Studio 2022 Developer Command Prompt。
- 默认 Windows 编译器：MSVC。
- 默认使用 Visual Studio 自带的 CMake 和 Ninja。
- 不使用 MSYS2 UCRT64 作为主构建方案。
- 图像处理、矩阵运算和交互显示：OpenCV。
- TIFF 局部窗口读取：libtiff。
- 像点坐标：0 基、像素中心坐标。
- 判读范围：以理论像点为中心的 `200 m × 200 m` 正方形。
- 判读方法：单点自动检测后，人工调整。
- 鼠标点击：候选点立即跳到点击位置。
- 像点显示：在 2 倍放大窗口中判读。
- 方向键：以 0.1 pixel 为步长调整。
- 像点坐标：允许小数，量测结果保留 3 位小数。
- 确认键：`q`。
- RPC 反算：方案一和方案二都使用 LM。
- 第一版权重：等权，\(W=I\)。
- 像点量测文件：只保存 `sample` 和 `line`。
- RFM 结果文件：保存反射器标识、方法和经纬度高程。
- 精度评价：使用月球局部表面距离，不计算大圆距离交叉校验。

## 2. 端到端技术路线

1. 读取项目参数文件、目标表和真值文件。
2. 对每景影像执行像点量测，生成一个量测 TXT。
3. 对每景影像执行方案一，固定高程反算经纬度。
4. 对每个目标执行方案二，使用两景影像联合求解经纬度和高程。
5. 批量读取方案一和方案二的有效结果，与真值比较。
6. 计算误差分量和月球表面距离。
7. 在控制台打印结果，并保存精度评价 TXT。

完整数据流：

1. `rpc_project.ini` 提供目录和默认参数。
2. `targets.csv` 提供影像、目标、真值标识和分辨率映射。
3. `真值坐标.txt` 提供月面真值。
4. `lunar_rpc_tool measure` 输出量测 TXT。
5. `lunar_rpc_tool localize --method fixed_height` 输出方案一结果。
6. `lunar_rpc_tool localize --method two_image` 读取两景方案一结果作为初值，输出来源方案二结果。
7. `lunar_rpc_tool evaluate` 批量读取方案一和方案二结果，输出精度报告。

## 3. 项目目录与文件约定

- `CMakeLists.txt`：CMake 总入口。
- `CMakePresets.json`：统一构建配置。
- `cmake/`：自定义 CMake 模块。
- `config/`：运行参数、目标表和默认配置。
- `data/images/`：默认原始影像目录。
- `data/rpc/`：默认 RPC 目录。
- `data/truth/`：默认真值目录。
- `include/`：公共头文件。
- `src/`：模块源文件和程序入口。
- `third_party/`：本地 OpenCV 和 libtiff 依赖。
- `scripts/`：依赖安装和辅助脚本。
- `output/`：所有程序输出。
- `output/measurements/`：像点量测 TXT。
- `output/rfm/fixed_height/`：方案一结果。
- `output/rfm/two_image/`：方案二结果。
- `output/accuracy/`：精度评价结果。
- `output/logs/`：运行日志。
- `tmp/`：可删除的临时文件。

建议源码和头文件按模块划分：

- `include/common/`、`src/common/`：配置、路径、日志和通用数据结构。
- `include/point_measurement/`、`src/point_measurement/`：像点判读和量测。
- `include/rfm/`、`src/rfm/`：RPC 解析、正算和 LM 反算。
- `include/accuracy/`、`src/accuracy/`：真值匹配、误差统计和结果输出。
- `src/point_measurement/main.cpp`：量测程序入口。
- `src/rfm/main.cpp`：RFM 程序入口。
- `src/accuracy/main.cpp`：精度评定程序入口。

原始 6 GB 级 TIFF 数据不复制到仓库中。默认配置可以使用相对路径指向现有数据目录；如果以后把数据整理到 `data/images/`，只修改 `rpc_project.ini`，不修改代码。

量测文件命名：

- `<影像名称>_point_measurement.txt`。

RFM 结果命名：

- 方案一：`<影像名称>_rfm_fixed_height.txt`。
- 方案二：`<目标名称>_rfm_two_image.txt`。

精度文件命名：

- `accuracy_report.txt`。

原始 TIFF、RPC 和真值文件只读，不修改、不覆盖。

## 4. 参数文件与目标表

### 4.1 默认参数文件

- 默认路径：`RPC_localization/config/rpc_project.ini`。
- 所有命令行程序都支持 `--config`。
- 参数优先级：命令行参数、参数文件、项目默认路径。
- 找不到参数文件时必须报错，并列出搜索路径。

### 4.2 rpc_project.ini 建议字段

所有目录字段都使用相对于项目根目录的路径，不写绝对路径。

| 字段 | 含义 |
|---|---|
| `image_dir` | 原始 TIFF 目录，默认 `data/images/` |
| `rpc_dir` | RPC 文件目录，默认 `data/rpc/` |
| `measurement_dir` | 量测 TXT 目录，默认 `output/measurements/` |
| `rfm_fixed_height_dir` | 方案一结果目录，默认 `output/rfm/fixed_height/` |
| `rfm_two_image_dir` | 方案二结果目录，默认 `output/rfm/two_image/` |
| `accuracy_dir` | 精度评价结果目录，默认 `output/accuracy/` |
| `target_table` | 目标映射表路径 |
| `truth_file` | 真值文件路径 |
| `roi_size_m` | 初始判读窗口边长，默认 200 |
| `moon_radius_m` | 月球平均半径，默认 1737400 |
| `lm_max_iterations` | LM 最大迭代次数，默认 50 |
| `lm_residual_tolerance_px` | 像点残差阈值，默认 1e-4 |
| `lm_step_tolerance` | 参数增量阈值 |
| `lm_mu_initial` | LM 初始阻尼，默认 1e-3 |

### 4.3 targets.csv 建议列

| 列名 | 含义 |
|---|---|
| `reflector_id` | 统一反射器标识 |
| `truth_id` | 真值文件中的标识 |
| `image_1` | 第一景影像名称 |
| `image_2` | 第二景影像名称 |
| `resolution_1_mpp` | 第一景分辨率，m/pixel |
| `resolution_2_mpp` | 第二景分辨率，m/pixel |

统一标识使用：

- `Apollo11`。
- `Apollo14`。
- `LK1`。

真值文件中的 `A11、A14、LK1` 通过 `truth_id` 映射到统一标识。

## 5. 像点坐标量测模块

### 5.1 功能

- 根据目标理论坐标和 RPC 正算得到理论像点。
- 将理论像点作为 `200 m × 200 m` 窗口中心。
- 使用 libtiff 读取窗口。
- 使用 OpenCV 显示和调整窗口。
- 自动生成唯一候选点。
- 使用 2 倍最近邻放大显示窗口，便于观察小目标。
- 显示使用 ROI 有效值的最小值和最大值线性映射到 `[0, 255]`，输出单通道 `CV_8U`。
- 显示阶段不做 2%～98% 分位数拉伸，也不做 `GRAY -> BGR` 转换。
- 判读标记使用黑色描边加白色细十字，减少对局部影像的遮挡。
- 人工进行鼠标粗调和方向键 0.1 pixel 微调。
- 按 `q` 确认最终像点。

### 5.2 反射器图像特征

- LRRR 预计表现为小尺寸、局部高对比、相对孤立的亮斑。
- 目标可能只占 1 至数个 pixel。
- 窗口内可能存在多个高对比目标。
- 不可只使用亮度阈值，否则容易误检岩石、坑缘、坏点和噪声。

### 5.3 单点自动判读算法

1. 使用原始 `float32` 数据，不使用 8 位显示图进行检测。
2. 检测和显示严格分离：检测始终基于 `CV_32F`，显示使用 ROI 的 `[min, max]` 线性映射到 8 位窗口。
3. 屏蔽 NoData 和窗口边界。
4. 对不同尺度计算局部高斯背景。
5. 计算原始值减局部背景的高通响应。
6. 使用 MAD 估计噪声并归一化响应。
7. 初始尺度取 1、2、4 pixel。
8. 对多尺度正响应取最大值。
9. 搜索局部极大值并抑制相邻重复候选。
10. 排除不具有稳定点扩散形态的单像素异常。
11. 计算候选的对比度、面积、紧致度、尺度稳定性和中心距离。
12. 综合评分，只输出得分最高的一个候选点。
13. 对最高分像点邻域做一维抛物线插值，得到亚像素自动初值。

初始评分权重：

| 指标 | 权重 |
|---|---:|
| 局部对比度 | 0.45 |
| 形状紧致度 | 0.20 |
| 尺度稳定性 | 0.20 |
| 与理论中心距离 | 0.15 |

如果最高分过低，返回唯一候选并标记低置信度；如果无有效候选，可以使用理论像点作为交互初值，但不能直接作为最终量测结果。

采用亚像素初值和 0.1 pixel 微调的原因是：LRRR 目标可能只占 1 至数个
pixel，若始终限制为整数像点，会引入约半像素量级的量化误差。保留小数后，
后续 RFM 反算和高程求解可以直接使用更连续的像点坐标。

窗口默认放大 2 倍。200 m 窗口在现有数据中约为 167 至 501 pixel，放大后
约为 334 至 1002 pixel，既能辨认小目标，又不会因过度放大造成窗口尺寸和
每帧重绘开销过大。

### 5.4 命令行接口

程序名：`lunar_rpc_tool`

子命令：`measure`

参数：

- `--config`：可选，指定参数文件。
- `--image`：可选，直接指定 TIFF。
- `--image-name`：可选，只指定影像名称。
- `--image-dir`：可选，覆盖影像目录。
- `--output-dir`：可选，覆盖量测输出目录。

默认和错误规则：

- 未指定 `--output-dir` 时，使用 `rpc_project.ini` 中的 `measurement_dir`。
- 未指定 `--image` 且未指定 `--image-name` 时，必须报错。
- 指定 `--image-name` 时，在指定或默认影像目录中查找对应 TIFF。
- 指定目录或默认目录没有对应影像时，报出完整查找路径和期望文件名。
- 根据影像名称在 `targets.csv` 中查找 `reflector_id` 和 `truth_id`。
- 根据 `truth_id` 从真值文件读取经纬度和高程；任一步匹配失败时立即报错。
- 量测结果已存在时直接覆盖，并在控制台提示已覆盖原文件。

### 5.5 输出接口

文件：`<影像名称>_point_measurement.txt`

内容固定为两行：

`sample=2487.000`

`line=23766.000`

坐标是原影像全局 0 基像素中心坐标，允许小数并保留 3 位小数。

## 6. RFM 经纬度高程计算模块

### 6.1 正算公式

设真实经度为 `λ`、纬度为 `φ`、高程为 `h`。

归一化：

\[
L_n=\frac{\lambda-LONG\_OFF}{LONG\_SCALE}
\]

\[
P_n=\frac{\varphi-LAT\_OFF}{LAT\_SCALE}
\]

\[
H_n=\frac{h-HEIGHT\_OFF}{HEIGHT\_SCALE}
\]

20 项基函数：

\[
\mathbf q=
[1,L_n,P_n,H_n,L_nP_n,L_n^2,P_n^2,H_n^2,P_nH_n,L_nH_n,
L_n^3,L_nP_n^2,L_nH_n^2,L_n^2P_n,P_n^3,P_nH_n^2,
L_n^2H_n,P_n^2H_n,H_n^3,L_nP_nH_n]^T
\]

像点：

\[
u=u_0+u_s\frac{\mathbf a_u^T\mathbf q}{\mathbf b_u^T\mathbf q}
\]

\[
v=v_0+v_s\frac{\mathbf a_v^T\mathbf q}{\mathbf b_v^T\mathbf q}
\]

其中 `u` 是 `sample`，`v` 是 `line`。

### 6.2 方案一：固定高程 LM 反算

已知一景影像的 `sample、line` 和高程 \(h_{fixed}\)，未知量：

\[
\mathbf x=[\lambda,\varphi]^T
\]

残差：

\[
\mathbf r_1(\mathbf x)=
\begin{bmatrix}
u(\lambda,\varphi,h_{fixed})-u_{obs}\\
v(\lambda,\varphi,h_{fixed})-v_{obs}
\end{bmatrix}
\]

LM 方程：

\[
(J_1^TW_1J_1+\mu D_1)\Delta\mathbf x
=-J_1^TW_1\mathbf r_1
\]

等权时：

\[
W_1=I_2
\]

初值：

\[
\lambda_0=LONG\_OFF
\]

\[
\varphi_0=LAT\_OFF
\]

在归一化变量中相当于从 \((L_n,P_n)=(0,0)\) 开始。

### 6.3 方案二：双影像 LM 联合反算

未知量：

\[
\mathbf x=[\lambda,\varphi,h]^T
\]

第 `i` 景的残差：

\[
\mathbf r_i(\mathbf x)=
\begin{bmatrix}
u_i(\lambda,\varphi,h)-u_{i,obs}\\
v_i(\lambda,\varphi,h)-v_{i,obs}
\end{bmatrix}
\]

总残差：

\[
\mathbf R(\mathbf x)=
\begin{bmatrix}
\mathbf r_1(\mathbf x)\\
\mathbf r_2(\mathbf x)
\end{bmatrix}
\in\mathbb R^4
\]

目标函数：

\[
\min_{\lambda,\varphi,h}\mathbf R^TW\mathbf R
\]

等权时：

\[
W=I_4
\]

LM 方程：

\[
(J^TWJ+\mu D)\Delta\mathbf x
=-J^TW\mathbf R
\]

雅可比矩阵：

\[
J=
\begin{bmatrix}
\partial u_1/\partial\lambda & \partial u_1/\partial\varphi & \partial u_1/\partial h\\
\partial v_1/\partial\lambda & \partial v_1/\partial\varphi & \partial v_1/\partial h\\
\partial u_2/\partial\lambda & \partial u_2/\partial\varphi & \partial u_2/\partial h\\
\partial v_2/\partial\lambda & \partial v_2/\partial\varphi & \partial v_2/\partial h
\end{bmatrix}
\]

方案二初值：

- 先对两景影像分别执行方案一。
- 经纬度初值取两景方案一经纬度的平均值。
- 高程初值取两景 `HEIGHT_OFF` 的平均值。
- 不使用真值高程作为初值。
- 如果两景方案一结果差异过大，应报警告并检查像点是否对应同一物理位置。
- 如果方案二不收敛，可以退回两景 RPC 的 `LONG_OFF`、`LAT_OFF` 平均值作为备用初值。

### 6.4 LM 与奇异矩阵

直接高斯-牛顿正规方程：

\[
(J^TWJ)\Delta\mathbf x=-J^TW\mathbf r
\]

LM 方程：

\[
(J^TWJ+\mu D)\Delta\mathbf x=-J^TW\mathbf r
\]

因此，带 \(\mu D\) 的公式就是 LM 方程。

LM 的优点：

- 初值较差时比直接高斯-牛顿稳定。
- 矩阵接近奇异时仍可求逆。
- 能控制过大的迭代步。
- 适合方案二的弱高程观测条件。

阻尼更新：

1. 初始 \(\mu=10^{-3}\)。
2. 残差下降时接受增量并减小 \(\mu\)。
3. 残差上升时拒绝增量并增大 \(\mu\)。
4. 重复到收敛。

方阵 \(A\) 奇异表示 \(A\) 不可逆，等价于：

\[
\det(A)=0
\]

如果 \(J^TWJ\) 接近奇异，说明不同未知量组合几乎产生相同像点变化，参数解对噪声非常敏感。

方案二中最典型的情况是两张影像观测方向过于接近，高程变化可以由经纬度变化近似补偿。此时高程没有真正可分辨性。

诊断指标：

\[
\kappa(J)=\frac{\sigma_{max}}{\sigma_{min}}
\]

条件数越大越接近奇异。LM 只能改善数值稳定性，不能创造实际不存在的高程观测能力。

当前实现采用前向差分计算雅可比矩阵：

- 经度和纬度扰动步长取 `1e-7 degree`。
- 高程扰动步长取 `1e-3 m`。
- LM 初始阻尼系数为 `1e-3`，接受增量后乘 `0.1`，拒绝增量后乘 `10`。
- 最多迭代 `100` 次。
- 残差 RMS 小于 `1e-6 pixel` 或增量范数小于 `1e-10` 时认为收敛。
- 使用 OpenCV SVD 计算最终雅可比矩阵的条件数。

### 6.5 命令行接口

程序名：`lunar_rpc_tool`

子命令：`localize`

参数：

- `--config`：可选，指定参数文件。
- `--method`：必选，取值为 `fixed_height` 或 `two_image`。
- `--target`：可选，指定目标，由 `targets.csv` 得到影像对。
- `--image-name`：可重复，直接指定一景或两景影像。
- `--measurement-dir`：可选，覆盖量测目录。
- `--rpc-dir`：可选，覆盖 RPC 目录。
- `--output-dir`：可选，覆盖对应方法的 RFM 结果目录。

方法一参数规则：

- 必须指定且只能指定一景影像。
- 可以用 `--image-name` 指定该影像。
- 可以同时用 `--target` 校验影像是否属于该目标。

方法二参数规则：

- 可以指定 `--target`，程序根据 `targets.csv` 自动取得两景影像。
- 也可以直接指定两景影像名称。
- 必须恰好得到两景不同影像，否则报错。

方案一：

- 输入一景影像名称或路径。
- 读取一个量测 TXT 和一个 RPC。
- 从目标表读取统一反射器标识和固定高程。
- 使用 LM 反算经纬度。
- 未指定 `--output-dir` 时输出到 `output/rfm/fixed_height/`。

方案二：

- 输入同一目标的两景影像名称。
- 读取两个量测 TXT 和两个 RPC。
- 读取两景方案一结果作为初值。
- 缺少任一量测文件、RPC、方案一结果时立即报错。
- 使用 LM 联合反算经纬度和高程。
- 未指定 `--output-dir` 时输出到 `output/rfm/two_image/`。

### 6.6 RFM 结果文件

方案一文件：

`<影像名称>_rfm_fixed_height.txt`

方案二文件：

`<目标名称>_rfm_two_image.txt`

固定字段：

| 字段 | 含义 |
|---|---|
| `reflector_id` | 统一反射器标识 |
| `method` | `fixed_height` 或 `two_image` |
| `source_images` | 来源影像，逗号分隔 |
| `longitude` | 解算经度，degree |
| `latitude` | 解算纬度，degree |
| `height_m` | 解算高程，m |

只有 LM 收敛后才写出有效结果文件。未收敛时打印错误，不生成可被精度模块读取的结果。

## 7. 精度评定模块

### 7.1 功能

- 批量读取方案一和方案二 RFM 结果 TXT。
- 根据 `reflector_id` 查找真值。
- 比较解算经纬度高程与真值。
- 计算误差分量和月球表面距离。
- 控制台打印结果。
- 保存精度评价 TXT。

### 7.2 命令行接口

程序名：`lunar_rpc_tool`

子命令：`evaluate`

参数：

- `--config`：可选，指定参数文件。
- `--input`：可重复，指定一个或多个 RFM 结果 TXT。
- `--result-dir`：可重复，指定结果目录。
- `--truth-file`：可选，覆盖真值文件。
- `--output`：可选，指定最终 TXT。
- `--output-dir`：可选，指定输出目录。

默认行为：

- 未指定输入时，扫描 `output/rfm/fixed_height/` 和 `output/rfm/two_image/`。
- 未指定真值文件时，使用 `rpc_project.ini` 中的 `truth_file`。
- 未指定输出时，保存到 `output/accuracy/accuracy_report.txt`。

错误规则：

- 输入目录不存在时，报目录不存在。
- 没有找到任何 TXT 时，报扫描目录和匹配规则。
- 结果文件缺少必需字段时，报文件名和字段名。
- `reflector_id` 无法匹配真值时，报文件名和目标标识。

### 7.3 结果文件必需字段

- `reflector_id`
- `method`
- `source_images`
- `longitude`
- `latitude`
- `height_m`

### 7.4 月面距离计算

设解算坐标：

\[
(\lambda_s,\varphi_s,h_s)
\]

真值坐标：

\[
(\lambda_t,\varphi_t,h_t)
\]

将角度差转为弧度：

\[
\Delta\lambda_{rad}
=(\lambda_s-\lambda_t)\frac{\pi}{180}
\]

\[
\Delta\varphi_{rad}
=(\varphi_s-\varphi_t)\frac{\pi}{180}
\]

采用平均纬度和平均高程：

\[
\varphi_m=\frac{\varphi_s+\varphi_t}{2}
\]

\[
h_m=\frac{h_s+h_t}{2}
\]

东西方向误差：

\[
\Delta E=(R_m+h_m)\cos(\varphi_m)\Delta\lambda_{rad}
\]

南北方向误差：

\[
\Delta N=(R_m+h_m)\Delta\varphi_{rad}
\]

高程方向误差：

\[
\Delta H=h_s-h_t
\]

水平月球表面距离：

\[
D_{horizontal}=\sqrt{\Delta E^2+\Delta N^2}
\]

三维距离：

\[
D_{3D}=\sqrt{D_{horizontal}^2+\Delta H^2}
\]

月球平均半径：

\[
R_m=1737400\ \mathrm{m}
\]

### 7.5 统计结果

对同一反射器的 `N` 个结果计算：

\[
Mean_h=\frac{1}{N}\sum D_{horizontal,i}
\]

\[
RMSE_h=\sqrt{\frac{1}{N}\sum D_{horizontal,i}^2}
\]

输出内容：

- 每条结果的反射器标识、方法、来源影像。
- 解算经纬度高程和真值经纬度高程。
- 经度差、纬度差和高程差。
- 东西方向误差、南北方向误差。
- 水平距离和三维距离。
- 按反射器分组的平均误差和 RMSE。
- 全部结果的总体平均误差和 RMSE。
- 控制台信息与保存到 TXT 的内容一致。

## 8. 错误处理与可复现性

- 所有输入文件缺失都必须报出完整路径。
- 所有结果文件写盘前检查必要字段。
- 量测结果默认直接覆盖，并在控制台提示是否覆盖了已有文件。
- RFM 未收敛时不得写出有效结果。
- 方案二缺少任一依赖时不得继续计算。
- 真值和 RPC 不得修改。
- 所有默认目录来自 `rpc_project.ini`。
- 所有运行参数和来源文件名称写入结果文件或日志。

## 9. CMake 构建与依赖管理

### 9.1 CMake 目标划分

- `lunar_rpc_tool`：唯一的可执行程序目标。
- `common/`：配置、路径、日志和通用数据结构。
- `point_measurement/`：像点判读、窗口读取和量测结果输出。
- `rfm/`：RPC 解析、RFM 正算和 LM 反算。
- `accuracy/`：真值匹配、距离计算和统计输出。

依赖关系：

- 上述模块是源码目录划分，不再分别创建静态库目标。
- `lunar_rpc_tool` 直接编译各模块 `.cpp` 文件，避免在 `bin/` 中产生项目内部的 `.lib`。
- 尚未实现的子命令仍然明确报错。

### 9.2 无绝对路径原则

- `CMakeLists.txt` 中只使用 `${CMAKE_CURRENT_SOURCE_DIR}`、`${PROJECT_SOURCE_DIR}` 和相对路径。
- `rpc_project.ini` 中只保存相对项目根目录的路径。
- 源码中不保存具体磁盘路径。
- 程序运行时从当前工作目录或可执行文件目录向上查找项目根目录。
- 项目根目录的判定依据是同时存在 `CMakeLists.txt` 和 `config/rpc_project.ini`。
- 找到项目根目录后，再解析 `data/`、`output/` 和 `config/` 相对路径。

### 9.3 本地第三方依赖

项目不再自动下载 OpenCV 和 libtiff。

- `third_party/opencv/`：OpenCV 4.12.0 Windows x64 MSVC 预编译包。
- `third_party/opencv/include/`：OpenCV 头文件。
- `third_party/opencv/lib/`：OpenCV Debug 和 Release 导入库。
- `third_party/opencv/bin/`：OpenCV Debug 和 Release DLL。
- `third_party/libtiff/x64-windows/`：libtiff 4.7.2 x64 MSVC 安装结果。
- `third_party/libtiff/x64-windows/include/`：libtiff 头文件。
- `third_party/libtiff/x64-windows/lib/` 和 `debug/lib/`：libtiff 导入库。
- `third_party/libtiff/x64-windows/bin/` 和 `debug/bin/`：libtiff DLL。

根目录 `CMakeLists.txt` 直接定义两个导入目标：

- `opencv_local`。
- `tiff_local`。

业务模块统一链接 `third_party_deps`，不再执行 `find_package()`，不再调用 vcpkg，也不下载第三方源码。

这些二进制目录体积较大，保存在本地但不提交 Git。

### 9.4 CMake Presets

建议提供以下预设：

- `vs2022-x64-debug`。
- `vs2022-x64-release`。
- `linux-gcc-debug`。
- `linux-gcc-release`。
- `macos-clang-debug`。
- `macos-clang-release`。

Windows 构建目录：

- Debug 使用 `build/vs2022-x64-debug/`。
- Release 使用 `build/vs2022-x64-release/`。
- 可执行文件、第三方运行库和调试文件统一放在对应构建目录的 `bin/` 中。
- 项目内部模块直接编译进 `lunar_rpc_tool`，不再生成 `rpc_*.lib`。
- CMake 和 Ninja 自己的中间文件保留在构建目录根层及 `CMakeFiles/` 中。

Visual Studio 2022 的预设优先使用 `%VSINSTALLDIR%Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe` 和 VS 自带 Ninja。CMake 直接读取项目内的本地依赖，不执行依赖下载。

预设与 IDE 的配合：

- Visual Studio 和 VS Code 可以直接读取 `CMakePresets.json`。
- CLion 和 Qt Creator 可以导入 CMake 配置并在 CMake options 中选择预设参数。
- 命令行始终可以使用 `cmake --preset`。
- IDE 切换不会改变依赖管理方式，只要仍通过 CMake 配置项目。

### 9.5 数据目录

- `data/images/`：默认原始 TIFF。
- `data/rpc/`：默认 RPC。
- `data/truth/`：默认真值。
- 现有 6 景 TIFF 已移动到 `data/images/`。
- 6 个 RPC 文件已移动到 `data/rpc/`。
- 真值文件已移动到 `data/truth/`。
- 6 GB 级原始 TIFF 不提交到 GitHub。

### 9.6 输出目录

- `output/measurements/`：量测结果。
- `output/rfm/fixed_height/`：方案一结果。
- `output/rfm/two_image/`：方案二结果。
- `output/accuracy/`：精度评价结果。
- `output/logs/`：日志。
- 程序在写入前自动创建缺失的输出目录。

### 9.7 构建流程

1. 打开 Visual Studio 2022 Developer Command Prompt。
2. 确认 MSVC、VS 自带 CMake 和 VS 自带 Ninja 可用。
3. 确认本地 OpenCV 和 libtiff 已位于 `third_party/`。
4. 使用 CMake Preset 配置项目。
5. 使用 CMake 构建统一可执行程序 `lunar_rpc_tool`。
6. 使用参数文件和相对路径运行程序。

构建完成后，统一可执行文件位于当前构建目录的 `bin/` 中。例如
`build/vs2022-x64-debug/bin/lunar_rpc_tool.exe` 或
`build/vs2022-x64-release/bin/lunar_rpc_tool.exe`。

首次构建示例流程：

1. 打开 Visual Studio 2022 Developer Command Prompt。
2. 执行 `cmake --preset vs2022-x64-release`。
3. 执行 `cmake --build --preset vs2022-x64-release`。

用户拿到项目后，需要确保本地 `third_party/opencv` 和 `third_party/libtiff` 存在。CMake 不再下载或编译第三方库。

## 10. 修订记录

| 版本 | 日期 | 修改内容 |
|---|---|---|
| v0.1 | 2026-10-07 | 建立首版技术方案 |
| v0.2 | 2026-10-07 | 改为 C++、CMake 和 OpenCV 方案 |
| v0.3 | 2026-10-07 | 简化为大致处理流程和可能使用的库 |
| v0.4 | 2026-10-07 | 将技术工具分为编程语言、项目构建和相关库 |
| v0.5 | 2026-10-07 | 将处理流程压缩为四个大方向 |
| v0.6 | 2026-10-07 | 增加影像判读范围、ROI 量测方法及采用原因 |
| v0.7 | 2026-10-07 | 确认引入 libtiff，采用理论像点中心和米制搜索半径确定窗口 |
| v0.8 | 2026-10-07 | 建议 500 m 正方形范围框，并比较人工与自动判读方式 |
| v0.9 | 2026-10-07 | 确认自动候选加人工调整流程，补充反射器图像特征和坐标约定 |
| v0.10 | 2026-10-07 | 明确单点自动判读算法、鼠标跳转和 1 pixel 方向键微调 |
| v0.11 | 2026-10-07 | 增加像点量测 TXT 文件规范和命令行程序调用约定 |
| v0.12 | 2026-10-07 | 固定 TXT 为五字段格式，只保存最终确认像点，不保存候选点 |
| v0.13 | 2026-10-07 | TXT 精简为仅保存 `sample` 和 `line` 两行 |
| v0.14 | 2026-10-08 | 增加 RFM 已知高程反算和双影像联合反算的数学方案 |
| v0.15 | 2026-10-08 | 补充 LM 原理、权重选择方法和两种方案的初值策略 |
| v0.16 | 2026-10-08 | 确认两种方案均用 LM、等权处理，并补充模块接口和默认路径规则 |
| v0.17 | 2026-10-08 | 增加 RFM 结果文件格式和批量精度评定模块方案 |
| v0.18 | 2026-10-08 | 删除大圆距离，统一模块接口和目录，补齐参数文件与真值映射 |
| v0.19 | 2026-10-08 | 明确 CMake 工程分层、无绝对路径规则、vcpkg 自动依赖和 data/output 目录 |
| v0.20 | 2026-10-08 | 细化 vcpkg 自动下载、CMake 联动和首次构建流程 |
| v0.21 | 2026-10-08 | 说明 manifest 模式与 IDE 无关，补充编译器 triplet 和 IDE 适配规则 |
| v0.22 | 2026-10-08 | 优先使用 VS2022 Developer Command Prompt、VS CMake/Ninja/vcpkg，并更新数据目录 |
| v0.23 | 2026-10-08 | 移除自动下载方案，改用本地 OpenCV 和 libtiff 依赖 |
| v0.24 | 2026-10-08 | 默认判读范围缩减为 200 m，优化显示刷新和中文控制台输出 |
| v0.25 | 2026-10-08 | 增加 2 倍显示、亚像素自动候选、0.1 pixel 微调和小数坐标输出 |
| v0.26 | 2026-10-08 | 合并为单一可执行程序并使用子命令分发，量测结果改为自动覆盖 |
| v0.27 | 2026-10-08 | 统一可执行文件改到构建目录根层，移除旧程序生成物 |
| v0.28 | 2026-10-08 | 对齐参考项目输出布局，构建产物统一放入各自构建目录的 `bin` |
| v0.29 | 2026-10-08 | 移除内部静态库目标，所有模块源文件直接编译进 `lunar_rpc_tool` |
| v0.30 | 2026-10-08 | 保留原 Visual Studio 预设构建目录，仅在各自目录内统一使用 `bin` 布局 |
| v0.31 | 2026-10-08 | 显示改为原始 `CV_32F` 单通道，取消分位数拉伸和 `GRAY -> BGR` |
| v0.32 | 2026-10-08 | 显示改为 ROI 有效值 `[min, max]` 到 `[0, 255]` 的线性映射 |
| v0.33 | 2026-10-08 | 缩小并细化判读十字丝，降低对影像细节的遮挡 |
| v0.34 | 2026-10-08 | 实现固定高程和双影像两种 LM 反算、结果文件输出和 `localize` 子命令 |
