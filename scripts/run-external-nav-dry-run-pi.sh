#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
stamp="$(date -u +"%Y%m%dT%H%M%SZ")"
log_dir="${VISUAL_HOMING_LOG_DIR:-${repo_root}/artifacts/logs}"
preflight_log="${VISUAL_HOMING_EXTERNAL_NAV_PREFLIGHT_LOG:-${log_dir}/external-nav-preflight-${stamp}.log}"
run_log="${VISUAL_HOMING_RUN_LOG:-${log_dir}/external-nav-dry-run-${stamp}.log}"
readiness_json="${VISUAL_HOMING_EXTERNAL_NAV_READINESS_JSON:-${run_log%.log}.json}"

altitude_preset="${VISUAL_HOMING_EXTERNAL_NAV_ALTITUDE_PRESET:-stand}"
case "${altitude_preset}" in
    floor)
        default_expected_altitude_m=0.05
        default_altitude_tolerance_m=0.15
        ;;
    stand)
        default_expected_altitude_m=0.5
        default_altitude_tolerance_m=0.25
        ;;
    custom)
        if [[ -z "${VISUAL_HOMING_EXTERNAL_NAV_EXPECTED_RELATIVE_ALTITUDE_M:-}" \
            || -z "${VISUAL_HOMING_EXTERNAL_NAV_EXPECTED_RELATIVE_ALTITUDE_TOLERANCE_M:-}" ]]; then
            echo "VISUAL_HOMING_EXTERNAL_NAV_ALTITUDE_PRESET=custom requires VISUAL_HOMING_EXTERNAL_NAV_EXPECTED_RELATIVE_ALTITUDE_M and VISUAL_HOMING_EXTERNAL_NAV_EXPECTED_RELATIVE_ALTITUDE_TOLERANCE_M" >&2
            exit 2
        fi
        default_expected_altitude_m=0
        default_altitude_tolerance_m=0
        ;;
    *)
        echo "VISUAL_HOMING_EXTERNAL_NAV_ALTITUDE_PRESET must be one of: custom, floor, stand" >&2
        exit 2
        ;;
esac
expected_altitude_m="${VISUAL_HOMING_EXTERNAL_NAV_EXPECTED_RELATIVE_ALTITUDE_M:-${default_expected_altitude_m}}"
expected_altitude_tolerance_m="${VISUAL_HOMING_EXTERNAL_NAV_EXPECTED_RELATIVE_ALTITUDE_TOLERANCE_M:-${default_altitude_tolerance_m}}"
external_nav_minimum_match_confidence="${VISUAL_HOMING_EXTERNAL_NAV_MINIMUM_MATCH_CONFIDENCE:-0.82}"
nominal_route_length_m="${VISUAL_HOMING_EXTERNAL_NAV_NOMINAL_ROUTE_LENGTH_M:-1.0}"
visual_scale_diagnostics="${VISUAL_HOMING_VISUAL_SCALE_DIAGNOSTICS:-0}"
visual_scale_reference_altitude_m="${VISUAL_HOMING_VISUAL_SCALE_REFERENCE_ALTITUDE_M:-${expected_altitude_m}}"
frames="${VISUAL_HOMING_CAMERA_FRAMES:-150}"
target_width="${VISUAL_HOMING_CAMERA_TARGET_WIDTH:-64}"
target_height="${VISUAL_HOMING_CAMERA_TARGET_HEIGHT:-48}"
preflight_duration_ms="${VISUAL_HOMING_EXTERNAL_NAV_PREFLIGHT_DURATION_MS:-15000}"
operator_cue_seconds="${VISUAL_HOMING_OPERATOR_CUE_SECONDS:-5}"

mkdir -p "${log_dir}"

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

extract_field() {
    local line="$1"
    local key="$2"
    local token
    token="$(printf '%s\n' "${line}" | tr ' ' '\n' | awk -F= -v key="${key}" '$1 == key { print $2; exit }')"
    printf '%s' "${token}"
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

write_preflight_blocked_json() {
    local sanity_line="$1"
    local reason
    local altitude_min_avg_max
    local mode
    local armed
    local telemetry_health
    local relative_altitude_samples
    local relative_altitude_window_m
    local relative_altitude_span_m
    local distance_sensor_seen

    reason="$(extract_field "${sanity_line}" reason)"
    altitude_min_avg_max="$(extract_field "${sanity_line}" relative_altitude_min_avg_max_m)"
    mode="$(extract_field "${sanity_line}" mode)"
    armed="$(extract_field "${sanity_line}" armed)"
    relative_altitude_samples="$(extract_field "${sanity_line}" relative_altitude_samples)"
    relative_altitude_window_m="$(extract_field "${sanity_line}" relative_altitude_window_m)"
    relative_altitude_span_m="$(extract_field "${sanity_line}" relative_altitude_span_m)"
    distance_sensor_seen="$(extract_field "${sanity_line}" distance_sensor_seen)"
    telemetry_health=false
    if [[ "$(extract_field "${sanity_line}" relative_altitude_seen)" == "true" ]]; then
        telemetry_health=true
    fi

    mkdir -p "$(dirname "${readiness_json}")"
    cat > "${readiness_json}" <<EOF
{
  "schema": "visual_homing.external_nav_readiness.v1",
  "source_log": "$(json_string "${preflight_log}")",
  "stage": "preflight",
  "operator_inputs": {
    "altitude_preset": "$(json_string "${altitude_preset}")",
    "requested_handoff_distance_m": $(json_number "${VISUAL_HOMING_HANDOFF_REQUESTED_DISTANCE_M:-}"),
    "requested_handoff_altitude_m": $(json_number "${VISUAL_HOMING_HANDOFF_REQUESTED_ALTITUDE_M:-}")
  },
  "resolved_config": {
    "altitude_expected_m": $(json_number "${expected_altitude_m}"),
    "altitude_tolerance_m": $(json_number "${expected_altitude_tolerance_m}"),
    "nominal_route_length_m": $(json_number "${nominal_route_length_m}"),
    "handoff_distance_supported": false,
    "handoff_distance_reason": "route_metric_scale_not_authoritative"
  },
  "operator": {
    "readiness": "blocked",
    "reason": "$(json_string "${reason}")"
  },
  "handoff": {
    "route_complete": false,
    "visual_homing_ready": false,
    "candidate": false,
    "decision": "blocked",
    "reason": "$(json_string "${reason}")"
  },
  "jt_zero": {
    "available": false,
    "ready": false,
    "reason": "not_integrated"
  },
  "route": {
    "passed": false,
    "frames_captured": 0,
    "frames_requested": $(json_number "${frames}"),
    "valid_matches": 0,
    "progress_start": null,
    "progress_end": null,
    "progress_min": null,
    "progress_max": null,
    "endpoint_passed": false,
    "progress_gate_passed": false,
    "endpoint_stop": false,
    "stop_reason": "preflight_failed",
    "confidence_min": null,
    "confidence_avg": null
  },
  "altitude": {
    "preset": "$(json_string "${altitude_preset}")",
    "expected_required": true,
    "expected_m": $(json_number "${expected_altitude_m}"),
    "tolerance_m": $(json_number "${expected_altitude_tolerance_m}"),
    "observed_min_m": $(json_number "$(triple_first "${altitude_min_avg_max}")"),
    "observed_avg_m": $(json_number "$(triple_second "${altitude_min_avg_max}")"),
    "observed_max_m": $(json_number "$(triple_third "${altitude_min_avg_max}")"),
    "window_passed": false,
    "blocker": "$(json_string "${reason}")"
  },
  "telemetry": {
    "health": $(json_bool "${telemetry_health}"),
    "dropped_bytes": null,
    "mode": "$(json_string "${mode}")",
    "armed": $(json_bool "${armed}")
  },
  "dry_run_command": {
    "quality": false,
    "valid": 0,
    "total": 0
  },
  "external_nav": {
    "session_ready": false,
    "session_reason": "$(json_string "${reason}")",
    "strict_session_ready": false,
    "strict_session_reason": "$(json_string "${reason}")",
    "quality_ready": false,
    "quality_reason": "$(json_string "${reason}")",
    "valid": 0,
    "total": 0,
    "valid_fraction": 0,
    "max_invalid_streak": 0,
    "invalid_reasons": "$(json_string "${reason}")"
  },
  "visual_scale": {
    "required": false,
    "valid": 0,
    "total": 0,
    "valid_fraction": 0,
    "ratio_min": 0,
    "ratio_avg": 0,
    "ratio_max": 0,
    "confidence_min": 0,
    "confidence_avg": 0
  },
  "safety_gate": {
    "live_output_allowed": 0,
    "live_output_blocked": 0,
    "block_reasons": "preflight_failed",
    "final_reason": "preflight_failed"
  },
  "preflight": {
    "passed": false,
    "reason": "$(json_string "${reason}")",
    "log_path": "$(json_string "${preflight_log}")",
    "relative_altitude_samples": $(json_number "${relative_altitude_samples}"),
    "relative_altitude_window_m": "$(json_string "${relative_altitude_window_m}")",
    "relative_altitude_span_m": $(json_number "${relative_altitude_span_m}"),
    "distance_sensor_seen": $(json_bool "${distance_sensor_seen}")
  }
}
EOF
    echo "external_nav_readiness_json path=${readiness_json} schema=visual_homing.external_nav_readiness.v1 stage=preflight reason=${reason}"
}

export_route_readiness_json() {
    VISUAL_HOMING_EXTERNAL_NAV_ALTITUDE_PRESET="${altitude_preset}" \
    VISUAL_HOMING_EXTERNAL_NAV_NOMINAL_ROUTE_LENGTH_M="${nominal_route_length_m}" \
    VISUAL_HOMING_HANDOFF_REQUESTED_DISTANCE_M="${VISUAL_HOMING_HANDOFF_REQUESTED_DISTANCE_M:-}" \
    VISUAL_HOMING_HANDOFF_REQUESTED_ALTITUDE_M="${VISUAL_HOMING_HANDOFF_REQUESTED_ALTITUDE_M:-}" \
    "${repo_root}/scripts/export-external-nav-readiness-json.sh" "${run_log}" "${readiness_json}"
}

cat <<EOF
###############################################################################
### EXTERNAL-NAV DRY-RUN READINESS
### This command sends no MAVLink commands and writes no external-nav MAVLink.
### It runs read-only altitude sanity first, then live route matching with
### external-nav log-quality gates.
### preflight_log=${preflight_log}
### run_log=${run_log}
### readiness_json=${readiness_json}
### altitude_preset=${altitude_preset}
### expected_relative_altitude_m=${expected_altitude_m}
### expected_relative_altitude_tolerance_m=${expected_altitude_tolerance_m}
### nominal_route_length_m=${nominal_route_length_m}
### visual_scale_diagnostics=${visual_scale_diagnostics}
### visual_scale_reference_altitude_m=${visual_scale_reference_altitude_m}
### requested_handoff_distance_m=${VISUAL_HOMING_HANDOFF_REQUESTED_DISTANCE_M:-}
### requested_handoff_altitude_m=${VISUAL_HOMING_HANDOFF_REQUESTED_ALTITUDE_M:-}
### preflight_duration_ms=${preflight_duration_ms}
### operator_cue_seconds=${operator_cue_seconds}
###############################################################################
EOF

VISUAL_HOMING_RUN_LOG="${preflight_log}" \
VISUAL_HOMING_EXTERNAL_NAV_ALTITUDE_PRESET="${altitude_preset}" \
VISUAL_HOMING_MAVLINK_TELEMETRY_DEVICE="${VISUAL_HOMING_MAVLINK_TELEMETRY_DEVICE:-/dev/serial0}" \
VISUAL_HOMING_MAVLINK_TELEMETRY_BAUD="${VISUAL_HOMING_MAVLINK_TELEMETRY_BAUD:-115200}" \
VISUAL_HOMING_MAVLINK_TELEMETRY_DURATION_MS="${preflight_duration_ms}" \
VISUAL_HOMING_EXTERNAL_NAV_EXPECTED_RELATIVE_ALTITUDE_M="${expected_altitude_m}" \
VISUAL_HOMING_EXTERNAL_NAV_EXPECTED_RELATIVE_ALTITUDE_TOLERANCE_M="${expected_altitude_tolerance_m}" \
VISUAL_HOMING_EXTERNAL_NAV_PREFLIGHT_MIN_RELATIVE_ALTITUDE_SAMPLES="${VISUAL_HOMING_EXTERNAL_NAV_PREFLIGHT_MIN_RELATIVE_ALTITUDE_SAMPLES:-5}" \
VISUAL_HOMING_EXTERNAL_NAV_MAX_RELATIVE_ALTITUDE_SPAN_M="${VISUAL_HOMING_EXTERNAL_NAV_MAX_RELATIVE_ALTITUDE_SPAN_M:-0.25}" \
VISUAL_HOMING_EXTERNAL_NAV_PREFLIGHT_REQUIRE_DISTANCE_SENSOR="${VISUAL_HOMING_EXTERNAL_NAV_PREFLIGHT_REQUIRE_DISTANCE_SENSOR:-0}" \
"${repo_root}/scripts/check-external-nav-telemetry-sanity-pi.sh" || {
    sanity_line="$(grep '^external_nav_telemetry_sanity ' "${preflight_log}" | tail -1 || true)"
    if [[ -n "${sanity_line}" ]]; then
        write_preflight_blocked_json "${sanity_line}"
    fi
    exit 2
}

VISUAL_HOMING_RUN_LOG="${run_log}" \
VISUAL_HOMING_USE_ACTIVE_CAMERA_PROFILE=1 \
VISUAL_HOMING_MATCH_LIVE_ROUTE=1 \
VISUAL_HOMING_LIVE_ROUTE_DRY_RUN_COMMANDS=1 \
VISUAL_HOMING_MATCH_LIVE_ROUTE_USE_LIVE_MAVLINK_TELEMETRY=1 \
VISUAL_HOMING_MATCH_LIVE_ROUTE_REQUIRE_LIVE_TELEMETRY_HEALTH=1 \
VISUAL_HOMING_MATCH_LIVE_ROUTE_TELEMETRY_MAX_AGE_MS="${VISUAL_HOMING_MATCH_LIVE_ROUTE_TELEMETRY_MAX_AGE_MS:-750}" \
VISUAL_HOMING_ROUTE_TELEMETRY_WARMUP_MS="${VISUAL_HOMING_ROUTE_TELEMETRY_WARMUP_MS:-5000}" \
VISUAL_HOMING_OPERATOR_CUE_SECONDS="${operator_cue_seconds}" \
VISUAL_HOMING_MAVLINK_TELEMETRY_DEVICE="${VISUAL_HOMING_MAVLINK_TELEMETRY_DEVICE:-/dev/serial0}" \
VISUAL_HOMING_MAVLINK_TELEMETRY_BAUD="${VISUAL_HOMING_MAVLINK_TELEMETRY_BAUD:-115200}" \
VISUAL_HOMING_CAMERA_FPS="${VISUAL_HOMING_CAMERA_FPS:-15}" \
VISUAL_HOMING_CAMERA_FRAMES="${frames}" \
VISUAL_HOMING_CAMERA_TARGET_WIDTH="${target_width}" \
VISUAL_HOMING_CAMERA_TARGET_HEIGHT="${target_height}" \
VISUAL_HOMING_LIVE_ROUTE_MATCH_EXPECTED_PROGRESS="${VISUAL_HOMING_LIVE_ROUTE_MATCH_EXPECTED_PROGRESS:-forward}" \
VISUAL_HOMING_LIVE_ROUTE_MATCH_REQUIRE_ENDPOINT_PROGRESS=1 \
VISUAL_HOMING_EXTERNAL_NAV_ESTIMATES=1 \
VISUAL_HOMING_EXTERNAL_NAV_NOMINAL_ROUTE_LENGTH_M="${nominal_route_length_m}" \
VISUAL_HOMING_EXTERNAL_NAV_MINIMUM_MATCH_CONFIDENCE="${external_nav_minimum_match_confidence}" \
VISUAL_HOMING_VISUAL_SCALE_DIAGNOSTICS="${visual_scale_diagnostics}" \
VISUAL_HOMING_VISUAL_SCALE_REFERENCE_ALTITUDE_M="${visual_scale_reference_altitude_m}" \
VISUAL_HOMING_EXTERNAL_NAV_EXPECTED_RELATIVE_ALTITUDE_M="${expected_altitude_m}" \
VISUAL_HOMING_EXTERNAL_NAV_EXPECTED_RELATIVE_ALTITUDE_TOLERANCE_M="${expected_altitude_tolerance_m}" \
"${repo_root}/scripts/test-core-pi.sh" || {
    if grep -q '^live_route_match_compact ' "${run_log}"; then
        export_route_readiness_json
    fi
    exit 2
}

export_route_readiness_json

VISUAL_HOMING_EXPECTED_LIVE_ROUTE_FRAMES="${VISUAL_HOMING_EXPECTED_LIVE_ROUTE_FRAMES:-${frames}/${frames}}" \
VISUAL_HOMING_EXPECTED_LIVE_ROUTE_VALID_MATCHES="${VISUAL_HOMING_EXPECTED_LIVE_ROUTE_VALID_MATCHES:-${frames}}" \
VISUAL_HOMING_EXPECTED_LIVE_ROUTE_DRY_RUN_VALID="${VISUAL_HOMING_EXPECTED_LIVE_ROUTE_DRY_RUN_VALID:-${frames}/${frames}}" \
VISUAL_HOMING_EXPECTED_LIVE_OUTPUT_GATE_BLOCK_REASONS="${VISUAL_HOMING_EXPECTED_LIVE_OUTPUT_GATE_BLOCK_REASONS:-vehicle_not_armed:${frames}}" \
VISUAL_HOMING_EXPECTED_EXTERNAL_NAV_STRICT_SESSION_READY="${VISUAL_HOMING_EXPECTED_EXTERNAL_NAV_STRICT_SESSION_READY:-0}" \
"${repo_root}/scripts/check-external-nav-readiness-log.sh" "${run_log}"
