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
frames="${VISUAL_HOMING_CAMERA_FRAMES:-150}"
target_width="${VISUAL_HOMING_CAMERA_TARGET_WIDTH:-64}"
target_height="${VISUAL_HOMING_CAMERA_TARGET_HEIGHT:-48}"
preflight_duration_ms="${VISUAL_HOMING_EXTERNAL_NAV_PREFLIGHT_DURATION_MS:-15000}"
operator_cue_seconds="${VISUAL_HOMING_OPERATOR_CUE_SECONDS:-5}"

mkdir -p "${log_dir}"

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
"${repo_root}/scripts/check-external-nav-telemetry-sanity-pi.sh"

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
VISUAL_HOMING_EXTERNAL_NAV_EXPECTED_RELATIVE_ALTITUDE_M="${expected_altitude_m}" \
VISUAL_HOMING_EXTERNAL_NAV_EXPECTED_RELATIVE_ALTITUDE_TOLERANCE_M="${expected_altitude_tolerance_m}" \
"${repo_root}/scripts/test-core-pi.sh"

VISUAL_HOMING_EXTERNAL_NAV_ALTITUDE_PRESET="${altitude_preset}" \
VISUAL_HOMING_EXTERNAL_NAV_NOMINAL_ROUTE_LENGTH_M="${nominal_route_length_m}" \
VISUAL_HOMING_HANDOFF_REQUESTED_DISTANCE_M="${VISUAL_HOMING_HANDOFF_REQUESTED_DISTANCE_M:-}" \
VISUAL_HOMING_HANDOFF_REQUESTED_ALTITUDE_M="${VISUAL_HOMING_HANDOFF_REQUESTED_ALTITUDE_M:-}" \
"${repo_root}/scripts/export-external-nav-readiness-json.sh" "${run_log}" "${readiness_json}"

VISUAL_HOMING_EXPECTED_LIVE_ROUTE_FRAMES="${VISUAL_HOMING_EXPECTED_LIVE_ROUTE_FRAMES:-${frames}/${frames}}" \
VISUAL_HOMING_EXPECTED_LIVE_ROUTE_VALID_MATCHES="${VISUAL_HOMING_EXPECTED_LIVE_ROUTE_VALID_MATCHES:-${frames}}" \
VISUAL_HOMING_EXPECTED_LIVE_ROUTE_DRY_RUN_VALID="${VISUAL_HOMING_EXPECTED_LIVE_ROUTE_DRY_RUN_VALID:-${frames}/${frames}}" \
VISUAL_HOMING_EXPECTED_LIVE_OUTPUT_GATE_BLOCK_REASONS="${VISUAL_HOMING_EXPECTED_LIVE_OUTPUT_GATE_BLOCK_REASONS:-vehicle_not_armed:${frames}}" \
VISUAL_HOMING_EXPECTED_EXTERNAL_NAV_STRICT_SESSION_READY="${VISUAL_HOMING_EXPECTED_EXTERNAL_NAV_STRICT_SESSION_READY:-0}" \
"${repo_root}/scripts/check-external-nav-readiness-log.sh" "${run_log}"
