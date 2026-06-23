#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
stamp="$(date -u +"%Y%m%dT%H%M%SZ")"
artifact_dir="${VISUAL_HOMING_ARTIFACT_DIR:-${repo_root}/artifacts}"
log_dir="${VISUAL_HOMING_LOG_DIR:-${artifact_dir}/logs}"
route_dir="${VISUAL_HOMING_FIELD_ROUTE_DIR:-${artifact_dir}/field_routes}"
route_output="${VISUAL_HOMING_ROUTE_OUTPUT:-${route_dir}/field-route-${stamp}.vhrs}"
route_keyframe_dir="${VISUAL_HOMING_ROUTE_KEYFRAME_DIR:-${route_dir}/field-route-${stamp}-keyframes}"
record_log="${VISUAL_HOMING_RUN_LOG:-${log_dir}/field-route-record-${stamp}.log}"

frames="${VISUAL_HOMING_CAMERA_FRAMES:-150}"
target_width="${VISUAL_HOMING_CAMERA_TARGET_WIDTH:-64}"
target_height="${VISUAL_HOMING_CAMERA_TARGET_HEIGHT:-48}"
fps="${VISUAL_HOMING_CAMERA_FPS:-15}"
operator_cue_seconds="${VISUAL_HOMING_OPERATOR_CUE_SECONDS:-5}"
route_keyframe_scale="${VISUAL_HOMING_ROUTE_KEYFRAME_SCALE:-5}"
use_live_telemetry="${VISUAL_HOMING_FIELD_ROUTE_USE_LIVE_TELEMETRY:-1}"
expected_entries="${VISUAL_HOMING_EXPECTED_ROUTE_QUALITY_ENTRIES:-${frames}}"

case "${use_live_telemetry}" in
    0|1)
        ;;
    *)
        echo "VISUAL_HOMING_FIELD_ROUTE_USE_LIVE_TELEMETRY must be 0 or 1, got '${use_live_telemetry}'" >&2
        exit 2
        ;;
esac

mkdir -p "${log_dir}" "${route_dir}" "${route_keyframe_dir}"

cat <<EOF
###############################################################################
### FIELD ROUTE RECORD READINESS
### This command records a route artifact and validates route quality.
### It sends no MAVLink commands and writes no external-nav MAVLink.
### record_log=${record_log}
### route_output=${route_output}
### route_keyframe_dir=${route_keyframe_dir}
### frames=${frames}
### fps=${fps}
### target=${target_width}x${target_height}
### use_live_telemetry=${use_live_telemetry}
### expected_route_quality_entries=${expected_entries}
### operator_cue_seconds=${operator_cue_seconds}
###############################################################################
EOF

VISUAL_HOMING_RUN_LOG="${record_log}" \
VISUAL_HOMING_USE_ACTIVE_CAMERA_PROFILE=1 \
VISUAL_HOMING_RECORD_LIVE_ROUTE=1 \
VISUAL_HOMING_ROUTE_USE_LIVE_MAVLINK_TELEMETRY="${use_live_telemetry}" \
VISUAL_HOMING_ROUTE_OUTPUT="${route_output}" \
VISUAL_HOMING_ROUTE_KEYFRAME_DIR="${route_keyframe_dir}" \
VISUAL_HOMING_ROUTE_KEYFRAME_SCALE="${route_keyframe_scale}" \
VISUAL_HOMING_CAMERA_FPS="${fps}" \
VISUAL_HOMING_CAMERA_FRAMES="${frames}" \
VISUAL_HOMING_CAMERA_TARGET_WIDTH="${target_width}" \
VISUAL_HOMING_CAMERA_TARGET_HEIGHT="${target_height}" \
VISUAL_HOMING_OPERATOR_CUE_SECONDS="${operator_cue_seconds}" \
VISUAL_HOMING_MAVLINK_TELEMETRY_DEVICE="${VISUAL_HOMING_MAVLINK_TELEMETRY_DEVICE:-/dev/serial0}" \
VISUAL_HOMING_MAVLINK_TELEMETRY_BAUD="${VISUAL_HOMING_MAVLINK_TELEMETRY_BAUD:-115200}" \
"${repo_root}/scripts/test-core-pi.sh"

VISUAL_HOMING_EXPECTED_ROUTE_QUALITY_ENTRIES="${expected_entries}" \
"${repo_root}/scripts/check-route-quality-log.sh" "${record_log}"

echo "field_route_record_ready route=${route_output} keyframes=${route_keyframe_dir} log=${record_log} entries=${expected_entries}"
