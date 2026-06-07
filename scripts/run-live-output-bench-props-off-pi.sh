#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
confirmation="${VISUAL_HOMING_LIVE_OUTPUT_BENCH_PROPS_OFF_CONFIRM:-}"
required_confirmation="I_UNDERSTAND_PROPS_ARE_REMOVED"
frames="${VISUAL_HOMING_CAMERA_FRAMES:-150}"
stamp="$(date -u +"%Y%m%dT%H%M%SZ")"
run_log="${VISUAL_HOMING_RUN_LOG:-${repo_root}/artifacts/logs/bench-props-off-live-output-${stamp}.log}"
audit_log="${VISUAL_HOMING_LIVE_ROUTE_SESSION_AUDIT_PATH:-${repo_root}/artifacts/logs/bench-props-off-live-output-audit-${stamp}.log}"
expected_allowed="${VISUAL_HOMING_EXPECTED_LIVE_SESSION_AUDIT_ALLOWED_COMMANDS:-0}"
expected_blocked="${VISUAL_HOMING_EXPECTED_LIVE_SESSION_AUDIT_BLOCKED_COMMANDS:-${frames}}"
expected_reason="${VISUAL_HOMING_EXPECTED_LIVE_SESSION_AUDIT_REASON:-live_output_unavailable}"
expected_gate_reasons="${VISUAL_HOMING_EXPECTED_LIVE_OUTPUT_GATE_BLOCK_REASONS:-${expected_reason}:${expected_blocked}}"

if [[ "${confirmation}" != "${required_confirmation}" ]]; then
    echo "refusing bench props-off live-output run: set VISUAL_HOMING_LIVE_OUTPUT_BENCH_PROPS_OFF_CONFIRM=${required_confirmation}" >&2
    exit 2
fi

cat <<EOF
###############################################################################
### BENCH PROPS-OFF LIVE-OUTPUT BOUNDARY
### This command is for a physically restrained bench setup with propellers removed.
### A serial MAVLink writer library exists, but it is not attached or available.
### Expected fail-closed result before runtime attachment: allowed=${expected_allowed} blocked=${expected_blocked} reason=${expected_reason}
### run_log=${run_log}
### audit_log=${audit_log}
###############################################################################
EOF

VISUAL_HOMING_RUN_LOG="${run_log}" \
VISUAL_HOMING_USE_ACTIVE_CAMERA_PROFILE=1 \
VISUAL_HOMING_MATCH_LIVE_ROUTE=1 \
VISUAL_HOMING_CAMERA_FRAMES="${frames}" \
VISUAL_HOMING_CAMERA_TARGET_WIDTH="${VISUAL_HOMING_CAMERA_TARGET_WIDTH:-64}" \
VISUAL_HOMING_CAMERA_TARGET_HEIGHT="${VISUAL_HOMING_CAMERA_TARGET_HEIGHT:-48}" \
VISUAL_HOMING_LIVE_ROUTE_MATCH_EXPECTED_PROGRESS="${VISUAL_HOMING_LIVE_ROUTE_MATCH_EXPECTED_PROGRESS:-forward}" \
VISUAL_HOMING_LIVE_ROUTE_MATCH_REQUIRE_ENDPOINT_PROGRESS=1 \
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
VISUAL_HOMING_LIVE_OUTPUT_BENCH_PROPS_OFF_CONFIRM="${confirmation}" \
VISUAL_HOMING_LIVE_OUTPUT_MAX_COMMANDS="${VISUAL_HOMING_LIVE_OUTPUT_MAX_COMMANDS:-${frames}}" \
VISUAL_HOMING_LIVE_OUTPUT_MAX_SECONDS="${VISUAL_HOMING_LIVE_OUTPUT_MAX_SECONDS:-10}" \
VISUAL_HOMING_OPERATOR_CUE_SECONDS="${VISUAL_HOMING_OPERATOR_CUE_SECONDS:-10}" \
"${repo_root}/scripts/test-core-pi.sh"

VISUAL_HOMING_EXPECTED_LIVE_OUTPUT_GATE_BLOCK_REASONS="${expected_gate_reasons}" \
"${repo_root}/scripts/check-live-readiness-log.sh" "${run_log}"

VISUAL_HOMING_EXPECTED_LIVE_SESSION_AUDIT_COMMANDS="${frames}" \
VISUAL_HOMING_EXPECTED_LIVE_SESSION_AUDIT_ALLOWED_COMMANDS="${expected_allowed}" \
VISUAL_HOMING_EXPECTED_LIVE_SESSION_AUDIT_BLOCKED_COMMANDS="${expected_blocked}" \
VISUAL_HOMING_EXPECTED_LIVE_SESSION_AUDIT_REASON="${expected_reason}" \
"${repo_root}/scripts/check-live-session-audit-log.sh" "${audit_log}"
