# Claude Code Prompt

Use this prompt when asking Claude Code to refactor or continue an existing Visual-Homing repository toward the same architecture and safety direction.

```text
You are working in an existing Visual-Homing repository. First inspect the repository structure, build files, docs, and current implementation before editing. Adapt names and file layout to the existing project instead of blindly copying paths. If there is already a `core/`, use it; otherwise create a clean `core/` area. Preserve the existing implementation as reference/prototype code unless explicitly instructed otherwise.

Primary goal:
Refactor Visual-Homing toward a replay-first, deterministic, flight-safety-oriented core for a coarse GPS-denied visual return system. Target hardware is Raspberry Pi Zero 2W class companion computer with ArduPilot/Matek H743-class flight controller. This is not long-range metric SLAM and not precision hover. It is StabX-like coarse visual return/homing: record compact visual route signatures, match current frames against route progress, estimate confidence and coarse direction error, use flight-controller attitude/altitude telemetry for interpretation, then produce bounded navigation corrections only after offline/replay validation.

Important operating target:
- The intended long-term behavior is coarse GPS-denied return over much larger distances than bench tests, with large acceptable terminal error. Think route-corridor reacquisition and coarse homing, not centimeter positioning.
- Short 1-3 m tests are only hardware/pipeline validation, not the target capability.
- Future longer-distance behavior depends on camera/FOV profiles, ArduPilot telemetry, altitude/scale-aware metadata, route segmentation, reacquire behavior, and confidence gates.
- GPS-denied visual hold is a separate future mode (`VisualHold`) from route return (`RouteReturn`). It should use camera plus flight-controller roll/pitch/yaw/altitude, but it must not be mixed into the route-return matcher accidentally.

Repository rules:
- Inspect first, then adapt.
- Preserve existing implementation as reference/prototype code.
- Build the new flight-critical implementation separately under a clean `core/` area or equivalent.
- Do not use Python/web code as the realtime scheduler.
- Prefer C++20, deterministic timing, bounded state, explicit health/failsafe behavior.
- Keep hardware and live MAVLink output behind dry-run/read-only boundaries until replay and bench tests pass.
- Every completed task must be committed and pushed.
- Update docs, roadmap, session log, project memory, and decision log after each milestone.

Architecture to implement or preserve:

1. Replay input
- Manifest format: CSV `id,timestamp_ns,path`.
- Initial image format: binary PGM P5 Gray8.
- Implement replay source behind a `CameraSource`-style interface.

2. Preprocessing
- Implement deterministic Gray8 resize preprocessing.
- Use block-average resize.
- Avoid OpenCV for the flight-critical core baseline.

3. Health/timing
- Implement health monitor.
- Track frame age, processing latency, frames seen/dropped, route match confidence, camera/mavlink/navigation health.
- Health states: Booting, Ready, Degraded, Failsafe, Shutdown.

4. Local validation
- Add `.gitignore`.
- Add build docs.
- Add one-command test script.
- Use CMake + CTest or the repo's existing equivalent.
- Add focused unit tests for each core component.

5. Pipeline harness
- Add replay -> preprocess -> health metrics loop.
- Add CLI or test harness modes for replay/pipeline smoke testing.

6. Route signature format
- Add binary route file format `VHRS` v1 or equivalent.
- Explicit little-endian fields.
- Versioned header.
- Entry metadata: frame id, timestamp ns, altitude band, heading hint rad, width, height, pixel format, payload length.
- Current recorder writes Gray8 preprocessed payloads first.
- Preserve room for Thermal16 payloads.
- Add writer/reader round-trip tests.
- Add format documentation.

7. Route recorder
- Implement route recorder.
- Convert preprocessed `Frame + NavigationEstimate` into route entries.
- Add CLI/harness path to record route file from replay.
- Add live camera route recording only as an explicit hardware validation path, not a future realtime command loop.

8. Route matching
- Implement Gray8 route matcher.
- Use normalized mean absolute byte difference over Gray8 route entries.
- Support optional route window radius and minimum confidence.
- Return route index, progress, confidence, valid.
- Add coarse `direction_error_rad` by bounded horizontal pixel-shift search.
- Make `max_direction_shift_px` and `radians_per_pixel` configurable initially.
- Later replace ad hoc radians-per-pixel with camera-profile FOV-derived values.
- Add synthetic perturbation tests: brightness offsets, small noise, left/right shift, aligned frame.

9. Navigation command model
- Implement bounded navigator.
- Input: `RouteMatch + HealthSnapshot`.
- Output: `NavigationCommand`.
- Gates:
  - health must be Ready;
  - camera/mavlink/navigation links must be ok;
  - match must be valid;
  - confidence must exceed threshold;
  - match age must be within limit.
- Output invalid zero command on gate failure.
- Convert direction error to yaw-rate using gain.
- Clamp yaw-rate.
- Slew-limit yaw-rate across successive valid commands.
- Add tests for clamping, negative correction, low confidence, stale match, degraded health, slew limiting, reset after invalid state.

10. Dry-run MAVLink boundary
- Do not add live MAVLink command output first.
- Add dry-run command sink implementing the command output boundary.
- Add dry-run MAVLink bridge with scripted telemetry:
  - heartbeat;
  - armed;
  - flight mode;
  - roll/pitch/yaw;
  - relative altitude.
- It must record commands through a single-writer boundary.
- Add tests for telemetry polling, command logging, rejecting send while stopped.
- Live MAVLink command output must remain a later explicit step.

11. Read-only flight-controller telemetry before live commands
- Add real MAVLink read-only telemetry transport before any live command output.
- Read heartbeat, armed state, mode, roll, pitch, yaw, relative altitude, and telemetry freshness.
- Feed telemetry into health/navigation state:
  - heartbeat -> `mavlink_ok`;
  - mode/armed -> command permission gates;
  - attitude/altitude -> navigation estimate / route metadata path.
- Log camera frames with attitude/altitude snapshots for replay analysis.
- Still no live ArduPilot command output.

12. Camera profiles and FOV calibration
- Add explicit camera profiles.
- Profile fields should include:
  - camera id/name;
  - sensor type: visible, thermal, other;
  - pixel format;
  - capture width/height;
  - target resize width/height;
  - horizontal FOV rad/deg;
  - vertical FOV rad/deg;
  - crop/binning policy;
  - exposure or thermal normalization hints;
  - matcher thresholds;
  - route-quality thresholds;
  - derived radians-per-pixel values.
- Add docs/template for camera profile setup.
- FOV must be operator-configurable because different supported cameras have different FOV.
- Add tests for FOV-derived radians-per-pixel and profile validation.

13. Thermal camera path
- Preserve `Thermal16` in route format.
- Add thermal profile placeholder even before full thermal matching.
- Add thermal normalization policy before thermal matching:
  - deterministic normalization;
  - bounded output;
  - offline tests with synthetic thermal gradients/noise.
- Do not assume visible-light thresholds work for thermal cameras.

14. Altitude/scale-aware matching
- Use ArduPilot altitude/attitude telemetry to annotate route entries and interpret current frames.
- Add altitude bands and scale assumptions to route metadata.
- Add route-quality checks by segment and altitude band.
- Do not claim long-range capability until replay logs prove stable confidence/progress across altitude and viewpoint changes.

15. Route artifact validation
- Add offline route inspection: entry count, timestamps, dimensions, payload sizes, Gray8/Thermal status.
- Add stateless self-match artifact check.
- Add deterministic perturbation check: brightness, noise, shift, malformed-payload rejection.
- Add route distinctiveness diagnostics:
  - low-texture entries;
  - exact duplicate entries;
  - ambiguous nearest-neighbor entries;
  - payload range;
  - adjacent mean absolute byte difference;
  - nearest-neighbor mean absolute byte difference.
- Add route-quality policy:
  - low-texture fraction threshold;
  - ambiguous nearest-neighbor fraction threshold;
  - minimum average nearest mean absolute byte difference;
  - duplicate policy.
- Route-quality policy is an offline artifact-quality gate for bench/field route capture, not live flight authorization.

16. Long-route behavior
- Plan for route segmentation/keyframes before long route tests.
- Avoid relying on full linear search for multi-kilometer routes as the final architecture.
- Add window/progress policy, confidence hysteresis, lost/reacquire state, and segment-level diagnostics.
- Target behavior is coarse route-corridor reacquisition and return, not metric SLAM.

17. Visual hold mode
- Treat GPS-denied visual hold as a separate future mode from route return.
- Possible future modes:
  - Idle/Failsafe;
  - RouteReturn;
  - VisualHold.
- VisualHold should use a local reference/anchor plus attitude/altitude telemetry and bounded `vx/vy/yaw_rate`.
- Do not mix VisualHold estimator state with route-progress matcher state.
- VisualHold must fail closed on low confidence, stale frame, attitude/altitude instability, or lost anchor.

Suggested core data shapes:
- `Frame`: id, timestamp, width, height, pixel format, byte payload.
- `NavigationEstimate`: timestamp, course error rad, ground speed mps, altitude m, confidence.
- `RouteMatch`: timestamp, route index, progress, direction error rad, confidence, valid.
- `NavigationCommand`: timestamp, vx, vy, yaw_rate_radps, confidence, valid.
- `HealthSnapshot`: state, timestamp, frames_seen, frames_dropped, frame_age_ms, processing_latency_ms, route_match_confidence, camera_ok, mavlink_ok, navigation_ok.
- `MavlinkTelemetry`: timestamp, heartbeat_seen, armed, mode, roll_rad, pitch_rad, yaw_rad, relative_altitude_m.
- `CameraProfile`: id, sensor type, capture size, target size, pixel format, horizontal_fov_rad, vertical_fov_rad, normalization/matcher/quality settings.

Current Codex baseline to match or preserve if already present:
- C++20 `core/` exists.
- Replay PGM/CSV pipeline exists.
- Gray8 block-average preprocessor exists.
- Health monitor exists.
- `VHRS` route signature v1 exists.
- Route recorder exists.
- Gray8 route matcher exists with confidence/progress/direction error.
- Bounded navigator exists with health/confidence/age gates and yaw slew limiting.
- Dry-run MAVLink bridge and telemetry adapter exist.
- Pi camera source with libcamera backend exists behind compile-time and runtime gates.
- Pi bootstrap/test scripts exist.
- Live camera route recording exists as explicit validation mode.
- Route inspection, stateless self-match, perturbation checks, distinctiveness diagnostics, and route-quality policy exist.
- Active camera profile support exists for IMX219-class visible camera validation.
- Live camera route matching exists with endpoint-progress gates, dry-run command quality checks, live read-only MAVLink telemetry health, and operator cue timing.
- Live MAVLink output remains blocked. `LiveMavlinkOutputSafetyGate`, `LiveMavlinkOutputAuditLog`, and `LiveMavlinkOutputSession` exist only as non-live safety/audit scaffolding.
- The first future live-output scope is yaw-rate only with `vx_mps=0`, bench-only, propellers removed, explicit runtime enable and operator confirmation, and audit logging ready before any command can pass.
- Pi Zero 2W class validation has passed CTest and live IMX219 route capture/matching on `jtzero`.
- Current readiness evidence is `2/3` accepted clean dry-runs in `docs/LIVE_OUTPUT_READINESS_RECORD.md`.
- Accepted readiness log 1/3: `artifacts/logs/test-core-pi-20260604T205416Z.log`, validated with `scripts/check-live-readiness-log.sh`.
- Accepted readiness log 2/3: `artifacts/logs/test-core-pi-20260605T194839Z.log`, with session audit `artifacts/logs/live-output-session-audit-20260605T194839Z.log`, validated with both `scripts/check-live-readiness-log.sh` and `scripts/check-live-session-audit-log.sh`.
- The latest session-audited Pi run captured 150/150 live matches, passed endpoint/progress gates, kept read-only telemetry healthy with zero dropped bytes, produced 150/150 valid dry-run commands, and blocked all live-output decisions with `vehicle_not_armed:150`.
- The source route for readiness 2/3 had `route_distinctiveness warning=true quality_pass=false`; this is acceptable as safety plumbing evidence only, not as route-quality approval.

Immediate next useful task:
- Add an offline route-quality/readiness checker so route recording logs can be machine-checked before spending time on another match/audit dry-run.
- The checker should parse the route recording validation output from `scripts/test-core-pi.sh`, especially `route_self_match`, `route_perturb_check`, and `route_distinctiveness`.
- It should pass only when route self-match and perturbation checks pass and route distinctiveness meets the current route-quality policy, unless an explicit diagnostic override is used.
- It must be offline, deterministic, shell-script friendly on Pi, and documented in `docs/PI_BUILDING.md` and the live-output readiness docs.
- Do not treat a route-quality checker pass as flight authorization; it is only a filter for collecting the final `3/3` pre-live readiness evidence.

Validation requirements:
- Build with CMake or the repo's existing build system.
- Add focused tests for every component.
- Keep all tests offline and deterministic by default.
- Hardware/live camera/MAVLink tests must be explicit opt-in.
- Commit after each coherent milestone.
- Update docs/roadmap/session log/project memory/decision log after each milestone.
- Push after each completed task.

Safety constraints:
- No disk I/O in future live camera/command loops.
- No unbounded queues in flight-critical paths.
- Every sample/command carries monotonic timestamp.
- MAVLink command output must remain single-writer.
- Web/UI/diagnostics must read immutable snapshots only.
- Low-confidence, stale, degraded, ambiguous, or invalid matches must fail closed.
- Live ArduPilot command output remains blocked until read-only telemetry, replay logs, bench replay, route-quality gates, and safety review pass.
- Do not change compile-time or runtime live-output blockers until `docs/LIVE_OUTPUT_SAFETY_PLAN.md` is complete, reviewed, and `docs/LIVE_OUTPUT_READINESS_RECORD.md` has `3/3` accepted clean dry-runs.
- Do not escalate from yaw-only to pitch/roll/forward velocity in the first live-output stage.

Required review before flight-test ladder:
- Perform a code review of replay/camera/route/matching/navigation/MAVLink dry-run baseline.
- Search for weak points:
  - unbounded state;
  - timing assumptions;
  - stale data handling;
  - low-confidence behavior;
  - route ambiguity;
  - malformed input handling;
  - thread-safety;
  - single-writer command output;
  - disk I/O near future live loops.
- Review safety boundaries:
  - fail-closed behavior;
  - health transitions;
  - permission gates;
  - route-quality gates;
  - live-output blockers.
- Review security and operational hardening:
  - artifact parsing robustness;
  - path handling;
  - dependency assumptions;
  - install/bootstrap scripts;
  - web/UI boundaries if reintroduced;
  - log privacy;
  - unauthenticated network surfaces.

Work method:
1. Inspect repository first.
2. Summarize current structure and likely adaptation plan.
3. Implement in small milestones.
4. Validate after every milestone.
5. Commit and push after each completed task.
6. Keep reference/prototype code separate from the new deterministic core.
7. Do not open live MAVLink command output until it is explicitly requested and the safety gates above are complete.
```
