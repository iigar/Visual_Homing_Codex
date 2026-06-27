#!/usr/bin/env bash
set -euo pipefail

usage() {
    echo "usage: $0 <external-nav-dry-run.log> [<external-nav-dry-run.log> ...]" >&2
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
    printf '%s' "${token:-}"
}

triple_first() {
    local value="$1"
    printf '%s' "${value%%/*}"
}

triple_second() {
    local value="$1"
    value="${value#*/}"
    printf '%s' "${value%%/*}"
}

range_delta() {
    local value="$1"
    if [[ ! "${value}" =~ ^-?[0-9]+([.][0-9]+)?[.][.]-?[0-9]+([.][0-9]+)?$ ]]; then
        printf ''
        return
    fi
    local start="${value%%..*}"
    local end="${value##*..}"
    awk -v start="${start}" -v end="${end}" 'BEGIN { printf "%.6f", start - end }'
}

ms_to_seconds() {
    local value="$1"
    if [[ ! "${value}" =~ ^[0-9]+([.][0-9]+)?$ ]]; then
        printf ''
        return
    fi
    awk -v ms="${value}" 'BEGIN { printf "%.3f", ms / 1000.0 }'
}

print_header() {
    printf 'log\tpassed\toperator\treason\tframes\tfps\telapsed_ms\tendpoint_time_s\tstop_reason\tconfidence_min\tconfidence_avg\talt_min\talt_avg\talt_max\tprogress\tprogress_delta\ttracked_progress\ttracked_delta\ttracked_ok\tendpoint\tendpoint_stop\tquality\tdry_run\tscale_avg\ttracked_scale\tscale_histogram\n'
}

summarize_log() {
    local log_path="$1"
    if [[ ! -f "${log_path}" ]]; then
        printf '%s\tmissing_log\n' "${log_path}"
        return 1
    fi

    local compact_line done_line
    compact_line="$(grep '^live_route_match_compact ' "${log_path}" | tail -1 || true)"
    done_line="$(grep '^live_route_match_done ' "${log_path}" | tail -1 || true)"
    if [[ -z "${compact_line}" ]]; then
        printf '%s\tmissing_compact_line\n' "${log_path}"
        return 1
    fi

    local confidence altitude scale_ratio progress tracked_progress elapsed_ms
    confidence="$(extract_field "${compact_line}" confidence_min_avg)"
    altitude="$(extract_field "${compact_line}" external_nav_relative_altitude_min_avg_max_m)"
    scale_ratio="$(extract_field "${compact_line}" visual_scale_ratio_min_avg_max)"
    progress="$(extract_field "${compact_line}" progress)"
    tracked_progress="$(extract_field "${compact_line}" tracked_progress)"
    elapsed_ms="$(extract_field "${done_line}" elapsed_ms)"

    printf '%s\t' "${log_path}"
    printf '%s\t' "$(extract_field "${compact_line}" passed)"
    printf '%s\t' "$(extract_field "${compact_line}" external_nav_operator_readiness)"
    printf '%s\t' "$(extract_field "${compact_line}" external_nav_operator_reason)"
    printf '%s\t' "$(extract_field "${compact_line}" frames)"
    printf '%s\t' "$(extract_field "${done_line}" effective_fps)"
    printf '%s\t' "${elapsed_ms}"
    printf '%s\t' "$(ms_to_seconds "${elapsed_ms}")"
    printf '%s\t' "$(extract_field "${compact_line}" stop_reason)"
    printf '%s\t' "$(triple_first "${confidence}")"
    printf '%s\t' "$(triple_second "${confidence}")"
    printf '%s\t' "$(triple_first "${altitude}")"
    printf '%s\t' "$(triple_second "${altitude}")"
    printf '%s\t' "$(printf '%s' "${altitude##*/}")"
    printf '%s\t' "${progress}"
    printf '%s\t' "$(range_delta "${progress}")"
    printf '%s\t' "${tracked_progress}"
    printf '%s\t' "$(range_delta "${tracked_progress}")"
    printf '%s\t' "$(extract_field "${compact_line}" tracked_directional_progress)"
    printf '%s\t' "$(extract_field "${compact_line}" endpoint_passed)"
    printf '%s\t' "$(extract_field "${compact_line}" endpoint_stop)"
    printf '%s\t' "$(extract_field "${compact_line}" dry_run_quality)"
    printf '%s\t' "$(extract_field "${compact_line}" dry_run_valid)"
    printf '%s\t' "$(triple_second "${scale_ratio}")"
    printf '%s\t' "$(extract_field "${compact_line}" tracked_visual_scale_ratio)"
    printf '%s\n' "$(extract_field "${compact_line}" visual_scale_ratio_histogram)"
}

print_header
status=0
for log_path in "$@"; do
    summarize_log "${log_path}" || status=1
done

exit "${status}"
