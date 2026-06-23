# Field Dry-Run Plan

This plan defines the next project stage: prove short outdoor Visual Homing route recording and return matching without live command output.

## Current Inventory

Working and validated:

- Pi camera active-profile capture, route recording, live route matching, and `64x48` route signatures.
- Route artifact inspection, keyframe export, self-match, perturbation checks, and route distinctiveness diagnostics.
- Read-only MAVLink telemetry capture/inspection/streaming with heartbeat, attitude, global position, altitude, mode, armed state, and freshness metrics.
- External-nav dry-run estimates, altitude-window sanity, operator readiness, and `visual_homing.external_nav_readiness.v1` JSON.
- Candidate-only handoff contract: Visual Homing can report `handoff.candidate=true`, but real handoff remains blocked by `jt_zero_not_integrated`.
- Live output remains blocked by default and must stay blocked during field dry-run work.

Not yet proven:

- Outdoor route distinctiveness and repeatability over a real short route.
- Accepted ExternalNav writer or FC position-provider evidence.
- Allowed-send bench, real return flight, and JT_Zero runtime handoff.

## Stage 1 - Bench Baseline Freeze

- Keep the latest clean stand evidence as the baseline: `external_nav_operator_readiness=ready`, `external_nav_valid=150/150`, strict session ready, route progress `0.0805369..1`, altitude `0.497/0.524653/0.536`, and `handoff.candidate=true`.
- Do not change readiness thresholds before first outdoor dry-runs unless fixing logging/JSON correctness.
- Treat `handoff.candidate=true` as readiness to hand off only; it is not a real transfer of control.

## Stage 2 - Short Outdoor Route Recording

Use a short `5-20 m` route first. Record with:

```bash
cd ~/Visual_Homing_Codex

VISUAL_HOMING_FIELD_ROUTE_USE_LIVE_TELEMETRY=1 \
./scripts/run-field-route-record-pi.sh
```

The wrapper records a new route artifact under `artifacts/field_routes/`, exports keyframes, runs route inspection, self-match, perturbation, distinctiveness diagnostics, and then validates the log with `check-route-quality-log.sh`.

Accept a route only when:

- `route_quality_log_check passed=true`;
- route keyframes visually show a clear start and end;
- the route was recorded with no intentional yawing or pauses except the operator cue/warmup;
- the route path can be repeated manually in the same direction for dry-run matching.

## Stage 3 - Short Outdoor Return Dry-Run

Run the existing external-nav readiness wrapper against the accepted route:

```bash
cd ~/Visual_Homing_Codex

VISUAL_HOMING_ROUTE_OUTPUT=<accepted-route.vhrs> \
VISUAL_HOMING_EXTERNAL_NAV_ALTITUDE_PRESET=custom \
VISUAL_HOMING_EXTERNAL_NAV_EXPECTED_RELATIVE_ALTITUDE_M=<measured-height-m> \
VISUAL_HOMING_EXTERNAL_NAV_EXPECTED_RELATIVE_ALTITUDE_TOLERANCE_M=<tolerance-m> \
VISUAL_HOMING_HANDOFF_REQUESTED_DISTANCE_M=3 \
VISUAL_HOMING_HANDOFF_REQUESTED_ALTITUDE_M=<measured-height-m> \
./scripts/run-external-nav-dry-run-pi.sh
```

Accept dry-run evidence when:

- preflight sanity passes or any disabled altitude check is explicitly documented;
- `external_nav_readiness_log_check passed=true`;
- JSON `operator.readiness` is `ready`, or `marginal` only for a documented soft diagnostic;
- JSON `handoff.candidate=true`, `decision=candidate_only`, and `reason=jt_zero_not_integrated`;
- live output remains blocked, normally `vehicle_not_armed:<frames>`.

## Stage 4 - ExternalNav Before Command Output

After stable outdoor dry-run evidence, prioritize an ExternalNav writer path before any allowed command-output attempt:

- add a separate ExternalNav writer boundary, compile-time flag, runtime confirmation, and audit/checker path;
- prove FC accepts position/odometry provider data separately from any movement command;
- keep command output blocked during the first ExternalNav writer evidence.

## Stage 5 - Handoff Contract Before JT_Zero Runtime

JT_Zero remains a future same-Pi module. The next handoff work should extend the JSON/API contract before running JT_Zero in the control path:

- report JT_Zero process health, readiness, and reason fields;
- keep handoff policy on the Pi, not Android;
- allow a real handoff only when Visual Homing candidate, JT_Zero readiness, and live-output safety policy are all separately satisfied.
