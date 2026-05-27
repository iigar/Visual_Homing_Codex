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
- Added `Gray8ResizePreprocessor` for deterministic small-frame preprocessing with block-average resizing.
- Added `HealthMonitor` for per-frame timing, dropped-frame counters, route confidence, and health snapshots.
- Installed/activated Visual Studio Build Tools C++ workload and validated `core/` with MSVC/CMake: build passed and 3/3 CTest tests passed with `-C Debug`.
- Agreed to insert Milestone 1.5 before Visual Route Signature work: add `.gitignore`, `docs/BUILDING.md`, `scripts/test-core.ps1`, and an end-to-end replay/preprocess/health pipeline harness.
- Completed Milestone 1.5 with build artifact ignore rules, Windows build documentation, a one-command core test script, and `PipelineHarness` for replay -> preprocess -> health metrics.
- Validated Milestone 1.5 using `.\scripts\test-core.ps1 -Clean`: build passed and 4/4 CTest tests passed in Debug.
- Started Milestone 2 by adding route signature format v1: binary `VHRS` files with explicit little-endian fields, entry metadata, writer/reader functions, and payload round-trip tests.
- Validated route signature v1 using `.\scripts\test-core.ps1 -Clean`: build passed and 5/5 CTest tests passed in Debug.
- Added `RouteSignatureRecorder` to convert preprocessed frames and navigation estimates into route signature entries and write `VHRS` route files.
- Validated `RouteSignatureRecorder` using `.\scripts\test-core.ps1`: build passed and 6/6 CTest tests passed in Debug.
- Added replay-to-route recording path through `record_replay_route` and CLI `--record-route`, producing `.vhrs` files from Gray8 preprocessed replay frames.
- Validated replay-to-route recording using `.\scripts\test-core.ps1`: build passed and 6/6 CTest tests passed in Debug.
