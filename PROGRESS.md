# Project Progress

> **Last updated:** 2026-10-08
> **Plan version:** v0.28

## Status Legend

| Status | Meaning |
|---|---|
| 未开始 | Work has not started |
| 进行中 | Work is currently in progress |
| 已完成 | Deliverable is complete and verified |
| 阻塞 | Waiting for an external decision or dependency |

## Current Progress

| ID | Module or Task | Status | Progress | Current Result | Next Step |
|---|---|---:|---|---|---|
| P0 | Requirements and data understanding | 已完成 | 100% | Requirement PDF and data inventory complete; data moved to `data/images`, `data/rpc`, and `data/truth` | Keep as reference for implementation |
| P1 | Technical plan and interfaces | 已完成 | 100% | `TECHNICAL_PLAN.md` v0.28 complete | Implement according to the confirmed interfaces |
| P2 | Git repository and remote connection | 已完成 | 100% | GitHub remote configured | Maintain one commit per completed code task |
| P3 | CMake project scaffold | 已完成 | 100% | MSVC 19.44 Debug/Release configure/build passed with Visual Studio CMake and Ninja using local OpenCV and libtiff; all runtime, import, static, and debug artifacts are emitted under `out/build/<config>/bin` | Keep the scaffold stable while adding module targets |
| P4 | Local third-party dependency setup | 已完成 | 100% | OpenCV 4.12.0 copied into `third_party/opencv`; libtiff 4.7.2 core installed into `third_party/libtiff` | Keep binaries local and ignored by Git |
| P5 | Common module | 已完成 | 100% | Configuration, path resolution, target/truth parsing, shared types implemented | Extend shared utilities only when later modules need them |
| P6 | Point measurement module | 进行中 | 98% | TIFF ROI reading, 200 m default window, subpixel automatic candidate, 2x display, precomputed refresh, 0.1 pixel arrow adjustment, `q` confirmation, automatic overwrite, decimal TXT output, and unified `lunar_rpc_tool measure` entry implemented; MSVC build and six-image non-interactive checks passed | Perform final interactive measurements and tuning |
| P7 | RPC parser and forward model | 进行中 | 60% | RPC parser and forward model implemented for theoretical image-point calculation | Implement shared inverse-model base for methods one and two |
| P8 | LM fixed-height inversion | 未开始 | 0% | None | Implement method one and result TXT output |
| P9 | LM two-image inversion | 未开始 | 0% | None | Implement joint solution, conditioning diagnostics, and result TXT output |
| P10 | Accuracy evaluation module | 未开始 | 0% | None | Implement batch input, truth matching, statistics, and report output |
| P11 | End-to-end integration | 未开始 | 0% | None | Connect all three command-line programs |
| P12 | Tests and final verification | 已取消 | 100% | User requested no test files or test targets | Excluded from the project scope |

## Update Rule

After each code task is completed:

1. Update this table and the overall progress values.
2. Record the completed deliverables in `Current Result`.
3. Update the next pending item.
4. Commit and push the corresponding code and this progress table together.
