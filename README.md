# Lunar RPC Localization

C++/CMake/OpenCV implementation of RPC direct localization for LRRRs in LROC NAC imagery.

The project supports point measurement, fixed-height LM inversion, two-image LM inversion, and batch accuracy evaluation.

## Current Status

- Technical plan: complete, see `TECHNICAL_PLAN.md`.
- Progress tracking: see `PROGRESS.md`.
- Implementation: not started.

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
- vcpkg manifest mode for third-party dependency installation
- Visual Studio bundled vcpkg is preferred on Windows

## Data Policy

Large TIFF files and downloaded third-party libraries are not committed.

- TIFF images are stored under `data/images/` and ignored by Git.
- RPC files are stored under `data/rpc/`.
- Truth coordinates are stored under `data/truth/`.
- Generated results belong under `output/`.
- Downloaded vcpkg content belongs under `third_party/vcpkg/` and is ignored by Git.

## Repository Layout

- `config/`: runtime configuration and target tables.
- `data/`: local input data with separate subdirectories.
- `include/`: public headers grouped by module.
- `src/`: source files grouped by module.
- `third_party/`: dependency manifests and bootstrap support.
- `scripts/`: dependency setup and helper scripts.
- `tests/`: module and integration tests.
- `output/`: generated measurement, RFM, accuracy, and log files.
