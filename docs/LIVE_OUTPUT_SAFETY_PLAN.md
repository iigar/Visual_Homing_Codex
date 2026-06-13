# Live MAVLink Output Safety Plan

This document defines the required safety readiness work before any live MAVLink command output can replace the current fail-closed boundary.

Live output is still blocked. This plan does not authorize flight, tethered tests, ground movement, or real command output.

## Current Boundary

- `VISUAL_HOMING_ENABLE_LIVE_MAVLINK_OUTPUT=ON` without the reviewed bench props-off CMake scope must continue to fail configuration.
- `VISUAL_HOMING_ENABLE_BENCH_PROPS_OFF_LIVE_OUTPUT=ON` is only valid together with `VISUAL_HOMING_ENABLE_LIVE_MAVLINK_OUTPUT=ON`.
- The combined bench-scope build may configure for implementation work only while `VISUAL_HOMING_LIVE_MAVLINK_OUTPUT_AVAILABLE=0` and the live bridge remains fail-closed.
- `LiveMavlinkBridge` must continue to be unavailable and reject sends.
- `LiveMavlinkOutputSafetyGate` is the required pre-writer contract for any future live writer.
- `LiveMavlinkOutputAuditLog` is the non-live audit boundary for future writer integration; it must be ready before the gate can allow a command.
- `LiveMavlinkOutputSession` is the non-live writer-shaped coordinator for audit readiness, safety-gate evaluation, and bridge sends; with the current blocked bridge it cannot produce live MAVLink output.
- Current validated command shape is yaw-rate only with `vx_mps=0`.
- `LiveMavlinkOutputSafetyGate` defaults to `require_zero_forward_speed=true`; any nonzero `vx_mps` is blocked with `command_forward_speed_not_zero` before the forward-speed bound is considered.
- The concrete writer shape is `SET_POSITION_TARGET_LOCAL_NED`; flight-controller acceptance depends on ArduPilot mode and configuration. Any attach-enabled bench stage must explicitly verify and log the expected mode, currently `Guided`, before an allowed writer decision can be considered valid.
- The attach-enabled bench stage must use the separate reviewed attach wrapper, not the ordinary fail-closed wrapper. The attach wrapper must require its own confirmation string, prove `attach_writer_cmake=ON` and `live_output_writer_attached=true`, and default to safety-gate blocking unless a send-enabled bench attempt explicitly overrides expected allowed/blocked counts and reasons.
- This is an operator-in-the-loop assist boundary, not an autonomous controller. ArduPilot remains responsible for stabilization, failsafe behavior, mode management, and motor mixing.
- Route artifacts and serial telemetry are not trust authorities. Malformed route data, modified route artifacts, malformed MAVLink frames, wrong sysid/compid, stale heartbeat, or unexpected mode must fail closed.

## First Future Live Test Boundary

The first possible live-output test, after this plan is complete, is:

- bench-only;
- propellers removed;
- vehicle restrained and unable to move;
- yaw-rate-only command output;
- `vx_mps=0`;
- short duration;
- operator physically present;
- audit log enabled and ready before any command can pass;
- explicit runtime enable and operator confirmation required.
- flight controller mode explicitly verified for the writer command type before any allowed decision is accepted.

The first live-output test is not:

- a flight test;
- a tethered flight;
- a ground-drive or ground-slide test;
- a forward-velocity test;
- an autonomous return test;
- permission to relax route, telemetry, or command-quality gates.

## Readiness Evidence

Before changing any compile-time or runtime live-output blocker, collect at least three clean Pi dry-runs.

Each clean run must show:

- Pi CTest passes.
- `live_route_match_compact passed=true`.
- `frames=150/150`.
- `valid_matches=150`.
- `endpoint_passed=true`.
- `progress_gate_passed=true`.
- `telemetry_health=true`.
- `telemetry_dropped=0`.
- `dry_run_quality=true`.
- `dry_run_valid=150/150`.
- `live_output_gate_allowed=0`.
- `live_output_gate_blocked=150`.
- `live_output_gate_block_reasons` is expected for the test state, such as `vehicle_not_armed:150`.

The three runs should be logged as artifact paths in `docs/LIVE_OUTPUT_READINESS_RECORD.md`.

When a new route is recorded for readiness evidence, validate the route-recording log before running the match/audit pass:

```bash
VISUAL_HOMING_EXPECTED_ROUTE_QUALITY_ENTRIES=150 \
./scripts/check-route-quality-log.sh artifacts/logs/test-core-pi-<record-run>.log
```

The route-quality checker is a prefilter only. It is not flight authorization and does not replace the live route match, dry-run command quality, telemetry health, or live-output blocking checks.

Use the readiness checker on Pi logs:

```bash
./scripts/check-live-readiness-log.sh artifacts/logs/test-core-pi-<run-1>.log artifacts/logs/test-core-pi-<run-2>.log artifacts/logs/test-core-pi-<run-3>.log
```

The default expected live-output gate block reason is `vehicle_not_armed:150`. For a future reviewed stage with a different expected reason, set `VISUAL_HOMING_EXPECTED_LIVE_OUTPUT_GATE_BLOCK_REASONS=<reason:count>`.

When `VISUAL_HOMING_LIVE_ROUTE_SESSION_AUDIT=1` is used, validate the audit artifact too:

```bash
./scripts/check-live-session-audit-log.sh artifacts/logs/live-output-session-audit-<run>.log
```

The audit checker confirms that the non-live writer-shaped session started, audited every dry-run command, blocked every command for the expected reason, and emitted the final stop record.

Current readiness evidence status:

- `docs/LIVE_OUTPUT_READINESS_RECORD.md` records 3/3 accepted clean Pi dry-runs.
- The third accepted run used a route-quality prechecked 150-entry route and produced strictly monotonic forward progress.
- This satisfies the evidence-count requirement only; live output remains blocked until a separate reviewed implementation step changes the current boundary.

## Operator Checklist

Before any future bench props-off live-output test:

- Propellers are removed.
- Vehicle is physically restrained.
- A separate RC/operator stop path is available.
- Flight controller is powered from a safe bench setup.
- Pi and FC telemetry serial link are stable.
- Active camera profile is known and logged.
- Route artifact or replay input is known and logged.
- Audit log path is known before output starts.
- Operator confirms runtime enable intentionally for that single run.
- Operator can stop the process without reaching over moving parts.

## Required Gate Conditions

Any future writer must be blocked unless all conditions are true:

- live output compiled in by an intentional reviewed CMake change;
- runtime live-output flag enabled;
- operator confirmation provided for the current run;
- single writer owns command output;
- command target system/component ids match the reviewed configuration;
- audit logging is enabled and ready before the writer starts;
- dry-run command quality has passed;
- MAVLink heartbeat is present and fresh;
- vehicle is armed only for the later reviewed test stage that requires it;
- vehicle mode is `Guided` only for the later reviewed test stage that requires it;
- the expected FC mode for `SET_POSITION_TARGET_LOCAL_NED` acceptance is documented for the reviewed stage and logged in the run;
- route match is valid, fresh, high-confidence, and finite;
- command is valid, finite, and inside yaw-rate and forward-speed bounds;
- forward speed remains exactly zero for the first writer scope.

## Stop And Failsafe Policy

The future writer must stop sending commands immediately when any required gate fails.

Stop conditions include:

- process receives termination signal;
- operator stop command is received;
- telemetry becomes stale;
- heartbeat disappears;
- mode leaves the allowed mode for the current reviewed stage;
- route match becomes invalid or low-confidence;
- command becomes invalid, non-finite, or out of bounds;
- audit log cannot be written;
- single-writer ownership is lost.
- command serial transport reports partial write, write failure, or unexpected target ownership.

On stop, the writer must emit a final audit line with the stop reason and must not attempt to recover automatically in the same run.

## Implementation Readiness Checklist

Before implementing a real writer:

- Add or update tests proving the writer rejects sends while stopped. Current non-live session tests cover stopped sessions.
- Add tests proving each safety gate reason blocks output. Current safety-gate tests cover gate reasons; current session tests cover blocked decisions not reaching the bridge.
- Add or update integration tests proving the future writer wires audit-log readiness into the safety gate before any command is accepted. Current session tests cover failed audit startup and not-ready audit state before bridge start.
- Add tests proving forward speed cannot become nonzero in the first writer scope.
- Add tests proving compile-time disabled builds still reject live output.
- Document the exact Pi command for the future bench props-off run.
- Document the expected FC mode and how the run proves the FC accepted or rejected the writer command stream without leaving the props-off bench boundary.

## Completion Criteria For Milestone 6.7

Milestone 6.7 is complete only when:

- this safety plan is reviewed and up to date;
- roadmap and decision logs reference this plan;
- three clean Pi dry-runs are recorded;
- the first writer scope remains yaw-only with zero forward velocity;
- the first future live test remains bench-only with propellers removed;
- no live-output blocker has been changed without a follow-up reviewed implementation plan.

As of the 2026-06-05 `jtzero` evidence set, the three clean dry-runs are recorded and validated. The only approved blocker change is the reviewed bench-scope compile-time split; live command output must remain unavailable until the follow-up writer phases are implemented and reviewed.

The follow-up implementation scope is defined in `docs/LIVE_OUTPUT_BENCH_PROPS_OFF_PLAN.md`.
