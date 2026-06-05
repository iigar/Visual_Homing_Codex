#!/usr/bin/env bash
set -euo pipefail

expected_entries="${VISUAL_HOMING_EXPECTED_ROUTE_QUALITY_ENTRIES:-any}"
require_exact_self_match="${VISUAL_HOMING_REQUIRE_EXACT_ROUTE_SELF_MATCH:-1}"
allow_quality_warning="${VISUAL_HOMING_ALLOW_ROUTE_QUALITY_WARNING:-0}"

usage() {
    echo "usage: $0 <test-core-pi-route-log> [<test-core-pi-route-log> ...]" >&2
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
        echo "route_quality_log_check path=${log_path} passed=false missing_field=${key}" >&2
        return 1
    fi
    if [[ "${expected}" != "any" && "${actual}" != "${expected}" ]]; then
        echo "route_quality_log_check path=${log_path} passed=false field=${key} expected=${expected} actual=${actual}" >&2
        return 1
    fi
}

require_equal_fields() {
    local log_path="$1"
    local line="$2"
    local left_key="$3"
    local right_key="$4"
    local left right
    left="$(extract_field "${line}" "${left_key}")"
    right="$(extract_field "${line}" "${right_key}")"
    if [[ -z "${left}" || -z "${right}" ]]; then
        echo "route_quality_log_check path=${log_path} passed=false missing_field=${left_key}_or_${right_key}" >&2
        return 1
    fi
    if [[ "${left}" != "${right}" ]]; then
        echo "route_quality_log_check path=${log_path} passed=false field_mismatch=${left_key}:${right_key} left=${left} right=${right}" >&2
        return 1
    fi
}

check_log() {
    local log_path="$1"
    if [[ ! -f "${log_path}" ]]; then
        echo "route_quality_log_check path=${log_path} passed=false error=log_not_found" >&2
        return 1
    fi

    local record_line self_match_line perturb_line distinctiveness_line
    record_line="$(grep '^live_route_record_done ' "${log_path}" | tail -1 || true)"
    self_match_line="$(grep '^route_self_match ' "${log_path}" | tail -1 || true)"
    perturb_line="$(grep '^route_perturb_check ' "${log_path}" | tail -1 || true)"
    distinctiveness_line="$(grep '^route_distinctiveness ' "${log_path}" | tail -1 || true)"

    local failed=0

    if [[ -n "${record_line}" ]]; then
        require_field "${log_path}" "${record_line}" "route_written" "true" || failed=1
        require_equal_fields "${log_path}" "${record_line}" "frames_captured" "entries" || failed=1
        if [[ "${expected_entries}" != "any" ]]; then
            require_field "${log_path}" "${record_line}" "entries" "${expected_entries}" || failed=1
        fi
    fi

    if [[ -z "${self_match_line}" ]]; then
        echo "route_quality_log_check path=${log_path} passed=false error=missing_route_self_match" >&2
        failed=1
    else
        require_field "${log_path}" "${self_match_line}" "passed" "true" || failed=1
        require_equal_fields "${log_path}" "${self_match_line}" "entries_checked" "valid_matches" || failed=1
        if [[ "${require_exact_self_match}" == "1" ]]; then
            require_equal_fields "${log_path}" "${self_match_line}" "entries_checked" "exact_index_matches" || failed=1
        fi
        if [[ "${expected_entries}" != "any" ]]; then
            require_field "${log_path}" "${self_match_line}" "entries_checked" "${expected_entries}" || failed=1
        fi
    fi

    if [[ -z "${perturb_line}" ]]; then
        echo "route_quality_log_check path=${log_path} passed=false error=missing_route_perturb_check" >&2
        failed=1
    else
        require_field "${log_path}" "${perturb_line}" "passed" "true" || failed=1
        require_equal_fields "${log_path}" "${perturb_line}" "entries_checked" "brightness_valid_matches" || failed=1
        require_equal_fields "${log_path}" "${perturb_line}" "entries_checked" "noise_valid_matches" || failed=1
        require_equal_fields "${log_path}" "${perturb_line}" "entries_checked" "shift_valid_matches" || failed=1
        require_field "${log_path}" "${perturb_line}" "malformed_rejected" "true" || failed=1
        if [[ "${expected_entries}" != "any" ]]; then
            require_field "${log_path}" "${perturb_line}" "entries_checked" "${expected_entries}" || failed=1
        fi
    fi

    if [[ -z "${distinctiveness_line}" ]]; then
        echo "route_quality_log_check path=${log_path} passed=false error=missing_route_distinctiveness" >&2
        failed=1
    else
        if [[ "${allow_quality_warning}" == "1" ]]; then
            require_field "${log_path}" "${distinctiveness_line}" "quality_pass" "any" || failed=1
        else
            require_field "${log_path}" "${distinctiveness_line}" "quality_pass" "true" || failed=1
        fi
        require_field "${log_path}" "${distinctiveness_line}" "exact_duplicate_entries" "0" || failed=1
        if [[ "${expected_entries}" != "any" ]]; then
            require_field "${log_path}" "${distinctiveness_line}" "entries_checked" "${expected_entries}" || failed=1
        fi
    fi

    if [[ "${failed}" != "0" ]]; then
        return 1
    fi

    local entries quality_pass warning low_texture ambiguous average_nearest
    entries="$(extract_field "${distinctiveness_line}" "entries_checked")"
    quality_pass="$(extract_field "${distinctiveness_line}" "quality_pass")"
    warning="$(extract_field "${distinctiveness_line}" "warning")"
    low_texture="$(extract_field "${distinctiveness_line}" "low_texture_fraction")"
    ambiguous="$(extract_field "${distinctiveness_line}" "ambiguous_nearest_fraction")"
    average_nearest="$(extract_field "${distinctiveness_line}" "average_nearest_mean_abs_diff")"

    echo "route_quality_log_check path=${log_path} passed=true entries=${entries} quality_pass=${quality_pass} warning=${warning} low_texture_fraction=${low_texture} ambiguous_nearest_fraction=${ambiguous} average_nearest_mean_abs_diff=${average_nearest}"
}

overall_status=0
for log_path in "$@"; do
    if ! check_log "${log_path}"; then
        overall_status=2
    fi
done

exit "${overall_status}"
