# Field Dry-Run Plan

This plan defines the next project stage: prove short outdoor Visual Homing route recording and return matching without live command output.

## Current Inventory

Working and validated:

- Pi camera active-profile capture, route recording, live route matching, and `64x48` route signatures.
- `96x72` short outdoor route recording is now the preferred field baseline after `64x48` showed excessive ambiguity on the first outdoor route.
- Route artifact inspection, keyframe export, self-match, perturbation checks, and route distinctiveness diagnostics.
- Read-only MAVLink telemetry capture/inspection/streaming with heartbeat, attitude, global position, altitude, mode, armed state, and freshness metrics.
- External-nav dry-run estimates, altitude-window sanity, operator readiness, and `visual_homing.external_nav_readiness.v1` JSON.
- Candidate-only handoff contract: Visual Homing can report `handoff.candidate=true`, but real handoff remains blocked by `jt_zero_not_integrated`.
- Live output remains blocked by default and must stay blocked during field dry-run work.

Not yet proven:

- Outdoor route distinctiveness and repeatability over a real short route with progress/scale stability strong enough for live authority.
- Accepted ExternalNav writer or FC position-provider evidence.
- Allowed-send bench, real return flight, and JT_Zero runtime handoff.
- Scale-aware route matching under meaningful altitude changes.

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

`marginal` evidence is acceptable for this stage when the only issue is documented soft directional progress jitter and the route, altitude, telemetry, external-nav, and safety gates are otherwise coherent. Live authority or real handoff will require stronger progress/scale stability than hand-carried dry-run evidence.

### Accepted Field Evidence - 2026-06-25

The first accepted short outdoor dry-run evidence uses the `96x72` field route:

- route: `/home/pi/Visual_Homing_Codex/artifacts/field_routes/field-route-20260624T210459Z.vhrs`;
- route-quality log: `/home/pi/Visual_Homing_Codex/artifacts/logs/field-route-record-20260624T210459Z.log`;
- reverse dry-run log: `/home/pi/Visual_Homing_Codex/artifacts/logs/external-nav-dry-run-20260625T210832Z.log`;
- readiness JSON: `/home/pi/Visual_Homing_Codex/artifacts/logs/external-nav-dry-run-20260625T210832Z.json`.

Accepted signal:

- `external_nav_operator_readiness=ready`, `external_nav_operator_reason=valid`;
- `external_nav_valid=150/150`, `external_nav_session_ready=true`, `external_nav_strict_session_ready=true`, `external_nav_quality_ready=true`;
- route recognition stayed strong: `valid_matches=150/150`, confidence min/avg `0.838374/0.923113`;
- endpoint and route gates passed: `endpoint_passed=true`, `progress_gate_passed=true`;
- raw progress remained noisy, but the directional tracked progress passed for reverse return: `tracked_directional_progress=true`, `tracked_regressions=12/0`, `tracked_rollback=0.596757/0`;
- altitude was coherent for the configured custom window: expected `0.75 +/- 0.25 m`, observed `0.613/0.80012/0.904 m`, `external_nav_relative_altitude_window_passed=true`;
- handoff is only a candidate: JSON reports `handoff.candidate=true`, `decision=candidate_only`, `reason=jt_zero_not_integrated`;
- live output remained blocked: `vehicle_not_armed:150`.

This evidence proves a hand-carried short outdoor Visual Homing return dry-run can reach Pi-owned `READY` with temporal directional progress tracking. It does not prove live command output, ExternalNav writer acceptance, real flight return, or JT_Zero handoff.

## Scale And Jitter Notes

The controlling design note is `docs/VISUAL_SCALE_ALTITUDE_STRATEGY.md`.

For current field dry-runs:

- prefer `96x72` over `64x48`;
- do not assume full-route `320x240` or `640x480` matching is viable on Pi Zero 2W;
- record and inspect altitude windows, but treat visual scale as diagnostic until implemented;
- expect hand-carried runs to show more pitch/viewpoint jitter than a stabilized flight path;
- use `marginal` to describe soft progress jitter, not route loss.

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
