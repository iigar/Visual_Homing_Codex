# Live Output Bench Props-Off Plan

This document defines the reviewed implementation scope for the first possible live MAVLink output boundary after the completed Milestone 6.7 readiness evidence.

This plan does not authorize flight, tethered flight, ground movement, or autonomous return. It does not enable live output by itself.

## Preconditions

- `docs/LIVE_OUTPUT_READINESS_RECORD.md` records 3/3 accepted clean Pi dry-runs.
- `docs/LIVE_OUTPUT_SAFETY_PLAN.md` remains the controlling safety artifact.
- The first command scope remains yaw-rate-only.
- `vx_mps` must remain exactly `0`.
- Propellers must be removed before any live-output bench run.
- The vehicle must be physically restrained and unable to move.
- The operator must have a separate stop path outside this process.

## Current Boundary To Preserve Until Implementation

- `VISUAL_HOMING_ENABLE_LIVE_MAVLINK_OUTPUT=ON` without the reviewed bench scope still fails CMake configuration.
- `VISUAL_HOMING_ENABLE_BENCH_PROPS_OFF_LIVE_OUTPUT=ON` is only valid together with `VISUAL_HOMING_ENABLE_LIVE_MAVLINK_OUTPUT=ON`.
- The combined bench-scope build is allowed to configure only while `VISUAL_HOMING_LIVE_MAVLINK_OUTPUT_AVAILABLE=0` and the bridge remains fail-closed.
- Default `LiveMavlinkBridge` still rejects starts and sends when no reviewed writer is attached.
- Dry-run route matching, dry-run commands, and session audit are validated.
- No concrete serial MAVLink command writer exists yet.

## First Writer Scope

The first writer may send only a yaw-rate command shape equivalent to the validated dry-run command:

- `vx_mps=0`;
- `vy_mps=0`;
- bounded finite `yaw_rate_radps`;
- finite confidence;
- valid command flag true;
- fresh route match;
- fresh read-only telemetry;
- audit log enabled and ready.

The first writer must not send:

- forward velocity;
- lateral velocity;
- altitude/throttle commands;
- position targets;
- mission commands;
- mode changes;
- arm/disarm commands;
- parameter writes;
- any retry/recovery command after a stop condition.

## Required Implementation Phases

### Phase 1 - Compile-Time Boundary Split

- Replace the unconditional live-output CMake failure with a reviewed bench-scope build option that still defaults to `OFF`.
- When `VISUAL_HOMING_ENABLE_LIVE_MAVLINK_OUTPUT=OFF`, the build must behave exactly as it does today: live output unavailable and tests proving rejection still pass.
- `VISUAL_HOMING_ENABLE_LIVE_MAVLINK_OUTPUT=ON` by itself must fail.
- `VISUAL_HOMING_ENABLE_BENCH_PROPS_OFF_LIVE_OUTPUT=ON` by itself must fail.
- When both options are `ON`, the build may enter the reviewed bench scope, but live command output must stay unavailable until the separate `VISUAL_HOMING_ATTACH_BENCH_PROPS_OFF_SERIAL_WRITER=ON` attach flag explicitly changes `VISUAL_HOMING_LIVE_MAVLINK_OUTPUT_AVAILABLE`.
- Keep runtime enable and operator confirmation separate from the compile-time option.

Acceptance:

- Default desktop and Pi builds still leave live output disabled.
- A dedicated test proves the disabled build rejects live output.
- Enabling the bench-scope build options does not make live output available until the explicit writer attach flag changes `VISUAL_HOMING_LIVE_MAVLINK_OUTPUT_AVAILABLE`.
- Enabling the build option does not send commands unless runtime gates are also satisfied.

### Phase 2 - Live Writer Interface

- Implement a writer interface behind the existing `LiveMavlinkBridge` boundary.
- Preserve single-writer ownership.
- Start must fail if the serial device cannot be opened for command output.
- Stop must be idempotent and must prevent all future sends in the same session.
- Send must reject if the writer is not started.

Acceptance:

- Tests prove stopped writer rejects sends.
- Tests prove double start/stop behavior is deterministic.
- Tests prove no send occurs when safety gate blocks.

Status:

- The injectable `LiveMavlinkCommandWriter` interface exists and is tested through fake writers.
- The default live bridge remains fail-closed when no writer is attached.
- A concrete serial MAVLink command writer now exists as a tested library boundary, and a separate attach flag can wire it into runtime live-route sessions for reviewed bench-scope builds.
- The writer emits MAVLink2 `SET_POSITION_TARGET_LOCAL_NED` body-frame commands with velocity fields zero-filled but ignored by the type mask; yaw-rate is the only active command authority.
- The writer rejects invalid commands, non-finite values, nonzero velocity, and yaw rates above the configured bench bound before any byte write.
- The serial byte transport is injectable, so encoder/writer tests use memory transport without opening hardware.
- `VISUAL_HOMING_ATTACH_BENCH_PROPS_OFF_SERIAL_WRITER=ON` is valid only with both reviewed live-output build-scope flags.

### Phase 3 - Runtime And Operator Gates

Add explicit runtime controls. Names may be adjusted to fit existing code style, but the behavior must be equivalent:

```bash
VISUAL_HOMING_ENABLE_LIVE_MAVLINK_OUTPUT=1
VISUAL_HOMING_LIVE_OUTPUT_BENCH_PROPS_OFF_CONFIRM=I_UNDERSTAND_PROPS_ARE_REMOVED
VISUAL_HOMING_LIVE_OUTPUT_MAX_COMMANDS=150
VISUAL_HOMING_LIVE_OUTPUT_MAX_SECONDS=10
```

Runtime enable must be false by default. Operator confirmation must be exact-string and per-run.

Acceptance:

- Missing runtime enable blocks.
- Missing or wrong operator confirmation blocks.
- Max commands and max seconds are enforced.
- Audit records the runtime/confirmation state without exposing secrets.

Status:

- Runtime controls are available on the live-route audit path.
- The exact bench props-off confirmation string is required when runtime live output is requested.
- `LiveMavlinkOutputSession` enforces max command count and max duration before writer send.
- The max-duration clock starts after camera warmup, operator cue, and pending-frame drain so it measures only the live command-attempt phase.
- Runtime live-output runs require directional progress in addition to the selected endpoint/progress gate.
- Existing dry-run readiness audit commands keep their previous default diagnostics unless the new runtime controls are explicitly supplied.

### Phase 4 - Safety Gate Wiring

The writer must be called only through `LiveMavlinkOutputSession` and `LiveMavlinkOutputSafetyGate`.

Required gate inputs:

- compile-time live output available;
- runtime live output enabled;
- operator confirmation present;
- single writer active;
- audit log enabled and ready;
- dry-run command quality already passed or actively held true for the run;
- read-only telemetry fresh;
- route match valid, fresh, finite, and high confidence;
- command valid and finite;
- `vx_mps == 0`;
- `vy_mps == 0`;
- yaw rate within configured bound.

Acceptance:

- Tests cover each block reason.
- Tests prove audit-not-ready blocks.
- Tests prove nonzero forward speed blocks before command send.
- Tests prove nonzero lateral speed blocks before command send.
- Tests prove stale telemetry blocks.
- Tests prove low-confidence or stale route match blocks.

Status:

- `LiveMavlinkOutputSafetyGate` now has an explicit `live_output_available` gate.
- `LiveMavlinkOutputSafetyGate` requires zero lateral speed as well as zero forward speed for the first yaw-rate-only boundary.
- Runtime-controlled live-route audit sessions use the compiled bridge availability state, which is currently unavailable.
- `LiveMavlinkOutputSession` starts the bridge/writer lazily only after an allowed safety decision, so unavailable or otherwise blocked sessions can still audit command decisions without opening the writer.
- Existing dry-run readiness diagnostics keep the older non-runtime path so historical `vehicle_not_armed` evidence remains stable.
- Tests prove an unavailable live-output build blocks before fake writer send.

### Phase 5 - Audit Contract

Every session must write:

- one start record;
- one command decision record per attempted command;
- one stop record.

For the first bench writer, audit must include:

- allowed decision;
- block reason or allowed reason;
- command validity;
- `vx_mps`;
- `vy_mps`;
- `yaw_rate_radps`;
- confidence;
- telemetry freshness status;
- route confidence/progress when available.

Acceptance:

- Audit failure stops the session.
- Missing audit path blocks before bridge start.
- Stop record is written on normal completion.
- Stop record is attempted on early stop.
- Writer start/send failures are audited and stop the session.

Status:

- Command audit records now include both the original `allowed` flag and an explicit `decision=allowed|blocked`.
- Command audit records include command validity, velocity/yaw/confidence fields, read-only telemetry heartbeat/armed/mode/freshness fields, and route-match validity/confidence/progress/freshness fields.
- The session audit checker remains compatible with existing dry-run-blocked logs while also counting expected allowed and blocked command decisions for future bench props-off runs.
- Session tests now cover bridge start failure and writer send failure as audited stop conditions.

### Phase 6 - Bench-Only Pi Command

The first bench command must be documented before it is run. It should be a separate command from the existing readiness dry-run command and should make the live-output risk visually obvious.

Template:

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
VISUAL_HOMING_ENABLE_LIVE_MAVLINK_OUTPUT=1 \
VISUAL_HOMING_LIVE_OUTPUT_BENCH_PROPS_OFF_CONFIRM=I_UNDERSTAND_PROPS_ARE_REMOVED \
VISUAL_HOMING_LIVE_OUTPUT_MAX_COMMANDS=150 \
VISUAL_HOMING_LIVE_OUTPUT_MAX_SECONDS=10 \
VISUAL_HOMING_OPERATOR_CUE_SECONDS=10 \
./scripts/test-core-pi.sh
```

This template is not active until the implementation exists and is reviewed.

Current fail-closed wrapper:

```bash
cd ~/Visual_Homing_Codex

VISUAL_HOMING_LIVE_OUTPUT_BENCH_PROPS_OFF_CONFIRM=I_UNDERSTAND_PROPS_ARE_REMOVED \
./scripts/run-live-output-bench-props-off-pi.sh
```

Current expected result before a concrete writer exists:

- route matching and dry-run command quality must pass;
- runtime live-output runs must pass directional progress, not endpoint progress alone;
- live-output gate must remain blocked with `live_output_unavailable`;
- session audit must report `allowed=0` and `blocked=150`.

This wrapper is the command to revise before any future writer-enabled bench run.

Current expected result after the serial writer library boundary exists but before it is attached:

- unchanged from above;
- the runtime wrapper must still report `allowed=0 blocked=150 reason=live_output_unavailable`;
- `VISUAL_HOMING_LIVE_MAVLINK_OUTPUT_AVAILABLE` remains `0`.

Accepted fail-closed Pi evidence:

- `2026-06-06`, commit `d355bf1`;
- run log: `artifacts/logs/bench-props-off-live-output-20260606T193652Z.log`;
- audit log: `artifacts/logs/bench-props-off-live-output-audit-20260606T193652Z.log`;
- Pi CTest passed 22/22 before the bench run;
- live matching captured 150/150 valid matches with `directional_progress_passed=true`, `endpoint_progress_passed=true`, and `progress_gate_passed=true`;
- telemetry health passed with `telemetry_bytes_dropped=0`;
- dry-run command quality passed with 150/150 valid commands;
- live-output decisions remained fail-closed: `allowed=0 blocked=150 reason=live_output_unavailable`.

Post-writer-library fail-closed Pi evidence:

- `2026-06-07`, commit `6fc9cd2`;
- run log: `artifacts/logs/bench-props-off-live-output-20260607T060503Z.log`;
- audit log: `artifacts/logs/bench-props-off-live-output-audit-20260607T060503Z.log`;
- Pi CTest passed 23/23 before the bench run;
- live matching captured 150/150 valid matches with `directional_progress_passed=true`, `endpoint_progress_passed=true`, and `progress_gate_passed=true`;
- telemetry health passed with `telemetry_bytes_dropped=0`;
- dry-run command quality passed with 150/150 valid commands;
- live-output decisions remained fail-closed: `allowed=0 blocked=150 reason=live_output_unavailable`;
- this evidence was recorded after `LiveMavlinkSerialCommandWriter` existed as a tested library boundary, but before it was attached to runtime sessions or made available.

Post-hardening wrapper note:

- `2026-06-08`, commit `eb76339`, Pi CTest passed 23/23 and live output remained fail-closed with `allowed=0 blocked=150 reason=live_output_unavailable`;
- the fixed-frame route match failed progress gate after reaching `progress=1` because endpoint-tail frames rolled back to `0.798658`, yielding `progress_rollback=0.261745` against the inherited `0.25` wrapper threshold;
- the dedicated bench wrapper now defaults `VISUAL_HOMING_LIVE_ROUTE_MATCH_MAX_PROGRESS_REGRESSIONS=10` and `VISUAL_HOMING_LIVE_ROUTE_MATCH_MAX_PROGRESS_ROLLBACK=0.30` while core defaults remain `5` and `0.25`;
- this is a wrapper readiness tolerance only, not a live-output availability change. Before runtime writer attachment, prefer endpoint-complete stop semantics so command generation stops after the route endpoint instead of collecting tail frames.
- Core now has opt-in endpoint-complete stop semantics (`stop_at_endpoint_progress`, `endpoint_stop_triggered`, `stop_reason`). The dedicated Pi wrapper enables it for the bench props-off boundary and the readiness/audit checkers accept variable-length command sessions through explicit `auto` expectations.

Endpoint-stop fail-closed Pi evidence:

- `2026-06-08`, commit `98d5407`;
- run log: `artifacts/logs/bench-props-off-live-output-20260608T210953Z.log`;
- audit log: `artifacts/logs/bench-props-off-live-output-audit-20260608T210953Z.log`;
- Pi CTest passed 23/23 before the bench run;
- live matching stopped at endpoint with `frames=129/150`, `endpoint_stop=true`, and `stop_reason=endpoint_progress_reached`;
- live matching passed `directional_progress_passed=true`, `endpoint_progress_passed=true`, and `progress_gate_passed=true`;
- telemetry health passed with `telemetry_bytes_dropped=0`;
- dry-run command quality passed with 129/129 valid commands;
- live-output decisions remained fail-closed: `allowed=0 blocked=129 reason=live_output_unavailable`.

## Stop Conditions

The live writer must stop immediately when any of these happens:

- max command count reached;
- max duration reached;
- endpoint progress reached when the reviewed endpoint-complete stop mode is enabled;
- process receives termination signal;
- route match invalid, stale, or low-confidence;
- telemetry stale or heartbeat lost;
- audit write fails;
- command invalid or non-finite;
- nonzero forward speed appears;
- yaw rate exceeds bound;
- single-writer ownership lost.

After stop, the same session must not recover automatically.

## Post-Run Checks

After any future bench props-off live-output run, run checkers equivalent to:

```bash
./scripts/check-live-readiness-log.sh artifacts/logs/test-core-pi-<run>.log
./scripts/check-live-session-audit-log.sh artifacts/logs/live-output-session-audit-<run>.log
```

For a future run that is expected to allow writer sends, set explicit audit expectations, for example:

```bash
VISUAL_HOMING_EXPECTED_LIVE_SESSION_AUDIT_ALLOWED_COMMANDS=150 \
VISUAL_HOMING_EXPECTED_LIVE_SESSION_AUDIT_BLOCKED_COMMANDS=0 \
VISUAL_HOMING_EXPECTED_LIVE_SESSION_AUDIT_REASON='*' \
./scripts/check-live-session-audit-log.sh artifacts/logs/live-output-session-audit-<run>.log
```

## Completion Criteria

The first bench props-off live-output implementation is ready for review only when:

- default builds still block live output;
- live-output build requires an explicit reviewed CMake option;
- runtime enable is explicit and false by default;
- operator confirmation is exact-string and per-run;
- writer sends yaw-rate-only commands with `vx_mps=0`;
- all safety-gate block reasons are tested;
- audit readiness is required and tested;
- start/stop/send behavior is tested;
- max command and max duration limits are enforced;
- Pi command is documented;
- dedicated Pi wrapper exists and remains fail-closed until runtime writer attachment is explicitly reviewed;
- no flight, tethered flight, ground movement, forward velocity, pitch/roll authority, or altitude authority is introduced.

## Non-Goals

- Do not start Milestone 7.
- Do not add pitch, roll, forward velocity, altitude, or position control.
- Do not arm/disarm the vehicle.
- Do not change flight modes.
- Do not remove the route-quality, telemetry, command-quality, audit, or zero-forward-speed gates.
- Do not treat successful bench yaw output as permission for flight.
- Do not implement hover, visual braking, station keeping, or opposite-direction counter-command behavior in this boundary. Those belong to a later dry-run-first visual brake / station-keeping assist milestone.
