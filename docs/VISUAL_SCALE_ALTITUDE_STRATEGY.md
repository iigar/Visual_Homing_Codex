# Visual Scale And Altitude Strategy

This document records the project direction for altitude changes, image scale, vibration jitter, and resolution choices in Visual Homing. It is a design contract, not an implemented feature list.

## Problem

Visual Homing cannot assume that a live return frame has the same geometry as the recorded route frame.

Important differences:

- altitude changes alter image scale;
- pitch, roll, and yaw changes alter perspective;
- drone vibration adds frame-to-frame jitter even with mechanical damping;
- motion blur and rolling shutter can reduce match stability;
- hand-carried dry-runs are useful evidence, but real flight will add higher-frequency disturbances;
- increasing resolution alone does not solve scale or viewpoint mismatch.

The current matcher can recognize a short outdoor route at `96x72`, but field dry-runs show local progress jitter even when global route recognition is strong:

- high confidence;
- `valid_matches=150/150`;
- endpoint/progress gates pass;
- operator readiness may still be `marginal` because local progress jumps exceed soft directional thresholds.

This means the next quality problem is not only route texture. It is progress tracking under viewpoint and scale disturbance.

## Current Evidence

Accepted short-field evidence on `2026-06-24`:

- `64x48` outdoor route quality failed due excessive ambiguous nearest entries.
- `96x72` route quality passed and gave substantially better distinctiveness.
- Reverse dry-runs on the accepted `96x72` route repeatedly matched `150/150` frames.
- Reverse endpoint evidence reached approximately `0.97 -> 0.04` route progress.
- Altitude-window checks passed when configured to the measured hand-carried height.
- Live output stayed blocked by `vehicle_not_armed`.
- Operator readiness was `marginal` because of route directional progress jitter, not because Visual Homing lost the route.

Interpretation:

- `96x72` is a practical field baseline for Pi Zero 2W class hardware.
- The route was recognizable.
- The per-frame progress signal still needs temporal smoothing and scale/viewpoint awareness before live authority.

## Core Principle

Visual Homing should remain a coarse route-return system. It should not become metric SLAM.

However, coarse route return still needs scale-aware matching and temporally stable progress:

```text
route recognition
  + altitude/scale hints
  + temporal progress tracking
  + vibration/viewpoint jitter tolerance
  = usable return evidence
```

Resolution is only one input. It is not the main control authority.

## Altitude And Scale Model

Altitude changes affect ground footprint. If the camera is twice as high as during recording, the live image covers roughly twice the ground width/height, assuming similar attitude and terrain.

Required direction:

- store or derive route-frame altitude assumptions;
- use live relative altitude as a scale hint;
- compare live altitude against route altitude before trusting route progress for handoff or output;
- log scale mismatch explicitly instead of hiding it behind match confidence;
- treat visual-scale diagnostics as dry-run evidence first.

Future route metadata should move from a coarse `altitude_band_m` toward per-frame or per-segment altitude metadata:

```text
route frame:
  progress
  timestamp
  altitude estimate
  attitude snapshot
  camera profile
  compact image signature
```

The first implementation can be segment-level rather than perfect per-frame metadata.

## Multi-Resolution Strategy

Do not make `320x240` or `640x480` full-route brute-force matching the default Pi Zero 2W path.

Pixel cost grows quickly:

```text
64x48    =   3,072 px
96x72    =   6,912 px
160x120  =  19,200 px
320x240  =  76,800 px
640x480  = 307,200 px
```

Relative to `96x72`:

- `160x120` is about `2.8x` pixels;
- `320x240` is about `11.1x` pixels;
- `640x480` is about `44.4x` pixels.

For a route matcher that compares the live frame against many route entries, this cost directly affects latency and FPS. Latency is safety-critical; higher resolution that drops frame rate or increases control delay can be worse than a lower-resolution stable tracker.

Preferred architecture:

```text
camera frame
  -> low-res full-route coarse match: 96x72 or 128x96
  -> temporal tracker narrows candidate progress range
  -> optional higher-res refinement: 160x120, maybe 320x240 ROI/top-N only
  -> smoothed progress + scale confidence + readiness
```

`640x480` may be useful for debug, route keyframes, and offline analysis, but should not be assumed as the realtime baseline on Pi Zero 2W.

## Scale-Aware Matching

Scale-aware matching should be added in layers:

1. Altitude-window gate
   - Already partially present through expected relative altitude checks.
   - It verifies that the run is physically plausible for the route.

2. Visual-scale diagnostics
   - Existing `visual_scale_*` log fields are the right hook.
   - They should become meaningful field diagnostics, not remain mostly zero/inactive.

3. Limited multi-scale search
   - Try a small scale set around the altitude-derived expectation.
   - Example: `0.85x`, `1.0x`, `1.15x`.
   - Do this near likely progress candidates, not across the entire route at high resolution.

4. Temporal scale smoothing
   - Do not let the chosen scale jump every frame.
   - Smooth scale just like route progress.

5. Readiness semantics
   - `visual_scale_ready=false` should make handoff/live-output readiness `marginal` or `blocked`, depending on severity.
   - Android should display this as a dedicated card, but the Pi decides the threshold.

## Progress Tracking And Jitter

Current route matching is mostly frame-local. Field evidence shows that even with strong recognition, route progress can jump because of:

- vibration;
- pitch/roll/yaw changes;
- non-identical camera viewpoint;
- short route length;
- coarse matcher candidate changes.

The next matcher layer should track progress over time:

```text
raw_match_progress
  -> reject implausible jumps
  -> apply expected direction and speed envelope
  -> smooth progress
  -> preserve raw progress diagnostics
```

The tracker should not hide failures. It should log both:

- raw matcher progress/confidence;
- smoothed tracker progress/confidence/reason.

The first tracker implementation reports `tracked_progress`, `tracked_minmax_progress`, `tracked_regressions`, `tracked_rollback`, and `tracked_directional_progress` next to the raw progress fields. Operator readiness is allowed to use the tracked soft diagnostic, while raw jitter remains visible for field analysis and future tuning. The tracked signal is a directional envelope, not a replacement for raw matching: it follows the expected route direction and ignores short opposite-direction raw jumps, while raw fields continue to expose jitter for analysis.

## Android UI Implications

Android should not implement these thresholds independently.

Android should display Pi-owned fields such as:

- route recognition;
- raw progress;
- smoothed progress;
- endpoint state;
- altitude window;
- visual scale;
- telemetry health;
- operator readiness;
- handoff candidate;
- safety gate.

For UI clarity:

- `READY`: route, altitude, scale, telemetry, and safety gates are coherent;
- `MARGINAL`: route is recognized and endpoint may be reached, but progress/scale jitter needs attention;
- `BLOCKED`: a hard gate failed, such as altitude window, missing telemetry, route not recognized, or safety gate.

The UI may allow operator inputs such as altitude preset, expected handoff altitude, requested handoff distance, and selected route. The Pi must resolve those inputs into final readiness.

## Field Scale Characterization

The first field scale characterization should use the accepted short `96x72` route and repeat reverse dry-runs at roughly:

- `0.65 m`;
- `0.95 m`;
- `1.5 m`.

These heights intentionally cover a larger ratio than the current `0.75 m` accepted run while staying practical for hand-carried testing. This is not a substitute for later large-altitude testing. A `0.65..1.5 m` range is about a `2.3x` altitude change, while a future `20..200 m` scenario is a `10x` change and also changes texture density, haze, lighting, camera exposure, and field-of-view content. The short test only proves that the diagnostic channel reacts coherently to scale changes and gives a first estimate of jitter/confidence behavior.

For this stage, use field visual-scale diagnostics as log-only evidence:

- enable `VISUAL_HOMING_VISUAL_SCALE_DIAGNOSTICS=1`;
- set `VISUAL_HOMING_VISUAL_SCALE_REFERENCE_ALTITUDE_M` to the route-recording height or the best accepted reference height;
- keep `visual_scale_required=false` for readiness so a deliberate scale stress-test can be recorded without being treated as a live-authority gate failure.

Later flight-scale testing should be a separate evidence ladder after the system can fly the route safely:

- repeat the same route at progressively larger altitude deltas;
- compare route confidence, tracked progress, visual-scale ratio, and telemetry altitude;
- only then decide whether higher-resolution candidate refinement, route pyramids, or altitude-banded route records are required.

### First Field Scale Evidence - 2026-06-26

The first three hand-carried field scale runs used the accepted `96x72` route
`field-route-20260624T210459Z.vhrs`, reverse progress, and
`VISUAL_HOMING_VISUAL_SCALE_REFERENCE_ALTITUDE_M=0.75`.

Observed results:

| Expected height | Observed relative altitude min/avg/max | Operator readiness | Visual scale min/avg/max | Visual scale confidence min/avg |
| --- | --- | --- | --- | --- |
| `0.65 m` | `0.508/0.63858/0.7 m` | `ready` | `0.5/0.931667/1.25` | `0.849034/0.926032` |
| `0.95 m` | `1.014/1.14841/1.217 m` | `blocked` by altitude window | `0.5/0.890333/1.25` | `0.86822/0.925884` |
| `1.5 m` | `1.333/1.49387/1.585 m` | `ready` | `0.5/0.777667/1.25` | `0.872125/0.928726` |

Interpretation:

- route recognition stayed strong across the three heights: each run had `valid_matches=150/150`;
- tracked reverse progress stayed coherent: each run had `tracked_directional_progress=true` and zero reverse tracked regressions;
- the average visual scale ratio moved in the expected direction as height increased: about `0.93`, `0.89`, then `0.78`;
- the visual scale min/max repeatedly hit the candidate bounds `0.5` and `1.25`, so this diagnostic is not yet stable enough to be used as a metric altitude estimator or a handoff/live-output gate.

Next scale work should improve the estimator before using it in readiness:

- inspect per-frame scale distributions instead of only min/avg/max;
- add temporal smoothing or a scale tracker, similar to route progress tracking;
- consider ROI or higher-resolution refinement around the matched route keyframe;
- keep field scale diagnostics log-only until the ratio no longer repeatedly hits candidate bounds in otherwise clean route runs.

The next diagnostic output adds `visual_scale_ratio_histogram` to the compact log and readiness JSON. This should be used before changing thresholds because min/avg/max alone can hide a bimodal or bound-hitting scale estimate.

The first histogram repeat at the `1.5 m` target used log
`external-nav-dry-run-20260626T210401Z.log`. The run stayed route-ready:
`valid_matches=150/150`, `external_nav_operator_readiness=ready`,
`tracked_directional_progress=true`, altitude `1.339/1.38882/1.446 m`, and
confidence min/avg `0.853823/0.923017`. The visual-scale histogram was:

```text
0.5:110,0.7:2,0.8:2,0.9:1,0.95:8,1:6,1.05:2,1.1:5,1.15:12,1.25:2
```

This confirms that the current scale diagnostic has a useful directional signal
but saturates at the lower candidate bound for larger height deltas. The next
implementation step should widen/improve candidate search and add temporal scale
tracking before visual scale is considered for readiness.

The first follow-up implementation widens raw scale candidates to `0.30..1.50`
and adds a log-only tracked visual-scale ratio with a bounded per-frame step.
The raw histogram remains visible; the tracked ratio is intended to show the
session trend without allowing one noisy frame to redefine scale readiness.

The first repeat after widening the candidate range used log
`external-nav-dry-run-20260626T220034Z.log`. The route still matched every frame
with confidence min/avg `0.857079/0.919244`, but the route session failed endpoint
completion: progress ended at `0.657718`, `endpoint_passed=false`, and
`external_nav_operator_readiness=blocked` with `route_session_not_passed`. Scale
diagnostics became more informative:

```text
visual_scale_ratio_histogram=0.3:53,0.35:6,0.4:6,0.45:8,0.5:4,0.55:1,0.65:4,0.75:3,0.8:1,0.85:1,0.9:6,0.95:3,1:22,1.1:3,1.15:1,1.2:7,1.25:3,1.3:5,1.35:2,1.4:1,1.5:10
tracked_visual_scale_ratio=0.3..0.65
tracked_visual_scale_ratio_min_avg_max=0.3/0.658667/1.15
```

This confirms that larger-height runs can require raw scale below `0.5`, but also
that scale-only improvements do not solve route endpoint stability at this field
height. The next useful implementation should be selective higher-resolution or
ROI refinement around the matched route candidate, while preserving the fast
`96x72` coarse matcher.

The first implementation step toward refinement is a scale-aware local matcher
pass. `Gray8RouteMatcher` keeps the normal fast coarse search, then can recheck a
small neighborhood around the coarse route index with scaled Gray8 MAD
candidates. This is intentionally still cheap and uses the existing `96x72`
route artifact; it is not a replacement for a future route pyramid or real
higher-resolution candidate refinement.

Field testing on Pi Zero 2W showed that running this scale-aware refinement on
every frame with a wide radius is too expensive for the control loop. Visual
scale diagnostics and matcher refinement are therefore separate controls:
`VISUAL_HOMING_VISUAL_SCALE_DIAGNOSTICS=1` keeps the scale histogram/tracker
log-only path enabled, while `VISUAL_HOMING_SCALE_REFINEMENT=1` explicitly opts
into the heavier matcher refinement. The default refinement radius is `1` so the
feature remains a local experiment rather than a second full matcher.

## Milestone Direction

Near-term field work:

1. Keep `96x72` as the accepted outdoor baseline.
2. Optionally test `128x96` and `160x120` for FPS/latency/quality evidence.
3. Do not jump to `320x240` full-route realtime matching until coarse+refinement is designed.
4. Add progress tracker/smoother before live authority.
5. Reactivate field-useful `visual_scale_*` diagnostics.
6. Add route altitude/attitude metadata policy before depending on handoff distance/altitude.

Flight-readiness implication:

- A route dry-run can be accepted as evidence with `operator_readiness=marginal` if endpoint, confidence, altitude, telemetry, and safety are good and the only issue is documented soft progress jitter.
- A live output or real handoff decision should require stronger progress/scale stability than the current hand-carried dry-run evidence.
