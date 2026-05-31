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

The default active-profile state path is ignored by git:

```bash
artifacts/active_camera_profile.txt
```

Override the profile directory or active-profile state file with:

```bash
VISUAL_HOMING_CAMERA_PROFILE_DIR=/path/to/camera_profiles
VISUAL_HOMING_ACTIVE_CAMERA_PROFILE=/path/to/active_camera_profile.txt
```

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
VISUAL_HOMING_ROUTE_ALTITUDE_M=0.0
VISUAL_HOMING_ROUTE_HEADING_HINT_RAD=0.0
```

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
./core/build-pi/visual_homing_core --record-live-route <camera_width> <camera_height> <fps> <frames> <route.vhrs> <target_width> <target_height> <altitude_m> [heading_hint_rad]
./core/build-pi/visual_homing_core --record-live-route-profile <camera.profile> <fps> <frames> <route.vhrs> <altitude_m> [heading_hint_rad]
./core/build-pi/visual_homing_core --record-live-route-active-profile <profile_dir> <active_profile_state> <fps> <frames> <route.vhrs> <altitude_m> [heading_hint_rad]
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
./core/build-pi/visual_homing_core --route-distinctiveness <route.vhrs>
```

Live route recording through `scripts/test-core-pi.sh` also runs this diagnostic automatically after perturbation checks. It reports low-texture entries, exact duplicate entries, ambiguous nearest-neighbor entries, payload range, adjacent mean absolute byte difference, nearest-neighbor mean absolute byte difference, and route-quality policy fields. This is an offline diagnostic, not a flight gate; `warning=true` means the route has some repetitive or low-texture samples, while `quality_pass=false` means the current artifact should not be used as a good bench/field-test route without recapture or explicit operator review.

The diagnostic also prints small sample lists for `low_texture_frame_ids`, `exact_duplicate_frame_ids`, and `ambiguous_nearest_frame_ids`. These are the first affected route frame ids and are intended to show whether a failure came from start/end pauses, a short flat segment, or repeated visual content throughout the route.

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
