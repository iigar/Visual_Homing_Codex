# Roadmap

## Milestone 0 - Repository Baseline

- Import legacy project under `reference/`.
- Create clean C++ core skeleton.
- Document architecture direction.

## Milestone 1 - Replay Pipeline

- Define replay input format. Initial CSV manifest: `id,timestamp_ns,path`.
- Load image/video sequences with timestamps. Initial implementation supports PGM P5 Gray8 image sequences.
- Run preprocessing on small frames. Initial implementation supports deterministic Gray8 block-average resizing.
- Emit per-frame timing and health metrics. Initial `HealthMonitor` tracks frame age, processing latency, counters, confidence, and link health.
- Status: initial implementation complete and validated on Windows with MSVC/CMake; 3/3 CTest tests pass in Debug.

## Milestone 1.5 - Local Validation And Pipeline Harness

- Add `.gitignore` entries for generated build artifacts, especially `core/build/`.
- Add `docs/BUILDING.md` with the validated Windows MSVC/CMake setup and test commands.
- Add `scripts/test-core.ps1` to configure, build, and run CTest from one command.
- Add a minimal end-to-end replay loop: `ReplayFrameSource -> Gray8ResizePreprocessor -> HealthMonitor -> metrics output`.
- Keep the harness replay-first and dependency-light; it should not add camera, MAVLink, or route matching behavior yet.
- Status: complete and validated with `.\scripts\test-core.ps1 -Clean`; 4/4 CTest tests pass in Debug.

## Milestone 2 - Visual Route Signature

- Define route file format v1 with magic, version, endian policy, header size, entry count or streaming mode, metadata fields, and payload length. Initial implementation uses binary `VHRS` v1 with explicit little-endian fields.
- Store compact grayscale/thermal signatures.
- Add route metadata: frame id, time, altitude band, coarse heading hint. Initial recorder maps Gray8 preprocessed frames and navigation estimates into route signature entries.
- Keep format versioned and stream-friendly.
- Add writer/reader tests that round-trip metadata and payload bytes before matching logic begins.
- Status: route signature v1 writer/reader, route inspection summary, `RouteSignatureRecorder`, and replay-to-route CLI recording path are implemented and validated with round-trip and summary tests.

## Milestone 3 - Coarse Route Matching

- Match current frame against a route window. Initial `Gray8RouteMatcher` uses normalized mean absolute byte distance over route signature entries and is exposed through replay-to-route matching.
- Estimate route progress, confidence, and direction error. Initial implementation estimates progress/confidence and uses horizontal pixel-shift search for coarse direction error.
- Add offline tests with synthetic route perturbations. Initial coverage includes brightness offsets and small payload perturbations.
- Status: complete as a deterministic baseline; replay-to-route matching reports route index, progress, confidence, validity, and coarse direction error.

## Milestone 4 - Navigation Command Model

- Convert route match into bounded course correction. Initial `BoundedNavigator` converts direction error into bounded yaw-rate commands.
- Add command age, confidence gates, acceleration/yaw-rate limits. Initial implementation gates by match age/confidence, clamps yaw-rate, and slew-limits yaw-rate changes between valid commands.
- Define failsafe behavior for stale or low-confidence matches. Initial implementation returns invalid zero commands when health, age, validity, or confidence gates fail.
- Integrate command generation with replay matching. Initial `--match-route` metrics include command validity and yaw-rate output, with relaxed offline age gating for replay timestamps.
- Status: complete as a deterministic baseline; navigation commands are bounded, confidence/age/health gated, slew-limited, dry-run emitted, and validated in unit and replay harness tests.

## Milestone 5 - MAVLink Integration

- Read ArduPilot heartbeat, attitude, altitude, and mode. Initial dry-run telemetry source models heartbeat, armed state, mode, attitude, and relative altitude without live MAVLink.
- Send commands from a single writer. Initial dry-run bridge records command output through the same `MavlinkBridge` boundary.
- Add dry-run and guided-command modes. Dry-run mode is implemented first; live guided-command output remains blocked.
- Start from `DryRunCommandSink`; do not add live ArduPilot output until dry-run command logs and safety gates are validated.
- Status: dry-run bridge and telemetry adapter are implemented; replay route matching now uses scripted heartbeat telemetry to drive MAVLink health, maps yaw/altitude into navigation estimates, reports `mavlink_ok`/`navigation_ok` in match metrics, and blocks commands unless telemetry is fresh, heartbeat is present, vehicle is armed, and mode is `Guided`. Live ArduPilot output remains blocked.

## Milestone 6 - Hardware Capture

- Add Pi camera source.
- Add USB/thermal source.
- Validate CPU, memory, frame drops, latency on Pi Zero 2W.
- Status: in progress with `PiCameraSource`, initial libcamera backend, live camera pipeline smoke, live route recording, route inspection, and tests. Default desktop builds fail closed without live capture. Pi automation exists through `VISUAL_HOMING_ENABLE_LIBCAMERA`, `docs/PI_BUILDING.md`, `scripts/bootstrap-pi.sh`, and `scripts/test-core-pi.sh`; enabling libcamera requires CMake pkg-config discovery of `libcamera`. The Pi build path is validated on `jtzero` with libcamera 0.7.1 and 13/13 CTest tests passing. Live IMX219 pipeline smoke is validated on `jtzero`: 320x240 Gray8 frames preprocess to 32x24 in about 0.9 ms with effective FPS about 15.9. Live route recording is validated on `jtzero`: 120 live IMX219 frames produced 120 `VHRS` entries at effective FPS about 15.2. Route inspection is implemented and awaits Pi validation against the live artifact.

## Milestone 7 - Flight Test Ladder

- Bench replay.
- Tethered/ground test.
- 50 m return.
- 100-300 m return.
- 1 km+ only after replay logs show stable behavior.
