# Session Log

## 2026-05-22

- Cloned `iigar/Visual_Homing_Codex`.
- Imported the previous Visual Homing project under `reference/`.
- Created a clean C++20 core skeleton under `core/`.
- Added architecture and roadmap documentation.
- Pushed initial baseline commit `d9eb930`.

## 2026-05-27

- Continued Milestone 1 replay pipeline work.
- Added `ReplayFrameSource` under `core/` with manifest parsing for `id,timestamp_ns,path`.
- Added dependency-light binary PGM P5 Gray8 frame loading for deterministic replay fixtures.
- Split the core into `visual_homing_core_lib` plus CLI executable so replay components can be tested.
- Added a CTest executable covering manifest loading, frame payloads, timestamps, and source lifecycle.
