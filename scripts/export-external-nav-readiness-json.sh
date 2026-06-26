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
nominal_route_length_m="${VISUAL_HOMING_EXTERNAL_NAV_NOMINAL_ROUTE_LENGTH_M:-unknown}"
requested_handoff_distance_m="${VISUAL_HOMING_HANDOFF_REQUESTED_DISTANCE_M:-}"
requested_handoff_altitude_m="${VISUAL_HOMING_HANDOFF_REQUESTED_ALTITUDE_M:-}"

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
tracked_progress="$(field tracked_progress)"
tracked_minmax_progress="$(field tracked_minmax_progress)"
tracked_directional_progress="$(field tracked_directional_progress)"
tracked_regressions="$(field tracked_regressions)"
tracked_rollback="$(field tracked_rollback)"
confidence_min_avg="$(field confidence_min_avg)"
dry_run_valid="$(field dry_run_valid)"
external_nav_valid="$(field external_nav_valid)"
altitude_min_avg_max="$(field external_nav_relative_altitude_min_avg_max_m)"
visual_scale_valid="$(field visual_scale_valid)"
visual_scale_ratio_min_avg_max="$(field visual_scale_ratio_min_avg_max)"
visual_scale_confidence_min_avg="$(field visual_scale_confidence_min_avg)"
visual_scale_ratio_histogram="$(field visual_scale_ratio_histogram)"

route_complete=false
if [[ "$(field endpoint_passed)" == "true" && "$(field progress_gate_passed)" == "true" ]]; then
    route_complete=true
fi

operator_readiness="$(field external_nav_operator_readiness)"
operator_reason="$(field external_nav_operator_reason)"
visual_homing_ready=false
if [[ "${operator_readiness}" == "ready" ]]; then
    visual_homing_ready=true
fi

handoff_candidate=false
handoff_decision=blocked
handoff_reason="${operator_reason}"
if [[ "${route_complete}" == "true" && "${visual_homing_ready}" == "true" ]]; then
    handoff_candidate=true
    handoff_decision=candidate_only
    handoff_reason=jt_zero_not_integrated
elif [[ "${route_complete}" != "true" ]]; then
    handoff_reason=route_not_complete
elif [[ "${operator_readiness}" == "marginal" ]]; then
    handoff_reason=visual_homing_marginal
elif [[ -z "${handoff_reason}" || "${operator_readiness}" != "blocked" ]]; then
    handoff_reason=visual_homing_not_ready
fi

histogram_json_object() {
    local value="$1"
    if [[ -z "${value}" || "${value}" == "none" ]]; then
        printf '{}'
        return
    fi

    local output="{"
    local first=1
    local item
    local key
    local count
    IFS=',' read -ra items <<< "${value}"
    for item in "${items[@]}"; do
        key="${item%%:*}"
        count="${item##*:}"
        if [[ -z "${key}" || -z "${count}" ]]; then
            continue
        fi
        if [[ "${first}" == "0" ]]; then
            output+=", "
        fi
        first=0
        output+="\"$(json_string "${key}")\": $(json_number "${count}")"
    done
    output+="}"
    printf '%s' "${output}"
}

json="$(
cat <<EOF
{
  "schema": "visual_homing.external_nav_readiness.v1",
  "source_log": "$(json_string "${log_path}")",
  "operator_inputs": {
    "altitude_preset": "$(json_string "${altitude_preset}")",
    "requested_handoff_distance_m": $(json_number "${requested_handoff_distance_m}"),
    "requested_handoff_altitude_m": $(json_number "${requested_handoff_altitude_m}")
  },
  "resolved_config": {
    "altitude_expected_m": $(json_number "$(field external_nav_expected_relative_altitude_m)"),
    "altitude_tolerance_m": $(json_number "$(field external_nav_expected_relative_altitude_tolerance_m)"),
    "nominal_route_length_m": $(json_number "${nominal_route_length_m}"),
    "handoff_distance_supported": false,
    "handoff_distance_reason": "route_metric_scale_not_authoritative"
  },
  "operator": {
    "readiness": "$(json_string "${operator_readiness}")",
    "reason": "$(json_string "${operator_reason}")"
  },
  "handoff": {
    "route_complete": $(json_bool "${route_complete}"),
    "visual_homing_ready": $(json_bool "${visual_homing_ready}"),
    "candidate": $(json_bool "${handoff_candidate}"),
    "decision": "$(json_string "${handoff_decision}")",
    "reason": "$(json_string "${handoff_reason}")"
  },
  "jt_zero": {
    "available": false,
    "ready": false,
    "reason": "not_integrated"
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
    "tracked_progress_start": $(json_number "$(range_left "${tracked_progress}")"),
    "tracked_progress_end": $(json_number "$(range_right "${tracked_progress}")"),
    "tracked_progress_min": $(json_number "$(range_left "${tracked_minmax_progress}")"),
    "tracked_progress_max": $(json_number "$(range_right "${tracked_minmax_progress}")"),
    "tracked_directional_progress": $(json_bool "${tracked_directional_progress}"),
    "tracked_regressions_forward": $(json_number "$(pair_left "${tracked_regressions}")"),
    "tracked_regressions_reverse": $(json_number "$(pair_right "${tracked_regressions}")"),
    "tracked_rollback_forward": $(json_number "$(pair_left "${tracked_rollback}")"),
    "tracked_rollback_reverse": $(json_number "$(pair_right "${tracked_rollback}")"),
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
    "confidence_avg": $(json_number "$(pair_right "${visual_scale_confidence_min_avg}")"),
    "ratio_histogram": "$(json_string "${visual_scale_ratio_histogram}")",
    "ratio_histogram_counts": $(histogram_json_object "${visual_scale_ratio_histogram}")
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
