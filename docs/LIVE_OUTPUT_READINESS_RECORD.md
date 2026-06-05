# Live Output Readiness Record

This record tracks the clean Pi dry-run evidence required by `docs/LIVE_OUTPUT_SAFETY_PLAN.md`.

Live MAVLink output remains blocked. This record is evidence tracking only.

## Current Count

- Clean readiness logs accepted: 2/3.
- Required before changing any live-output blocker: 3/3.
- Old physical route: dismantled; do not rebuild it only to chase the remaining logs.
- Remaining logs should be collected on the next stable route or bench stand.

## Accepted Logs

| Count | Pi log | Audit log | Checker result | Notes |
| --- | --- | --- | --- | --- |
| 1/3 | `artifacts/logs/test-core-pi-20260604T205416Z.log` | n/a | `passed=true live_output_gate_block_reasons=vehicle_not_armed:150` | Validated with `scripts/check-live-readiness-log.sh`; captured 150/150 valid matches, telemetry health, dry-run quality, and expected live-output blocking. |
| 2/3 | `artifacts/logs/test-core-pi-20260605T194839Z.log` | `artifacts/logs/live-output-session-audit-20260605T194839Z.log` | `passed=true live_output_gate_block_reasons=vehicle_not_armed:150 session_audit=true audit_commands=150` | Recorded after the new route in `artifacts/logs/test-core-pi-20260605T194631Z.log`; match captured 150/150 valid matches, endpoint/progress gates, live telemetry health, dry-run quality, and a real non-live session audit with all commands blocked by `vehicle_not_armed`. Route distinctiveness warned on the source route, so keep this as readiness evidence only, not route-quality approval. |

## Pending Evidence

- 1/3 clean readiness log remains pending.
- Collect them only when a comparable stable route or repeatable no-yaw bench stand exists.
- Each future run log must pass `scripts/check-live-readiness-log.sh`.
- When `VISUAL_HOMING_LIVE_ROUTE_SESSION_AUDIT=1` is enabled, the audit artifact must also pass `scripts/check-live-session-audit-log.sh`.

## Non-Goals

- Do not recreate the dismantled route just for count completion.
- Do not treat this 2/3 record as permission to implement or enable live MAVLink output.
- Do not start Milestone 7 until the safety plan and 3/3 readiness evidence are complete and reviewed.
