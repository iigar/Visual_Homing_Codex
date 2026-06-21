#!/usr/bin/env bash
set -euo pipefail

usage() {
    echo "usage: $0 <test-core-pi-log> [output.json]" >&2
}

if [[ "$#" -lt 1 || "$#" -gt 2 ]]; then
    usage
    exit 2
fi

log_path="$1"
output_path="${2:-}"
altitude_preset="${VISUAL_HOMING_EXTERNAL_NAV_ALTITUDE_PRESET:-unknown}"

if [[ ! -f "${log_path}" ]]; then
    echo "external_nav_readiness_json passed=false error=log_not_found path=${log_path}" >&2
    exit 2
fi

compact_line="$(grep '^live_route_match_compact ' "${log_path}" | tail -1 || true)"
if [[ -z "${compact_line}" ]]; then
    echo "external_nav_readiness_json passed=false error=missing_live_route_match_compact path=${log_path}" >&2
    exit 2
fi

extract_field() {
    local line="$1"
    local key="$2"
    local token
    token="$(printf '%s\n' "${line}" | tr ' ' '\n' | awk -F= -v key="${key}" '$1 == key { print $2; exit }')"
    printf '%s' "${token}"
}

json_string() {
    printf '%s' "$1" | sed 's/\\/\\\\/g; s/"/\\"/g; s/	/\\t/g'
}

json_bool() {
    case "$1" in
        true|false)
            printf '%s' "$1"
            ;;
        *)
            printf 'null'
            ;;
    esac
}

json_number() {
    if [[ "$1" =~ ^-?[0-9]+([.][0-9]+)?([eE][-+]?[0-9]+)?$ ]]; then
        printf '%s' "$1"
    else
        printf 'null'
    fi
}

field() {
    extract_field "${compact_line}" "$1"
}

pair_left() {
    local value="$1"
    printf '%s' "${value%%/*}"
}

pair_right() {
    local value="$1"
    printf '%s' "${value#*/}"
}

range_left() {
    local value="$1"
    printf '%s' "${value%%..*}"
}

range_right() {
    local value="$1"
    printf '%s' "${value##*..}"
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

triple_third() {
    local value="$1"
    printf '%s' "${value##*/}"
}

frames="$(field frames)"
progress="$(field progress)"
minmax_progress="$(field minmax_progress)"
confidence_min_avg="$(field confidence_min_avg)"
dry_run_valid="$(field dry_run_valid)"
external_nav_valid="$(field external_nav_valid)"
altitude_min_avg_max="$(field external_nav_relative_altitude_min_avg_max_m)"
visual_scale_valid="$(field visual_scale_valid)"
visual_scale_ratio_min_avg_max="$(field visual_scale_ratio_min_avg_max)"
visual_scale_confidence_min_avg="$(field visual_scale_confidence_min_avg)"

json="$(
cat <<EOF
{
  "schema": "visual_homing.external_nav_readiness.v1",
  "source_log": "$(json_string "${log_path}")",
  "operator": {
    "readiness": "$(json_string "$(field external_nav_operator_readiness)")",
    "reason": "$(json_string "$(field external_nav_operator_reason)")"
  },
  "route": {
    "passed": $(json_bool "$(field passed)"),
    "frames_captured": $(json_number "$(pair_left "${frames}")"),
    "frames_requested": $(json_number "$(pair_right "${frames}")"),
    "valid_matches": $(json_number "$(field valid_matches)"),
    "progress_start": $(json_number "$(range_left "${progress}")"),
    "progress_end": $(json_number "$(range_right "${progress}")"),
    "progress_min": $(json_number "$(range_left "${minmax_progress}")"),
    "progress_max": $(json_number "$(range_right "${minmax_progress}")"),
    "endpoint_passed": $(json_bool "$(field endpoint_passed)"),
    "progress_gate_passed": $(json_bool "$(field progress_gate_passed)"),
    "endpoint_stop": $(json_bool "$(field endpoint_stop)"),
    "stop_reason": "$(json_string "$(field stop_reason)")",
    "confidence_min": $(json_number "$(pair_left "${confidence_min_avg}")"),
    "confidence_avg": $(json_number "$(pair_right "${confidence_min_avg}")")
  },
  "altitude": {
    "preset": "$(json_string "${altitude_preset}")",
    "expected_required": $(json_bool "$(field external_nav_expected_relative_altitude_required)"),
    "expected_m": $(json_number "$(field external_nav_expected_relative_altitude_m)"),
    "tolerance_m": $(json_number "$(field external_nav_expected_relative_altitude_tolerance_m)"),
    "observed_min_m": $(json_number "$(triple_first "${altitude_min_avg_max}")"),
    "observed_avg_m": $(json_number "$(triple_second "${altitude_min_avg_max}")"),
    "observed_max_m": $(json_number "$(triple_third "${altitude_min_avg_max}")"),
    "window_passed": $(json_bool "$(field external_nav_relative_altitude_window_passed)"),
    "blocker": "$(json_string "$(field external_nav_altitude_blocker)")"
  },
  "telemetry": {
    "health": $(json_bool "$(field telemetry_health)"),
    "dropped_bytes": $(json_number "$(field telemetry_dropped)"),
    "mode": "$(json_string "$(field external_nav_latest_telemetry_mode)")",
    "armed": $(json_bool "$(field external_nav_latest_telemetry_armed)")
  },
  "dry_run_command": {
    "quality": $(json_bool "$(field dry_run_quality)"),
    "valid": $(json_number "$(pair_left "${dry_run_valid}")"),
    "total": $(json_number "$(pair_right "${dry_run_valid}")")
  },
  "external_nav": {
    "session_ready": $(json_bool "$(field external_nav_session_ready)"),
    "session_reason": "$(json_string "$(field external_nav_session_reason)")",
    "strict_session_ready": $(json_bool "$(field external_nav_strict_session_ready)"),
    "strict_session_reason": "$(json_string "$(field external_nav_strict_session_reason)")",
    "quality_ready": $(json_bool "$(field external_nav_quality_ready)"),
    "quality_reason": "$(json_string "$(field external_nav_quality_reason)")",
    "valid": $(json_number "$(pair_left "${external_nav_valid}")"),
    "total": $(json_number "$(pair_right "${external_nav_valid}")"),
    "valid_fraction": $(json_number "$(field external_nav_valid_fraction)"),
    "max_invalid_streak": $(json_number "$(field external_nav_max_invalid_streak)"),
    "invalid_reasons": "$(json_string "$(field external_nav_invalid_reasons)")"
  },
  "visual_scale": {
    "required": $(json_bool "$(field visual_scale_required)"),
    "valid": $(json_number "$(pair_left "${visual_scale_valid}")"),
    "total": $(json_number "$(pair_right "${visual_scale_valid}")"),
    "valid_fraction": $(json_number "$(field visual_scale_valid_fraction)"),
    "ratio_min": $(json_number "$(triple_first "${visual_scale_ratio_min_avg_max}")"),
    "ratio_avg": $(json_number "$(triple_second "${visual_scale_ratio_min_avg_max}")"),
    "ratio_max": $(json_number "$(triple_third "${visual_scale_ratio_min_avg_max}")"),
    "confidence_min": $(json_number "$(pair_left "${visual_scale_confidence_min_avg}")"),
    "confidence_avg": $(json_number "$(pair_right "${visual_scale_confidence_min_avg}")")
  },
  "safety_gate": {
    "live_output_allowed": $(json_number "$(field live_output_gate_allowed)"),
    "live_output_blocked": $(json_number "$(field live_output_gate_blocked)"),
    "block_reasons": "$(json_string "$(field live_output_gate_block_reasons)")",
    "final_reason": "$(json_string "$(field final_live_output_gate_reason)")"
  }
}
EOF
)"

if [[ -n "${output_path}" ]]; then
    mkdir -p "$(dirname "${output_path}")"
    printf '%s\n' "${json}" > "${output_path}"
    echo "external_nav_readiness_json path=${output_path} schema=visual_homing.external_nav_readiness.v1"
else
    printf '%s\n' "${json}"
fi
