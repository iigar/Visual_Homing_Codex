# Raspberry Pi Build

This document describes the intended Raspberry Pi build path for the C++ core.

The default desktop build remains replay-first and fail-closed. Pi camera capture is enabled only when the hardware backend is explicitly requested.

## Supported Target

- Raspberry Pi OS 64-bit or Debian-based Raspberry Pi OS.
- Raspberry Pi Zero 2W class hardware is the target performance class.
- Pi camera support is expected to use the Raspberry Pi libcamera stack.

## One-Time Bootstrap

Run this on the Pi from the repository root:

```bash
./scripts/bootstrap-pi.sh
```

The bootstrap script installs the expected system packages and then runs the Pi test script.

## Build And Test

Run:

```bash
./scripts/test-core-pi.sh
```

The script configures `core/build-pi` with:

```bash
-DCMAKE_BUILD_TYPE=MinSizeRel
-DVISUAL_HOMING_ENABLE_LIBCAMERA=ON
```

Then it builds the core and runs CTest.

When `VISUAL_HOMING_ENABLE_LIBCAMERA=ON`, CMake requires `pkg-config` and the `libcamera` development package. The bootstrap script installs both before configuring the build.

Each script run writes the full console output to a timestamped artifact log:

```bash
artifacts/logs/test-core-pi-<UTC>.log
```

The script also prints `pi_test_run_start` and `pi_test_run_done` lines with UTC wall-clock time, elapsed seconds, exit code, route artifact path, and log path. Override or disable this with:

```bash
VISUAL_HOMING_RUN_LOG=/path/to/run.log
VISUAL_HOMING_LOG_DIR=/path/to/logs
VISUAL_HOMING_DISABLE_RUN_LOG=1
```

The core executable also prints `wall_time_utc` on each `Visual Homing Core boot` line. This wall-clock time is for operator log correlation; route diagnostics still use monotonic route time from the `VHRS` entries for frame-level chronology.

The default Pi build is intentionally conservative for Pi Zero 2W memory limits:

```bash
VISUAL_HOMING_PI_BUILD_TYPE=MinSizeRel
VISUAL_HOMING_BUILD_JOBS=1
```

If a larger Pi has enough RAM, `VISUAL_HOMING_BUILD_JOBS` may be increased.

If a previous parallel build was killed by the OS, rerun:

```bash
git pull
./scripts/test-core-pi.sh --clean
```

## Camera Profile Inspection

Validate the default tracked camera profile without touching camera hardware:

```bash
VISUAL_HOMING_INSPECT_CAMERA_PROFILE=1 ./scripts/test-core-pi.sh
```

The default profile path is:

```bash
config/camera_profiles/imx219-visible-wide.profile
```

Override it with:

```bash
VISUAL_HOMING_CAMERA_PROFILE=/path/to/camera.profile
```

The underlying CLI is:

```bash
./core/build-pi/visual_homing_core --inspect-camera-profile <camera.profile>
```

Profile inspection reports profile id, sensor type, pixel format, capture/target dimensions, FOV values, derived radians-per-pixel values, and route-quality thresholds. The IMX219 profile currently contains placeholder FOV values until measured for the exact lens and crop mode.

List available profiles:

```bash
VISUAL_HOMING_LIST_CAMERA_PROFILES=1 ./scripts/test-core-pi.sh
```

Set the active profile after validating it:

```bash
VISUAL_HOMING_SET_CAMERA_PROFILE_ID=imx219-visible-wide ./scripts/test-core-pi.sh
```

Read the active profile:

```bash
VISUAL_HOMING_GET_ACTIVE_CAMERA_PROFILE=1 ./scripts/test-core-pi.sh
```

Run the machine-readable profile API payload commands for future Android/Pi integration:

```bash
VISUAL_HOMING_API_LIST_CAMERA_PROFILES=1 ./scripts/test-core-pi.sh
VISUAL_HOMING_API_GET_ACTIVE_CAMERA_PROFILE=1 ./scripts/test-core-pi.sh
VISUAL_HOMING_API_SET_CAMERA_PROFILE_ID=imx219-visible-wide ./scripts/test-core-pi.sh
```

These commands output JSON from the same validated profile registry and active-profile state used by the human-readable commands. They do not open camera hardware and do not grant flight permission.

The default active-profile state path is ignored by git:

```bash
artifacts/active_camera_profile.txt
```

Override the profile directory or active-profile state file with:

```bash
VISUAL_HOMING_CAMERA_PROFILE_DIR=/path/to/camera_profiles
VISUAL_HOMING_ACTIVE_CAMERA_PROFILE=/path/to/active_camera_profile.txt
```

## MAVLink Telemetry Inspection

Inspect a captured MAVLink byte stream without opening a serial port and without sending any commands:

```bash
VISUAL_HOMING_MAVLINK_TELEMETRY_INPUT=artifacts/mavlink_telemetry.bin VISUAL_HOMING_INSPECT_MAVLINK_TELEMETRY=1 ./scripts/test-core-pi.sh
```

The underlying CLI is:

```bash
./core/build-pi/visual_homing_core --inspect-mavlink-telemetry <mavlink.bin>
```

The inspector parses MAVLink v1/v2 framing and currently extracts `HEARTBEAT`, `ATTITUDE`, `GLOBAL_POSITION_INT`, and `ALTITUDE` payloads for heartbeat presence, armed state, coarse ArduPilot mode, roll/pitch/yaw, and relative altitude when a relative-altitude source is present. It does not validate MAVLink CRC yet and does not open a live device; it is a read-only diagnostic layer before adding serial transport.

The heartbeat output includes raw `heartbeat_custom_mode`, `heartbeat_type`, `heartbeat_autopilot`, `heartbeat_base_mode`, `heartbeat_system_status`, and `heartbeat_mavlink_version` fields so unsupported vehicle mode mappings can be diagnosed without changing wiring.

For ArduCopter, `heartbeat_custom_mode=2` is reported as `mode=AltHold`. It remains a telemetry label only; the command-permission gate still requires armed `Guided` mode.

Capture a short read-only MAVLink byte dump from a POSIX serial device and immediately inspect it:

```bash
VISUAL_HOMING_MAVLINK_TELEMETRY_DEVICE=/dev/ttyAMA0 \
VISUAL_HOMING_MAVLINK_TELEMETRY_BAUD=57600 \
VISUAL_HOMING_MAVLINK_TELEMETRY_DURATION_MS=1000 \
VISUAL_HOMING_CAPTURE_MAVLINK_TELEMETRY=1 \
./scripts/test-core-pi.sh
```

The default output is:

```bash
artifacts/mavlink_telemetry.bin
```

The underlying CLI is:

```bash
./core/build-pi/visual_homing_core --capture-mavlink-telemetry <device> <baud_rate> <duration_ms> <output.bin>
```

This capture path opens the serial device read-only and writes the raw received bytes to disk for diagnostics. It still does not send MAVLink messages and does not enable live command output.

For a strict read-only telemetry smoke test, validate the captured dump after capture:

```bash
VISUAL_HOMING_MAVLINK_TELEMETRY_DEVICE=/dev/serial0 \
VISUAL_HOMING_MAVLINK_TELEMETRY_BAUD=115200 \
VISUAL_HOMING_MAVLINK_TELEMETRY_DURATION_MS=5000 \
VISUAL_HOMING_CAPTURE_MAVLINK_TELEMETRY=1 \
VISUAL_HOMING_VALIDATE_MAVLINK_TELEMETRY=1 \
./scripts/test-core-pi.sh
```

By default this requires at least one `HEARTBEAT`, one `ATTITUDE`, one `GLOBAL_POSITION_INT`, and zero malformed frames. Override the thresholds with `VISUAL_HOMING_MAVLINK_MIN_HEARTBEAT_MESSAGES`, `VISUAL_HOMING_MAVLINK_MIN_ATTITUDE_MESSAGES`, `VISUAL_HOMING_MAVLINK_MIN_GLOBAL_POSITION_INT_MESSAGES`, and `VISUAL_HOMING_MAVLINK_MAX_MALFORMED_FRAMES`.

To attach the latest validated MAVLink telemetry snapshot to a live route recording, enable route telemetry use after capture:

```bash
VISUAL_HOMING_MAVLINK_TELEMETRY_DEVICE=/dev/serial0 \
VISUAL_HOMING_MAVLINK_TELEMETRY_BAUD=115200 \
VISUAL_HOMING_MAVLINK_TELEMETRY_DURATION_MS=5000 \
VISUAL_HOMING_CAPTURE_MAVLINK_TELEMETRY=1 \
VISUAL_HOMING_VALIDATE_MAVLINK_TELEMETRY=1 \
VISUAL_HOMING_ROUTE_USE_MAVLINK_TELEMETRY=1 \
VISUAL_HOMING_USE_ACTIVE_CAMERA_PROFILE=1 \
VISUAL_HOMING_RECORD_LIVE_ROUTE=1 \
./scripts/test-core-pi.sh
```

The recorder validates `artifacts/mavlink_telemetry.bin` again before recording and writes the snapshot relative altitude into each route entry's altitude band. The snapshot yaw is stored as `heading_hint_rad`. This is still a run-level baseline, not continuous per-frame telemetry.

`route_inspect` reports `min_altitude_band_m`, `max_altitude_band_m`, `min_heading_hint_rad`, `max_heading_hint_rad`, `uniform_altitude_band`, and `uniform_heading_hint` so the recorded route artifact can be checked after capture.

For a first live read-only telemetry buffer during route recording, skip the pre-captured snapshot and read the serial stream while camera frames are recorded:

```bash
VISUAL_HOMING_MAVLINK_TELEMETRY_DEVICE=/dev/serial0 \
VISUAL_HOMING_MAVLINK_TELEMETRY_BAUD=115200 \
VISUAL_HOMING_ROUTE_USE_LIVE_MAVLINK_TELEMETRY=1 \
VISUAL_HOMING_USE_ACTIVE_CAMERA_PROFILE=1 \
VISUAL_HOMING_RECORD_LIVE_ROUTE=1 \
./scripts/test-core-pi.sh
```

This starts a background read-only serial stream for the route-recording duration. Each frame uses the latest validated telemetry for altitude band and heading hint when heartbeat, attitude, and global-position messages have all been seen; otherwise it falls back to the configured route altitude/heading values. Live command output remains blocked.

The live telemetry route path waits up to `VISUAL_HOMING_ROUTE_TELEMETRY_WARMUP_MS` before recording camera frames; the default is `1500`. If heartbeat, attitude, and global-position telemetry are not valid before the timeout, the run fails closed instead of writing a mixed fallback/telemetry route.

## Camera Smoke Test

After a real libcamera backend is implemented and camera hardware is attached, run:

```bash
VISUAL_HOMING_RUN_CAMERA_SMOKE=1 ./scripts/test-core-pi.sh
```

Optional camera smoke settings:

```bash
VISUAL_HOMING_CAMERA_WIDTH=320
VISUAL_HOMING_CAMERA_HEIGHT=240
VISUAL_HOMING_CAMERA_FPS=15
VISUAL_HOMING_CAMERA_FRAMES=30
VISUAL_HOMING_CAMERA_TARGET_WIDTH=32
VISUAL_HOMING_CAMERA_TARGET_HEIGHT=24
```

To use a profile file for capture and target dimensions:

```bash
VISUAL_HOMING_USE_CAMERA_PROFILE=1 VISUAL_HOMING_RUN_CAMERA_SMOKE=1 ./scripts/test-core-pi.sh
```

To use the active profile selected by `VISUAL_HOMING_SET_CAMERA_PROFILE_ID`:

```bash
VISUAL_HOMING_USE_ACTIVE_CAMERA_PROFILE=1 VISUAL_HOMING_RUN_CAMERA_SMOKE=1 ./scripts/test-core-pi.sh
```

The underlying CLI is:

```bash
./core/build-pi/visual_homing_core --pi-camera-smoke <width> <height> <fps> <frames> [target_width target_height]
./core/build-pi/visual_homing_core --pi-camera-smoke-profile <camera.profile> <fps> <frames>
./core/build-pi/visual_homing_core --pi-camera-smoke-active-profile <profile_dir> <active_profile_state> <fps> <frames>
```

The smoke command captures live camera frames, preprocesses them to the target Gray8 size, and reports frame age, processing latency, empty polls, elapsed time, and effective FPS.

## Live Route Recording

After camera smoke passes, a live route signature can be recorded from the same Pi camera path:

```bash
VISUAL_HOMING_RECORD_LIVE_ROUTE=1 ./scripts/test-core-pi.sh
```

Optional live route settings:

```bash
VISUAL_HOMING_ROUTE_OUTPUT=artifacts/visual_homing_live_route.vhrs
VISUAL_HOMING_CAMERA_FRAMES=120
VISUAL_HOMING_CAMERA_TARGET_WIDTH=64
VISUAL_HOMING_CAMERA_TARGET_HEIGHT=48
VISUAL_HOMING_ROUTE_WARMUP_FRAMES=3
VISUAL_HOMING_ROUTE_ALTITUDE_M=0.0
VISUAL_HOMING_ROUTE_HEADING_HINT_RAD=0.0
```

Before live route recording starts, the Pi script prints a large operator cue, emits terminal bell characters, and counts down for `5` seconds by default. Tune or disable it with:

```bash
VISUAL_HOMING_OPERATOR_CUE=1
VISUAL_HOMING_OPERATOR_CUE_SECONDS=5
VISUAL_HOMING_OPERATOR_CUE_BELL=1
```

`VISUAL_HOMING_ROUTE_WARMUP_FRAMES` defaults to `3` in the Pi script. Warmup frames are captured and logged as `live_route_warmup_frame`, but they are not preprocessed into the route and are not written to the `VHRS` artifact. Set it to `0` to preserve raw startup behavior for diagnostics.

To record using a profile file for capture and target dimensions:

```bash
VISUAL_HOMING_USE_CAMERA_PROFILE=1 VISUAL_HOMING_RECORD_LIVE_ROUTE=1 ./scripts/test-core-pi.sh
```

To record using the active profile:

```bash
VISUAL_HOMING_USE_ACTIVE_CAMERA_PROFILE=1 VISUAL_HOMING_RECORD_LIVE_ROUTE=1 ./scripts/test-core-pi.sh
```

When `VISUAL_HOMING_CAMERA_TARGET_WIDTH` and `VISUAL_HOMING_CAMERA_TARGET_HEIGHT` are set together, active-profile live telemetry recording overrides only the route signature size while keeping the active profile's capture dimensions and field of view. For example, use `64x48` signatures and export larger operator keyframes:

```bash
VISUAL_HOMING_CAMERA_TARGET_WIDTH=64 \
VISUAL_HOMING_CAMERA_TARGET_HEIGHT=48 \
VISUAL_HOMING_ROUTE_KEYFRAME_SCALE=5 \
VISUAL_HOMING_USE_ACTIVE_CAMERA_PROFILE=1 \
VISUAL_HOMING_RECORD_LIVE_ROUTE=1 \
./scripts/test-core-pi.sh
```

The underlying CLI is:

```bash
./core/build-pi/visual_homing_core --record-live-route <camera_width> <camera_height> <fps> <frames> <route.vhrs> <target_width> <target_height> <altitude_m> [heading_hint_rad [warmup_frames]]
./core/build-pi/visual_homing_core --record-live-route-profile <camera.profile> <fps> <frames> <route.vhrs> <altitude_m> [heading_hint_rad [warmup_frames]]
./core/build-pi/visual_homing_core --record-live-route-active-profile <profile_dir> <active_profile_state> <fps> <frames> <route.vhrs> <altitude_m> [heading_hint_rad [warmup_frames]]
```

When live route recording succeeds through `scripts/test-core-pi.sh`, the script automatically inspects the written file with `--inspect-route`.

It also exports route orientation keyframes as binary Gray8 PGM files to `VISUAL_HOMING_ROUTE_KEYFRAME_DIR`, defaulting to `artifacts/route_keyframes`:

```text
start.pgm
025.pgm
050.pgm
075.pgm
end.pgm
```

Use these files to identify where the route began and ended before running forward or reverse live-match tests.

Set `VISUAL_HOMING_ROUTE_KEYFRAME_SCALE` to export larger PGM images for human review. The default is `1`; `5` turns a `64x48` route keyframe into a `320x240` PGM without changing the route artifact itself.

This path is still a hardware validation/recording mode, not a flight loop. It intentionally writes to disk after collecting live frames and should not be used inside future realtime command loops.

## Route Inspection

Inspect an existing route file without touching camera hardware:

```bash
VISUAL_HOMING_INSPECT_ROUTE=1 ./scripts/test-core-pi.sh
```

The underlying CLI is:

```bash
./core/build-pi/visual_homing_core --inspect-route <route.vhrs>
```

The inspector reports version, entry count, frame id range, timestamp range, signature dimensions, payload byte totals, monotonic timestamp status, dimension/payload uniformity, and whether all entries are Gray8.

The default route artifact path is `artifacts/visual_homing_live_route.vhrs`. The `artifacts/` directory is intentionally ignored by git.

Export keyframes from an existing route artifact without touching camera hardware:

```bash
VISUAL_HOMING_EXPORT_ROUTE_KEYFRAMES=1 ./scripts/test-core-pi.sh
```

For larger exported keyframes:

```bash
VISUAL_HOMING_EXPORT_ROUTE_KEYFRAMES=1 VISUAL_HOMING_ROUTE_KEYFRAME_SCALE=5 ./scripts/test-core-pi.sh
```

## Route Validation

Run the full offline validation chain on an existing route artifact without touching camera hardware:

```bash
VISUAL_HOMING_VALIDATE_ROUTE=1 ./scripts/test-core-pi.sh
```

This runs route inspection, self-match, perturbation checks, and distinctiveness diagnostics against `VISUAL_HOMING_ROUTE_OUTPUT`. It respects the same optional controls as the individual checks:

```bash
VISUAL_HOMING_SELF_MATCH_MIN_CONFIDENCE=0.99
VISUAL_HOMING_PERTURB_MIN_CONFIDENCE=0.90
VISUAL_HOMING_ROUTE_EDGE_TRIM=3
```

After recording or validating a route, check the route-quality portion of the Pi log without rereading the full output:

```bash
./scripts/check-route-quality-log.sh artifacts/logs/test-core-pi-<UTC>.log
```

The checker requires route self-match to pass, perturbation checks to pass, malformed payload rejection to pass, zero exact duplicate entries, and `route_distinctiveness quality_pass=true`. For a Milestone 6.7 readiness route, also require the expected entry count:

```bash
VISUAL_HOMING_EXPECTED_ROUTE_QUALITY_ENTRIES=150 \
./scripts/check-route-quality-log.sh artifacts/logs/test-core-pi-<UTC>.log
```

Set `VISUAL_HOMING_ALLOW_ROUTE_QUALITY_WARNING=1` only for diagnostics when you intentionally want to inspect a weak route; do not use that override for readiness evidence.

## Live Route Matching

Match live camera frames against an existing route artifact without overwriting the route:

```bash
VISUAL_HOMING_MATCH_LIVE_ROUTE=1 VISUAL_HOMING_USE_ACTIVE_CAMERA_PROFILE=1 ./scripts/test-core-pi.sh
```

The underlying CLI is:

```bash
./core/build-pi/visual_homing_core --match-live-route-active-profile <profile_dir> <active_profile_state> <fps> <frames> <route.vhrs> <warmup_frames> <window_radius> <minimum_confidence> <max_direction_shift_px> [any|forward|reverse [max_progress_regressions [max_progress_rollback [target_width target_height] [require_endpoint_progress endpoint_start_progress endpoint_end_progress]]]] [--external-nav-estimates enabled nominal_route_length_m minimum_match_confidence maximum_altitude_age_ms source_tag]
```

The Pi script reads `VISUAL_HOMING_ROUTE_OUTPUT` and does not write a new `VHRS` artifact. It logs each `live_route_match_frame` with route index, progress, confidence, validity, and FOV-derived direction error. The final `live_route_match_done` line reports captured frames, valid matches, forward and reverse progress regressions, rollback totals, endpoint progress, confidence summary, effective FPS, and `passed`.

`VISUAL_HOMING_LIVE_ROUTE_MATCH_EXPECTED_PROGRESS=any` checks recognition only and is the default. Use `forward` when repeating the recorded route direction and `reverse` when testing a return pass back along the route. Directional modes tolerate small live matcher jumps by default; set both tolerance values to zero for strict monotonic testing.

Manual field passes rarely start and stop at exact route endpoints. Set `VISUAL_HOMING_LIVE_ROUTE_MATCH_REQUIRE_ENDPOINT_PROGRESS=1` to make pass/fail use endpoint zones instead of local monotonic progress. With the default endpoint thresholds, `forward` passes when progress starts at or below `0.15` and reaches at least `0.85`; `reverse` passes when progress starts at or above `0.85` and reaches at most `0.15`. The log still reports `directional_progress_passed` for local direction diagnostics, plus `min_progress_seen`, `max_progress_seen`, `endpoint_progress_passed`, and `progress_gate_passed` for the actual selected gate.

Live route matching uses the same operator cue countdown as live recording before camera matching starts.

Optional controls:

```bash
VISUAL_HOMING_CAMERA_TARGET_WIDTH=64
VISUAL_HOMING_CAMERA_TARGET_HEIGHT=48
VISUAL_HOMING_LIVE_ROUTE_MATCH_WINDOW_RADIUS=30
VISUAL_HOMING_LIVE_ROUTE_MATCH_MIN_CONFIDENCE=0.75
VISUAL_HOMING_LIVE_ROUTE_MATCH_MAX_DIRECTION_SHIFT_PX=4
VISUAL_HOMING_LIVE_ROUTE_MATCH_EXPECTED_PROGRESS=any
VISUAL_HOMING_LIVE_ROUTE_MATCH_MAX_PROGRESS_REGRESSIONS=5
VISUAL_HOMING_LIVE_ROUTE_MATCH_MAX_PROGRESS_ROLLBACK=0.25
VISUAL_HOMING_LIVE_ROUTE_MATCH_REQUIRE_ENDPOINT_PROGRESS=0
VISUAL_HOMING_LIVE_ROUTE_MATCH_ENDPOINT_START_PROGRESS=0.15
VISUAL_HOMING_LIVE_ROUTE_MATCH_ENDPOINT_END_PROGRESS=0.85
```

If the route was recorded with a target override such as `64x48`, use the same `VISUAL_HOMING_CAMERA_TARGET_WIDTH` and `VISUAL_HOMING_CAMERA_TARGET_HEIGHT` values for live matching.

To inspect the bounded navigator output without sending any MAVLink commands, enable dry-run command logging during live matching:

```bash
VISUAL_HOMING_LIVE_ROUTE_DRY_RUN_COMMANDS=1
VISUAL_HOMING_LIVE_ROUTE_NAVIGATOR_MIN_CONFIDENCE=0.75
VISUAL_HOMING_LIVE_ROUTE_NAVIGATOR_MAX_MATCH_AGE_MS=250
VISUAL_HOMING_LIVE_ROUTE_NAVIGATOR_YAW_GAIN=1.0
VISUAL_HOMING_LIVE_ROUTE_NAVIGATOR_MAX_YAW_RATE_RADPS=0.35
VISUAL_HOMING_LIVE_ROUTE_NAVIGATOR_MAX_YAW_ACCEL_RADPS2=1.0
VISUAL_HOMING_LIVE_ROUTE_NAVIGATOR_FORWARD_SPEED_MPS=0.0
VISUAL_HOMING_LIVE_ROUTE_DRY_RUN_REQUIRE_COMMAND_QUALITY=0
VISUAL_HOMING_LIVE_ROUTE_DRY_RUN_MIN_VALID_COMMAND_FRACTION=0.95
VISUAL_HOMING_LIVE_ROUTE_DRY_RUN_MAX_INVALID_COMMAND_STREAK=3
VISUAL_HOMING_LIVE_ROUTE_DRY_RUN_MAX_ABS_YAW_RATE_RADPS=0.35
VISUAL_HOMING_LIVE_ROUTE_DRY_RUN_MAX_YAW_RATE_SIGN_FLIPS=20
VISUAL_HOMING_LIVE_ROUTE_DRY_RUN_MAX_YAW_RATE_DELTA_RADPS=0.15
```

This path writes `dry_run_command`, per-frame `command_*` metrics, and final dry-run command quality fields such as valid command fraction, max invalid streak, yaw-rate sign flips, and max yaw-rate delta. It does not open a live MAVLink command output path. Set `VISUAL_HOMING_LIVE_ROUTE_DRY_RUN_REQUIRE_COMMAND_QUALITY=1` only when you want the command-quality gate to participate in the final `passed` result.

To gate the same dry-run command path with live read-only MAVLink telemetry health, enable the live telemetry stream during matching:

```bash
VISUAL_HOMING_MAVLINK_TELEMETRY_DEVICE=/dev/serial0
VISUAL_HOMING_MAVLINK_TELEMETRY_BAUD=115200
VISUAL_HOMING_MATCH_LIVE_ROUTE_USE_LIVE_MAVLINK_TELEMETRY=1
VISUAL_HOMING_MATCH_LIVE_ROUTE_TELEMETRY_MAX_AGE_MS=500
VISUAL_HOMING_MATCH_LIVE_ROUTE_REQUIRE_LIVE_TELEMETRY_HEALTH=1
VISUAL_HOMING_ROUTE_TELEMETRY_WARMUP_MS=1500
```

This mode still sends no MAVLink commands. It opens the serial device read-only, waits for heartbeat, attitude, and global-position telemetry, then uses fresh heartbeat telemetry as the `mavlink_ok` health input for `BoundedNavigator`. The final `live_route_match_done` line reports telemetry byte/frame/message counts plus `telemetry_health_ready_frames`, `telemetry_health_degraded_frames`, and `live_telemetry_health_passed`.

For bench dry-run matching, this health gate only checks read-only telemetry freshness. Armed/GUIDED command permission remains reserved for a later live-output safety step.

To log proposed external-navigation estimates without sending `VISION_POSITION_ESTIMATE`, `ODOMETRY`, or command-output MAVLink messages, enable the external-nav dry-run block on top of live matching with read-only telemetry:

```bash
VISUAL_HOMING_EXTERNAL_NAV_ESTIMATES=1
VISUAL_HOMING_EXTERNAL_NAV_NOMINAL_ROUTE_LENGTH_M=<recorded-route-length-meters>
VISUAL_HOMING_EXTERNAL_NAV_MINIMUM_MATCH_CONFIDENCE=0.90
VISUAL_HOMING_EXTERNAL_NAV_MAXIMUM_ALTITUDE_AGE_MS=500
VISUAL_HOMING_EXTERNAL_NAV_SOURCE=visual_route_progress
VISUAL_HOMING_EXTERNAL_NAV_BENCH_ALTITUDE_M=0
VISUAL_HOMING_EXTERNAL_NAV_EXPECTED_RELATIVE_ALTITUDE_M=0
VISUAL_HOMING_EXTERNAL_NAV_EXPECTED_RELATIVE_ALTITUDE_TOLERANCE_M=0
```

For this external-nav dry-run mode only, live-route telemetry warmup accepts heartbeat plus attitude without requiring `GLOBAL_POSITION_INT`, because a GPS-denied FC may not publish that message before an external-navigation provider exists. If no `GLOBAL_POSITION_INT` or MAVLink `ALTITUDE` relative-height source is seen, the run can still collect route/matcher evidence, but each estimate remains `valid_for_fc=false` with an altitude/scale reason.

This writes one `external_nav_estimate` line per matched frame and adds `external_nav_valid_for_fc` plus `external_nav_invalid_reasons` to the final summaries. An estimate is marked FC-ready only when the route match is valid/confident, telemetry relative altitude/yaw is fresh, and nominal route scale is configured. `VISUAL_HOMING_EXTERNAL_NAV_BENCH_ALTITUDE_M` is an optional bench-only diagnostic fallback for computing loggable `x/y/z` scale when the FC reports a zero or negative relative altitude; estimates that use it remain `valid_for_fc=false` with `reason=bench_diagnostic_altitude_not_fc_ready`. When that bench/reference altitude is positive, the same log line also includes a camera-only scale diagnostic: `visual_scale_valid`, `visual_scale_ratio`, `visual_altitude_m`, and `visual_scale_confidence`. This compares the live frame against nearby scaled versions of the matched route keyframe and is only a diagnostic for scale stability; it is not an FC-ready altitude source. Final summaries report `visual_scale_required`; it is `true` only for positive bench/reference-altitude diagnostic runs, so a barometer-altitude FC-ready run without bench altitude can pass external-nav quality with `visual_scale_required=false` and `visual_scale_valid=0/<frames>`. This is Milestone 6.9 log evidence only; it does not make ArduPilot accept `Guided` and it does not attach an external-nav writer.

For operator readiness, use `external_nav_operator_readiness=ready|marginal|blocked` and `external_nav_operator_reason`. `blocked` means the tolerant log-quality contract failed. `marginal` means the safety/readiness gates passed but a diagnostic was not clean, such as route directional progress exceeding the operator soft threshold or the strict all-frames external-nav diagnostic. The operator route-progress soft threshold is intentionally looser than the core directional gate: up to `15` progress regressions and `1.0` total rollback can still be `ready` when endpoint and quality gates pass. `ready` means both the tolerant quality gate and the currently tracked soft diagnostics passed. The lower-level `external_nav_session_ready` and `external_nav_quality_ready` fields still expose the tolerant log-quality contract directly: route session passed, required telemetry health passed, enough FC-ready external-nav estimates were produced, invalid streaks stayed short, and optional altitude/visual-scale gates passed. The strict all-frames diagnostic is reported separately as `external_nav_strict_session_ready`; it can be false for small route-match glitches such as `148/150` valid estimates while the tolerant session remains valid.

Set `VISUAL_HOMING_EXTERNAL_NAV_EXPECTED_RELATIVE_ALTITUDE_M` and `VISUAL_HOMING_EXTERNAL_NAV_EXPECTED_RELATIVE_ALTITUDE_TOLERANCE_M` together to require a physical barometer sanity window in the log-only quality gate. When enabled, final summaries report `external_nav_expected_relative_altitude_required`, the configured expected/tolerance values, and `external_nav_relative_altitude_window_passed`. If the observed relative-altitude min/max range is outside the expected window, `external_nav_quality_ready=false` with `external_nav_quality_reason=relative_altitude_out_of_expected_window`, even when the route match and telemetry stream otherwise pass. Leave both values at `0` to disable this check.

The MAVLink telemetry inspector also reports long-capture drift diagnostics: `relative_altitude_min_avg_max_m` and, when the FC publishes MAVLink `DISTANCE_SENSOR`, `distance_sensor_current_min_avg_max_m`. Rangefinder use remains optional; these fields are diagnostic until a later reviewed change explicitly gates or estimates against rangefinder telemetry.

Before spending time on a route-match dry-run that depends on FC relative altitude, run the read-only preflight telemetry sanity checker. It captures MAVLink telemetry, inspects it, and fails before the route pass when the full observed relative-altitude range is outside the configured physical stand window:

```bash
cd ~/Visual_Homing_Codex

VISUAL_HOMING_MAVLINK_TELEMETRY_DEVICE=/dev/serial0 \
VISUAL_HOMING_MAVLINK_TELEMETRY_BAUD=115200 \
VISUAL_HOMING_MAVLINK_TELEMETRY_DURATION_MS=60000 \
VISUAL_HOMING_EXTERNAL_NAV_EXPECTED_RELATIVE_ALTITUDE_M=0.5 \
VISUAL_HOMING_EXTERNAL_NAV_EXPECTED_RELATIVE_ALTITUDE_TOLERANCE_M=0.25 \
VISUAL_HOMING_EXTERNAL_NAV_PREFLIGHT_MIN_RELATIVE_ALTITUDE_SAMPLES=5 \
VISUAL_HOMING_EXTERNAL_NAV_MAX_RELATIVE_ALTITUDE_SPAN_M=0.25 \
./scripts/check-external-nav-telemetry-sanity-pi.sh
```

The checker prints a single `external_nav_telemetry_sanity` line with `passed=true|false`, the reason, the source log path, `relative_altitude_min_avg_max_m`, the accepted window, and the optional drift span check. `VISUAL_HOMING_EXTERNAL_NAV_MAX_RELATIVE_ALTITUDE_SPAN_M=0` disables the span check. Leave `VISUAL_HOMING_EXTERNAL_NAV_PREFLIGHT_REQUIRE_DISTANCE_SENSOR=0` unless the FC is actually publishing MAVLink `DISTANCE_SENSOR` on the Pi telemetry port.

Set `VISUAL_HOMING_EXTERNAL_NAV_ALTITUDE_PRESET=floor` to use the floor sanity defaults (`0.05 +/- 0.15 m`) or `VISUAL_HOMING_EXTERNAL_NAV_ALTITUDE_PRESET=stand` to use the bench stand defaults (`0.5 +/- 0.25 m`). Use `custom` plus explicit expected/tolerance values for measured field setups.

For the standard external-nav dry-run readiness sequence, use the wrapper that runs both the preflight sanity check and live route match, then validates the final compact line:

```bash
cd ~/Visual_Homing_Codex

VISUAL_HOMING_EXTERNAL_NAV_ALTITUDE_PRESET=stand \
VISUAL_HOMING_EXTERNAL_NAV_MINIMUM_MATCH_CONFIDENCE=0.82 \
./scripts/run-external-nav-dry-run-pi.sh
```

The wrapper still sends no MAVLink commands and writes no external-nav MAVLink. It expects live output to remain blocked by the safety gate, normally `vehicle_not_armed:<frames>`, and prints `external_nav_readiness_log_check passed=true` only when the route, telemetry, dry-run command, altitude-window, and external-nav quality gates pass.

The wrapper also exports a dependency-free JSON readiness artifact next to the dry-run log, normally `artifacts/logs/external-nav-dry-run-<stamp>.json`, using schema `visual_homing.external_nav_readiness.v1`. Set `VISUAL_HOMING_EXTERNAL_NAV_READINESS_JSON=/path/to/output.json` to choose a different output path, or run `scripts/export-external-nav-readiness-json.sh <log> [output.json]` against an existing log. This JSON is the intended future Android/Pi API contract seed; Android should consume it rather than parsing raw key-value logs.

The readiness JSON separates operator/UI intent from Pi-owned decisions. `operator_inputs` records per-run inputs such as the altitude preset plus optional `VISUAL_HOMING_HANDOFF_REQUESTED_DISTANCE_M` and `VISUAL_HOMING_HANDOFF_REQUESTED_ALTITUDE_M`. `resolved_config` records the altitude window and nominal route length that the Pi actually used. `handoff` reports whether route completion plus Visual Homing readiness creates a handoff candidate, while `jt_zero` currently remains `available=false`, `ready=false`, `reason=not_integrated`. These fields are a contract for a later same-Pi JT_Zero module; they do not enable handoff or live output by themselves.

The readiness wrapper defaults to a short operator workflow: `VISUAL_HOMING_EXTERNAL_NAV_PREFLIGHT_DURATION_MS=15000` and `VISUAL_HOMING_OPERATOR_CUE_SECONDS=5`. Override those values when deliberately running a longer drift/debug capture, but keep the short defaults for ordinary repeated stand/floor readiness checks.

The future Android companion UI should expose the same altitude preset as an operator choice, for example "Floor" and "Stand", before invoking the Pi readiness API. Android should send the preset/intent to the Pi and display the resolved expected altitude, tolerance, and final `external_nav_telemetry_sanity` or `external_nav_readiness_log_check` verdict; it should not silently invent or store a separate height calibration path.

For the future Android readiness screen, keep the Pi as the source of truth and make the phone a visual explanation layer. The first viewport should show the top-level `external_nav_operator_readiness` as `READY`, `MARGINAL`, or `BLOCKED`, then group the reason into compact cards for Route, Altitude, Telemetry, ExternalNav, Safety Gate, and Handoff Candidate. Good field UI should show the selected altitude preset, expected altitude window, observed min/avg/max altitude, route start/end progress, regressions against the operator threshold, external-nav valid counts, telemetry freshness, the expected live-output block reason, and the current handoff state. Android may send per-run requested values, but it must not keep independent thresholds or decide whether handoff/live output is allowed. Use calm green for ready, amber with subtle emphasis for marginal, and red blocked framing for blocked; avoid making Android a second calibration or gate-policy implementation.

To also create a real non-live session audit artifact during the full dry-run telemetry match path, enable:

```bash
VISUAL_HOMING_LIVE_ROUTE_SESSION_AUDIT=1
VISUAL_HOMING_LIVE_ROUTE_SESSION_AUDIT_PATH=artifacts/logs/live-output-session-audit.log
```

This requires dry-run commands, live read-only telemetry, and explicit camera target dimensions. It writes `LiveMavlinkOutputSession` audit records through the real file audit boundary while still using a dry-run bridge. It does not enable live MAVLink command output.

Milestone 6.7 readiness logs can be checked without rereading the full run output:

```bash
./scripts/check-live-readiness-log.sh artifacts/logs/test-core-pi-20260604T205416Z.log
```

The checker reads the final `live_route_match_compact` line and requires the current safety-readiness defaults: `passed=true`, `frames=150/150`, `valid_matches=150`, endpoint/progress gates passed, read-only telemetry healthy with zero dropped bytes, dry-run command quality passed with `150/150` valid commands, zero live-output allowed frames, and `live_output_gate_block_reasons=vehicle_not_armed:150`. Pass multiple log paths to verify the three clean dry-runs required by `docs/LIVE_OUTPUT_SAFETY_PLAN.md`.

Before spending time on a readiness match/audit run against a newly recorded route, check the route recording log first:

```bash
VISUAL_HOMING_EXPECTED_ROUTE_QUALITY_ENTRIES=150 \
./scripts/check-route-quality-log.sh artifacts/logs/test-core-pi-<record-run>.log
```

When session audit is enabled, validate the real audit artifact as a separate file:

```bash
./scripts/check-live-session-audit-log.sh artifacts/logs/live-output-session-audit-20260605T194839Z.log
```

## Bench Props-Off Live-Output Boundary Wrapper

This wrapper is for the reviewed Milestone 6.8 bench boundary only. It does not authorize flight, tethered flight, ground movement, or autonomous return. Propellers must be removed and the vehicle must be physically restrained.

The repository has a concrete serial MAVLink writer library, but it is not attached to runtime sessions and live output remains unavailable. The expected result is still fail-closed: route matching and dry-run command quality pass, while live-output decisions remain blocked with `live_output_unavailable`.

```bash
cd ~/Visual_Homing_Codex

VISUAL_HOMING_LIVE_OUTPUT_BENCH_PROPS_OFF_CONFIRM=I_UNDERSTAND_PROPS_ARE_REMOVED \
./scripts/run-live-output-bench-props-off-pi.sh
```

The wrapper writes timestamped logs under `artifacts/logs/` and then runs:

```bash
./scripts/check-live-readiness-log.sh <bench-run-log>
./scripts/check-live-session-audit-log.sh <bench-audit-log>
```

Default expected audit result before runtime writer attachment:

```text
allowed=0 blocked=150 reason=live_output_unavailable
```

By default, the audit checker preserves the original readiness contract: one start event, `150` command audit records, one stop event with `reason=match_live_route_complete`, and every command record blocked with `allowed=false`, `reason=vehicle_not_armed`, `valid=true`, and `vx_mps=0`. The bench wrapper overrides the expected allowed/blocked counts and reason for its current fail-closed `live_output_unavailable` stage.

## Reviewed Attach-Build Bench Step

This is a separate reviewed bench step, not the ordinary wrapper. It configures the attach-capable Pi build directory, attaches the serial writer, requires a different confirmation string, and still does not authorize flight, tethered flight, ground movement, or autonomous return. Propellers must be removed and the vehicle must be physically restrained.

The default expected result is still blocked by the safety gate, normally `vehicle_not_armed`, while proving the build/log path reports `attach_writer_cmake=ON` and `live_output_writer_attached=true`.

```bash
cd ~/Visual_Homing_Codex

VISUAL_HOMING_LIVE_OUTPUT_BENCH_ATTACH_CONFIRM=I_UNDERSTAND_SERIAL_WRITER_IS_ATTACHED_AND_PROPS_ARE_REMOVED \
./scripts/run-live-output-bench-props-off-attach-pi.sh
```

For a reviewed send-enabled bench attempt, do not use the ordinary wrapper. Set explicit expected allowed/blocked audit counts and expected gate reasons for that run; leave the default unless the operator has intentionally prepared the FC mode/configuration and reviewed the bench conditions.

## Reviewed Send-Enabled Bench Step

This step is prepared for a later reviewed props-off bench run that can send bounded yaw-rate-only MAVLink commands. It is not the ordinary wrapper and not the attach-default wrapper. It does not authorize flight, tethered flight, ground movement, or autonomous return.

Before running it, verify propellers are removed, the vehicle is physically restrained, the operator stop path is available, and the FC is intentionally in the armed `Guided` bench state required for this stage.

```bash
cd ~/Visual_Homing_Codex

VISUAL_HOMING_LIVE_OUTPUT_BENCH_SEND_CONFIRM=I_UNDERSTAND_THIS_WILL_SEND_BOUNDED_MAVLINK_COMMANDS_WITH_PROPS_REMOVED \
VISUAL_HOMING_LIVE_OUTPUT_BENCH_ARMED_GUIDED_CONFIRM=I_HAVE_VERIFIED_ARMED_GUIDED_BENCH_STATE \
./scripts/run-live-output-bench-props-off-send-pi.sh
```

Expected result:

```text
attach_writer_cmake=ON
live_output_writer_attached=true
allowed=<positive>
blocked=0
reason=allowed
send_bench_audit_check passed=true
```

If any blocked command appears, route progress fails, telemetry health fails, dry-run command quality fails, or physical behavior is unexpected, treat the run as non-evidence and stop the bench sequence.

## Route Self-Match

Self-match an existing route artifact without touching camera hardware:

```bash
VISUAL_HOMING_SELF_MATCH_ROUTE=1 ./scripts/test-core-pi.sh
```

The underlying CLI is:

```bash
./core/build-pi/visual_homing_core --self-match-route <route.vhrs> [minimum_confidence]
```

Live route recording through `scripts/test-core-pi.sh` also runs self-match automatically after recording and inspection. The self-match check feeds each `VHRS` entry back through `Gray8RouteMatcher` and reports checked entries, valid matches, exact index matches, minimum/average confidence, last progress, monotonic progress, and pass/fail status. Exact index matches and progress monotonicity are diagnostics for route distinctiveness; high-confidence valid matches are the self-match pass gate.

## Route Perturbation Check

Run deterministic perturbation checks on an existing route artifact without touching camera hardware:

```bash
VISUAL_HOMING_PERTURB_ROUTE=1 ./scripts/test-core-pi.sh
```

The underlying CLI is:

```bash
./core/build-pi/visual_homing_core --perturb-route <route.vhrs> [minimum_confidence]
```

Live route recording through `scripts/test-core-pi.sh` also runs perturbation checks automatically after recording, inspection, and self-match. The perturbation check applies a brightness offset, small deterministic byte noise, and a one-pixel horizontal shift to route entries, then reports valid-match counts and minimum confidence for each case. It also verifies that a malformed payload is rejected. The default minimum perturbation confidence is `0.90` and can be overridden with `VISUAL_HOMING_PERTURB_MIN_CONFIDENCE`.

## Route Distinctiveness Diagnostic

Run lightweight distinctiveness diagnostics on an existing route artifact without touching camera hardware:

```bash
VISUAL_HOMING_ROUTE_DISTINCTIVENESS=1 ./scripts/test-core-pi.sh
```

The underlying CLI is:

```bash
./core/build-pi/visual_homing_core --route-distinctiveness <route.vhrs> [edge_trim_entries]
```

Live route recording through `scripts/test-core-pi.sh` also runs this diagnostic automatically after perturbation checks. It reports the evaluated frame/time span, low-texture entries, exact duplicate entries, ambiguous nearest-neighbor entries, payload range, adjacent mean absolute byte difference, nearest-neighbor mean absolute byte difference, and route-quality policy fields. This is an offline diagnostic, not a flight gate; `warning=true` means the route has some repetitive or low-texture samples, while `quality_pass=false` means the current artifact should not be used as a good bench/field-test route without recapture or explicit operator review.

The diagnostic also prints small sample lists for `low_texture_samples`, `exact_duplicate_samples`, and `ambiguous_nearest_samples`. Entries use `frame_id@route_time_ms` format, where `route_time_ms` is relative to the first `VHRS` timestamp. These samples are intended to show whether a failure came from start/end pauses, a short flat segment, or repeated visual content throughout the route.

If the sample lists show only deliberate start/end pauses, rerun the diagnostic with an explicit edge trim:

```bash
VISUAL_HOMING_ROUTE_EDGE_TRIM=3 VISUAL_HOMING_ROUTE_DISTINCTIVENESS=1 ./scripts/test-core-pi.sh
```

The trim is diagnostic only: it does not modify the `VHRS` artifact. It evaluates route quality after ignoring the first and last N route entries and reports `entries_ignored_at_start`, `entries_ignored_at_end`, `first_evaluated_frame_id`, `last_evaluated_frame_id`, `first_evaluated_route_time_ms`, and `last_evaluated_route_time_ms`.

Default route-quality policy:

- low-texture entry fraction must be `<= 0.05`;
- ambiguous nearest-neighbor entry fraction must be `<= 0.10`;
- average nearest mean absolute byte difference must be `>= 5.0`;
- exact duplicate entries are rejected.

## Hardware Backend Policy

- `VISUAL_HOMING_ENABLE_LIBCAMERA` defaults to `OFF`.
- Desktop and CI-style builds should leave it `OFF`.
- Pi builds may set it `ON`; when enabled, CMake requires the `libcamera` pkg-config module.
- Live capture remains runtime opt-in through explicit camera smoke or live route recording commands.
- Live loops must not do disk I/O.
- Every live frame must carry a monotonic timestamp from the core clock at capture receipt.
- Live ArduPilot command output remains blocked; camera validation is separate from flight command validation.

## Expected Validation Metrics

When the real backend is added, Pi validation should record:

- configured frame size and frame rate;
- frames captured;
- frames dropped;
- preprocessing latency;
- end-to-end frame age;
- CPU and memory use;
- whether `PiCameraSource` ever fails to deliver frames while running.
