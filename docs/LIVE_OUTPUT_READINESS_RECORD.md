# Live Output Readiness Record

This record tracks the clean Pi dry-run evidence required by `docs/LIVE_OUTPUT_SAFETY_PLAN.md`.

Live MAVLink output remains blocked. This record is evidence tracking only.

## Current Count

- Clean readiness logs accepted: 3/3.
- Required before changing any live-output blocker: 3/3.
- Old physical route: dismantled; do not rebuild it only to chase the remaining logs.
- Readiness evidence count is complete; live output remains blocked pending a reviewed bench props-off implementation step.

## Accepted Logs

| Count | Pi log | Audit log | Checker result | Notes |
| --- | --- | --- | --- | --- |
| 1/3 | `artifacts/logs/test-core-pi-20260604T205416Z.log` | n/a | `passed=true live_output_gate_block_reasons=vehicle_not_armed:150` | Validated with `scripts/check-live-readiness-log.sh`; captured 150/150 valid matches, telemetry health, dry-run quality, and expected live-output blocking. |
| 2/3 | `artifacts/logs/test-core-pi-20260605T194839Z.log` | `artifacts/logs/live-output-session-audit-20260605T194839Z.log` | `passed=true live_output_gate_block_reasons=vehicle_not_armed:150 session_audit=true audit_commands=150` | Recorded after the new route in `artifacts/logs/test-core-pi-20260605T194631Z.log`; match captured 150/150 valid matches, endpoint/progress gates, live telemetry health, dry-run quality, and a real non-live session audit with all commands blocked by `vehicle_not_armed`. Route distinctiveness warned on the source route, so keep this as readiness evidence only, not route-quality approval. |
| 3/3 | `artifacts/logs/test-core-pi-20260605T203907Z.log` | `artifacts/logs/live-output-session-audit-20260605T203907Z.log` | `passed=true live_output_gate_block_reasons=vehicle_not_armed:150 session_audit=true audit_commands=150` | Recorded after route-quality precheck passed on `artifacts/logs/live-route-record-64x48-10s-current-room-20260605T203701Z.log` with `entries=150 quality_pass=true`. Match captured 150/150 valid matches, endpoint/progress gates, strict forward monotonic progress, telemetry health, dry-run quality, and a real non-live session audit with all commands blocked by `vehicle_not_armed`. |

## Evidence Status

- Required 3/3 clean readiness logs are complete.
- The third route was prechecked with `scripts/check-route-quality-log.sh`.
- The third run was validated with `scripts/check-live-readiness-log.sh`.
- The third session audit artifact was validated with `scripts/check-live-session-audit-log.sh`.
- Any future re-baselining should keep the same checker discipline.

## Bench Props-Off Fail-Closed Evidence

This is separate from the 3/3 non-live readiness count. It records the first runtime-controlled bench props-off boundary after the fail-closed wrapper and directional-progress tightening.

| Date | Commit | Pi log | Audit log | Checker result | Notes |
| --- | --- | --- | --- | --- | --- |
| 2026-06-06 | `d355bf1` | `artifacts/logs/bench-props-off-live-output-20260606T193652Z.log` | `artifacts/logs/bench-props-off-live-output-audit-20260606T193652Z.log` | `readiness passed=true live_output_gate_block_reasons=live_output_unavailable:150; session_audit passed=true commands=150 allowed=0 blocked=150 reason=live_output_unavailable` | Pi CTest passed 22/22. Live route matching captured 150/150 valid matches, `directional_progress_passed=true`, `endpoint_progress_passed=true`, `progress_gate_passed=true`, telemetry health passed with `telemetry_bytes_dropped=0`, dry-run quality passed with 150/150 valid commands, and live-output remained blocked because no concrete writer exists. |
| 2026-06-07 | `6fc9cd2` | `artifacts/logs/bench-props-off-live-output-20260607T060503Z.log` | `artifacts/logs/bench-props-off-live-output-audit-20260607T060503Z.log` | `readiness passed=true live_output_gate_block_reasons=live_output_unavailable:150; session_audit passed=true commands=150 allowed=0 blocked=150 reason=live_output_unavailable` | Pi CTest passed 23/23 after the serial writer library boundary was added. Live route matching captured 150/150 valid matches, `directional_progress_passed=true`, `endpoint_progress_passed=true`, `progress_gate_passed=true`, telemetry health passed with `telemetry_bytes_dropped=0`, dry-run quality passed with 150/150 valid commands, and live-output remained blocked because the writer is not attached and `VISUAL_HOMING_LIVE_MAVLINK_OUTPUT_AVAILABLE` remains `0`. |
| 2026-06-08 | `98d5407` | `artifacts/logs/bench-props-off-live-output-20260608T210953Z.log` | `artifacts/logs/bench-props-off-live-output-audit-20260608T210953Z.log` | `readiness passed=true live_output_gate_block_reasons=live_output_unavailable:129; session_audit passed=true commands=129 allowed=0 blocked=129 reason=live_output_unavailable stop_reason=endpoint_progress_reached` | Pi CTest passed 23/23 after endpoint-complete stop was enabled in the bench wrapper. Live route matching stopped at endpoint with `frames=129/150`, `endpoint_stop=true`, `stop_reason=endpoint_progress_reached`, `directional_progress_passed=true`, `endpoint_progress_passed=true`, `progress_gate_passed=true`, telemetry health passed with `telemetry_bytes_dropped=0`, dry-run quality passed with 129/129 valid commands, and live-output remained blocked because the writer is not attached and `VISUAL_HOMING_LIVE_MAVLINK_OUTPUT_AVAILABLE` remains `0`. |
| 2026-06-09 | `375f6cd` | `artifacts/logs/bench-props-off-live-output-20260609T063923Z.log` | `artifacts/logs/bench-props-off-live-output-audit-20260609T063923Z.log` | `readiness passed=true live_output_gate_block_reasons=live_output_unavailable:117; session_audit passed=true commands=117 allowed=0 blocked=117 reason=live_output_unavailable stop_reason=endpoint_progress_reached` | Pi CTest passed 23/23 after the explicit serial-writer attach flag was added. The ordinary/default Pi wrapper reported `live_output_writer_attached=false`, live matching stopped at endpoint with `frames=117/225`, `endpoint_stop=true`, `stop_reason=endpoint_progress_reached`, `directional_progress_passed=true`, `endpoint_progress_passed=true`, `progress_gate_passed=true`, telemetry health passed with `telemetry_bytes_dropped=0`, dry-run quality passed with 117/117 valid commands, and live-output remained blocked because the writer is not attached in the default build. |

Rejected/non-evidence runs:

- `artifacts/logs/bench-props-off-live-output-20260609T064000Z.log` is not readiness evidence. It preserved fail-closed output with `live_output_writer_attached=false` and `live_output_unavailable:225`, but the route pass did not reach endpoint (`max_progress_seen=0.697987`, `endpoint_stop=false`, `stop_reason=frame_limit_reached`, `passed=false`).

## Non-Goals

- Do not recreate the dismantled route just for count completion.
- Do not treat this 3/3 record as permission to implement or enable live MAVLink output.
- Do not start Milestone 7 until the safety plan, first live-output implementation plan, and bench props-off boundary are reviewed.
