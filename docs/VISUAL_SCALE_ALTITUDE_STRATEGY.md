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
