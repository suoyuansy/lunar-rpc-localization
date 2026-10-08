# Lunar RPC Localization

C++/CMake/OpenCV implementation of RPC direct localization for LRRRs in LROC NAC imagery.

The project supports point measurement, fixed-height LM inversion, two-image LM inversion, and batch accuracy evaluation.

All functions use one executable:

```bat
build\vs2022-x64-debug\bin\lunar_rpc_tool.exe measure --image-name M175124932RE
build\vs2022-x64-debug\bin\lunar_rpc_tool.exe localize --method fixed_height --image-name M175124932RE
build\vs2022-x64-debug\bin\lunar_rpc_tool.exe evaluate
```

Release 构建使用：

```bat
build\vs2022-x64-release\bin\lunar_rpc_tool.exe
```

## Current Status

- Technical plan: complete, see `TECHNICAL_PLAN.md`.
- Progress tracking: see `PROGRESS.md`.
- Build scaffold: complete.
- Unified CLI: one `lunar_rpc_tool` executable dispatches `measure`, `localize`, and `evaluate`.
- Point measurement module: initial implementation complete; supports TIFF ROI reading, subpixel automatic candidate detection, raw `CV_32F` single-channel display, mouse/0.1-pixel arrow-key adjustment, `q` confirmation, automatic overwrite, and decimal TXT output.
- RPC inverse and accuracy modules: not started.
- Build output contains one project executable; module `.cpp` files are compiled directly and no `rpc_*.lib` files are emitted.

## Planned Modules

- `point_measurement`: inspect local TIFF windows, detect one candidate, and manually confirm the LRRR image point.
- `rfm`: parse RPC files, implement RFM forward evaluation, and solve LM inverse problems.
- `accuracy`: compare RFM solutions with LRRR truth coordinates and compute lunar surface errors.
- `common`: configuration, project-root discovery, path handling, logging, and shared data structures.

## Planned Data Flow

1. Read paths and target mappings from `config/`.
2. Measure LRRR image points and write TXT files under `output/measurements/`.
3. Run fixed-height RPC inversion for each image.
4. Run two-image joint inversion for each target.
5. Batch-evaluate RFM results against truth coordinates.

## Technology Stack

- C++17
- Visual Studio 2022 Developer Command Prompt
- Visual Studio CMake and Ninja
- OpenCV
- libtiff
- Local prebuilt third-party dependencies under `third_party/`

## Data Policy

Large TIFF files and local third-party libraries are not committed.

- TIFF images are stored under `data/images/` and ignored by Git.
- RPC files are stored under `data/rpc/`.
- Truth coordinates are stored under `data/truth/`.
- Generated results belong under `output/`.
- Local OpenCV binaries belong under `third_party/opencv/` and are ignored by Git.
- Local libtiff binaries belong under `third_party/libtiff/` and are ignored by Git.

## Repository Layout

- `config/rpc_project.ini`: runtime paths and ROI settings.
- `config/targets.csv`: target, image, truth ID, and resolution mapping.
- `config/`: runtime configuration and target tables.
- `data/`: local input data with separate subdirectories.
- `include/`: public headers grouped by module.
- `src/`: source files grouped by module.
- `third_party/`: local OpenCV and libtiff dependencies.
- `output/`: generated measurement, RFM, accuracy, and log files.
