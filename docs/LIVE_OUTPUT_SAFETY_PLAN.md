# Live MAVLink Output Safety Plan

This document defines the required safety readiness work before any live MAVLink command output can replace the current fail-closed boundary.

Live output is still blocked. This plan does not authorize flight, tethered tests, ground movement, or real command output.

## Current Boundary

- `VISUAL_HOMING_ENABLE_LIVE_MAVLINK_OUTPUT=ON` must continue to fail CMake configuration until this plan is complete and reviewed.
- `LiveMavlinkBridge` must continue to be unavailable and reject sends.
- `LiveMavlinkOutputSafetyGate` is the required pre-writer contract for any future live writer.
- Current validated command shape is yaw-rate only with `vx_mps=0`.
- `LiveMavlinkOutputSafetyGate` defaults to `require_zero_forward_speed=true`; any nonzero `vx_mps` is blocked with `command_forward_speed_not_zero` before the forward-speed bound is considered.

## First Future Live Test Boundary

The first possible live-output test, after this plan is complete, is:

- bench-only;
- propellers removed;
- vehicle restrained and unable to move;
- yaw-rate-only command output;
- `vx_mps=0`;
- short duration;
- operator physically present;
- audit log enabled;
- explicit runtime enable and operator confirmation required.

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

Use the readiness checker on Pi logs:

```bash
./scripts/check-live-readiness-log.sh artifacts/logs/test-core-pi-<run-1>.log artifacts/logs/test-core-pi-<run-2>.log artifacts/logs/test-core-pi-<run-3>.log
```

The default expected live-output gate block reason is `vehicle_not_armed:150`. For a future reviewed stage with a different expected reason, set `VISUAL_HOMING_EXPECTED_LIVE_OUTPUT_GATE_BLOCK_REASONS=<reason:count>`.

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
- audit logging is enabled before the writer starts;
- dry-run command quality has passed;
- MAVLink heartbeat is present and fresh;
- vehicle is armed only for the later reviewed test stage that requires it;
- vehicle mode is `Guided` only for the later reviewed test stage that requires it;
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

On stop, the writer must emit a final audit line with the stop reason and must not attempt to recover automatically in the same run.

## Implementation Readiness Checklist

Before implementing a real writer:

- Add or update tests proving the writer rejects sends while stopped.
- Add tests proving each safety gate reason blocks output.
- Add tests proving audit logging starts before any command is accepted.
- Add tests proving forward speed cannot become nonzero in the first writer scope.
- Add tests proving compile-time disabled builds still reject live output.
- Document the exact Pi command for the future bench props-off run.

## Completion Criteria For Milestone 6.7

Milestone 6.7 is complete only when:

- this safety plan is reviewed and up to date;
- roadmap and decision logs reference this plan;
- three clean Pi dry-runs are recorded;
- the first writer scope remains yaw-only with zero forward velocity;
- the first future live test remains bench-only with propellers removed;
- no live-output blocker has been changed without a follow-up reviewed implementation plan.
