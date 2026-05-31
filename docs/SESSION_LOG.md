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
- Extended camera smoke into a live camera pipeline smoke: `PiCameraSource -> Gray8ResizePreprocessor -> HealthMonitor -> metrics`, reporting processed size, frame age, processing latency, empty polls, elapsed time, and effective FPS.
- Validated the default desktop path using `.\scripts\test-core.ps1`: build passed and 13/13 CTest tests passed in Debug.
- Validated live camera pipeline smoke on `jtzero` using `VISUAL_HOMING_RUN_CAMERA_SMOKE=1 ./scripts/test-core-pi.sh --clean`: CTest passed 13/13, 30 live IMX219 frames were captured at 320x240, preprocessed to 32x24 / 768 bytes, last latency was ~0.87 ms, frame age stayed around 0.6-2.0 ms, and effective FPS was ~15.9.

## 2026-05-31

- Added live camera route recording through `record_live_camera_route` and CLI `--record-live-route`, using `PiCameraSource -> Gray8ResizePreprocessor -> HealthMonitor -> RouteSignatureRecorder -> VHRS`.
- Added `VISUAL_HOMING_RECORD_LIVE_ROUTE=1` support to `scripts/test-core-pi.sh` so Pi validation can build, run CTest, and optionally write a live route file.
- Validated the default desktop path using `.\scripts\test-core.ps1`: build passed and 13/13 CTest tests passed in Debug.
- Validated live route recording on `jtzero` using `VISUAL_HOMING_RECORD_LIVE_ROUTE=1 ./scripts/test-core-pi.sh --clean`: CTest passed 13/13, 120 live IMX219 frames were preprocessed to 32x24 / 768 bytes, 120 `VHRS` entries were written to `/tmp/visual_homing_live_route.vhrs`, last latency was ~1.48 ms, frame age stayed mostly around 0.5-2.6 ms with a late 4.19 ms sample, and effective FPS was ~15.19.
- Added offline route inspection through `summarize_route_signature`, `inspect_route_signature_file`, and CLI `--inspect-route <route.vhrs>`, reporting entry count, frame/timestamp ranges, dimensions, payload totals, monotonic timestamps, uniform dimensions/payloads, and Gray8-only status.
- Updated `scripts/test-core-pi.sh` so live route recording automatically inspects the written route and `VISUAL_HOMING_INSPECT_ROUTE=1` can inspect an existing route artifact.
- Tightened `scripts/test-core.ps1` so configure, build, and CTest non-zero exit codes fail the helper instead of allowing stale test binaries to mask a failed build.
- Validated the default desktop path using `.\scripts\test-core.ps1`: build passed and 13/13 CTest tests passed in Debug.
- After `jtzero` inspection showed `/tmp/visual_homing_live_route.vhrs` was no longer present, changed the default Pi route artifact path to ignored persistent `artifacts/visual_homing_live_route.vhrs` and made the Pi script create the parent directory before recording or inspection.
- Validated persistent live route artifact recording and inspection on `jtzero`: `VISUAL_HOMING_RECORD_LIVE_ROUTE=1 ./scripts/test-core-pi.sh` passed 13/13 CTest tests, wrote `artifacts/visual_homing_live_route.vhrs`, recorded 120 32x24 Gray8 entries at effective FPS ~15.23, and auto-inspection reported 120 entries, 92160 total payload bytes, monotonic timestamps, uniform dimensions, uniform payload size, and all Gray8. A follow-up `VISUAL_HOMING_INSPECT_ROUTE=1 ./scripts/test-core-pi.sh` also passed 13/13 CTest tests and inspected the same persistent artifact successfully.
- Added offline route self-match through `self_match_route_signature`, `self_match_route_signature_file`, and CLI `--self-match-route <route.vhrs> [minimum_confidence]`; the Pi script now self-matches a route after live recording and supports `VISUAL_HOMING_SELF_MATCH_ROUTE=1` for existing artifacts.
- Validated the default desktop path using `.\scripts\test-core.ps1`: build passed and 14/14 CTest tests passed in Debug. Also validated direct `--self-match-route` on the route artifact check test file with 3/3 exact matches and confidence 1.0.
- Validated route self-match on `jtzero`: `VISUAL_HOMING_SELF_MATCH_ROUTE=1 ./scripts/test-core-pi.sh` passed 14/14 CTest tests and matched the persistent live route artifact with 120/120 valid matches, 120/120 exact index matches, confidence 1.0, monotonic progress, and `passed=true`. A follow-up `VISUAL_HOMING_RECORD_LIVE_ROUTE=1 ./scripts/test-core-pi.sh` passed 14/14 CTest tests, recorded a fresh 120-entry live route, inspected it, and self-matched it with the same pass metrics.
- Added deterministic route perturbation checks through `perturbation_check_route_signature`, `perturbation_check_route_signature_file`, and CLI `--perturb-route <route.vhrs> [minimum_confidence]`, covering brightness offset, small deterministic byte noise, horizontal shift, and malformed-payload rejection. The Pi script now runs perturbation checks after live recording and supports `VISUAL_HOMING_PERTURB_ROUTE=1` for existing artifacts.
- Validated the default desktop path using `.\scripts\test-core.ps1`: build passed and 14/14 CTest tests passed in Debug. Also validated direct `--perturb-route` on the perturbation test artifact with brightness/noise/shift valid matches, malformed rejection, and `passed=true`.
- After `jtzero` exposed low-texture live artifacts that could lock a stateful self-match/perturbation window onto an earlier ambiguous entry, changed route artifact checks to use stateless full-route matching and treat exact index/progress monotonicity as diagnostics rather than pass gates.
- Validated the stateless artifact checks on `jtzero`: `VISUAL_HOMING_PERTURB_ROUTE=1 ./scripts/test-core-pi.sh` passed 14/14 CTest tests and reported 120/120 brightness, noise, shift, and direction matches with malformed-payload rejection. A follow-up `VISUAL_HOMING_RECORD_LIVE_ROUTE=1 ./scripts/test-core-pi.sh` passed 14/14 CTest tests, recorded a fresh 120-entry live route at effective FPS ~15.22, inspection passed, self-match reported 120/120 valid matches with 118/120 exact index diagnostics and confidence 1.0, and perturbation checks passed with 120/120 valid matches in all perturbation cases.
- Added offline route distinctiveness diagnostics through `analyze_route_distinctiveness`, file/CLI `--route-distinctiveness <route.vhrs>`, and `VISUAL_HOMING_ROUTE_DISTINCTIVENESS=1` Pi script support. The diagnostic reports low-texture entries, exact duplicate entries, ambiguous nearest-neighbor entries, payload range, adjacent mean absolute byte difference, nearest-neighbor mean absolute byte difference, and a warning flag without becoming a flight permission gate.
- Validated the default desktop path using `.\scripts\test-core.ps1`: build passed and 14/14 CTest tests passed in Debug. Also validated direct `--route-distinctiveness` on the route artifact check test file with non-warning distinctiveness metrics.
- Validated route distinctiveness diagnostics on `jtzero`: `VISUAL_HOMING_ROUTE_DISTINCTIVENESS=1 ./scripts/test-core-pi.sh` passed 14/14 CTest tests and reported the existing live artifact as warning-worthy with 3 low-texture entries, 3 exact duplicate entries, 112/120 ambiguous nearest entries, minimum adjacent mean absolute byte difference 0, and average nearest mean absolute byte difference ~1.17. A fresh `VISUAL_HOMING_RECORD_LIVE_ROUTE=1 ./scripts/test-core-pi.sh` run passed 14/14 CTest tests, recorded 120 entries at effective FPS ~15.23, passed inspection, self-match, and perturbation checks, then reported `warning=true` with 3 low-texture entries, no exact duplicate entries, 119/120 ambiguous nearest entries, minimum nearest mean absolute byte difference ~0.027, and average nearest mean absolute byte difference ~0.439.
- Validated a more realistic hand-carried bench route on `jtzero` using `VISUAL_HOMING_CAMERA_FRAMES=180 VISUAL_HOMING_RECORD_LIVE_ROUTE=1 ./scripts/test-core-pi.sh`: CTest passed 14/14, 180 entries were recorded at effective FPS ~15.42, inspection passed, self-match reported 180/180 exact valid matches with confidence 1.0, perturbation checks passed with 180/180 brightness/noise/shift valid matches, and distinctiveness improved substantially to 8/180 ambiguous nearest entries, no exact duplicates, average adjacent mean absolute byte difference ~9.10, and average nearest mean absolute byte difference ~7.44. The diagnostic still returned `warning=true` because a few low-texture/ambiguous samples remain, but the route is qualitatively much better than the static table capture.
- Added an initial route-quality policy to distinctiveness diagnostics: low-texture fraction `<= 0.05`, ambiguous nearest-neighbor fraction `<= 0.10`, average nearest mean absolute byte difference `>= 5.0`, and no exact duplicate entries. The output now includes fractions plus `quality_pass`; this is an offline artifact-quality gate for bench/field route capture, not live flight authorization.
- Validated the route-quality policy on `jtzero`: `VISUAL_HOMING_ROUTE_DISTINCTIVENESS=1 ./scripts/test-core-pi.sh` passed 14/14 CTest tests and the persistent 180-entry hand-carried artifact reported low-texture fraction ~0.0167, ambiguous nearest fraction ~0.0444, average nearest mean absolute byte difference ~7.44, no exact duplicate entries, `warning=true`, and `quality_pass=true`.
