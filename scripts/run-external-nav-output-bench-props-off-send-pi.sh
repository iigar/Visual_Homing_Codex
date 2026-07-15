#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
send_confirmation="${VISUAL_HOMING_EXTERNAL_NAV_OUTPUT_BENCH_SEND_CONFIRM:-}"
required_send_confirmation="I_UNDERSTAND_THIS_WILL_SEND_EXTERNAL_NAV_PROVIDER_MESSAGES_WITH_PROPS_REMOVED"
fc_state_confirmation="${VISUAL_HOMING_EXTERNAL_NAV_OUTPUT_BENCH_FC_STATE_CONFIRM:-}"
required_fc_state_confirmation="I_HAVE_VERIFIED_REVIEWED_BENCH_FC_STATE"
props_off_confirmation="I_UNDERSTAND_PROPS_ARE_REMOVED"
single_writer_confirmation="I_UNDERSTAND_EXTERNAL_NAV_IS_THE_ONLY_POSITION_PROVIDER"
frames="${VISUAL_HOMING_CAMERA_FRAMES:-150}"
stamp="$(date -u +"%Y%m%dT%H%M%SZ")"
log_dir="${VISUAL_HOMING_LOG_DIR:-${repo_root}/artifacts/logs}"
run_log="${VISUAL_HOMING_RUN_LOG:-${log_dir}/external-nav-output-send-${stamp}.log}"
audit_log="${VISUAL_HOMING_EXTERNAL_NAV_OUTPUT_SESSION_AUDIT_PATH:-${log_dir}/external-nav-output-send-audit-${stamp}.log}"
max_messages="${VISUAL_HOMING_EXTERNAL_NAV_OUTPUT_MAX_MESSAGES:-${frames}}"
max_seconds="${VISUAL_HOMING_EXTERNAL_NAV_OUTPUT_MAX_SECONDS:-10}"

if [[ "${VISUAL_HOMING_EXTERNAL_NAV_ROUTE_FRAME_ALIGNMENT_KNOWN:-0}" != "1"
    || "${VISUAL_HOMING_EXTERNAL_NAV_ALTITUDE_ORIGIN_ALIGNED:-0}" != "1" ]]; then
    echo "refusing external-nav provider send bench run: verified route-frame and altitude-origin alignment confirmations are required" >&2
    exit 2
fi
for required_alignment_env in \
    VISUAL_HOMING_EXTERNAL_NAV_ROUTE_ORIGIN_NORTH_M \
    VISUAL_HOMING_EXTERNAL_NAV_ROUTE_ORIGIN_EAST_M \
    VISUAL_HOMING_EXTERNAL_NAV_ROUTE_ORIGIN_DOWN_M \
    VISUAL_HOMING_EXTERNAL_NAV_ROUTE_HEADING_NED_RAD; do
    if [[ ! -v ${required_alignment_env} || -z "${!required_alignment_env}" ]]; then
        echo "refusing external-nav provider send bench run: explicit ${required_alignment_env} is required" >&2
        exit 2
    fi
done

mkdir -p "${log_dir}"

extract_field() {
    local line="$1"
    local key="$2"
    local token
    token="$(printf '%s\n' "${line}" | tr ' ' '\n' | awk -F= -v key="${key}" '$1 == key { print $2; exit }')"
    printf '%s' "${token}"
}

require_field() {
    local line="$1"
    local key="$2"
    local expected="$3"
    local actual
    actual="$(extract_field "${line}" "${key}")"
    if [[ "${actual}" != "${expected}" ]]; then
        echo "external_nav_output_send_check passed=false field=${key} expected=${expected} actual=${actual:-missing} log=${run_log}" >&2
        return 1
    fi
}

require_positive_field() {
    local line="$1"
    local key="$2"
    local actual
    actual="$(extract_field "${line}" "${key}")"
    if [[ ! "${actual}" =~ ^[1-9][0-9]*$ ]]; then
        echo "external_nav_output_send_check passed=false field=${key} expected=positive_uint actual=${actual:-missing} log=${run_log}" >&2
        return 1
    fi
}

if [[ "${send_confirmation}" != "${required_send_confirmation}" ]]; then
    echo "refusing external-nav provider send bench run: set VISUAL_HOMING_EXTERNAL_NAV_OUTPUT_BENCH_SEND_CONFIRM=${required_send_confirmation}" >&2
    exit 2
fi

if [[ "${fc_state_confirmation}" != "${required_fc_state_confirmation}" ]]; then
    echo "refusing external-nav provider send bench run: set VISUAL_HOMING_EXTERNAL_NAV_OUTPUT_BENCH_FC_STATE_CONFIRM=${required_fc_state_confirmation}" >&2
    exit 2
fi

if [[ ! "${frames}" =~ ^[1-9][0-9]*$ ]]; then
    echo "refusing external-nav provider send bench run: VISUAL_HOMING_CAMERA_FRAMES must be a positive integer" >&2
    exit 2
fi

if [[ ! "${max_messages}" =~ ^[1-9][0-9]*$ ]]; then
    echo "refusing external-nav provider send bench run: VISUAL_HOMING_EXTERNAL_NAV_OUTPUT_MAX_MESSAGES must be a positive integer" >&2
    exit 2
fi

if [[ ! "${max_seconds}" =~ ^[0-9]+([.][0-9]+)?$ || "${max_seconds}" == "0" || "${max_seconds}" == "0.0" ]]; then
    echo "refusing external-nav provider send bench run: VISUAL_HOMING_EXTERNAL_NAV_OUTPUT_MAX_SECONDS must be positive" >&2
    exit 2
fi

cat <<EOF
###############################################################################
### REVIEWED SEND-ENABLED BENCH PROPS-OFF EXTERNAL-NAV PROVIDER BOUNDARY
### This command can send bounded MAVLink external-nav provider messages.
### It is for a reviewed bench setup with propellers removed only.
### It is not flight, tethered flight, ground movement, or autonomous return.
### Expected result: external_nav_writer_attached=true allowed>0 sent>0 blocked=0
### max_messages=${max_messages} max_seconds=${max_seconds}
### run_log=${run_log}
### audit_log=${audit_log}
###############################################################################
EOF

VISUAL_HOMING_RUN_LOG="${run_log}" \
VISUAL_HOMING_PI_CMAKE_ENABLE_EXTERNAL_NAV_OUTPUT=1 \
VISUAL_HOMING_PI_CMAKE_ENABLE_BENCH_PROPS_OFF_EXTERNAL_NAV_OUTPUT=1 \
VISUAL_HOMING_PI_CMAKE_ATTACH_BENCH_PROPS_OFF_EXTERNAL_NAV_WRITER=1 \
VISUAL_HOMING_EXTERNAL_NAV_OUTPUT_SESSION_AUDIT=1 \
VISUAL_HOMING_EXTERNAL_NAV_OUTPUT_SESSION_AUDIT_PATH="${audit_log}" \
VISUAL_HOMING_ENABLE_EXTERNAL_NAV_MAVLINK_OUTPUT=1 \
VISUAL_HOMING_EXTERNAL_NAV_OUTPUT_BENCH_PROPS_OFF_CONFIRM="${props_off_confirmation}" \
VISUAL_HOMING_EXTERNAL_NAV_OUTPUT_SINGLE_WRITER_CONFIRM="${single_writer_confirmation}" \
VISUAL_HOMING_EXTERNAL_NAV_OUTPUT_MAX_MESSAGES="${max_messages}" \
VISUAL_HOMING_EXTERNAL_NAV_OUTPUT_MAX_SECONDS="${max_seconds}" \
VISUAL_HOMING_EXPECTED_EXTERNAL_NAV_STRICT_SESSION_READY=1 \
"${repo_root}/scripts/run-external-nav-dry-run-pi.sh"

start_line="$(grep '^live_route_match_start ' "${run_log}" | tail -1 || true)"
compact_line="$(grep '^live_route_match_compact ' "${run_log}" | tail -1 || true)"
if [[ -z "${start_line}" || -z "${compact_line}" ]]; then
    echo "external_nav_output_send_check passed=false error=missing_live_route_match_summary log=${run_log}" >&2
    exit 2
fi

failed=0
require_field "${start_line}" "external_nav_output_build_requested" "true" || failed=1
require_field "${start_line}" "external_nav_output_bench_scope" "true" || failed=1
require_field "${start_line}" "external_nav_output_available" "true" || failed=1
require_field "${start_line}" "external_nav_writer_attached" "true" || failed=1
require_positive_field "${compact_line}" "external_nav_output_allowed" || failed=1
require_positive_field "${compact_line}" "external_nav_output_sent" || failed=1
require_field "${compact_line}" "external_nav_output_blocked" "0" || failed=1
require_field "${compact_line}" "external_nav_output_session_audit" "true" || failed=1
require_field "${compact_line}" "final_external_nav_output_reason" "allowed" || failed=1
if [[ "${failed}" != "0" ]]; then
    exit 2
fi

audit_stop_reason="match_live_route_complete"
if [[ "${VISUAL_HOMING_LIVE_ROUTE_MATCH_STOP_AT_ENDPOINT_PROGRESS:-0}" == "1" ]]; then
    audit_stop_reason="endpoint_progress_reached"
fi

VISUAL_HOMING_EXPECTED_EXTERNAL_NAV_OUTPUT_AUDIT_ALLOWED=auto \
VISUAL_HOMING_EXPECTED_EXTERNAL_NAV_OUTPUT_AUDIT_SENT=auto \
VISUAL_HOMING_EXPECTED_EXTERNAL_NAV_OUTPUT_AUDIT_BLOCKED=0 \
VISUAL_HOMING_EXPECTED_EXTERNAL_NAV_OUTPUT_AUDIT_REASON=allowed \
VISUAL_HOMING_EXPECTED_EXTERNAL_NAV_OUTPUT_AUDIT_STOP_REASON="${audit_stop_reason}" \
VISUAL_HOMING_EXPECTED_EXTERNAL_NAV_OUTPUT_AUDIT_VALID_FOR_FC=true \
"${repo_root}/scripts/check-external-nav-output-audit-log.sh" "${audit_log}"

audit_counts="$(awk '
    /^external_nav_output_audit event=estimate / {
        allowed = ""
        sent = ""
        reason = ""
        for (i = 1; i <= NF; ++i) {
            split($i, kv, "=")
            if (kv[1] == "allowed") {
                allowed = kv[2]
            } else if (kv[1] == "sent") {
                sent = kv[2]
            } else if (kv[1] == "reason") {
                reason = kv[2]
            }
        }
        if (allowed == "true" && reason == "allowed") {
            ++allowed_count
        }
        if (sent == "true" && reason == "allowed") {
            ++sent_count
        }
    }
    END { printf "%d %d", allowed_count + 0, sent_count + 0 }
' "${audit_log}")"
read -r allowed_estimates sent_estimates <<< "${audit_counts}"

if [[ ! "${allowed_estimates}" =~ ^[1-9][0-9]*$ || ! "${sent_estimates}" =~ ^[1-9][0-9]*$ ]]; then
    echo "external_nav_output_send_audit_check path=${audit_log} passed=false allowed=${allowed_estimates} sent=${sent_estimates} expected=positive" >&2
    exit 2
fi

echo "external_nav_output_send_check passed=true run_log=${run_log} audit_log=${audit_log} allowed=${allowed_estimates} sent=${sent_estimates} blocked=0"
