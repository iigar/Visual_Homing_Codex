# Claude Code Prompt New

Use this prompt when asking Claude Code to create a new Visual-Homing-style project from scratch, or to rebuild the architecture in a fresh repository and bring it to the same working safety-readiness state.

```text
You are Claude Code working on a new flight-safety-oriented visual homing project.

Build a replay-first, deterministic, testable C++ core for a coarse GPS-denied visual return system. The target hardware is a Raspberry Pi Zero 2W class companion computer with a Pi Camera or similar small camera, connected to an ArduPilot/Matek H743-class flight controller. The system goal is StabX-like coarse visual return/homing, not metric SLAM, not precision hover, and not autonomous flight authorization.

This is not a direct port of a Python visual odometry system that feeds `VISION_POSITION_ESTIMATE` into ArduPilot EKF as an external navigation source. This project is a new conservative visual-route-following command-assist architecture: it estimates coarse route progress and direction error, then proposes bounded yaw-rate-only commands through explicit gates. Keep the EKF-position-estimate paradigm and the direct command-assist paradigm documented as different designs so later reviews do not compare them as if they were the same system.

Primary outcome:
- Build the project from an empty or rough repository to a working state where:
  - replay tests pass on desktop;
  - Pi camera route recording works;
  - route artifacts can be inspected and quality-checked;
  - live camera matching against a recorded route works;
  - read-only MAVLink telemetry health is consumed;
  - bounded dry-run navigation commands are generated;
  - a non-live writer-shaped session audits every command decision;
  - a bench-only serial writer library can be reviewed and unit-tested without being attached by default;
  - default live MAVLink command output remains blocked and unavailable;
  - at least three clean Pi dry-run readiness logs and fail-closed bench props-off logs are collected and validated before any attach-build step is considered.

Hard safety rule:
Do not implement live MAVLink command output as an early step. Do not enable flight, tethered flight, ground movement, pitch/roll/forward velocity, altitude authority, or autonomous return during this buildout. The first future live-output boundary, after readiness evidence and review, is bench-only, propellers removed, physically restrained, yaw-rate-only, `vx_mps=0`, `vy_mps=0`, audit log ready, explicit compile-time scope, explicit runtime enable, hard command/duration limits, and exact operator confirmation. Default builds and ordinary wrappers must stay fail-closed even after a writer library exists.

Implementation principles:
- Prefer C++20 for the flight-critical core.
- Use CMake + CTest or an equivalent simple build/test path.
- Keep tests offline and deterministic by default.
- Keep live camera, serial telemetry, and hardware paths opt-in.
- Avoid OpenCV or heavy dependencies in the core baseline unless there is a clear reason.
- Avoid unbounded queues/state in future live paths.
- Every frame, telemetry sample, match, and command must carry timestamps.
- Low confidence, stale data, invalid input, bad telemetry, command out of bounds, or audit failure must fail closed.
- A stale CMake cache must not be able to silently enable live output; ordinary scripts should explicitly pass live-output CMake options as OFF.
- Keep UI/web/Python tooling out of the realtime scheduler.

Session and context strategy:
- This is too large for one uninterrupted Claude Code context. Build in separate coherent sessions by milestone or small milestone group.
- Create and maintain `docs/PROJECT_MEMORY.md`, `docs/SESSION_LOG.md`, `docs/DECISIONS.md`, and `docs/ROADMAP.md` from the start.
- At the start of each new session, read those files plus `git log -5` before editing.
- Every session should leave the repo in a tested, documented, committed state, or explicitly document what remains incomplete.

Repository structure:
- `core/` for C++ flight-critical code.
- `scripts/` for repeatable build/test/checker commands.
- `docs/` for architecture, roadmap, Pi build instructions, safety plan, readiness record, decisions, and session log.
- `reference/` may contain imported prototype/reference code, but do not couple the core scheduler to it.
- `artifacts/` for ignored generated routes, logs, keyframes, telemetry captures, and local active-profile state.

Milestone 1: replay input
- Define `Frame`: id, timestamp_ns, width, height, pixel format, payload bytes.
- Implement replay input from CSV manifest: `id,timestamp_ns,path`.
- Support binary PGM P5 Gray8 image files first.
- Add tests for manifest parsing, monotonic timestamps, payload loading, and missing/malformed input.

Milestone 2: preprocessing and health
- Implement deterministic Gray8 resize preprocessing, preferably block-average.
- Implement `HealthSnapshot` and health monitor:
  - frames seen/dropped;
  - frame age;
  - processing latency;
  - camera_ok, mavlink_ok, navigation_ok;
  - route match confidence;
  - states such as Booting, Ready, Degraded, Failsafe, Shutdown.
- Add a replay -> preprocess -> health pipeline harness and tests.

Milestone 3: route signature artifact
- Define a binary route format, preferably `VHRS` v1:
  - magic;
  - version;
  - explicit little-endian fields;
  - entry count;
  - entry metadata: frame id, timestamp ns, altitude band, heading hint rad, width, height, pixel format, payload length;
  - payload bytes.
- Support Gray8 payloads first.
- Reserve room for Thermal16 or future sensor payloads.
- Add writer/reader round-trip tests.
- Add route inspection CLI/reporting: entry count, dimensions, payload sizes, timestamp monotonicity, pixel format, altitude/heading ranges.

Milestone 4: route recording
- Implement `RouteSignatureRecorder`.
- Convert preprocessed frames plus navigation/telemetry estimates into route entries.
- Add CLI/script paths to record a route from replay and later from live Pi camera.
- Live route recording must be explicit hardware validation mode, not a realtime command loop.

Milestone 5: route matching
- Implement `Gray8RouteMatcher`:
  - normalized mean absolute byte difference over route entries;
  - optional window radius;
  - minimum confidence;
  - output route index, progress, confidence, valid.
- Add coarse direction error:
  - bounded horizontal pixel shift search;
  - configurable `max_direction_shift_px`;
  - convert shift to radians with camera-profile-derived radians-per-pixel when profiles exist.
- Add tests for aligned matches, brightness changes, deterministic noise, left/right shifts, malformed payloads, low-confidence rejection, and window behavior.
- Treat normalized MAD as the first deterministic baseline, not the final perception algorithm.
- Add illumination robustness notes and diagnostics:
  - per-frame brightness/range statistics;
  - optional mean/contrast normalization before matching;
  - route-quality warnings for low texture and ambiguous nearest neighbors;
  - future fallback candidates such as gradient/census-like descriptors, feature/keypoint diagnostics, or multi-scale matching if brightness tests expose false drops.
- Keep fallback matchers behind tests and explicit config; do not add heavy dependencies just to hide weak route evidence.

Milestone 6: route artifact validation and quality
- Add stateless route self-match:
  - checked entries;
  - valid matches;
  - exact index matches;
  - confidence summary;
  - progress monotonic diagnostics.
- Add deterministic perturbation checks:
  - brightness;
  - byte noise;
  - horizontal shift;
  - malformed-payload rejection.
- Add route distinctiveness diagnostics:
  - low-texture entries;
  - exact duplicate entries;
  - ambiguous nearest-neighbor entries;
  - payload range;
  - adjacent mean absolute byte difference;
  - nearest-neighbor mean absolute byte difference;
  - sample frame ids/times for weak regions;
  - optional edge trim for start/end pauses.
- Add route-quality policy:
  - low texture fraction <= 0.05;
  - ambiguous nearest fraction <= 0.10;
  - average nearest mean absolute byte difference >= 5.0;
  - exact duplicate entries rejected.
- Add a shell checker similar to `check-route-quality-log.sh` that reads Pi logs and requires self-match pass, perturbation pass, malformed rejection, zero exact duplicates, and `quality_pass=true`.
- Make it clear in docs: route quality is a prefilter for readiness evidence, not flight authorization.

Milestone 7: navigation command model
- Define:
  - `RouteMatch`: timestamp, route index, progress, direction_error_rad, confidence, valid.
  - `NavigationCommand`: timestamp, vx_mps, vy_mps, yaw_rate_radps, confidence, valid.
- Implement `BoundedNavigator`:
  - gates health Ready;
  - gates camera_ok, mavlink_ok, navigation_ok;
  - gates valid match;
  - gates minimum confidence;
  - gates maximum match age;
  - outputs invalid zero command on failure.
- First command scope is yaw-rate only:
  - `vx_mps=0`;
  - `vy_mps=0`;
  - yaw rate from direction error * gain;
  - clamp max yaw rate;
  - slew-limit yaw rate.
- Add tests for low confidence, stale match, invalid match, degraded health, non-finite values, clamp, slew limit, reset after invalid state, and zero-forward-speed policy.

Milestone 8: read-only MAVLink telemetry
- Add read-only MAVLink serial capture/streaming before any live command output:
  - heartbeat;
  - armed state;
  - mode label;
  - attitude;
  - global position / relative altitude;
  - freshness;
  - byte/frame counters;
  - malformed frame diagnostics.
- Feed telemetry freshness into health:
  - heartbeat + freshness -> mavlink_ok;
  - attitude/altitude -> navigation estimate/route metadata.
- Do not send any MAVLink commands in this milestone.
- Add inspector/capture/validation scripts for Pi serial device, e.g. `/dev/serial0` at `115200`.

Milestone 9: dry-run MAVLink boundary
- Implement `DryRunCommandSink`.
- Implement dry-run MAVLink bridge:
  - scripted heartbeat and read-only telemetry snapshots;
  - armed state;
  - mode;
  - roll/pitch/yaw;
  - relative altitude;
  - command history with bounded retention and total counters.
- Feed read-only telemetry freshness into dry-run command validity; do not generate valid command proposals when FC state is stale or incompatible.
- Live MAVLink command output remains unavailable and fail-closed.
- Add tests for send while stopped, bounded command history, telemetry polling, stale telemetry blocking, and single-writer semantics.

Milestone 10: camera profiles
- Add explicit camera profile model and file format:
  - id;
  - sensor type: visible, thermal, other;
  - capture width/height;
  - target width/height;
  - pixel format;
  - horizontal_fov_rad;
  - vertical_fov_rad;
  - matcher thresholds;
  - route-quality thresholds;
  - normalization hints.
- Add profile validation, list/get/set active profile commands, and JSON output for future UI/API.
- Use profile FOV to derive radians-per-pixel for matcher direction error.
- Add FOV/altitude-derived ground footprint helpers:
  - input: validated camera profile plus positive altitude/range;
  - output: approximate ground width/height and meters-per-pixel for capture and target frames;
  - reject non-finite and non-positive values.
- Treat barometer/rangefinder altitude and image-scale drift as diagnostics first. Do not let visual scale or barometer scale affect live commands without dry-run evidence and a separate safety decision.
- Add an initial IMX219 visible camera profile, but document that FOV must be measured for the real lens/crop.

Milestone 11: Pi hardware capture
- Add Pi camera backend behind compile-time and runtime gates.
- Desktop builds should fail closed without live capture.
- Pi builds can enable libcamera.
- Define a Pi build strategy before hardware validation:
  - native Pi builds are acceptable for small CTest runs but may be slow on Pi Zero 2W;
  - keep incremental build directories stable and ignored;
  - document an optional cross-compile/sysroot path for longer builds;
  - never make cross-compilation a prerequisite for the first hardware smoke test unless native builds fail.
- Add scripts:
  - bootstrap Pi dependencies;
  - configure/build/test on Pi;
  - run camera smoke;
  - record live route;
  - inspect/validate route;
  - export keyframes;
  - match live route.
- Live route recording/matching must print operator cues and countdowns so the human knows when to move.
- Run CTest before hardware modes in the Pi script.

Milestone 12: live route matching dry-run
- Match live camera frames against existing route.
- Support:
  - expected progress: any/forward/reverse;
  - endpoint progress gate;
  - route-window radius;
  - minimum confidence;
  - dry-run command generation;
  - dry-run command quality gate;
  - read-only live MAVLink telemetry health gate.
- Final compact log must include:
  - passed;
  - requested frame count and effective FPS, for example `frames=150/150` at about 15 FPS means about 10 seconds of live capture;
  - configured capture FPS and elapsed milliseconds;
  - valid_matches=150;
  - progress range;
  - endpoint_passed;
  - progress_gate_passed;
  - confidence_min_avg;
  - telemetry_health;
  - telemetry_dropped;
  - dry_run_quality;
  - dry_run_valid=150/150;
  - live_output_gate_allowed=0;
  - live_output_gate_blocked=150;
  - live_output_gate_block_reasons.

Milestone 13: non-live live-output safety scaffolding
- Add `LiveMavlinkOutputSafetyGate`:
  - runtime enable required;
  - operator confirmation required;
  - single-writer ownership required;
  - audit logging enabled and ready;
  - dry-run quality passed;
  - fresh telemetry;
  - route match valid/fresh/high-confidence;
  - finite bounded command;
  - exact zero forward speed for first scope;
  - block reasons must be explicit.
- Add `LiveMavlinkOutputAuditLog`:
  - start record;
  - command decision records;
  - stop record;
  - fail closed if not ready.
- Add `LiveMavlinkOutputSession`:
  - writer-shaped coordinator;
  - starts audit;
  - evaluates gate;
  - uses dry-run bridge or blocked live bridge;
  - audits allowed and blocked decisions.
- Add fail-closed `LiveMavlinkBridge` stub:
  - compiled-out/disabled by default;
  - rejects starts/sends;
  - CMake live-output options must require an explicit bench props-off scope and remain unavailable unless a later attach flag is also set.
- Add tests for every safety gate reason, audit readiness, stopped session, blocked decisions, allowed dry-run decisions, and unavailable live bridge.

Milestone 14: readiness checkers and evidence
- Add shell checkers:
  - `check-route-quality-log.sh`;
  - `check-live-readiness-log.sh`;
  - `check-live-session-audit-log.sh`.
- `check-live-readiness-log.sh` should require:
  - `passed=true`;
  - `frames=150/150`;
  - `valid_matches=150`;
  - `endpoint_passed=true`;
  - `progress_gate_passed=true`;
  - `telemetry_health=true`;
  - `telemetry_dropped=0`;
  - `dry_run_quality=true`;
  - `dry_run_valid=150/150`;
  - `live_output_gate_allowed=0`;
  - `live_output_gate_blocked=150`;
  - expected block reason, initially `vehicle_not_armed:150`.
- `check-live-session-audit-log.sh` should require:
  - one start event;
  - 150 command events;
  - every command `allowed=false`;
  - every command `reason=vehicle_not_armed`;
  - every command `valid=true`;
  - every command `vx_mps=0`;
  - one stop event with `reason=match_live_route_complete`.
- Collect at least 3 clean Pi dry-run evidence logs before any live-output blocker change.
- Store them in `docs/LIVE_OUTPUT_READINESS_RECORD.md`.
- Evidence completion is not permission to enable live output.

Milestone 15: required 3/3 readiness state
- A complete working pre-live state should include:
  - 22-ish or all current CTest tests passing on Pi, depending on implemented test count;
  - a route-quality prechecked route with `quality_pass=true`;
  - a live match dry-run with 150/150 valid matches;
  - endpoint/progress gate passing;
  - preferably strict forward monotonic progress for the best evidence run;
  - telemetry health true and dropped bytes zero;
  - 150/150 valid dry-run commands;
  - zero live-output allowed frames;
  - 150 live-output blocked frames;
  - expected block reason `vehicle_not_armed:150`;
  - session audit artifact with 150 blocked command records.
- After 3/3 is recorded, update roadmap, safety plan, decisions, session log, and project memory.
- Still keep live output blocked.

Milestone 16: first future live-output plan, not automatic implementation
- Only after 3/3 evidence, write a reviewed bench props-off implementation plan.
- First live-output boundary:
  - bench-only;
  - propellers removed;
  - vehicle restrained;
  - yaw-rate-only;
  - `vx_mps=0`;
  - very short duration;
  - explicit compile-time enable;
  - explicit runtime enable;
  - operator confirmation;
  - audit log ready before any command;
  - command quality already passed;
  - fresh telemetry;
  - single writer;
  - stop/kill behavior documented.
- Do not start a flight ladder here.

Milestone 17: bench props-off writer library and fail-closed wrapper
- Add a concrete serial MAVLink writer library behind an injectable writer interface, but do not attach it to runtime by default.
- Encode unsigned MAVLink2 `SET_POSITION_TARGET_LOCAL_NED` in a body-frame yaw-rate-only shape:
  - yaw-rate active;
  - position ignored;
  - velocity and acceleration ignored by type mask;
  - yaw ignored;
  - `vx_mps=0`;
  - `vy_mps=0`;
  - reject non-finite, nonzero-forward/lateral, or out-of-bounds commands before bytes are written.
- Add memory-transport unit tests for frame shape, sequence increments, start/stop behavior, and command rejection.
- Split compile-time scope:
  - default: live output disabled/unavailable;
  - reviewed bench scope: explicit live-output + bench props-off CMake flags;
  - attach-capable scope: a third explicit attach flag.
- Ordinary desktop/Pi scripts must explicitly set all live-output CMake flags OFF.
- Attach-capable Pi builds must use a separate build directory by default so stale cache state cannot affect ordinary runs.
- Add a dedicated bench props-off wrapper:
  - exact props-off confirmation required;
  - CTest runs before hardware mode;
  - operator cue/countdown;
  - runtime live-output controls provided;
  - max commands and max seconds;
  - session audit path printed;
  - readiness and audit checkers run after the session.
- Before the attach flag is used, expected result is fail-closed:
  - `live_output_writer_attached=false`;
  - `allowed=0`;
  - blocked reason `live_output_unavailable`;
  - endpoint-stop runs may finish before the requested frame count if endpoint progress is reached.
- The attach-build step must be a separate reviewed bench step, not the ordinary wrapper, and still does not authorize flight.

Milestone 18: visual brake / station-keeping assist, separate future feature
- Treat visual braking and station-keeping as a separate post-return feature, not part of the first return/live-output boundary.
- Start dry-run-only:
  - compare reference/current frames;
  - estimate image displacement and scale drift;
  - compensate with FC attitude;
  - log proposed counter-commands only.
- Keep ArduPilot responsible for real hover/hold/failsafe.
- Require a reviewed scale source before any real lateral/forward station-keeping command:
  - rangefinder;
  - altitude model;
  - optical-flow-like visual scale;
  - VIO/UWB/other reviewed source.
- Use deadbands, small gains, slew/rate limits, cooldowns, max-duration, and timeout-to-autopilot-hold. Never implement naive immediate opposite-direction commands.

Representative Pi route recording command:
```bash
cd ~/Visual_Homing_Codex

LOG="artifacts/logs/live-route-record-64x48-10s-current-room-$(date -u +%Y%m%dT%H%M%SZ).log"

VISUAL_HOMING_MAVLINK_TELEMETRY_DEVICE=/dev/serial0 \
VISUAL_HOMING_MAVLINK_TELEMETRY_BAUD=115200 \
VISUAL_HOMING_ROUTE_USE_LIVE_MAVLINK_TELEMETRY=1 \
VISUAL_HOMING_USE_ACTIVE_CAMERA_PROFILE=1 \
VISUAL_HOMING_CAMERA_FRAMES=150 \
VISUAL_HOMING_CAMERA_TARGET_WIDTH=64 \
VISUAL_HOMING_CAMERA_TARGET_HEIGHT=48 \
VISUAL_HOMING_ROUTE_KEYFRAME_SCALE=5 \
VISUAL_HOMING_ROUTE_WARMUP_FRAMES=3 \
VISUAL_HOMING_OPERATOR_CUE_SECONDS=10 \
VISUAL_HOMING_RECORD_LIVE_ROUTE=1 \
./scripts/test-core-pi.sh 2>&1 | tee "$LOG"

VISUAL_HOMING_EXPECTED_ROUTE_QUALITY_ENTRIES=150 \
./scripts/check-route-quality-log.sh "$LOG"
```

Representative Pi match/audit readiness command:
```bash
cd ~/Visual_Homing_Codex

VISUAL_HOMING_USE_ACTIVE_CAMERA_PROFILE=1 \
VISUAL_HOMING_MATCH_LIVE_ROUTE=1 \
VISUAL_HOMING_CAMERA_FRAMES=150 \
VISUAL_HOMING_CAMERA_TARGET_WIDTH=64 \
VISUAL_HOMING_CAMERA_TARGET_HEIGHT=48 \
VISUAL_HOMING_LIVE_ROUTE_MATCH_EXPECTED_PROGRESS=forward \
VISUAL_HOMING_LIVE_ROUTE_MATCH_REQUIRE_ENDPOINT_PROGRESS=1 \
VISUAL_HOMING_LIVE_ROUTE_DRY_RUN_COMMANDS=1 \
VISUAL_HOMING_LIVE_ROUTE_DRY_RUN_REQUIRE_COMMAND_QUALITY=1 \
VISUAL_HOMING_MATCH_LIVE_ROUTE_USE_LIVE_MAVLINK_TELEMETRY=1 \
VISUAL_HOMING_MATCH_LIVE_ROUTE_REQUIRE_LIVE_TELEMETRY_HEALTH=1 \
VISUAL_HOMING_MAVLINK_TELEMETRY_DEVICE=/dev/serial0 \
VISUAL_HOMING_MAVLINK_TELEMETRY_BAUD=115200 \
VISUAL_HOMING_LIVE_ROUTE_NAVIGATOR_FORWARD_SPEED_MPS=0.0 \
VISUAL_HOMING_LIVE_ROUTE_SESSION_AUDIT=1 \
VISUAL_HOMING_OPERATOR_CUE_SECONDS=10 \
./scripts/test-core-pi.sh
```

Representative Pi evidence check commands:
```bash
./scripts/check-live-readiness-log.sh artifacts/logs/test-core-pi-<run>.log
./scripts/check-live-session-audit-log.sh artifacts/logs/live-output-session-audit-<run>.log
```

Documentation to maintain:
- `docs/ARCHITECTURE.md`;
- `docs/ROADMAP.md`;
- `docs/BUILDING.md`;
- `docs/PI_BUILDING.md`;
- `docs/ROUTE_SIGNATURE_FORMAT.md`;
- `docs/LIVE_OUTPUT_SAFETY_PLAN.md`;
- `docs/LIVE_OUTPUT_READINESS_RECORD.md`;
- `docs/DECISIONS.md`;
- `docs/SESSION_LOG.md`;
- `docs/PROJECT_MEMORY.md`.

Acceptance criteria for "working state":
- Desktop build/test passes.
- Pi build/test passes.
- Documentation explicitly states whether this project is a parallel replacement candidate or a companion experiment relative to any existing Python VO/EKF-position-estimate code; it must not be presented as a drop-in port unless the command/output paradigm is deliberately changed.
- Route recording works with camera and optional read-only telemetry.
- Route quality checker can reject weak routes and pass a good route.
- Live route matching dry-run passes on a good route.
- Session audit artifact is generated and validated.
- 3/3 readiness evidence is recorded.
- A bench props-off fail-closed wrapper is available and validated with `live_output_unavailable` before writer attachment.
- A serial writer library may exist, but ordinary/default builds still report writer unattached and unavailable.
- Live MAVLink command output remains disabled and blocked in default builds.
- The next step is a separate reviewed attach-build bench procedure, not flight.

Work method:
1. Inspect or create repository layout.
2. Implement one small milestone at a time.
3. Add tests before or with behavior.
4. Run local tests after each milestone.
5. Keep hardware tests opt-in and documented.
6. Commit and push each coherent completed milestone.
7. Update docs and logs every time the safety boundary or readiness state changes.
8. Never blur the distinction between dry-run evidence, bench props-off live-output planning, and flight authorization.
```
