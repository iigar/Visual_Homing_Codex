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

triple_second() {
    local value="$1"
    value="${value#*/}"
    printf '%s' "${value%%/*}"
}

ms_to_seconds() {
    local value="$1"
    if [[ ! "${value}" =~ ^[0-9]+([.][0-9]+)?$ ]]; then
        printf ''
        return
    fi
    awk -v ms="${value}" 'BEGIN { printf "%.2f", ms / 1000.0 }'
}

range_delta() {
    local value="$1"
    if [[ ! "${value}" =~ ^-?[0-9]+([.][0-9]+)?[.][.]-?[0-9]+([.][0-9]+)?$ ]]; then
        printf ''
        return
    fi
    local start="${value%%..*}"
    local end="${value##*..}"
    awk -v start="${start}" -v end="${end}" 'BEGIN { printf "%.2f", start - end }'
}

summarize_log() {
    local log_path="$1"
    if [[ ! -f "${log_path}" ]]; then
        echo "BLOCKED | log missing | ${log_path}"
        return 1
    fi

    local compact_line done_line
    compact_line="$(grep '^live_route_match_compact ' "${log_path}" | tail -1 || true)"
    done_line="$(grep '^live_route_match_done ' "${log_path}" | tail -1 || true)"
    if [[ -z "${compact_line}" ]]; then
        echo "BLOCKED | missing route summary | ${log_path}"
        return 1
    fi

    local status reason endpoint endpoint_stop stop_reason frames fps elapsed_s confidence altitude tracked_progress tracked_delta
    status="$(extract_field "${compact_line}" external_nav_operator_readiness)"
    reason="$(extract_field "${compact_line}" external_nav_operator_reason)"
    endpoint="$(extract_field "${compact_line}" endpoint_passed)"
    endpoint_stop="$(extract_field "${compact_line}" endpoint_stop)"
    stop_reason="$(extract_field "${compact_line}" stop_reason)"
    frames="$(extract_field "${compact_line}" frames)"
    fps="$(extract_field "${done_line}" effective_fps)"
    elapsed_s="$(ms_to_seconds "$(extract_field "${done_line}" elapsed_ms)")"
    confidence="$(extract_field "${compact_line}" confidence_min_avg)"
    altitude="$(extract_field "${compact_line}" external_nav_relative_altitude_min_avg_max_m)"
    tracked_progress="$(extract_field "${compact_line}" tracked_progress)"
    tracked_delta="$(range_delta "${tracked_progress}")"

    local label="BLOCKED"
    if [[ "${status}" == "ready" ]]; then
        label="READY"
    elif [[ "${status}" == "marginal" ]]; then
        label="MARGINAL"
    fi

    local route_text="route incomplete"
    if [[ "${endpoint}" == "true" ]]; then
        route_text="route complete"
    fi

    local stop_text="${elapsed_s}s"
    if [[ "${endpoint_stop}" == "true" ]]; then
        stop_text="endpoint ${elapsed_s}s"
    fi

    echo "${label} | ${route_text} | ${stop_text} | alt $(triple_second "${altitude}")m | FPS ${fps} | conf ${confidence} | reason ${reason}"
    echo "detail | frames ${frames} | tracked_delta ${tracked_delta} | endpoint_stop ${endpoint_stop} | stop ${stop_reason} | log ${log_path}"
}

status=0
for log_path in "$@"; do
    summarize_log "${log_path}" || status=1
done

exit "${status}"
