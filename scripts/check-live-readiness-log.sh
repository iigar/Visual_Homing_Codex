#!/usr/bin/env bash
set -euo pipefail

expected_gate_reasons="${VISUAL_HOMING_EXPECTED_LIVE_OUTPUT_GATE_BLOCK_REASONS:-vehicle_not_armed:150}"

usage() {
    echo "usage: $0 <test-core-pi-log> [<test-core-pi-log> ...]" >&2
}

if [[ "$#" -lt 1 ]]; then
    usage
    exit 2
fi

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
        echo "readiness_log_check path=${log_path} passed=false missing_field=${key}" >&2
        return 1
    fi
    if [[ "${actual}" != "${expected}" ]]; then
        echo "readiness_log_check path=${log_path} passed=false field=${key} expected=${expected} actual=${actual}" >&2
        return 1
    fi
}

check_log() {
    local log_path="$1"
    if [[ ! -f "${log_path}" ]]; then
        echo "readiness_log_check path=${log_path} passed=false error=log_not_found" >&2
        return 1
    fi

    local compact_line
    compact_line="$(grep '^live_route_match_compact ' "${log_path}" | tail -1 || true)"
    if [[ -z "${compact_line}" ]]; then
        echo "readiness_log_check path=${log_path} passed=false error=missing_live_route_match_compact" >&2
        return 1
    fi

    local failed=0
    require_field "${log_path}" "${compact_line}" "passed" "true" || failed=1
    require_field "${log_path}" "${compact_line}" "frames" "150/150" || failed=1
    require_field "${log_path}" "${compact_line}" "valid_matches" "150" || failed=1
    require_field "${log_path}" "${compact_line}" "endpoint_passed" "true" || failed=1
    require_field "${log_path}" "${compact_line}" "progress_gate_passed" "true" || failed=1
    require_field "${log_path}" "${compact_line}" "telemetry_health" "true" || failed=1
    require_field "${log_path}" "${compact_line}" "telemetry_dropped" "0" || failed=1
    require_field "${log_path}" "${compact_line}" "dry_run_quality" "true" || failed=1
    require_field "${log_path}" "${compact_line}" "dry_run_valid" "150/150" || failed=1
    require_field "${log_path}" "${compact_line}" "live_output_gate_allowed" "0" || failed=1
    require_field "${log_path}" "${compact_line}" "live_output_gate_blocked" "150" || failed=1

    if [[ "${expected_gate_reasons}" != "any" ]]; then
        require_field "${log_path}" "${compact_line}" "live_output_gate_block_reasons" "${expected_gate_reasons}" || failed=1
    fi

    if [[ "${failed}" != "0" ]]; then
        return 1
    fi

    local gate_reasons
    gate_reasons="$(extract_field "${compact_line}" "live_output_gate_block_reasons")"
    echo "readiness_log_check path=${log_path} passed=true live_output_gate_block_reasons=${gate_reasons}"
}

overall_status=0
for log_path in "$@"; do
    if ! check_log "${log_path}"; then
        overall_status=2
    fi
done

exit "${overall_status}"
