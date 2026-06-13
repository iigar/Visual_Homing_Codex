#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
send_confirmation="${VISUAL_HOMING_LIVE_OUTPUT_BENCH_SEND_CONFIRM:-}"
required_send_confirmation="I_UNDERSTAND_THIS_WILL_SEND_BOUNDED_MAVLINK_COMMANDS_WITH_PROPS_REMOVED"
mode_confirmation="${VISUAL_HOMING_LIVE_OUTPUT_BENCH_ARMED_GUIDED_CONFIRM:-}"
required_mode_confirmation="I_HAVE_VERIFIED_ARMED_GUIDED_BENCH_STATE"
attach_confirmation="I_UNDERSTAND_SERIAL_WRITER_IS_ATTACHED_AND_PROPS_ARE_REMOVED"
frames="${VISUAL_HOMING_CAMERA_FRAMES:-150}"
stamp="$(date -u +"%Y%m%dT%H%M%SZ")"
run_log="${VISUAL_HOMING_RUN_LOG:-${repo_root}/artifacts/logs/bench-props-off-live-output-send-${stamp}.log}"
audit_log="${VISUAL_HOMING_LIVE_ROUTE_SESSION_AUDIT_PATH:-${repo_root}/artifacts/logs/bench-props-off-live-output-send-audit-${stamp}.log}"
max_commands="${VISUAL_HOMING_LIVE_OUTPUT_MAX_COMMANDS:-${frames}}"
max_seconds="${VISUAL_HOMING_LIVE_OUTPUT_MAX_SECONDS:-10}"

if [[ "${send_confirmation}" != "${required_send_confirmation}" ]]; then
    echo "refusing bench props-off send run: set VISUAL_HOMING_LIVE_OUTPUT_BENCH_SEND_CONFIRM=${required_send_confirmation}" >&2
    exit 2
fi

if [[ "${mode_confirmation}" != "${required_mode_confirmation}" ]]; then
    echo "refusing bench props-off send run: set VISUAL_HOMING_LIVE_OUTPUT_BENCH_ARMED_GUIDED_CONFIRM=${required_mode_confirmation}" >&2
    exit 2
fi

if [[ ! "${max_commands}" =~ ^[1-9][0-9]*$ ]]; then
    echo "refusing bench props-off send run: VISUAL_HOMING_LIVE_OUTPUT_MAX_COMMANDS must be a positive integer" >&2
    exit 2
fi

if [[ ! "${frames}" =~ ^[1-9][0-9]*$ ]]; then
    echo "refusing bench props-off send run: VISUAL_HOMING_CAMERA_FRAMES must be a positive integer" >&2
    exit 2
fi

if [[ "${max_commands}" -lt "${frames}" ]]; then
    echo "refusing bench props-off send run: VISUAL_HOMING_LIVE_OUTPUT_MAX_COMMANDS must be >= VISUAL_HOMING_CAMERA_FRAMES for endpoint-stop evidence" >&2
    exit 2
fi

cat <<EOF
###############################################################################
### REVIEWED SEND-ENABLED BENCH PROPS-OFF LIVE-OUTPUT BOUNDARY
### This command can send bounded MAVLink commands to the flight controller.
### It is for a physically restrained bench setup with propellers removed only.
### It is not flight, tethered flight, ground movement, or autonomous return.
### Expected result: writer_attached=true allowed=auto_positive blocked=0 reason=allowed
### max_commands=${max_commands} max_seconds=${max_seconds}
### run_log=${run_log}
### audit_log=${audit_log}
###############################################################################
EOF

VISUAL_HOMING_RUN_LOG="${run_log}" \
VISUAL_HOMING_LIVE_ROUTE_SESSION_AUDIT_PATH="${audit_log}" \
VISUAL_HOMING_CAMERA_FRAMES="${frames}" \
VISUAL_HOMING_LIVE_OUTPUT_MAX_COMMANDS="${max_commands}" \
VISUAL_HOMING_LIVE_OUTPUT_MAX_SECONDS="${max_seconds}" \
VISUAL_HOMING_LIVE_OUTPUT_BENCH_ATTACH_CONFIRM="${attach_confirmation}" \
VISUAL_HOMING_EXPECTED_LIVE_OUTPUT_GATE_ALLOWED=auto \
VISUAL_HOMING_EXPECTED_LIVE_OUTPUT_GATE_BLOCKED=0 \
VISUAL_HOMING_EXPECTED_LIVE_OUTPUT_GATE_BLOCK_REASONS=any \
VISUAL_HOMING_EXPECTED_LIVE_SESSION_AUDIT_ALLOWED_COMMANDS=auto \
VISUAL_HOMING_EXPECTED_LIVE_SESSION_AUDIT_BLOCKED_COMMANDS=0 \
VISUAL_HOMING_EXPECTED_LIVE_SESSION_AUDIT_REASON=allowed \
"${repo_root}/scripts/run-live-output-bench-props-off-attach-pi.sh"

allowed_commands="$(awk '
    /^live_output_audit event=command / {
        allowed = ""
        reason = ""
        for (i = 1; i <= NF; ++i) {
            split($i, kv, "=")
            if (kv[1] == "allowed") {
                allowed = kv[2]
            } else if (kv[1] == "reason") {
                reason = kv[2]
            }
        }
        if (allowed == "true" && reason == "allowed") {
            ++allowed_count
        }
    }
    END { print allowed_count + 0 }
' "${audit_log}")"

if [[ ! "${allowed_commands}" =~ ^[1-9][0-9]*$ ]]; then
    echo "send_bench_audit_check path=${audit_log} passed=false field=allowed_commands expected=positive actual=${allowed_commands}" >&2
    exit 2
fi

echo "send_bench_audit_check path=${audit_log} passed=true allowed_commands=${allowed_commands} blocked_commands=0 reason=allowed"
