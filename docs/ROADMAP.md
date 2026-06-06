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
- Status: validated for the current safety stage with `PiCameraSource`, initial libcamera backend, live camera pipeline smoke, live route recording with warmup frame dropping, route inspection, stateless route self-match, stateless route perturbation checks, offline route distinctiveness diagnostics, route-quality policy, explicit edge-trim scoring for start/end pauses, read-only live route matching against existing `VHRS` artifacts, and tests. Default desktop builds fail closed without live capture. Pi automation exists through `VISUAL_HOMING_ENABLE_LIBCAMERA`, `docs/PI_BUILDING.md`, `scripts/bootstrap-pi.sh`, and `scripts/test-core-pi.sh`; enabling libcamera requires CMake pkg-config discovery of `libcamera`. The Pi build path is validated on `jtzero` with libcamera 0.7.1 and 19/19 CTest tests passing. Live IMX219 pipeline smoke and active-profile route matching are validated on `jtzero`; the physical route artifact is backed up, structurally validated, self-matched, perturbation-checked, and no longer needed for the next planning step. Static table captures and sideways/yawing manual fixtures correctly expose poor route distinctiveness or unstable progress and remain useful as negative route-quality examples. Further repeatable manual validation should use a small straight-line no-yaw stand or guide rail instead of hand-pushed boxes/chairs.

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
- Status: validated for active visible-camera/read-only telemetry dry-run work with an in-core `CameraProfile` model, profile validation, FOV-derived angular scale, unit tests, strict `key=value` profile file loading, an initial tracked IMX219 profile template, profile inspection/list/get-active/set-active CLI, machine-readable JSON profile registry commands for future Pi API endpoints, profile-backed camera smoke/live route recording/live route matching commands, replay and live route matching support for profile FOV-derived direction scaling, an initial read-only MAVLink v1/v2 telemetry byte-stream inspector/capture/validation path for heartbeat/attitude/global-position payloads validated on `jtzero` at 115200 baud, live route recording support for attaching validated telemetry snapshot metadata or reading a live telemetry stream during capture, live route matching support for read-only telemetry freshness health during dry-run command generation, and a documented Android/Pi ownership model for future profile selection. The `jtzero` compact live-route dry-run validated active profile matching, read-only telemetry health, zero telemetry drops, dry-run command quality, and expected live-output blocking. Timestamp-aligned long-running MAVLink telemetry integration, an HTTP wrapper over the profile registry, thermal normalization, and richer altitude-aware metadata policy remain pending.

## Milestone 6.6 - Baseline Review, Weak-Point Audit, And Safety/Security Hardening

- Perform a code review of the completed replay/camera/route/matching/navigation/MAVLink dry-run baseline.
- Search explicitly for weak points: unbounded state, timing assumptions, stale data handling, low-confidence behavior, route ambiguity, malformed input handling, thread-safety, single-writer command output, and disk I/O near future live loops.
- Review safety boundaries: fail-closed behavior, health transitions, permission gates, route-quality gates, and live-output blockers.
- Review security and operational hardening: artifact parsing robustness, path handling, dependency assumptions, install/bootstrap scripts, web/UI boundaries if reintroduced, log privacy, and unauthenticated network surfaces.
- Update roadmap/decisions with findings, residual risks, and required fixes before Milestone 7.
- Status: substantially complete for the dry-run baseline. `BoundedNavigator` config validation now rejects non-finite values and negative yaw gain, and unit tests cover per-link health blocking, future match timestamps, invalid-match command reset, and invalid config rejection. `VHRS` route artifact parsing now rejects excessive entry counts before allocation, oversized payloads, and payload-size mismatches against dimensions plus pixel format. The read-only MAVLink telemetry stream now retains a bounded byte buffer and reports captured/retained/dropped byte metrics for longer-run memory auditing. Dry-run command sinks now retain bounded command history while preserving total sent/dropped counters. Live MAVLink command output is represented by an explicit fail-closed `LiveMavlinkBridge` stub that is unavailable by default and tested to reject sends; `VISUAL_HOMING_ENABLE_LIVE_MAVLINK_OUTPUT=ON` still fails without the separate bench props-off CMake scope, and the combined bench-scope build remains `VISUAL_HOMING_LIVE_MAVLINK_OUTPUT_AVAILABLE=0` until a writer phase changes it. Live-route active-profile CLI parsing now requires complete numeric values, and the Pi test script rejects non-`0`/`1` boolean env flags plus invalid expected-progress values. A standalone `LiveMavlinkOutputSafetyGate` now defines the pre-writer permission contract for runtime enable, operator confirmation, single writer, audit log, dry-run quality, fresh armed Guided telemetry, route-match freshness/confidence, and bounded finite commands. Residual risk: no live writer exists yet, so the next step is bench-scope implementation rather than command output.

## Milestone 6.7 - Pre-Live MAVLink Output Safety Readiness

- Document the safety plan that must be satisfied before changing the compile-time or runtime live-output blockers. Current plan: `docs/LIVE_OUTPUT_SAFETY_PLAN.md`.
- Define the first future live-output boundary as bench-only with propellers removed; no tethered, ground, or flight testing is authorized by this milestone.
- Limit the first future command scope to yaw-rate only with zero forward velocity, matching the current validated dry-run shape.
- Require at least three clean Pi dry-runs before any live-output blocker change: 150/150 valid matches, endpoint/progress gate pass, read-only telemetry health pass, zero telemetry drops, dry-run command quality pass, and expected live-output gate blocked reasons.
- Define operator checklist, explicit runtime/operator enable, audit logging, single-writer ownership, stop/kill behavior, and failure handling before implementation of a real writer.
- Status: readiness evidence complete, live output still blocked. `docs/LIVE_OUTPUT_READINESS_RECORD.md` records 3/3 accepted clean Pi dry-runs. The third accepted run used a route-quality prechecked 150-entry route, captured 150/150 valid matches, had strict monotonic forward progress, passed endpoint/progress gates, passed read-only telemetry health with zero dropped bytes, passed dry-run command quality with 150/150 valid commands, wrote a session audit artifact with 150 blocked command records, and kept future live output blocked with `vehicle_not_armed:150`. Live MAVLink output remains blocked by default, `VISUAL_HOMING_ENABLE_LIVE_MAVLINK_OUTPUT=ON` remains invalid without the separate bench props-off CMake scope, and the fail-closed `LiveMavlinkBridge` boundary remains unavailable until the reviewed bench writer phase changes that boundary.

## Milestone 6.8 - Bench Props-Off Live Output Boundary

- Define the first reviewed live-output implementation scope after 3/3 readiness evidence.
- Keep the first live-output boundary bench-only, propellers removed, physically restrained, short duration, and operator-present.
- Keep first command authority yaw-rate-only with `vx_mps=0`.
- Require explicit compile-time enable, runtime enable, operator confirmation, audit readiness, single-writer ownership, telemetry freshness, route-match freshness/confidence, command quality, and hard max command/duration limits.
- Add tests before any real writer send path is considered.
- Status: Phase 1 compile-time boundary split, Phase 2 writer-interface scaffolding, Phase 3 runtime/operator/max-limit scaffolding, Phase 4 safety-gate availability wiring, and Phase 5 audit-contract extension are implemented; the concrete serial writer is not implemented. Default builds keep live output disabled, `VISUAL_HOMING_ENABLE_LIVE_MAVLINK_OUTPUT=ON` fails unless paired with `VISUAL_HOMING_ENABLE_BENCH_PROPS_OFF_LIVE_OUTPUT=ON`, and the paired bench-scope build still keeps `VISUAL_HOMING_LIVE_MAVLINK_OUTPUT_AVAILABLE=0`. `LiveMavlinkBridge` now has an injectable `LiveMavlinkCommandWriter` interface tested with fake writers for stopped-send rejection, deterministic double start/stop behavior, and no send when the safety gate blocks. `LiveMavlinkOutputSession` now enforces max command count and max duration before writer send, and the Pi wrapper can pass explicit runtime enable plus exact props-off confirmation into the live-route audit path. `LiveMavlinkOutputSafetyGate` now blocks `live_output_unavailable` before writer send when runtime-controlled live output is requested while the build remains unavailable. Session audit command records now include explicit decision, telemetry, and route-match context, and the audit checker can distinguish expected allowed writer decisions from blocked dry-run decisions. Live MAVLink output remains blocked.

## Milestone 7 - Flight Test Ladder

- Bench replay.
- Tethered/ground test.
- 50 m return.
- 100-300 m return.
- 1 km+ only after replay logs show stable behavior.
- Start by integrating the validated live matcher with the existing bounded navigation and command-sink safety gates; keep live MAVLink output blocked until dry-run command behavior is reviewed, logged, and explicitly enabled through a separate safety step.
- Status: deferred until the first bench props-off live-output boundary is separately implemented and reviewed. Live route matching can now feed `BoundedNavigator` and `DryRunCommandSink` metrics under `VISUAL_HOMING_LIVE_ROUTE_DRY_RUN_COMMANDS=1`, optionally gate the run on dry-run command quality, optionally use read-only live MAVLink telemetry freshness as the dry-run health input, and still keep live MAVLink command output blocked. Milestone 6.7 now has 3/3 validated dry-run evidence, which is sufficient to design the bench props-off live-output boundary, not to start the flight ladder.
