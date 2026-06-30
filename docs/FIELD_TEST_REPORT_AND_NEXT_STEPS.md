# Field Test Report And Next Steps

This document summarizes the Visual Homing field tests completed so far, the conclusions we can safely draw from them, and the recommended commands for the next field session.

Ukrainian version: [`FIELD_TEST_REPORT_AND_NEXT_STEPS_UA.md`](FIELD_TEST_REPORT_AND_NEXT_STEPS_UA.md).

## Current Status

The current proven baseline is a hand-carried Visual Homing return dry-run using:

- Raspberry Pi Zero 2W
- Pi camera active profile
- `96x72` route signatures
- live MAVLink telemetry read-only
- dry-run navigation commands only
- no live MAVLink command output
- endpoint-stop enabled for timing tests
- visual scale diagnostics enabled but not used as a readiness gate

The accepted route artifact used for the latest baseline is:

```text
/home/pi/Visual_Homing_Codex/artifacts/field_routes/field-route-20260624T210459Z.vhrs
```

This route is useful as an indoor/local baseline, but the next outdoor/field session should record a fresh route at the test location.

## What Has Been Proven

- `96x72` route matching runs at about `15 FPS` on the Pi.
- The route matcher can complete reverse route recognition and reach endpoint readiness.
- The tracked route-progress model handles hand-carried jitter better than raw progress.
- Endpoint-stop works: dry-runs stop when endpoint progress is reached instead of always consuming all `150` frames.
- External-nav readiness JSON and log checks work for full-frame and endpoint-stop sessions.
- Operator summaries are now available as short text output, suitable as a prototype for future Android UI cards.
- Live output remains blocked in the current tests, normally with `vehicle_not_armed:<frames>`.

## What Has Not Been Proven Yet

- Real flight return.
- Real MAVLink command output to the flight controller.
- Flight-controller acceptance of ExternalNav/position-provider data.
- JT_Zero runtime handoff.
- Visual scale as a safety or handoff gate.
- Long-distance or high-altitude behavior, such as `20 m` vs `200 m`.

## Important Conclusions

### Route Readiness

The latest route-completion tests show stable readiness at two hand-carried height ranges:

```text
High hand-carried baseline:
  altitude avg: about 1.75..1.85 m
  endpoint time: 7.053..7.584 s
  readiness: READY

Low hand-carried baseline:
  altitude avg: about 0.84..0.90 m
  endpoint time: 8.318..8.582 s
  readiness: READY
```

One blocked run in the series had healthy FPS, confidence, telemetry, dry-run commands, and altitude, but did not reach endpoint. Its tracked route delta was much smaller than the successful runs. This is interpreted as an incomplete physical reverse traversal, not as a matcher or Pi-performance failure.

### Scale Diagnostics

Visual scale diagnostics move in the expected general direction:

- lower hand-carried height produced higher average scale ratios;
- higher hand-carried height produced lower average scale ratios.

However, the histogram remains broad and often hits candidate bounds such as `0.3` or `1.5`. Therefore:

- keep `VISUAL_HOMING_VISUAL_SCALE_DIAGNOSTICS=1` for evidence;
- keep `VISUAL_HOMING_SCALE_REFINEMENT=0` for normal field tests;
- do not use visual scale as a readiness gate yet.

### Optical Flow

Current Visual Homing does not use classical optical flow for navigation.

No Lucas-Kanade, Farneback, KLT, ORB-flow, or dense optical-flow method is currently in the control path. The current visual logic is route matching based on `Gray8RouteMatcher`:

- normalized mean absolute difference over Gray8 route entries;
- horizontal shift search for direction/yaw error;
- temporal tracked progress;
- optional visual-scale diagnostics.

After the field aliasing runs, log-only top-k candidate diagnostics were added. This is not ORB yet, but it is the preparation layer for ORB: `VISUAL_HOMING_LIVE_ROUTE_MATCH_TOP_K_DIAGNOSTICS=1` reports the best Gray8 candidates as `route_index:progress:confidence` plus `top_match_gap`. Future ORB/refinement should check these top-k candidates instead of brute-forcing the whole route.

MAVLink optical-flow messages can be inspected if present, but they are not currently used by Visual Homing.

## Existing Commands Used So Far

Run all commands from the Pi repository root:

```bash
cd ~/Visual_Homing_Codex
```

### Update Pi Code

```bash
git pull
```

Pulls the latest committed code, scripts, and documentation from GitHub.

### Record A Field Route

Preferred route-recording command for the next field session:

```bash
VISUAL_HOMING_CAMERA_TARGET_WIDTH=96 \
VISUAL_HOMING_CAMERA_TARGET_HEIGHT=72 \
VISUAL_HOMING_CAMERA_FRAMES=150 \
VISUAL_HOMING_FIELD_ROUTE_USE_LIVE_TELEMETRY=1 \
./scripts/run-field-route-record-pi.sh
```

What it does:

- records a new `.vhrs` route artifact;
- exports keyframes;
- inspects route format and dimensions;
- runs self-match;
- runs perturbation checks;
- runs route distinctiveness diagnostics;
- writes a route-quality log.

Accept the route only if the wrapper prints `route_quality_log_check passed=true`. If quality fails, inspect keyframes and record again.

### External-Nav Reverse Dry-Run With Endpoint Stop

Use this after recording a new accepted route. Replace `<route.vhrs>` and the altitude fields.

```bash
VISUAL_HOMING_ROUTE_OUTPUT=<route.vhrs> \
VISUAL_HOMING_EXTERNAL_NAV_ALTITUDE_PRESET=custom \
VISUAL_HOMING_EXTERNAL_NAV_EXPECTED_RELATIVE_ALTITUDE_M=<expected-height-m> \
VISUAL_HOMING_EXTERNAL_NAV_EXPECTED_RELATIVE_ALTITUDE_TOLERANCE_M=<tolerance-m> \
VISUAL_HOMING_EXTERNAL_NAV_MAX_RELATIVE_ALTITUDE_SPAN_M=0 \
VISUAL_HOMING_CAMERA_TARGET_WIDTH=96 \
VISUAL_HOMING_CAMERA_TARGET_HEIGHT=72 \
VISUAL_HOMING_LIVE_ROUTE_MATCH_EXPECTED_PROGRESS=reverse \
VISUAL_HOMING_LIVE_ROUTE_MATCH_ENDPOINT_END_PROGRESS=0.75 \
VISUAL_HOMING_LIVE_ROUTE_MATCH_STOP_AT_ENDPOINT_PROGRESS=1 \
VISUAL_HOMING_VISUAL_SCALE_DIAGNOSTICS=1 \
VISUAL_HOMING_SCALE_REFINEMENT=0 \
VISUAL_HOMING_VISUAL_SCALE_REFERENCE_ALTITUDE_M=<record-reference-height-m> \
VISUAL_HOMING_HANDOFF_REQUESTED_DISTANCE_M=3 \
VISUAL_HOMING_HANDOFF_REQUESTED_ALTITUDE_M=<expected-height-m> \
./scripts/run-external-nav-dry-run-pi.sh
```

What it does:

- captures read-only MAVLink telemetry preflight;
- checks relative altitude sanity;
- runs live camera route matching;
- emits dry-run navigation commands only;
- emits external-nav readiness estimates;
- stops early if endpoint is reached;
- exports readiness JSON;
- checks readiness log.

Expected safe result for current dry-run work:

```text
external_nav_readiness_log_check passed=true
external_nav_operator_readiness=ready
endpoint_stop=true
stop_reason=endpoint_progress_reached
live_output_gate_block_reasons=vehicle_not_armed:<frames>
```

### Known Baseline Route Commands

These commands use the already accepted `96x72` route:

```bash
VISUAL_HOMING_ROUTE_OUTPUT=/home/pi/Visual_Homing_Codex/artifacts/field_routes/field-route-20260624T210459Z.vhrs \
VISUAL_HOMING_EXTERNAL_NAV_ALTITUDE_PRESET=custom \
VISUAL_HOMING_EXTERNAL_NAV_EXPECTED_RELATIVE_ALTITUDE_M=1.8 \
VISUAL_HOMING_EXTERNAL_NAV_EXPECTED_RELATIVE_ALTITUDE_TOLERANCE_M=0.45 \
VISUAL_HOMING_EXTERNAL_NAV_MAX_RELATIVE_ALTITUDE_SPAN_M=0 \
VISUAL_HOMING_CAMERA_TARGET_WIDTH=96 \
VISUAL_HOMING_CAMERA_TARGET_HEIGHT=72 \
VISUAL_HOMING_LIVE_ROUTE_MATCH_EXPECTED_PROGRESS=reverse \
VISUAL_HOMING_LIVE_ROUTE_MATCH_ENDPOINT_END_PROGRESS=0.75 \
VISUAL_HOMING_LIVE_ROUTE_MATCH_STOP_AT_ENDPOINT_PROGRESS=1 \
VISUAL_HOMING_VISUAL_SCALE_DIAGNOSTICS=1 \
VISUAL_HOMING_SCALE_REFINEMENT=0 \
VISUAL_HOMING_VISUAL_SCALE_REFERENCE_ALTITUDE_M=0.75 \
VISUAL_HOMING_HANDOFF_REQUESTED_DISTANCE_M=3 \
VISUAL_HOMING_HANDOFF_REQUESTED_ALTITUDE_M=1.8 \
./scripts/run-external-nav-dry-run-pi.sh
```

Use this only for local repeatability checks. For the next outdoor session, record a fresh route first.

### Engineering Summary Table

```bash
./scripts/summarize-external-nav-runs.sh \
  <external-nav-dry-run-1.log> \
  <external-nav-dry-run-2.log>
```

What it does:

- prints a TSV table;
- includes readiness, FPS, elapsed time, endpoint time, confidence, altitude, route progress, tracked progress, dry-run validity, and scale diagnostics.

Use this for comparing multiple runs.

### Operator Summary

```bash
./scripts/operator-readiness-summary.sh \
  <external-nav-dry-run-1.log> \
  <external-nav-dry-run-2.log>
```

What it does:

- prints a short human-readable summary;
- acts as a text prototype for future Android readiness UI.

Example:

```text
READY | route complete | endpoint 7.05s | alt 1.84525m | FPS 15.03 | conf 0.880073/0.913902 | reason valid
detail | frames 106/150 | tracked_delta 0.62 | endpoint_stop true | stop endpoint_progress_reached | log ...
```

## Next Field Session Plan

### Goal

Record a fresh outdoor route and prove reverse Visual Homing readiness on that route without live command output.

### Physical Setup

- Use a safe open location.
- Keep props removed if the drone is powered in a way that could ever allow arming.
- Do not arm for this stage.
- Keep the camera mounted as it would be in real operation.
- Prefer stable hand-carried motion with no abrupt yaw turns.

### Step 1 - Record New Route

```bash
cd ~/Visual_Homing_Codex
git pull

VISUAL_HOMING_CAMERA_TARGET_WIDTH=96 \
VISUAL_HOMING_CAMERA_TARGET_HEIGHT=72 \
VISUAL_HOMING_CAMERA_FRAMES=150 \
VISUAL_HOMING_FIELD_ROUTE_USE_LIVE_TELEMETRY=1 \
./scripts/run-field-route-record-pi.sh
```

Target route:

- short first: about `10..15 m`;
- repeatable straight or gently curved path;
- avoid featureless ground/walls if possible;
- keep height approximately stable.

If route quality fails, record again immediately.

### Step 2 - Inspect Keyframes

Check the printed keyframe directory. Confirm:

- start and end look different enough;
- no accidental pause dominates the route;
- no severe blur;
- route direction is clear.

### Step 3 - First Reverse Endpoint-Stop Dry-Run

Use the new route path printed by the record command:

```bash
VISUAL_HOMING_ROUTE_OUTPUT=<new-route.vhrs> \
VISUAL_HOMING_EXTERNAL_NAV_ALTITUDE_PRESET=custom \
VISUAL_HOMING_EXTERNAL_NAV_EXPECTED_RELATIVE_ALTITUDE_M=<measured-height-m> \
VISUAL_HOMING_EXTERNAL_NAV_EXPECTED_RELATIVE_ALTITUDE_TOLERANCE_M=0.45 \
VISUAL_HOMING_EXTERNAL_NAV_MAX_RELATIVE_ALTITUDE_SPAN_M=0 \
VISUAL_HOMING_CAMERA_TARGET_WIDTH=96 \
VISUAL_HOMING_CAMERA_TARGET_HEIGHT=72 \
VISUAL_HOMING_LIVE_ROUTE_MATCH_EXPECTED_PROGRESS=reverse \
VISUAL_HOMING_LIVE_ROUTE_MATCH_ENDPOINT_END_PROGRESS=0.75 \
VISUAL_HOMING_LIVE_ROUTE_MATCH_STOP_AT_ENDPOINT_PROGRESS=1 \
VISUAL_HOMING_VISUAL_SCALE_DIAGNOSTICS=1 \
VISUAL_HOMING_SCALE_REFINEMENT=0 \
VISUAL_HOMING_VISUAL_SCALE_REFERENCE_ALTITUDE_M=<record-height-m> \
VISUAL_HOMING_HANDOFF_REQUESTED_DISTANCE_M=3 \
VISUAL_HOMING_HANDOFF_REQUESTED_ALTITUDE_M=<measured-height-m> \
./scripts/run-external-nav-dry-run-pi.sh
```

Acceptance target:

- `passed=true`;
- `external_nav_operator_readiness=ready`;
- `endpoint_stop=true`;
- `stop_reason=endpoint_progress_reached`;
- `effective_fps` around `15`;
- `dry_run_valid=<frames>/<frames>`;
- live output still blocked by `vehicle_not_armed:<frames>`.

### Step 4 - Repeat

Repeat the same reverse dry-run at least two times.

Then summarize:

```bash
./scripts/operator-readiness-summary.sh <log1> <log2> <log3>

./scripts/summarize-external-nav-runs.sh <log1> <log2> <log3>
```

### Step 5 - If A Run Is Blocked

Use the summary to separate causes:

- `fps` low or `capture_timeout`: Pi performance problem.
- confidence low: visual route/matcher problem.
- altitude window false: wrong expected altitude/tolerance.
- `tracked_delta` much smaller than successful runs: likely incomplete physical traversal.
- `endpoint=false` with otherwise healthy metrics: likely did not reach the route start/end physically.
- `live_output_gate` not blocked while not armed: safety issue, stop testing.

## Props-Off Audit Rehearsal

This is not a real live-output test yet. It is the same endpoint-stop dry-run with props removed and the vehicle left unarmed, used to confirm that Visual Homing can be ready while live output remains blocked.

Use it only after recording and accepting a fresh route at the field location.

Expected result:

```text
route complete: yes
operator readiness: READY
endpoint_stop: true
live output: blocked
block reason: vehicle_not_armed:<frames>
```

Command shape:

```bash
VISUAL_HOMING_ROUTE_OUTPUT=<new-route.vhrs> \
VISUAL_HOMING_EXTERNAL_NAV_ALTITUDE_PRESET=custom \
VISUAL_HOMING_EXTERNAL_NAV_EXPECTED_RELATIVE_ALTITUDE_M=<measured-height-m> \
VISUAL_HOMING_EXTERNAL_NAV_EXPECTED_RELATIVE_ALTITUDE_TOLERANCE_M=0.45 \
VISUAL_HOMING_EXTERNAL_NAV_MAX_RELATIVE_ALTITUDE_SPAN_M=0 \
VISUAL_HOMING_CAMERA_TARGET_WIDTH=96 \
VISUAL_HOMING_CAMERA_TARGET_HEIGHT=72 \
VISUAL_HOMING_LIVE_ROUTE_MATCH_EXPECTED_PROGRESS=reverse \
VISUAL_HOMING_LIVE_ROUTE_MATCH_ENDPOINT_END_PROGRESS=0.75 \
VISUAL_HOMING_LIVE_ROUTE_MATCH_STOP_AT_ENDPOINT_PROGRESS=1 \
VISUAL_HOMING_VISUAL_SCALE_DIAGNOSTICS=1 \
VISUAL_HOMING_SCALE_REFINEMENT=0 \
VISUAL_HOMING_VISUAL_SCALE_REFERENCE_ALTITUDE_M=<record-height-m> \
VISUAL_HOMING_HANDOFF_REQUESTED_DISTANCE_M=3 \
VISUAL_HOMING_HANDOFF_REQUESTED_ALTITUDE_M=<measured-height-m> \
./scripts/run-external-nav-dry-run-pi.sh
```

Do not arm for this rehearsal. Do not enable attached live MAVLink writer. The purpose is to confirm that the route can reach `READY` while the output gate still blocks real command authority.

## Do Not Do Yet

- Do not arm for these dry-run route tests.
- Do not enable attached live MAVLink writer in the field.
- Do not use visual scale as a gate.
- Do not use old local route evidence as proof for a new outdoor location.
- Do not proceed to real flight return until ExternalNav writer, safety audit, and props-off live-output audit are separately proven.

## Recommended Next Development After Field Route Baseline

Once a fresh outdoor route has at least three `READY` endpoint-stop reverse dry-runs:

1. Add a props-off audit checklist and command set.
2. Make readiness JSON expose the same simplified operator fields used by `operator-readiness-summary.sh`.
3. Prepare a Pi-owned Android/API contract that reads readiness JSON, not raw logs.
4. Keep JT_Zero handoff as a future same-Pi module after Visual Homing route return is stable.
