#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
stamp="$(date -u +"%Y%m%dT%H%M%SZ")"
log_dir="${VISUAL_HOMING_LOG_DIR:-${repo_root}/artifacts/logs}"

probe_log="${VISUAL_HOMING_EXTERNAL_NAV_ACCEPTANCE_PROBE_LOG:-${log_dir}/external-nav-acceptance-probe-${stamp}.log}"
preflight_log="${VISUAL_HOMING_EXTERNAL_NAV_ACCEPTANCE_PREFLIGHT_LOG:-${log_dir}/external-nav-acceptance-pre-${stamp}.log}"
send_log="${VISUAL_HOMING_RUN_LOG:-${log_dir}/external-nav-acceptance-send-${stamp}.log}"
send_audit_log="${VISUAL_HOMING_EXTERNAL_NAV_OUTPUT_SESSION_AUDIT_PATH:-${log_dir}/external-nav-acceptance-send-audit-${stamp}.log}"
postflight_log="${VISUAL_HOMING_EXTERNAL_NAV_ACCEPTANCE_POSTFLIGHT_LOG:-${log_dir}/external-nav-acceptance-post-${stamp}.log}"

telemetry_duration_ms="${VISUAL_HOMING_EXTERNAL_NAV_ACCEPTANCE_TELEMETRY_DURATION_MS:-5000}"

mkdir -p "${log_dir}"

extract_field() {
    local line="$1"
    local key="$2"
    local token
    token="$(printf '%s\n' "${line}" | tr ' ' '\n' | awk -F= -v key="${key}" '$1 == key { print $2; exit }')"
    printf '%s' "${token}"
}

append_section() {
    local title="$1"
    {
        echo
        echo "## ${title}"
    } >> "${probe_log}"
}

append_key_value() {
    local key="$1"
    local value="$2"
    printf '%s=%s\n' "${key}" "${value}" >> "${probe_log}"
}

cat <<EOF
###############################################################################
### EXTERNAL-NAV FC/JT_ZERO ACCEPTANCE PROBE
### Props must be off. This wrapper can run the reviewed provider-send bench path.
### Probe shape:
### 1. read-only MAVLink telemetry capture before send
### 2. reviewed bounded external-nav provider send run
### 3. read-only MAVLink telemetry capture after send
###
### This proves Pi-side send plus before/after telemetry evidence only.
### It does not automatically prove EKF/JT_Zero provider acceptance unless the
### captured telemetry exposes an acceptance/status signal that can be reviewed.
###
### probe_log=${probe_log}
### preflight_log=${preflight_log}
### send_log=${send_log}
### send_audit_log=${send_audit_log}
### postflight_log=${postflight_log}
### telemetry_duration_ms=${telemetry_duration_ms}
###############################################################################
EOF

{
    echo "external_nav_acceptance_probe_start wall_time_utc=$(date -u +"%Y-%m-%dT%H:%M:%SZ")"
    echo "probe_log=${probe_log}"
    echo "preflight_log=${preflight_log}"
    echo "send_log=${send_log}"
    echo "send_audit_log=${send_audit_log}"
    echo "postflight_log=${postflight_log}"
    echo "telemetry_duration_ms=${telemetry_duration_ms}"
} > "${probe_log}"

append_section "pre-send telemetry"
VISUAL_HOMING_RUN_LOG="${preflight_log}" \
VISUAL_HOMING_MAVLINK_TELEMETRY_DURATION_MS="${telemetry_duration_ms}" \
"${repo_root}/scripts/check-external-nav-telemetry-sanity-pi.sh" | tee -a "${probe_log}"

pre_inspect_line="$(grep '^mavlink_telemetry_inspect ' "${preflight_log}" | tail -1 || true)"
pre_sanity_line="$(grep '^external_nav_telemetry_sanity ' "${probe_log}" | tail -1 || true)"
append_key_value "pre_inspect_line" "${pre_inspect_line:-missing}"
append_key_value "pre_sanity_line" "${pre_sanity_line:-missing}"

append_section "provider-send run"
VISUAL_HOMING_RUN_LOG="${send_log}" \
VISUAL_HOMING_EXTERNAL_NAV_OUTPUT_SESSION_AUDIT_PATH="${send_audit_log}" \
"${repo_root}/scripts/run-external-nav-output-bench-props-off-send-pi.sh" | tee -a "${probe_log}"

send_start_line="$(grep '^live_route_match_start ' "${send_log}" | tail -1 || true)"
send_compact_line="$(grep '^live_route_match_compact ' "${send_log}" | tail -1 || true)"
send_check_line="$(grep '^external_nav_output_send_check ' "${probe_log}" | tail -1 || true)"
append_key_value "send_start_line" "${send_start_line:-missing}"
append_key_value "send_compact_line" "${send_compact_line:-missing}"
append_key_value "send_check_line" "${send_check_line:-missing}"

append_section "post-send telemetry"
VISUAL_HOMING_RUN_LOG="${postflight_log}" \
VISUAL_HOMING_MAVLINK_TELEMETRY_DURATION_MS="${telemetry_duration_ms}" \
"${repo_root}/scripts/check-external-nav-telemetry-sanity-pi.sh" | tee -a "${probe_log}"

post_inspect_line="$(grep '^mavlink_telemetry_inspect ' "${postflight_log}" | tail -1 || true)"
post_sanity_line="$(grep '^external_nav_telemetry_sanity ' "${probe_log}" | tail -1 || true)"
append_key_value "post_inspect_line" "${post_inspect_line:-missing}"
append_key_value "post_sanity_line" "${post_sanity_line:-missing}"

allowed="$(extract_field "${send_compact_line:-}" "external_nav_output_allowed")"
sent="$(extract_field "${send_compact_line:-}" "external_nav_output_sent")"
blocked="$(extract_field "${send_compact_line:-}" "external_nav_output_blocked")"
send_reason="$(extract_field "${send_compact_line:-}" "final_external_nav_output_reason")"
external_nav_valid="$(extract_field "${send_compact_line:-}" "external_nav_valid")"
session_ready="$(extract_field "${send_compact_line:-}" "external_nav_session_ready")"
strict_ready="$(extract_field "${send_compact_line:-}" "external_nav_strict_session_ready")"

append_section "probe summary"
append_key_value "allowed" "${allowed:-missing}"
append_key_value "sent" "${sent:-missing}"
append_key_value "blocked" "${blocked:-missing}"
append_key_value "final_external_nav_output_reason" "${send_reason:-missing}"
append_key_value "external_nav_valid" "${external_nav_valid:-missing}"
append_key_value "external_nav_session_ready" "${session_ready:-missing}"
append_key_value "external_nav_strict_session_ready" "${strict_ready:-missing}"
append_key_value "acceptance_probe_result" "probe_complete_requires_fc_status_review"

echo "external_nav_acceptance_probe_done passed=true result=probe_complete_requires_fc_status_review probe_log=${probe_log} preflight_log=${preflight_log} send_log=${send_log} send_audit_log=${send_audit_log} postflight_log=${postflight_log} allowed=${allowed:-missing} sent=${sent:-missing} blocked=${blocked:-missing} reason=${send_reason:-missing} external_nav_valid=${external_nav_valid:-missing} session_ready=${session_ready:-missing} strict_ready=${strict_ready:-missing}"
