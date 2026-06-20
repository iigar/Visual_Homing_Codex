#!/usr/bin/env bash
set -euo pipefail

expected_frames="${VISUAL_HOMING_EXPECTED_LIVE_ROUTE_FRAMES:-150/150}"
expected_valid_matches="${VISUAL_HOMING_EXPECTED_LIVE_ROUTE_VALID_MATCHES:-150}"
expected_dry_run_valid="${VISUAL_HOMING_EXPECTED_LIVE_ROUTE_DRY_RUN_VALID:-150/150}"
expected_live_output_gate_reasons="${VISUAL_HOMING_EXPECTED_LIVE_OUTPUT_GATE_BLOCK_REASONS:-vehicle_not_armed:150}"
require_strict_session="${VISUAL_HOMING_EXPECTED_EXTERNAL_NAV_STRICT_SESSION_READY:-0}"

usage() {
    echo "usage: $0 <test-core-pi-log> [<test-core-pi-log> ...]" >&2
}

if [[ "$#" -lt 1 ]]; then
    usage
    exit 2
fi

case "${require_strict_session}" in
    0|1)
        ;;
    *)
        echo "VISUAL_HOMING_EXPECTED_EXTERNAL_NAV_STRICT_SESSION_READY must be 0 or 1, got '${require_strict_session}'" >&2
        exit 2
        ;;
esac

extract_field() {
    local line="$1"
    local key="$2"
    local token
    token="$(printf '%s\n' "${line}" | tr ' ' '\n' | awk -F= -v key="${key}" '$1 == key { print $2; exit }')"
    printf '%s' "${token}"
}

require_field() {
    local log_path="$1"
    local line="$2"
    local key="$3"
    local expected="$4"
    local actual
    actual="$(extract_field "${line}" "${key}")"
    if [[ -z "${actual}" ]]; then
        echo "external_nav_readiness_log_check path=${log_path} passed=false missing_field=${key}" >&2
        return 1
    fi
    if [[ "${actual}" != "${expected}" ]]; then
        echo "external_nav_readiness_log_check path=${log_path} passed=false field=${key} expected=${expected} actual=${actual}" >&2
        return 1
    fi
}

require_positive_uint() {
    local log_path="$1"
    local name="$2"
    local value="$3"
    if [[ ! "${value}" =~ ^[0-9]+$ || "${value}" == "0" ]]; then
        echo "external_nav_readiness_log_check path=${log_path} passed=false field=${name} expected=positive_uint actual=${value:-missing}" >&2
        return 1
    fi
}

check_log() {
    local log_path="$1"
    if [[ ! -f "${log_path}" ]]; then
        echo "external_nav_readiness_log_check path=${log_path} passed=false error=log_not_found" >&2
        return 1
    fi

    local compact_line
    compact_line="$(grep '^live_route_match_compact ' "${log_path}" | tail -1 || true)"
    if [[ -z "${compact_line}" ]]; then
        echo "external_nav_readiness_log_check path=${log_path} passed=false error=missing_live_route_match_compact" >&2
        return 1
    fi

    local failed=0
    local frames_value captured_frames requested_frames
    frames_value="$(extract_field "${compact_line}" "frames")"
    captured_frames="${frames_value%%/*}"
    requested_frames="${frames_value#*/}"

    require_field "${log_path}" "${compact_line}" "passed" "true" || failed=1
    if [[ "${expected_frames}" == "auto" ]]; then
        require_positive_uint "${log_path}" "frames.captured" "${captured_frames}" || failed=1
        require_positive_uint "${log_path}" "frames.requested" "${requested_frames}" || failed=1
    else
        require_field "${log_path}" "${compact_line}" "frames" "${expected_frames}" || failed=1
    fi
    if [[ "${expected_valid_matches}" == "auto" ]]; then
        require_field "${log_path}" "${compact_line}" "valid_matches" "${captured_frames}" || failed=1
    else
        require_field "${log_path}" "${compact_line}" "valid_matches" "${expected_valid_matches}" || failed=1
    fi

    require_field "${log_path}" "${compact_line}" "endpoint_passed" "true" || failed=1
    require_field "${log_path}" "${compact_line}" "progress_gate_passed" "true" || failed=1
    require_field "${log_path}" "${compact_line}" "telemetry_health" "true" || failed=1
    require_field "${log_path}" "${compact_line}" "telemetry_dropped" "0" || failed=1
    require_field "${log_path}" "${compact_line}" "dry_run_quality" "true" || failed=1
    if [[ "${expected_dry_run_valid}" == "auto" ]]; then
        require_field "${log_path}" "${compact_line}" "dry_run_valid" "${captured_frames}/${captured_frames}" || failed=1
    else
        require_field "${log_path}" "${compact_line}" "dry_run_valid" "${expected_dry_run_valid}" || failed=1
    fi

    require_field "${log_path}" "${compact_line}" "external_nav_session_ready" "true" || failed=1
    require_field "${log_path}" "${compact_line}" "external_nav_session_reason" "valid" || failed=1
    require_field "${log_path}" "${compact_line}" "external_nav_quality_ready" "true" || failed=1
    require_field "${log_path}" "${compact_line}" "external_nav_quality_reason" "valid" || failed=1
    require_field "${log_path}" "${compact_line}" "external_nav_relative_altitude_window_passed" "true" || failed=1
    require_field "${log_path}" "${compact_line}" "external_nav_altitude_blocker" "none" || failed=1
    require_field "${log_path}" "${compact_line}" "visual_scale_required" "false" || failed=1

    if [[ "${require_strict_session}" == "1" ]]; then
        require_field "${log_path}" "${compact_line}" "external_nav_strict_session_ready" "true" || failed=1
        require_field "${log_path}" "${compact_line}" "external_nav_strict_session_reason" "valid" || failed=1
    fi

    require_field "${log_path}" "${compact_line}" "live_output_gate_allowed" "0" || failed=1
    if [[ "${expected_live_output_gate_reasons}" != "any" ]]; then
        local resolved_gate_reasons="${expected_live_output_gate_reasons}"
        if [[ "${expected_live_output_gate_reasons}" == *":auto" ]]; then
            resolved_gate_reasons="${expected_live_output_gate_reasons%:auto}:${captured_frames}"
        fi
        require_field "${log_path}" "${compact_line}" "live_output_gate_block_reasons" "${resolved_gate_reasons}" || failed=1
    fi

    if [[ "${failed}" != "0" ]]; then
        return 1
    fi

    echo "external_nav_readiness_log_check path=${log_path} passed=true external_nav_valid=$(extract_field "${compact_line}" "external_nav_valid") external_nav_session_ready=$(extract_field "${compact_line}" "external_nav_session_ready") external_nav_strict_session_ready=$(extract_field "${compact_line}" "external_nav_strict_session_ready")"
}

overall_status=0
for log_path in "$@"; do
    if ! check_log "${log_path}"; then
        overall_status=2
    fi
done

exit "${overall_status}"
