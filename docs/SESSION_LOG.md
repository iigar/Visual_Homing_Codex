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
- Started Milestone 3 with `Gray8RouteMatcher`, an offline deterministic matcher using normalized mean absolute byte distance, route-window limiting, and confidence gating.
- Validated `Gray8RouteMatcher` using `.\scripts\test-core.ps1`: build passed and 7/7 CTest tests passed in Debug.
- Added replay-to-route matching path through `match_replay_route` and CLI `--match-route`, producing per-frame route index, progress, confidence, and validity metrics.
- Validated replay-to-route matching using `.\scripts\test-core.ps1`: build passed and 7/7 CTest tests passed in Debug.
- Added synthetic perturbation coverage for `Gray8RouteMatcher`, including brightness offsets and small payload perturbations.

## 2026-05-29

- Added coarse direction-error estimation to `Gray8RouteMatcher` using bounded horizontal pixel-shift search and exposed it in `--match-route` metrics.
- Completed Milestone 3 baseline: replay-to-route matching now reports route index, progress, confidence, validity, and coarse direction error.
- Validated Milestone 3 completion using `.\scripts\test-core.ps1`: build passed and 7/7 CTest tests passed in Debug.
- Started Milestone 4 with `BoundedNavigator`, converting route match direction error into bounded yaw-rate commands gated by health, confidence, validity, and match age.
- Validated `BoundedNavigator` using `.\scripts\test-core.ps1`: build passed and 8/8 CTest tests passed in Debug.
- Added yaw-rate slew limiting to `BoundedNavigator` so successive valid commands respect a configurable max yaw acceleration.
- Validated yaw-rate slew limiting using `.\scripts\test-core.ps1`: build passed and 8/8 CTest tests passed in Debug.
- Integrated `BoundedNavigator` into replay-to-route matching metrics so `--match-route` reports command validity and yaw-rate output; offline replay matching uses relaxed command-age gating because replay timestamps are not live `steady_clock::now()` values.
- Validated replay matching plus command generation using `.\scripts\test-core.ps1`: build passed and 8/8 CTest tests passed in Debug.
- Completed Milestone 4 baseline: navigation commands are bounded, confidence/age/health gated, yaw slew-limited, and validated both directly and through replay matching.
- Hardened the baseline before Milestone 5: parameterized matcher direction settings, added pipeline negative command tests, documented `.vhrs`, and added `DryRunCommandSink` as a no-MAVLink single-writer command sink.
- Validated the hardened baseline using `.\scripts\test-core.ps1`: build passed and 9/9 CTest tests passed in Debug.
- Started Milestone 5 with `DryRunMavlinkBridge`, a no-live-output MAVLink boundary that scripts heartbeat/mode/attitude/altitude telemetry and records command output through a single writer.
- Validated `DryRunMavlinkBridge` using `.\scripts\test-core.ps1`: build passed and 10/10 CTest tests passed in Debug.

## 2026-05-30

- Continued Milestone 5 by adding `MavlinkTelemetryAdapter` to convert dry-run MAVLink telemetry into health link state and `NavigationEstimate` values.
- Wired replay route matching through `DryRunMavlinkBridge` telemetry polling instead of manually forcing `mavlink_ok` in the health snapshot.
- Added deterministic tests for heartbeat-driven MAVLink health, stale telemetry rejection, health link application, and yaw/altitude mapping into navigation estimates.
- Validated the core using `.\scripts\test-core.ps1`: build passed and 11/11 CTest tests passed in Debug.
- Extended `MavlinkTelemetryAdapter` with dry-run command permission gates: telemetry must be fresh, heartbeat must be present, vehicle must be armed, and mode must be `Guided` before health allows navigation commands.
- Updated replay route matching scripted telemetry to model armed Guided state explicitly so command generation no longer depends on a manual health bypass.
- Validated the permission-gate update using `.\scripts\test-core.ps1`: build passed and 11/11 CTest tests passed in Debug.
- Hardened replay route matching diagnostics by adding `mavlink_ok` and `navigation_ok` to per-frame match metrics.
- Added pipeline harness coverage proving disarmed and wrong-mode dry-run telemetry keep MAVLink healthy but block navigation commands through `navigation_ok=false`.
- Validated the hardening pass using `.\scripts\test-core.ps1`: build passed and 11/11 CTest tests passed in Debug.
- Started Milestone 6 with a fail-closed `PiCameraSource` boundary that implements `CameraSource`, validates capture dimensions/rate/format, and refuses to start when no libcamera backend is compiled in.
- Added `PiCameraSource` tests for config validation, unsupported-backend startup failure, stopped state, and empty polling.
- Validated the hardware-capture boundary using `.\scripts\test-core.ps1`: build passed and 12/12 CTest tests passed in Debug.
- Added `VISUAL_HOMING_ENABLE_LIBCAMERA` CMake option with default `OFF` so desktop builds remain replay-first and Pi builds can explicitly request the hardware backend.
- Added Raspberry Pi automation docs plus `scripts/bootstrap-pi.sh` and `scripts/test-core-pi.sh` for one-command package install, Release configuration, build, and CTest on Pi.
- Validated the desktop/default path using `.\scripts\test-core.ps1`: build passed and 12/12 CTest tests passed in Debug.
- Added `run_pi_camera_smoke` and CLI `--pi-camera-smoke <width> <height> <fps> <frames>` for future Pi camera validation while preserving fail-closed desktop behavior.
- Added optional `VISUAL_HOMING_RUN_CAMERA_SMOKE=1` support to `scripts/test-core-pi.sh`.
- Validated the camera smoke harness using `.\scripts\test-core.ps1`: build passed and 13/13 CTest tests passed in Debug.
- Tightened the Pi hardware build contract so `VISUAL_HOMING_ENABLE_LIBCAMERA=ON` requires `pkg-config` and the `libcamera` development package through CMake.
- After first Pi bootstrap reached compilation but `cc1plus` was killed by the OS under parallel Ninja, changed `scripts/test-core-pi.sh` to default to `MinSizeRel` and one build job for Pi Zero 2W memory limits.
- After the Pi `MinSizeRel` build exposed tests that depended on `assert` side effects, changed CMake test compilation to undefine `NDEBUG` for test executables so CTest keeps assertions active in release-like builds.
- Validated the Raspberry Pi build path on `jtzero`: `scripts/test-core-pi.sh --clean` found `libcamera` 0.7.1, completed the C++ build on Pi hardware, and passed 13/13 CTest tests.
- Added the first libcamera-backed `PiCameraSource` implementation behind `VISUAL_HOMING_ENABLE_LIBCAMERA`, using CameraManager, Viewfinder stream configuration, FrameBufferAllocator, request completion callbacks, luma-plane copy into Gray8 `Frame`, and request reuse.
- Updated camera smoke polling to wait by elapsed time instead of a fixed empty-poll count for asynchronous capture.
- Validated the desktop/default non-libcamera path using `.\scripts\test-core.ps1`: build passed and 13/13 CTest tests passed in Debug.
- After Pi CTest opened the physical camera and segfaulted in `camera_smoke`/`pi_camera_source` tests, added runtime `enable_live_capture=false` by default so CTest remains offline and live capture is reached only through explicit `--pi-camera-smoke`.
- Validated live Pi camera capture on `jtzero` using `VISUAL_HOMING_RUN_CAMERA_SMOKE=1 ./scripts/test-core-pi.sh --clean`: CTest passed 13/13 and `--pi-camera-smoke 320 240 15 30` captured 30 Gray8 frames from the IMX219 camera, each 320x240 / 76800 bytes.
