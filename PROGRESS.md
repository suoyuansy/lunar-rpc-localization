# Project Progress

> **Last updated:** 2026-10-08
> **Plan version:** v0.48

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
| P1 | Technical plan and interfaces | 已完成 | 100% | `TECHNICAL_PLAN.md` v0.48 complete | Implement according to the confirmed interfaces |
| P2 | Git repository and remote connection | 已完成 | 100% | GitHub remote configured | Maintain one commit per completed code task |
| P3 | CMake project scaffold | 已完成 | 100% | MSVC 19.44 Debug/Release configure/build passed with Visual Studio CMake and Ninja; module sources compile directly into one executable, and artifacts are emitted under `build/<preset>/bin` without project-internal `.lib` files | Keep the single-target scaffold stable |
| P4 | Local third-party dependency setup | 已完成 | 100% | OpenCV 4.12.0 copied into `third_party/opencv`; libtiff 4.7.2 core installed into `third_party/libtiff` | Keep binaries local and ignored by Git |
| P5 | Common module | 已完成 | 100% | Configuration, path resolution, target/truth parsing, shared types implemented | Extend shared utilities only when later modules need them |
| P6 | Point measurement module | 已完成 | 100% | TIFF ROI reading, 500 m default window, subpixel automatic candidate, single-channel 8-bit display via 0.1%-99.9% percentile linear stretch, 0.1 pixel arrow adjustment, `q` confirmation, automatic overwrite, decimal TXT output, directory batch mode, and reflector/image information in both console and display implemented; six-image batch scan passed | Run final interactive batch if all six point measurements are to be regenerated |
| P7 | RPC parser and forward model | 已完成 | 100% | RPC parser, forward model, and exposed normalization offsets are implemented | Keep the model interface stable for later accuracy evaluation |
| P8 | LM fixed-height inversion | 已完成 | 100% | Fixed-height LM inversion and result TXT output implemented; verified with the existing M175124932RE measurement | Generate final measurements for all images |
| P9 | LM two-image inversion | 进行中 | 95% | Two-image joint LM solver, initial-value loading, condition-number diagnostics, and result TXT output implemented; a self-consistent two-view test converged, while inconsistent automatic candidate pairs correctly remain unresolved | Validate with final manually confirmed point pairs |
| P10 | Accuracy evaluation module | 已完成 | 100% | `evaluate` reads fixed-height and two-image RFM result files, matches truth through `targets.csv`, computes east/north/height errors, horizontal and 3D distances, grouped mean/RMSE statistics, and writes `output/accuracy/accuracy_report.txt`; verified with 9 result files | Re-run after final measurement and localization results are regenerated |
| P11 | End-to-end integration | 已完成 | 100% | `measure`, `localize`, and `evaluate` are integrated into the single `lunar_rpc_tool` executable and the full six-image / three-target workflow has been executed | Optional final rerun and report review |
| P12 | Tests and final verification | 已取消 | 100% | User requested no test files or test targets | Excluded from the project scope |

## Update Rule

After each code task is completed:

1. Update this table and the overall progress values.
2. Record the completed deliverables in `Current Result`.
3. Update the next pending item.
4. Commit and push the corresponding code and this progress table together.
