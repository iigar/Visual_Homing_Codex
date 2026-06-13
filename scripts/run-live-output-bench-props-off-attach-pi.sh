#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
confirmation="${VISUAL_HOMING_LIVE_OUTPUT_BENCH_ATTACH_CONFIRM:-}"
required_confirmation="I_UNDERSTAND_SERIAL_WRITER_IS_ATTACHED_AND_PROPS_ARE_REMOVED"
props_off_confirmation="I_UNDERSTAND_PROPS_ARE_REMOVED"
frames="${VISUAL_HOMING_CAMERA_FRAMES:-150}"
stamp="$(date -u +"%Y%m%dT%H%M%SZ")"
run_log="${VISUAL_HOMING_RUN_LOG:-${repo_root}/artifacts/logs/bench-props-off-live-output-attach-${stamp}.log}"
audit_log="${VISUAL_HOMING_LIVE_ROUTE_SESSION_AUDIT_PATH:-${repo_root}/artifacts/logs/bench-props-off-live-output-attach-audit-${stamp}.log}"
expected_allowed="${VISUAL_HOMING_EXPECTED_LIVE_SESSION_AUDIT_ALLOWED_COMMANDS:-0}"
expected_blocked="${VISUAL_HOMING_EXPECTED_LIVE_SESSION_AUDIT_BLOCKED_COMMANDS:-auto}"
expected_reason="${VISUAL_HOMING_EXPECTED_LIVE_SESSION_AUDIT_REASON:-vehicle_not_armed}"
expected_gate_allowed="${VISUAL_HOMING_EXPECTED_LIVE_OUTPUT_GATE_ALLOWED:-${expected_allowed}}"
expected_gate_blocked="${VISUAL_HOMING_EXPECTED_LIVE_OUTPUT_GATE_BLOCKED:-auto}"
expected_gate_reasons="${VISUAL_HOMING_EXPECTED_LIVE_OUTPUT_GATE_BLOCK_REASONS:-${expected_reason}:auto}"

if [[ "${confirmation}" != "${required_confirmation}" ]]; then
    echo "refusing bench props-off attach run: set VISUAL_HOMING_LIVE_OUTPUT_BENCH_ATTACH_CONFIRM=${required_confirmation}" >&2
    exit 2
fi

cat <<EOF
###############################################################################
### REVIEWED ATTACH-BUILD BENCH PROPS-OFF LIVE-OUTPUT BOUNDARY
### This command is for a physically restrained bench setup with propellers removed.
### It configures the separate attach-capable Pi build and attaches the serial writer.
### This command is not the ordinary fail-closed wrapper and does not authorize flight.
### Default expected result: writer_attached=true allowed=${expected_allowed} blocked=${expected_blocked} reason=${expected_reason}
### run_log=${run_log}
### audit_log=${audit_log}
###############################################################################
EOF

VISUAL_HOMING_RUN_LOG="${run_log}" \
VISUAL_HOMING_PI_CMAKE_ENABLE_LIVE_MAVLINK_OUTPUT=1 \
VISUAL_HOMING_PI_CMAKE_ENABLE_BENCH_PROPS_OFF_LIVE_OUTPUT=1 \
VISUAL_HOMING_PI_CMAKE_ATTACH_BENCH_PROPS_OFF_SERIAL_WRITER=1 \
VISUAL_HOMING_USE_ACTIVE_CAMERA_PROFILE=1 \
VISUAL_HOMING_MATCH_LIVE_ROUTE=1 \
VISUAL_HOMING_CAMERA_FRAMES="${frames}" \
VISUAL_HOMING_CAMERA_TARGET_WIDTH="${VISUAL_HOMING_CAMERA_TARGET_WIDTH:-64}" \
VISUAL_HOMING_CAMERA_TARGET_HEIGHT="${VISUAL_HOMING_CAMERA_TARGET_HEIGHT:-48}" \
VISUAL_HOMING_LIVE_ROUTE_MATCH_EXPECTED_PROGRESS="${VISUAL_HOMING_LIVE_ROUTE_MATCH_EXPECTED_PROGRESS:-forward}" \
VISUAL_HOMING_LIVE_ROUTE_MATCH_MAX_PROGRESS_REGRESSIONS="${VISUAL_HOMING_LIVE_ROUTE_MATCH_MAX_PROGRESS_REGRESSIONS:-10}" \
VISUAL_HOMING_LIVE_ROUTE_MATCH_MAX_PROGRESS_ROLLBACK="${VISUAL_HOMING_LIVE_ROUTE_MATCH_MAX_PROGRESS_ROLLBACK:-0.30}" \
VISUAL_HOMING_LIVE_ROUTE_MATCH_REQUIRE_ENDPOINT_PROGRESS=1 \
VISUAL_HOMING_LIVE_ROUTE_MATCH_STOP_AT_ENDPOINT_PROGRESS=1 \
VISUAL_HOMING_LIVE_ROUTE_DRY_RUN_COMMANDS=1 \
VISUAL_HOMING_LIVE_ROUTE_DRY_RUN_REQUIRE_COMMAND_QUALITY=1 \
VISUAL_HOMING_MATCH_LIVE_ROUTE_USE_LIVE_MAVLINK_TELEMETRY=1 \
VISUAL_HOMING_MATCH_LIVE_ROUTE_REQUIRE_LIVE_TELEMETRY_HEALTH=1 \
VISUAL_HOMING_MAVLINK_TELEMETRY_DEVICE="${VISUAL_HOMING_MAVLINK_TELEMETRY_DEVICE:-/dev/serial0}" \
VISUAL_HOMING_MAVLINK_TELEMETRY_BAUD="${VISUAL_HOMING_MAVLINK_TELEMETRY_BAUD:-115200}" \
VISUAL_HOMING_LIVE_ROUTE_NAVIGATOR_FORWARD_SPEED_MPS=0.0 \
VISUAL_HOMING_LIVE_ROUTE_SESSION_AUDIT=1 \
VISUAL_HOMING_LIVE_ROUTE_SESSION_AUDIT_PATH="${audit_log}" \
VISUAL_HOMING_ENABLE_LIVE_MAVLINK_OUTPUT=1 \
VISUAL_HOMING_LIVE_OUTPUT_BENCH_PROPS_OFF_CONFIRM="${props_off_confirmation}" \
VISUAL_HOMING_LIVE_OUTPUT_MAX_COMMANDS="${VISUAL_HOMING_LIVE_OUTPUT_MAX_COMMANDS:-${frames}}" \
VISUAL_HOMING_LIVE_OUTPUT_MAX_SECONDS="${VISUAL_HOMING_LIVE_OUTPUT_MAX_SECONDS:-10}" \
VISUAL_HOMING_OPERATOR_CUE_SECONDS="${VISUAL_HOMING_OPERATOR_CUE_SECONDS:-10}" \
"${repo_root}/scripts/test-core-pi.sh"

if ! grep -q 'attach_writer_cmake=ON' "${run_log}"; then
    echo "attach_build_log_check path=${run_log} passed=false field=attach_writer_cmake expected=ON" >&2
    exit 2
fi

if ! grep -q 'live_output_writer_attached=true' "${run_log}"; then
    echo "attach_build_log_check path=${run_log} passed=false field=live_output_writer_attached expected=true" >&2
    exit 2
fi

echo "attach_build_log_check path=${run_log} passed=true attach_writer_cmake=ON live_output_writer_attached=true"

VISUAL_HOMING_EXPECTED_LIVE_ROUTE_FRAMES=auto \
VISUAL_HOMING_EXPECTED_LIVE_ROUTE_VALID_MATCHES=auto \
VISUAL_HOMING_EXPECTED_LIVE_ROUTE_DRY_RUN_VALID=auto \
VISUAL_HOMING_EXPECTED_LIVE_OUTPUT_GATE_ALLOWED="${expected_gate_allowed}" \
VISUAL_HOMING_EXPECTED_LIVE_OUTPUT_GATE_BLOCKED="${expected_gate_blocked}" \
VISUAL_HOMING_EXPECTED_LIVE_OUTPUT_GATE_BLOCK_REASONS="${expected_gate_reasons}" \
VISUAL_HOMING_EXPECTED_LIVE_ROUTE_ENDPOINT_STOP=true \
VISUAL_HOMING_EXPECTED_LIVE_ROUTE_STOP_REASON=endpoint_progress_reached \
"${repo_root}/scripts/check-live-readiness-log.sh" "${run_log}"

VISUAL_HOMING_EXPECTED_LIVE_SESSION_AUDIT_COMMANDS=auto \
VISUAL_HOMING_EXPECTED_LIVE_SESSION_AUDIT_ALLOWED_COMMANDS="${expected_allowed}" \
VISUAL_HOMING_EXPECTED_LIVE_SESSION_AUDIT_BLOCKED_COMMANDS="${expected_blocked}" \
VISUAL_HOMING_EXPECTED_LIVE_SESSION_AUDIT_REASON="${expected_reason}" \
VISUAL_HOMING_EXPECTED_LIVE_SESSION_AUDIT_STOP_REASON=endpoint_progress_reached \
"${repo_root}/scripts/check-live-session-audit-log.sh" "${audit_log}"
