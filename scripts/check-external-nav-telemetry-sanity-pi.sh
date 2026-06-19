#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
log_dir="${VISUAL_HOMING_LOG_DIR:-${repo_root}/artifacts/logs}"

device="${VISUAL_HOMING_MAVLINK_TELEMETRY_DEVICE:-/dev/serial0}"
baud="${VISUAL_HOMING_MAVLINK_TELEMETRY_BAUD:-115200}"
duration_ms="${VISUAL_HOMING_MAVLINK_TELEMETRY_DURATION_MS:-60000}"
expected_altitude_m="${VISUAL_HOMING_EXTERNAL_NAV_EXPECTED_RELATIVE_ALTITUDE_M:-0}"
tolerance_m="${VISUAL_HOMING_EXTERNAL_NAV_EXPECTED_RELATIVE_ALTITUDE_TOLERANCE_M:-0}"
minimum_samples="${VISUAL_HOMING_EXTERNAL_NAV_PREFLIGHT_MIN_RELATIVE_ALTITUDE_SAMPLES:-5}"
max_malformed_frames="${VISUAL_HOMING_MAVLINK_MAX_MALFORMED_FRAMES:-0}"
max_altitude_span_m="${VISUAL_HOMING_EXTERNAL_NAV_MAX_RELATIVE_ALTITUDE_SPAN_M:-0}"
require_distance_sensor="${VISUAL_HOMING_EXTERNAL_NAV_PREFLIGHT_REQUIRE_DISTANCE_SENSOR:-0}"

require_number_gt_zero() {
    local name="$1"
    local value="$2"
    awk -v value="${value}" 'BEGIN { exit (!(value + 0 > 0)) }' || {
        echo "${name} must be a positive number, got '${value}'" >&2
        exit 2
    }
}

require_nonnegative_number() {
    local name="$1"
    local value="$2"
    awk -v value="${value}" 'BEGIN { exit (!(value + 0 >= 0)) }' || {
        echo "${name} must be a nonnegative number, got '${value}'" >&2
        exit 2
    }
}

require_bool() {
    local name="$1"
    local value="$2"
    case "${value}" in
        0|1)
            ;;
        *)
            echo "${name} must be 0 or 1, got '${value}'" >&2
            exit 2
            ;;
    esac
}

extract_field() {
    local line="$1"
    local key="$2"
    local value
    value="$(printf '%s\n' "${line}" | grep -o "${key}=[^ ]*" | tail -1 | cut -d= -f2- || true)"
    if [[ -z "${value}" ]]; then
        echo "missing"
    else
        printf '%s\n' "${value}"
    fi
}

float_ge() {
    awk -v a="$1" -v b="$2" 'BEGIN { exit (!(a + 0 >= b + 0)) }'
}

float_le() {
    awk -v a="$1" -v b="$2" 'BEGIN { exit (!(a + 0 <= b + 0)) }'
}

float_gt() {
    awk -v a="$1" -v b="$2" 'BEGIN { exit (!(a + 0 > b + 0)) }'
}

int_le() {
    awk -v a="$1" -v b="$2" 'BEGIN { exit (!(int(a) <= int(b))) }'
}

int_ge() {
    awk -v a="$1" -v b="$2" 'BEGIN { exit (!(int(a) >= int(b))) }'
}

require_number_gt_zero "VISUAL_HOMING_EXTERNAL_NAV_EXPECTED_RELATIVE_ALTITUDE_M" "${expected_altitude_m}"
require_number_gt_zero "VISUAL_HOMING_EXTERNAL_NAV_EXPECTED_RELATIVE_ALTITUDE_TOLERANCE_M" "${tolerance_m}"
require_nonnegative_number "VISUAL_HOMING_EXTERNAL_NAV_MAX_RELATIVE_ALTITUDE_SPAN_M" "${max_altitude_span_m}"
require_bool "VISUAL_HOMING_EXTERNAL_NAV_PREFLIGHT_REQUIRE_DISTANCE_SENSOR" "${require_distance_sensor}"

VISUAL_HOMING_CAPTURE_MAVLINK_TELEMETRY=1 \
VISUAL_HOMING_INSPECT_MAVLINK_TELEMETRY=1 \
VISUAL_HOMING_VALIDATE_MAVLINK_TELEMETRY=0 \
VISUAL_HOMING_RUN_CAMERA_SMOKE=0 \
VISUAL_HOMING_RECORD_LIVE_ROUTE=0 \
VISUAL_HOMING_MATCH_LIVE_ROUTE=0 \
VISUAL_HOMING_VALIDATE_ROUTE=0 \
VISUAL_HOMING_INSPECT_ROUTE=0 \
VISUAL_HOMING_EXPORT_ROUTE_KEYFRAMES=0 \
VISUAL_HOMING_SELF_MATCH_ROUTE=0 \
VISUAL_HOMING_PERTURB_ROUTE=0 \
VISUAL_HOMING_ROUTE_DISTINCTIVENESS=0 \
VISUAL_HOMING_MAVLINK_TELEMETRY_DEVICE="${device}" \
VISUAL_HOMING_MAVLINK_TELEMETRY_BAUD="${baud}" \
VISUAL_HOMING_MAVLINK_TELEMETRY_DURATION_MS="${duration_ms}" \
"${repo_root}/scripts/test-core-pi.sh"

latest_log="$(ls -t "${log_dir}"/test-core-pi-*.log | head -1)"
inspect_line="$(grep "mavlink_telemetry_inspect" "${latest_log}" | tail -1 || true)"
if [[ -z "${inspect_line}" ]]; then
    echo "external_nav_telemetry_sanity passed=false reason=missing_inspect_line log_path=${latest_log}"
    exit 2
fi

malformed_frames="$(extract_field "${inspect_line}" "malformed_frames")"
relative_altitude_seen="$(extract_field "${inspect_line}" "relative_altitude_seen")"
relative_altitude_samples="$(extract_field "${inspect_line}" "relative_altitude_samples")"
relative_altitude_min_avg_max_m="$(extract_field "${inspect_line}" "relative_altitude_min_avg_max_m")"
distance_sensor_seen="$(extract_field "${inspect_line}" "distance_sensor_seen")"
distance_sensor_messages="$(extract_field "${inspect_line}" "distance_sensor_messages")"
armed="$(extract_field "${inspect_line}" "armed")"
mode="$(extract_field "${inspect_line}" "mode")"

relative_altitude_min_m=0
relative_altitude_avg_m=0
relative_altitude_max_m=0
if [[ "${relative_altitude_min_avg_max_m}" != "missing" ]]; then
    IFS='/' read -r relative_altitude_min_m relative_altitude_avg_m relative_altitude_max_m <<< "${relative_altitude_min_avg_max_m}"
fi

altitude_min_allowed="$(awk -v e="${expected_altitude_m}" -v t="${tolerance_m}" 'BEGIN { print e - t }')"
altitude_max_allowed="$(awk -v e="${expected_altitude_m}" -v t="${tolerance_m}" 'BEGIN { print e + t }')"
relative_altitude_span_m="$(awk -v min="${relative_altitude_min_m}" -v max="${relative_altitude_max_m}" 'BEGIN { print max - min }')"

passed=true
reason=valid

if [[ "${malformed_frames}" == "missing" ]] || ! int_le "${malformed_frames}" "${max_malformed_frames}"; then
    passed=false
    reason=malformed_frames_high
elif [[ "${relative_altitude_seen}" != "true" ]]; then
    passed=false
    reason=relative_altitude_missing
elif [[ "${relative_altitude_samples}" == "missing" ]] || ! int_ge "${relative_altitude_samples}" "${minimum_samples}"; then
    passed=false
    reason=relative_altitude_samples_low
elif [[ "${relative_altitude_min_avg_max_m}" == "missing" ]]; then
    passed=false
    reason=relative_altitude_aggregate_missing
elif ! float_ge "${relative_altitude_min_m}" "${altitude_min_allowed}" || ! float_le "${relative_altitude_max_m}" "${altitude_max_allowed}"; then
    passed=false
    reason=relative_altitude_out_of_expected_window
elif float_gt "${max_altitude_span_m}" "0" && ! float_le "${relative_altitude_span_m}" "${max_altitude_span_m}"; then
    passed=false
    reason=relative_altitude_span_high
elif [[ "${require_distance_sensor}" == "1" && "${distance_sensor_seen}" != "true" ]]; then
    passed=false
    reason=distance_sensor_missing
fi

echo "external_nav_telemetry_sanity passed=${passed} reason=${reason} log_path=${latest_log} device=${device} baud_rate=${baud} duration_ms=${duration_ms} malformed_frames=${malformed_frames} max_malformed_frames=${max_malformed_frames} relative_altitude_seen=${relative_altitude_seen} relative_altitude_samples=${relative_altitude_samples} minimum_relative_altitude_samples=${minimum_samples} relative_altitude_min_avg_max_m=${relative_altitude_min_avg_max_m} expected_relative_altitude_m=${expected_altitude_m} expected_relative_altitude_tolerance_m=${tolerance_m} relative_altitude_window_m=${altitude_min_allowed}..${altitude_max_allowed} relative_altitude_span_m=${relative_altitude_span_m} max_relative_altitude_span_m=${max_altitude_span_m} distance_sensor_required=${require_distance_sensor} distance_sensor_seen=${distance_sensor_seen} distance_sensor_messages=${distance_sensor_messages} armed=${armed} mode=${mode}"

if [[ "${passed}" == "true" ]]; then
    exit 0
fi
exit 2
