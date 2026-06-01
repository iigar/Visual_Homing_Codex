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

The inspector parses MAVLink v1/v2 framing and currently extracts `HEARTBEAT`, `ATTITUDE`, and `GLOBAL_POSITION_INT` payloads for heartbeat presence, armed state, coarse ArduPilot mode, roll/pitch/yaw, and relative altitude. It does not validate MAVLink CRC yet and does not open a live device; it is a read-only diagnostic layer before adding serial transport.

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
VISUAL_HOMING_ROUTE_WARMUP_FRAMES=3
VISUAL_HOMING_ROUTE_ALTITUDE_M=0.0
VISUAL_HOMING_ROUTE_HEADING_HINT_RAD=0.0
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

The underlying CLI is:

```bash
./core/build-pi/visual_homing_core --record-live-route <camera_width> <camera_height> <fps> <frames> <route.vhrs> <target_width> <target_height> <altitude_m> [heading_hint_rad [warmup_frames]]
./core/build-pi/visual_homing_core --record-live-route-profile <camera.profile> <fps> <frames> <route.vhrs> <altitude_m> [heading_hint_rad [warmup_frames]]
./core/build-pi/visual_homing_core --record-live-route-active-profile <profile_dir> <active_profile_state> <fps> <frames> <route.vhrs> <altitude_m> [heading_hint_rad [warmup_frames]]
```

When live route recording succeeds through `scripts/test-core-pi.sh`, the script automatically inspects the written file with `--inspect-route`.

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

## Live Route Matching

Match live camera frames against an existing route artifact without overwriting the route:

```bash
VISUAL_HOMING_MATCH_LIVE_ROUTE=1 VISUAL_HOMING_USE_ACTIVE_CAMERA_PROFILE=1 ./scripts/test-core-pi.sh
```

The underlying CLI is:

```bash
./core/build-pi/visual_homing_core --match-live-route-active-profile <profile_dir> <active_profile_state> <fps> <frames> <route.vhrs> <warmup_frames> <window_radius> <minimum_confidence> <max_direction_shift_px>
```

The Pi script reads `VISUAL_HOMING_ROUTE_OUTPUT` and does not write a new `VHRS` artifact. It logs each `live_route_match_frame` with route index, progress, confidence, validity, and FOV-derived direction error. The final `live_route_match_done` line reports captured frames, valid matches, progress regressions, confidence summary, monotonic progress, effective FPS, and `passed`.

Optional controls:

```bash
VISUAL_HOMING_LIVE_ROUTE_MATCH_WINDOW_RADIUS=30
VISUAL_HOMING_LIVE_ROUTE_MATCH_MIN_CONFIDENCE=0.75
VISUAL_HOMING_LIVE_ROUTE_MATCH_MAX_DIRECTION_SHIFT_PX=4
```

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
