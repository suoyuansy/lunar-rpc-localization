# Project Progress

> **Last updated:** 2026-10-08
> **Plan version:** v0.22

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
| P1 | Technical plan and interfaces | 已完成 | 100% | `TECHNICAL_PLAN.md` v0.22 complete | Implement according to the confirmed interfaces |
| P2 | Git repository and remote connection | 已完成 | 100% | GitHub remote configured | Maintain one commit per completed code task |
| P3 | CMake project scaffold | 未开始 | 0% | None | Create root CMake config and module targets |
| P4 | vcpkg dependency bootstrap | 未开始 | 0% | None | Add manifest, presets, and bootstrap script |
| P5 | Common module | 未开始 | 0% | None | Implement config, paths, logging, and shared types |
| P6 | Point measurement module | 未开始 | 0% | None | Implement libtiff ROI reading, auto candidate, and manual UI |
| P7 | RPC parser and forward model | 未开始 | 0% | None | Implement RPC parsing and RFM forward evaluation |
| P8 | LM fixed-height inversion | 未开始 | 0% | None | Implement method one and result TXT output |
| P9 | LM two-image inversion | 未开始 | 0% | None | Implement joint solution, conditioning diagnostics, and result TXT output |
| P10 | Accuracy evaluation module | 未开始 | 0% | None | Implement batch input, truth matching, statistics, and report output |
| P11 | End-to-end integration | 未开始 | 0% | None | Connect all three command-line programs |
| P12 | Tests and final verification | 未开始 | 0% | None | Add unit tests, regression tests, and end-to-end verification |

## Update Rule

After each code task is completed:

1. Update this table and the overall progress values.
2. Record the completed deliverables in `Current Result`.
3. Update the next pending item.
4. Commit and push the corresponding code and this progress table together.
