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
- Status: route signature v1 writer/reader, route inspection summary, route self-match artifact check, route perturbation artifact check, `RouteSignatureRecorder`, and replay-to-route CLI recording path are implemented and validated with round-trip, summary, self-match, and perturbation tests.

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
- Status: in progress with `PiCameraSource`, initial libcamera backend, live camera pipeline smoke, live route recording with warmup frame dropping, route inspection, stateless route self-match, stateless route perturbation checks, offline route distinctiveness diagnostics, route-quality policy, explicit edge-trim scoring for start/end pauses, and tests. Default desktop builds fail closed without live capture. Pi automation exists through `VISUAL_HOMING_ENABLE_LIBCAMERA`, `docs/PI_BUILDING.md`, `scripts/bootstrap-pi.sh`, and `scripts/test-core-pi.sh`; enabling libcamera requires CMake pkg-config discovery of `libcamera`. The Pi build path is validated on `jtzero` with libcamera 0.7.1 and 14/14 CTest tests passing. Live IMX219 pipeline smoke is validated on `jtzero`: 320x240 Gray8 frames preprocess to 32x24 in about 0.9 ms with effective FPS about 15.9. Live route recording, inspection, self-match, perturbation checks, distinctiveness diagnostics, and route-quality policy are validated on `jtzero`: a 180-frame hand-carried bench route recorded at effective FPS ~15.42, inspection passed, self-match reported 180/180 exact valid matches with confidence 1.0, perturbation checks passed, distinctiveness improved to 8/180 ambiguous nearest entries with average nearest mean absolute byte difference ~7.44, and `quality_pass=true`. A 240-frame profile-backed route showed only startup frames `0,1,2` as low-texture/duplicate/ambiguous, motivating explicit trim diagnostics and default 3-frame route-recording warmup. Static table captures correctly report poor distinctiveness and remain useful as negative route-quality examples.

## Milestone 6.5 - Camera Profiles And Read-Only Flight Telemetry

- Add explicit camera profiles instead of hard-coded camera assumptions.
- Capture profile fields: camera id/name, sensor type, pixel format, capture size, target resize, horizontal/vertical FOV, crop policy, exposure/normalization hints, matcher thresholds, route-quality thresholds, and derived radians-per-pixel values.
- Add FOV-aware direction-error conversion using camera profile data instead of ad hoc `radians_per_pixel`.
- Add profile documentation/template so different visible-light and thermal cameras can be configured repeatably.
- Add Pi-owned camera profile files plus an API contract for companion UI selection; Android may use the reference Kotlin/Compose codebase, but the active profile must be stored and validated on the Pi/core side.
- Add thermal profile placeholder and thermal normalization policy; the `VHRS` format already reserves `Thermal16`, but matching/preprocessing still needs an implementation.
- Add read-only real MAVLink telemetry transport before any live command output: heartbeat, armed, mode, roll, pitch, yaw, relative altitude, and freshness metrics.
- Log camera frames together with flight-controller attitude/altitude snapshots for replay analysis.
- Add altitude-aware route metadata policy: altitude bands, heading hints, attitude snapshot, and scale/height assumptions.
- Keep live MAVLink command output blocked; this milestone is read-only telemetry plus calibration.
- Status: started with an in-core `CameraProfile` model, profile validation, FOV-derived angular scale, unit tests, strict `key=value` profile file loading, an initial tracked IMX219 profile template, profile inspection/list/get-active/set-active CLI, machine-readable JSON profile registry commands for future Pi API endpoints, profile-backed camera smoke/live route recording commands, replay route matching support for inline/file profile FOV-derived direction scaling, an initial read-only MAVLink v1/v2 telemetry byte-stream inspector/capture path for heartbeat/attitude/global-position payloads, and a documented Android/Pi ownership model for future profile selection. Continuous live MAVLink telemetry integration, an HTTP wrapper over the profile registry, thermal normalization, and altitude-aware metadata remain pending.

## Milestone 6.6 - Baseline Review, Weak-Point Audit, And Safety/Security Hardening

- Perform a code review of the completed replay/camera/route/matching/navigation/MAVLink dry-run baseline.
- Search explicitly for weak points: unbounded state, timing assumptions, stale data handling, low-confidence behavior, route ambiguity, malformed input handling, thread-safety, single-writer command output, and disk I/O near future live loops.
- Review safety boundaries: fail-closed behavior, health transitions, permission gates, route-quality gates, and live-output blockers.
- Review security and operational hardening: artifact parsing robustness, path handling, dependency assumptions, install/bootstrap scripts, web/UI boundaries if reintroduced, log privacy, and unauthenticated network surfaces.
- Update roadmap/decisions with findings, residual risks, and required fixes before Milestone 7.
- Status: planned before flight-test ladder work.

## Milestone 7 - Flight Test Ladder

- Bench replay.
- Tethered/ground test.
- 50 m return.
- 100-300 m return.
- 1 km+ only after replay logs show stable behavior.
