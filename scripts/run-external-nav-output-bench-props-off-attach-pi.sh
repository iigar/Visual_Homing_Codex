#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
stamp="$(date -u +"%Y%m%dT%H%M%SZ")"
log_dir="${VISUAL_HOMING_LOG_DIR:-${repo_root}/artifacts/logs}"
run_log="${VISUAL_HOMING_RUN_LOG:-${log_dir}/external-nav-output-attach-${stamp}.log}"
audit_log="${VISUAL_HOMING_EXTERNAL_NAV_OUTPUT_SESSION_AUDIT_PATH:-${log_dir}/external-nav-output-audit-${stamp}.log}"

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
        echo "external_nav_output_attach_check passed=false field=${key} expected=${expected} actual=${actual:-missing} log=${run_log}" >&2
        return 1
    fi
}

cat <<EOF
###############################################################################
### EXTERNAL-NAV OUTPUT ATTACH-ONLY BENCH CHECK
### Props must be off. Runtime provider send remains disabled in this wrapper.
### Expected evidence:
### - attach-capable external-nav output build selected
### - live route match produces FC-ready external-nav estimates
### - external-nav output audit starts and blocks every estimate
### - allowed=0, sent=0, reason=runtime_disabled
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
VISUAL_HOMING_ENABLE_EXTERNAL_NAV_MAVLINK_OUTPUT=0 \
VISUAL_HOMING_EXPECTED_EXTERNAL_NAV_STRICT_SESSION_READY=1 \
"${repo_root}/scripts/run-external-nav-dry-run-pi.sh"

start_line="$(grep '^live_route_match_start ' "${run_log}" | tail -1 || true)"
compact_line="$(grep '^live_route_match_compact ' "${run_log}" | tail -1 || true)"
if [[ -z "${start_line}" || -z "${compact_line}" ]]; then
    echo "external_nav_output_attach_check passed=false error=missing_live_route_match_summary log=${run_log}" >&2
    exit 2
fi

failed=0
require_field "${start_line}" "external_nav_output_build_requested" "true" || failed=1
require_field "${start_line}" "external_nav_output_bench_scope" "true" || failed=1
require_field "${start_line}" "external_nav_output_available" "true" || failed=1
require_field "${start_line}" "external_nav_writer_attached" "true" || failed=1
require_field "${compact_line}" "external_nav_output_allowed" "0" || failed=1
require_field "${compact_line}" "external_nav_output_sent" "0" || failed=1
require_field "${compact_line}" "external_nav_output_session_audit" "true" || failed=1
require_field "${compact_line}" "final_external_nav_output_reason" "runtime_disabled" || failed=1
if [[ "${failed}" != "0" ]]; then
    exit 2
fi

audit_stop_reason="match_live_route_complete"
if [[ "${VISUAL_HOMING_LIVE_ROUTE_MATCH_STOP_AT_ENDPOINT_PROGRESS:-0}" == "1" ]]; then
    audit_stop_reason="endpoint_progress_reached"
fi

VISUAL_HOMING_EXPECTED_EXTERNAL_NAV_OUTPUT_AUDIT_REASON=runtime_disabled \
VISUAL_HOMING_EXPECTED_EXTERNAL_NAV_OUTPUT_AUDIT_STOP_REASON="${audit_stop_reason}" \
VISUAL_HOMING_EXPECTED_EXTERNAL_NAV_OUTPUT_AUDIT_VALID_FOR_FC=true \
"${repo_root}/scripts/check-external-nav-output-audit-log.sh" "${audit_log}"

echo "external_nav_output_attach_check passed=true run_log=${run_log} audit_log=${audit_log}"
