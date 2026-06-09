#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
core_dir="${repo_root}/core"
build_type="${VISUAL_HOMING_PI_BUILD_TYPE:-MinSizeRel}"
build_jobs="${VISUAL_HOMING_BUILD_JOBS:-1}"
pi_cmake_live_output="${VISUAL_HOMING_PI_CMAKE_ENABLE_LIVE_MAVLINK_OUTPUT:-0}"
pi_cmake_bench_props_off_live_output="${VISUAL_HOMING_PI_CMAKE_ENABLE_BENCH_PROPS_OFF_LIVE_OUTPUT:-0}"
pi_cmake_attach_bench_props_off_serial_writer="${VISUAL_HOMING_PI_CMAKE_ATTACH_BENCH_PROPS_OFF_SERIAL_WRITER:-0}"
if [[ -n "${VISUAL_HOMING_PI_BUILD_DIR:-}" ]]; then
    build_dir="${VISUAL_HOMING_PI_BUILD_DIR}"
elif [[ "${pi_cmake_attach_bench_props_off_serial_writer}" == "1" ]]; then
    build_dir="${core_dir}/build-pi-live-output-attach"
elif [[ "${pi_cmake_live_output}" == "1" || "${pi_cmake_bench_props_off_live_output}" == "1" ]]; then
    build_dir="${core_dir}/build-pi-live-output-scope"
else
    build_dir="${core_dir}/build-pi"
fi
artifact_dir="${repo_root}/artifacts"
log_dir="${VISUAL_HOMING_LOG_DIR:-${artifact_dir}/logs}"
route_output="${VISUAL_HOMING_ROUTE_OUTPUT:-${artifact_dir}/visual_homing_live_route.vhrs}"
route_keyframe_dir="${VISUAL_HOMING_ROUTE_KEYFRAME_DIR:-${artifact_dir}/route_keyframes}"
route_keyframe_scale="${VISUAL_HOMING_ROUTE_KEYFRAME_SCALE:-1}"
route_warmup_frames="${VISUAL_HOMING_ROUTE_WARMUP_FRAMES:-3}"
camera_target_width="${VISUAL_HOMING_CAMERA_TARGET_WIDTH:-}"
camera_target_height="${VISUAL_HOMING_CAMERA_TARGET_HEIGHT:-}"
live_route_match_window_radius="${VISUAL_HOMING_LIVE_ROUTE_MATCH_WINDOW_RADIUS:-30}"
live_route_match_min_confidence="${VISUAL_HOMING_LIVE_ROUTE_MATCH_MIN_CONFIDENCE:-0.75}"
live_route_match_max_direction_shift_px="${VISUAL_HOMING_LIVE_ROUTE_MATCH_MAX_DIRECTION_SHIFT_PX:-4}"
live_route_match_expected_progress="${VISUAL_HOMING_LIVE_ROUTE_MATCH_EXPECTED_PROGRESS:-any}"
live_route_match_max_progress_regressions="${VISUAL_HOMING_LIVE_ROUTE_MATCH_MAX_PROGRESS_REGRESSIONS:-5}"
live_route_match_max_progress_rollback="${VISUAL_HOMING_LIVE_ROUTE_MATCH_MAX_PROGRESS_ROLLBACK:-0.25}"
live_route_match_require_endpoint_progress="${VISUAL_HOMING_LIVE_ROUTE_MATCH_REQUIRE_ENDPOINT_PROGRESS:-0}"
live_route_match_endpoint_start_progress="${VISUAL_HOMING_LIVE_ROUTE_MATCH_ENDPOINT_START_PROGRESS:-0.15}"
live_route_match_endpoint_end_progress="${VISUAL_HOMING_LIVE_ROUTE_MATCH_ENDPOINT_END_PROGRESS:-0.85}"
live_route_match_stop_at_endpoint_progress="${VISUAL_HOMING_LIVE_ROUTE_MATCH_STOP_AT_ENDPOINT_PROGRESS:-0}"
live_route_dry_run_commands="${VISUAL_HOMING_LIVE_ROUTE_DRY_RUN_COMMANDS:-0}"
live_route_navigator_min_confidence="${VISUAL_HOMING_LIVE_ROUTE_NAVIGATOR_MIN_CONFIDENCE:-${live_route_match_min_confidence}}"
live_route_navigator_max_match_age_ms="${VISUAL_HOMING_LIVE_ROUTE_NAVIGATOR_MAX_MATCH_AGE_MS:-250}"
live_route_navigator_yaw_gain="${VISUAL_HOMING_LIVE_ROUTE_NAVIGATOR_YAW_GAIN:-1.0}"
live_route_navigator_max_yaw_rate_radps="${VISUAL_HOMING_LIVE_ROUTE_NAVIGATOR_MAX_YAW_RATE_RADPS:-0.35}"
live_route_navigator_max_yaw_accel_radps2="${VISUAL_HOMING_LIVE_ROUTE_NAVIGATOR_MAX_YAW_ACCEL_RADPS2:-1.0}"
live_route_navigator_forward_speed_mps="${VISUAL_HOMING_LIVE_ROUTE_NAVIGATOR_FORWARD_SPEED_MPS:-0.0}"
live_route_dry_run_require_command_quality="${VISUAL_HOMING_LIVE_ROUTE_DRY_RUN_REQUIRE_COMMAND_QUALITY:-0}"
live_route_dry_run_minimum_valid_command_fraction="${VISUAL_HOMING_LIVE_ROUTE_DRY_RUN_MIN_VALID_COMMAND_FRACTION:-0.95}"
live_route_dry_run_max_invalid_command_streak="${VISUAL_HOMING_LIVE_ROUTE_DRY_RUN_MAX_INVALID_COMMAND_STREAK:-3}"
live_route_dry_run_max_abs_yaw_rate_radps="${VISUAL_HOMING_LIVE_ROUTE_DRY_RUN_MAX_ABS_YAW_RATE_RADPS:-${live_route_navigator_max_yaw_rate_radps}}"
live_route_dry_run_max_yaw_rate_sign_flips="${VISUAL_HOMING_LIVE_ROUTE_DRY_RUN_MAX_YAW_RATE_SIGN_FLIPS:-20}"
live_route_dry_run_max_yaw_rate_delta_radps="${VISUAL_HOMING_LIVE_ROUTE_DRY_RUN_MAX_YAW_RATE_DELTA_RADPS:-0.15}"
live_route_match_use_live_mavlink_telemetry="${VISUAL_HOMING_MATCH_LIVE_ROUTE_USE_LIVE_MAVLINK_TELEMETRY:-0}"
live_route_match_telemetry_max_age_ms="${VISUAL_HOMING_MATCH_LIVE_ROUTE_TELEMETRY_MAX_AGE_MS:-500}"
live_route_match_require_live_telemetry_health="${VISUAL_HOMING_MATCH_LIVE_ROUTE_REQUIRE_LIVE_TELEMETRY_HEALTH:-0}"
live_route_session_audit="${VISUAL_HOMING_LIVE_ROUTE_SESSION_AUDIT:-0}"
live_output_runtime_enabled="${VISUAL_HOMING_ENABLE_LIVE_MAVLINK_OUTPUT:-0}"
live_output_bench_props_off_confirm="${VISUAL_HOMING_LIVE_OUTPUT_BENCH_PROPS_OFF_CONFIRM:-}"
live_output_max_commands="${VISUAL_HOMING_LIVE_OUTPUT_MAX_COMMANDS:-0}"
live_output_max_seconds="${VISUAL_HOMING_LIVE_OUTPUT_MAX_SECONDS:-0}"
camera_profile_dir="${VISUAL_HOMING_CAMERA_PROFILE_DIR:-${repo_root}/config/camera_profiles}"
camera_profile="${VISUAL_HOMING_CAMERA_PROFILE:-${repo_root}/config/camera_profiles/imx219-visible-wide.profile}"
active_camera_profile="${VISUAL_HOMING_ACTIVE_CAMERA_PROFILE:-${artifact_dir}/active_camera_profile.txt}"
mavlink_telemetry_input="${VISUAL_HOMING_MAVLINK_TELEMETRY_INPUT:-${artifact_dir}/mavlink_telemetry.bin}"
mavlink_telemetry_device="${VISUAL_HOMING_MAVLINK_TELEMETRY_DEVICE:-/dev/ttyAMA0}"
mavlink_telemetry_baud="${VISUAL_HOMING_MAVLINK_TELEMETRY_BAUD:-57600}"
mavlink_telemetry_duration_ms="${VISUAL_HOMING_MAVLINK_TELEMETRY_DURATION_MS:-1000}"
mavlink_min_heartbeat_messages="${VISUAL_HOMING_MAVLINK_MIN_HEARTBEAT_MESSAGES:-1}"
mavlink_min_attitude_messages="${VISUAL_HOMING_MAVLINK_MIN_ATTITUDE_MESSAGES:-1}"
mavlink_min_global_position_int_messages="${VISUAL_HOMING_MAVLINK_MIN_GLOBAL_POSITION_INT_MESSAGES:-1}"
mavlink_max_malformed_frames="${VISUAL_HOMING_MAVLINK_MAX_MALFORMED_FRAMES:-0}"
route_telemetry_warmup_ms="${VISUAL_HOMING_ROUTE_TELEMETRY_WARMUP_MS:-1500}"
operator_cue_enabled="${VISUAL_HOMING_OPERATOR_CUE:-1}"
operator_cue_seconds="${VISUAL_HOMING_OPERATOR_CUE_SECONDS:-5}"
operator_cue_bell="${VISUAL_HOMING_OPERATOR_CUE_BELL:-1}"
run_started_wall_time_utc="$(date -u +"%Y-%m-%dT%H:%M:%SZ")"
run_log_stamp="$(date -u +"%Y%m%dT%H%M%SZ")"
run_started_epoch="$(date +%s)"
run_log_file="${VISUAL_HOMING_RUN_LOG:-${log_dir}/test-core-pi-${run_log_stamp}.log}"
live_route_session_audit_path="${VISUAL_HOMING_LIVE_ROUTE_SESSION_AUDIT_PATH:-${artifact_dir}/logs/live-output-session-audit-${run_log_stamp}.log}"

require_bool_env() {
    local name="$1"
    local value="${!name:-0}"
    case "${value}" in
        0|1)
            ;;
        *)
            echo "${name} must be 0 or 1, got '${value}'" >&2
            exit 2
            ;;
    esac
}

for bool_env in \
    VISUAL_HOMING_DISABLE_RUN_LOG \
    VISUAL_HOMING_OPERATOR_CUE \
    VISUAL_HOMING_OPERATOR_CUE_BELL \
    VISUAL_HOMING_INSPECT_CAMERA_PROFILE \
    VISUAL_HOMING_LIST_CAMERA_PROFILES \
    VISUAL_HOMING_GET_ACTIVE_CAMERA_PROFILE \
    VISUAL_HOMING_API_LIST_CAMERA_PROFILES \
    VISUAL_HOMING_API_GET_ACTIVE_CAMERA_PROFILE \
    VISUAL_HOMING_CAPTURE_MAVLINK_TELEMETRY \
    VISUAL_HOMING_INSPECT_MAVLINK_TELEMETRY \
    VISUAL_HOMING_VALIDATE_MAVLINK_TELEMETRY \
    VISUAL_HOMING_RUN_CAMERA_SMOKE \
    VISUAL_HOMING_RECORD_LIVE_ROUTE \
    VISUAL_HOMING_ROUTE_USE_MAVLINK_TELEMETRY \
    VISUAL_HOMING_ROUTE_USE_LIVE_MAVLINK_TELEMETRY \
    VISUAL_HOMING_USE_ACTIVE_CAMERA_PROFILE \
    VISUAL_HOMING_USE_CAMERA_PROFILE \
    VISUAL_HOMING_MATCH_LIVE_ROUTE \
    VISUAL_HOMING_VALIDATE_ROUTE \
    VISUAL_HOMING_INSPECT_ROUTE \
    VISUAL_HOMING_EXPORT_ROUTE_KEYFRAMES \
    VISUAL_HOMING_SELF_MATCH_ROUTE \
    VISUAL_HOMING_PERTURB_ROUTE \
    VISUAL_HOMING_ROUTE_DISTINCTIVENESS \
    VISUAL_HOMING_LIVE_ROUTE_MATCH_REQUIRE_ENDPOINT_PROGRESS \
    VISUAL_HOMING_LIVE_ROUTE_MATCH_STOP_AT_ENDPOINT_PROGRESS \
    VISUAL_HOMING_LIVE_ROUTE_DRY_RUN_COMMANDS \
    VISUAL_HOMING_LIVE_ROUTE_DRY_RUN_REQUIRE_COMMAND_QUALITY \
    VISUAL_HOMING_MATCH_LIVE_ROUTE_USE_LIVE_MAVLINK_TELEMETRY \
    VISUAL_HOMING_MATCH_LIVE_ROUTE_REQUIRE_LIVE_TELEMETRY_HEALTH \
    VISUAL_HOMING_LIVE_ROUTE_SESSION_AUDIT \
    VISUAL_HOMING_ENABLE_LIVE_MAVLINK_OUTPUT \
    VISUAL_HOMING_PI_CMAKE_ENABLE_LIVE_MAVLINK_OUTPUT \
    VISUAL_HOMING_PI_CMAKE_ENABLE_BENCH_PROPS_OFF_LIVE_OUTPUT \
    VISUAL_HOMING_PI_CMAKE_ATTACH_BENCH_PROPS_OFF_SERIAL_WRITER; do
    require_bool_env "${bool_env}"
done

if [[ "${pi_cmake_attach_bench_props_off_serial_writer}" == "1" \
    && ( "${pi_cmake_live_output}" != "1" || "${pi_cmake_bench_props_off_live_output}" != "1" ) ]]; then
    echo "VISUAL_HOMING_PI_CMAKE_ATTACH_BENCH_PROPS_OFF_SERIAL_WRITER=1 requires VISUAL_HOMING_PI_CMAKE_ENABLE_LIVE_MAVLINK_OUTPUT=1 and VISUAL_HOMING_PI_CMAKE_ENABLE_BENCH_PROPS_OFF_LIVE_OUTPUT=1" >&2
    exit 2
fi

case "${live_route_match_expected_progress}" in
    any|forward|reverse)
        ;;
    *)
        echo "VISUAL_HOMING_LIVE_ROUTE_MATCH_EXPECTED_PROGRESS must be one of: any, forward, reverse" >&2
        exit 2
        ;;
esac

if [[ "${live_route_match_stop_at_endpoint_progress}" == "1" && "${live_route_match_expected_progress}" == "any" ]]; then
    echo "VISUAL_HOMING_LIVE_ROUTE_MATCH_STOP_AT_ENDPOINT_PROGRESS=1 requires VISUAL_HOMING_LIVE_ROUTE_MATCH_EXPECTED_PROGRESS=forward or reverse" >&2
    exit 2
fi

if [[ "${VISUAL_HOMING_DISABLE_RUN_LOG:-0}" != "1" ]]; then
    mkdir -p "$(dirname "${run_log_file}")"
    exec > >(tee -a "${run_log_file}") 2>&1
fi

finish_log() {
    local status="$?"
    local finished_epoch
    finished_epoch="$(date +%s)"
    echo "pi_test_run_done wall_time_utc=$(date -u +"%Y-%m-%dT%H:%M:%SZ") exit_code=${status} elapsed_s=$((finished_epoch - run_started_epoch)) log_path=${run_log_file}"
}
trap finish_log EXIT

cmake_live_output_option=OFF
cmake_bench_props_off_live_output_option=OFF
cmake_attach_bench_props_off_serial_writer_option=OFF
if [[ "${pi_cmake_live_output}" == "1" ]]; then
    cmake_live_output_option=ON
fi
if [[ "${pi_cmake_bench_props_off_live_output}" == "1" ]]; then
    cmake_bench_props_off_live_output_option=ON
fi
if [[ "${pi_cmake_attach_bench_props_off_serial_writer}" == "1" ]]; then
    cmake_attach_bench_props_off_serial_writer_option=ON
fi

echo "pi_test_run_start wall_time_utc=${run_started_wall_time_utc} log_path=${run_log_file} repo_root=${repo_root} build_dir=${build_dir} live_output_cmake=${cmake_live_output_option} bench_props_off_cmake=${cmake_bench_props_off_live_output_option} attach_writer_cmake=${cmake_attach_bench_props_off_serial_writer_option} route_output=${route_output} route_warmup_frames=${route_warmup_frames}"

camera_target_override_args=()
if [[ -n "${camera_target_width}" || -n "${camera_target_height}" ]]; then
    if [[ -z "${camera_target_width}" || -z "${camera_target_height}" ]]; then
        echo "VISUAL_HOMING_CAMERA_TARGET_WIDTH and VISUAL_HOMING_CAMERA_TARGET_HEIGHT must be set together" >&2
        exit 2
    fi
    camera_target_override_args=("${camera_target_width}" "${camera_target_height}")
fi
operator_cue_args=("${operator_cue_enabled}" "${operator_cue_seconds}" "${operator_cue_bell}")
live_route_dry_run_command_args=()
if [[ "${live_route_dry_run_commands}" == "1" ]]; then
    live_route_dry_run_command_args=(
        "${live_route_dry_run_commands}"
        "${live_route_navigator_min_confidence}"
        "${live_route_navigator_max_match_age_ms}"
        "${live_route_navigator_yaw_gain}"
        "${live_route_navigator_max_yaw_rate_radps}"
        "${live_route_navigator_max_yaw_accel_radps2}"
        "${live_route_navigator_forward_speed_mps}"
        "${live_route_dry_run_require_command_quality}"
        "${live_route_dry_run_minimum_valid_command_fraction}"
        "${live_route_dry_run_max_invalid_command_streak}"
        "${live_route_dry_run_max_abs_yaw_rate_radps}"
        "${live_route_dry_run_max_yaw_rate_sign_flips}"
        "${live_route_dry_run_max_yaw_rate_delta_radps}"
    )
fi
live_route_match_telemetry_args=()
if [[ "${live_route_match_use_live_mavlink_telemetry}" == "1" ]]; then
    if [[ "${live_route_dry_run_commands}" != "1" ]]; then
        echo "VISUAL_HOMING_MATCH_LIVE_ROUTE_USE_LIVE_MAVLINK_TELEMETRY=1 currently requires VISUAL_HOMING_LIVE_ROUTE_DRY_RUN_COMMANDS=1" >&2
        exit 2
    fi
    live_route_match_telemetry_args=(
        "${live_route_match_use_live_mavlink_telemetry}"
        "${mavlink_telemetry_device}"
        "${mavlink_telemetry_baud}"
        "${route_telemetry_warmup_ms}"
        "${live_route_match_telemetry_max_age_ms}"
        "${live_route_match_require_live_telemetry_health}"
    )
fi
live_route_session_audit_args=()
live_route_endpoint_stop_args=()
if [[ "${live_route_session_audit}" == "1" ]]; then
    if [[ "${live_route_dry_run_commands}" != "1" ]]; then
        echo "VISUAL_HOMING_LIVE_ROUTE_SESSION_AUDIT=1 currently requires VISUAL_HOMING_LIVE_ROUTE_DRY_RUN_COMMANDS=1" >&2
        exit 2
    fi
    if [[ "${live_route_match_use_live_mavlink_telemetry}" != "1" ]]; then
        echo "VISUAL_HOMING_LIVE_ROUTE_SESSION_AUDIT=1 currently requires VISUAL_HOMING_MATCH_LIVE_ROUTE_USE_LIVE_MAVLINK_TELEMETRY=1" >&2
        exit 2
    fi
    if [[ ${#camera_target_override_args[@]} -ne 2 ]]; then
        echo "VISUAL_HOMING_LIVE_ROUTE_SESSION_AUDIT=1 currently requires VISUAL_HOMING_CAMERA_TARGET_WIDTH and VISUAL_HOMING_CAMERA_TARGET_HEIGHT" >&2
        exit 2
    fi
    if [[ "${live_output_runtime_enabled}" == "1" ]]; then
        if [[ "${live_output_bench_props_off_confirm}" != "I_UNDERSTAND_PROPS_ARE_REMOVED" ]]; then
            echo "VISUAL_HOMING_ENABLE_LIVE_MAVLINK_OUTPUT=1 requires VISUAL_HOMING_LIVE_OUTPUT_BENCH_PROPS_OFF_CONFIRM=I_UNDERSTAND_PROPS_ARE_REMOVED" >&2
            exit 2
        fi
        if [[ "${live_output_max_commands}" == "0" || "${live_output_max_seconds}" == "0" ]]; then
            echo "VISUAL_HOMING_ENABLE_LIVE_MAVLINK_OUTPUT=1 requires positive VISUAL_HOMING_LIVE_OUTPUT_MAX_COMMANDS and VISUAL_HOMING_LIVE_OUTPUT_MAX_SECONDS" >&2
            exit 2
        fi
    fi
    mkdir -p "$(dirname "${live_route_session_audit_path}")"
    live_route_session_audit_args=(
        "${live_route_session_audit}"
        "${live_route_session_audit_path}"
    )
    if [[ -n "${VISUAL_HOMING_ENABLE_LIVE_MAVLINK_OUTPUT:-}" || -n "${VISUAL_HOMING_LIVE_OUTPUT_BENCH_PROPS_OFF_CONFIRM:-}" || -n "${VISUAL_HOMING_LIVE_OUTPUT_MAX_COMMANDS:-}" || -n "${VISUAL_HOMING_LIVE_OUTPUT_MAX_SECONDS:-}" ]]; then
        live_route_session_audit_args+=(
            "${live_output_runtime_enabled}"
            "${live_output_bench_props_off_confirm}"
            "${live_output_max_commands}"
            "${live_output_max_seconds}"
        )
    fi
fi

if [[ "${live_route_match_stop_at_endpoint_progress}" == "1" ]]; then
    if [[ ${#live_route_session_audit_args[@]} -lt 6 ]]; then
        echo "VISUAL_HOMING_LIVE_ROUTE_MATCH_STOP_AT_ENDPOINT_PROGRESS currently requires live-output runtime session args" >&2
        exit 2
    fi
    live_route_endpoint_stop_args=("${live_route_match_stop_at_endpoint_progress}")
fi

clean=0
for arg in "$@"; do
    case "${arg}" in
        --clean)
            clean=1
            ;;
        *)
            echo "usage: $0 [--clean]" >&2
            exit 2
            ;;
    esac
done

if [[ "${clean}" == "1" && -d "${build_dir}" ]]; then
    case "$(realpath "${build_dir}")" in
        "$(realpath "${core_dir}")"/*)
            rm -rf "${build_dir}"
            ;;
        *)
            echo "refusing to remove unexpected build path: ${build_dir}" >&2
            exit 1
            ;;
    esac
fi

generator_args=()
if command -v ninja >/dev/null 2>&1; then
    generator_args=(-G Ninja)
fi

cmake -S "${core_dir}" -B "${build_dir}" \
    "${generator_args[@]}" \
    -DCMAKE_BUILD_TYPE="${build_type}" \
    -DVISUAL_HOMING_ENABLE_LIBCAMERA=ON \
    -DVISUAL_HOMING_ENABLE_LIVE_MAVLINK_OUTPUT="${cmake_live_output_option}" \
    -DVISUAL_HOMING_ENABLE_BENCH_PROPS_OFF_LIVE_OUTPUT="${cmake_bench_props_off_live_output_option}" \
    -DVISUAL_HOMING_ATTACH_BENCH_PROPS_OFF_SERIAL_WRITER="${cmake_attach_bench_props_off_serial_writer_option}"

cmake --build "${build_dir}" --parallel "${build_jobs}"
ctest --test-dir "${build_dir}" --output-on-failure

if [[ "${VISUAL_HOMING_INSPECT_CAMERA_PROFILE:-0}" == "1" ]]; then
    "${build_dir}/visual_homing_core" --inspect-camera-profile "${camera_profile}"
fi

if [[ "${VISUAL_HOMING_LIST_CAMERA_PROFILES:-0}" == "1" ]]; then
    "${build_dir}/visual_homing_core" --list-camera-profiles "${camera_profile_dir}"
fi

if [[ -n "${VISUAL_HOMING_SET_CAMERA_PROFILE_ID:-}" ]]; then
    "${build_dir}/visual_homing_core" --set-active-camera-profile \
        "${camera_profile_dir}" \
        "${active_camera_profile}" \
        "${VISUAL_HOMING_SET_CAMERA_PROFILE_ID}"
fi

if [[ "${VISUAL_HOMING_GET_ACTIVE_CAMERA_PROFILE:-0}" == "1" ]]; then
    "${build_dir}/visual_homing_core" --get-active-camera-profile \
        "${camera_profile_dir}" \
        "${active_camera_profile}"
fi

if [[ "${VISUAL_HOMING_API_LIST_CAMERA_PROFILES:-0}" == "1" ]]; then
    "${build_dir}/visual_homing_core" --api-list-camera-profiles \
        "${camera_profile_dir}" \
        "${active_camera_profile}"
fi

if [[ "${VISUAL_HOMING_API_GET_ACTIVE_CAMERA_PROFILE:-0}" == "1" ]]; then
    "${build_dir}/visual_homing_core" --api-get-active-camera-profile \
        "${camera_profile_dir}" \
        "${active_camera_profile}"
fi

if [[ -n "${VISUAL_HOMING_API_SET_CAMERA_PROFILE_ID:-}" ]]; then
    "${build_dir}/visual_homing_core" --api-set-active-camera-profile \
        "${camera_profile_dir}" \
        "${active_camera_profile}" \
        "${VISUAL_HOMING_API_SET_CAMERA_PROFILE_ID}"
fi

if [[ "${VISUAL_HOMING_CAPTURE_MAVLINK_TELEMETRY:-0}" == "1" ]]; then
    "${build_dir}/visual_homing_core" --capture-mavlink-telemetry \
        "${mavlink_telemetry_device}" \
        "${mavlink_telemetry_baud}" \
        "${mavlink_telemetry_duration_ms}" \
        "${mavlink_telemetry_input}"
fi

if [[ "${VISUAL_HOMING_INSPECT_MAVLINK_TELEMETRY:-0}" == "1" ]]; then
    "${build_dir}/visual_homing_core" --inspect-mavlink-telemetry "${mavlink_telemetry_input}"
fi

if [[ "${VISUAL_HOMING_VALIDATE_MAVLINK_TELEMETRY:-0}" == "1" ]]; then
    "${build_dir}/visual_homing_core" --validate-mavlink-telemetry \
        "${mavlink_telemetry_input}" \
        "${mavlink_min_heartbeat_messages}" \
        "${mavlink_min_attitude_messages}" \
        "${mavlink_min_global_position_int_messages}" \
        "${mavlink_max_malformed_frames}"
fi

if [[ "${VISUAL_HOMING_RECORD_LIVE_ROUTE:-0}" == "1" || "${VISUAL_HOMING_MATCH_LIVE_ROUTE:-0}" == "1" || "${VISUAL_HOMING_VALIDATE_ROUTE:-0}" == "1" || "${VISUAL_HOMING_INSPECT_ROUTE:-0}" == "1" || "${VISUAL_HOMING_SELF_MATCH_ROUTE:-0}" == "1" || "${VISUAL_HOMING_PERTURB_ROUTE:-0}" == "1" || "${VISUAL_HOMING_ROUTE_DISTINCTIVENESS:-0}" == "1" ]]; then
    mkdir -p "$(dirname "${route_output}")"
fi

if [[ "${VISUAL_HOMING_RUN_CAMERA_SMOKE:-0}" == "1" ]]; then
    if [[ "${VISUAL_HOMING_USE_ACTIVE_CAMERA_PROFILE:-0}" == "1" ]]; then
        "${build_dir}/visual_homing_core" --pi-camera-smoke-active-profile \
            "${camera_profile_dir}" \
            "${active_camera_profile}" \
            "${VISUAL_HOMING_CAMERA_FPS:-15}" \
            "${VISUAL_HOMING_CAMERA_FRAMES:-30}"
    elif [[ "${VISUAL_HOMING_USE_CAMERA_PROFILE:-0}" == "1" ]]; then
        "${build_dir}/visual_homing_core" --pi-camera-smoke-profile \
            "${camera_profile}" \
            "${VISUAL_HOMING_CAMERA_FPS:-15}" \
            "${VISUAL_HOMING_CAMERA_FRAMES:-30}"
    else
        "${build_dir}/visual_homing_core" --pi-camera-smoke \
            "${VISUAL_HOMING_CAMERA_WIDTH:-320}" \
            "${VISUAL_HOMING_CAMERA_HEIGHT:-240}" \
            "${VISUAL_HOMING_CAMERA_FPS:-15}" \
            "${VISUAL_HOMING_CAMERA_FRAMES:-30}" \
            "${VISUAL_HOMING_CAMERA_TARGET_WIDTH:-32}" \
            "${VISUAL_HOMING_CAMERA_TARGET_HEIGHT:-24}"
    fi
fi

if [[ "${VISUAL_HOMING_RECORD_LIVE_ROUTE:-0}" == "1" ]]; then
    route_telemetry_args=()
    if [[ "${VISUAL_HOMING_ROUTE_USE_MAVLINK_TELEMETRY:-0}" == "1" ]]; then
        route_telemetry_args=("${mavlink_telemetry_input}")
    fi

    if [[ "${VISUAL_HOMING_ROUTE_USE_LIVE_MAVLINK_TELEMETRY:-0}" == "1" ]]; then
        if [[ "${VISUAL_HOMING_USE_ACTIVE_CAMERA_PROFILE:-0}" != "1" ]]; then
            echo "VISUAL_HOMING_ROUTE_USE_LIVE_MAVLINK_TELEMETRY=1 currently requires VISUAL_HOMING_USE_ACTIVE_CAMERA_PROFILE=1" >&2
            exit 2
        fi
        "${build_dir}/visual_homing_core" --record-live-route-active-profile-live-telemetry \
            "${camera_profile_dir}" \
            "${active_camera_profile}" \
            "${VISUAL_HOMING_CAMERA_FPS:-15}" \
            "${VISUAL_HOMING_CAMERA_FRAMES:-120}" \
            "${route_output}" \
            "${VISUAL_HOMING_ROUTE_ALTITUDE_M:-0.0}" \
            "${VISUAL_HOMING_ROUTE_HEADING_HINT_RAD:-0.0}" \
            "${route_warmup_frames}" \
            "${mavlink_telemetry_device}" \
            "${mavlink_telemetry_baud}" \
            "${route_telemetry_warmup_ms}" \
            "${camera_target_override_args[@]}" \
            "${operator_cue_args[@]}"
    elif [[ "${VISUAL_HOMING_USE_ACTIVE_CAMERA_PROFILE:-0}" == "1" ]]; then
        "${build_dir}/visual_homing_core" --record-live-route-active-profile \
            "${camera_profile_dir}" \
            "${active_camera_profile}" \
            "${VISUAL_HOMING_CAMERA_FPS:-15}" \
            "${VISUAL_HOMING_CAMERA_FRAMES:-120}" \
            "${route_output}" \
            "${VISUAL_HOMING_ROUTE_ALTITUDE_M:-0.0}" \
            "${VISUAL_HOMING_ROUTE_HEADING_HINT_RAD:-0.0}" \
            "${route_warmup_frames}" \
            "${route_telemetry_args[@]}" \
            "${camera_target_override_args[@]}" \
            "${operator_cue_args[@]}"
    elif [[ "${VISUAL_HOMING_USE_CAMERA_PROFILE:-0}" == "1" ]]; then
        "${build_dir}/visual_homing_core" --record-live-route-profile \
            "${camera_profile}" \
            "${VISUAL_HOMING_CAMERA_FPS:-15}" \
            "${VISUAL_HOMING_CAMERA_FRAMES:-120}" \
            "${route_output}" \
            "${VISUAL_HOMING_ROUTE_ALTITUDE_M:-0.0}" \
            "${VISUAL_HOMING_ROUTE_HEADING_HINT_RAD:-0.0}" \
            "${route_warmup_frames}" \
            "${route_telemetry_args[@]}" \
            "${operator_cue_args[@]}"
    else
        "${build_dir}/visual_homing_core" --record-live-route \
            "${VISUAL_HOMING_CAMERA_WIDTH:-320}" \
            "${VISUAL_HOMING_CAMERA_HEIGHT:-240}" \
            "${VISUAL_HOMING_CAMERA_FPS:-15}" \
            "${VISUAL_HOMING_CAMERA_FRAMES:-120}" \
            "${route_output}" \
            "${VISUAL_HOMING_CAMERA_TARGET_WIDTH:-32}" \
            "${VISUAL_HOMING_CAMERA_TARGET_HEIGHT:-24}" \
            "${VISUAL_HOMING_ROUTE_ALTITUDE_M:-0.0}" \
            "${VISUAL_HOMING_ROUTE_HEADING_HINT_RAD:-0.0}" \
            "${route_warmup_frames}" \
            "${route_telemetry_args[@]}" \
            "${operator_cue_args[@]}"
    fi

    "${build_dir}/visual_homing_core" --inspect-route "${route_output}"
    "${build_dir}/visual_homing_core" --export-route-keyframes \
        "${route_output}" \
        "${route_keyframe_dir}" \
        "${route_keyframe_scale}"
    "${build_dir}/visual_homing_core" --self-match-route \
        "${route_output}" \
        "${VISUAL_HOMING_SELF_MATCH_MIN_CONFIDENCE:-0.99}"
    "${build_dir}/visual_homing_core" --perturb-route \
        "${route_output}" \
        "${VISUAL_HOMING_PERTURB_MIN_CONFIDENCE:-0.90}"
    if [[ -n "${VISUAL_HOMING_ROUTE_EDGE_TRIM:-}" ]]; then
        "${build_dir}/visual_homing_core" --route-distinctiveness \
            "${route_output}" \
            "${VISUAL_HOMING_ROUTE_EDGE_TRIM}"
    else
        "${build_dir}/visual_homing_core" --route-distinctiveness "${route_output}"
    fi
fi

if [[ "${VISUAL_HOMING_MATCH_LIVE_ROUTE:-0}" == "1" ]]; then
    if [[ "${VISUAL_HOMING_USE_ACTIVE_CAMERA_PROFILE:-0}" != "1" ]]; then
        echo "VISUAL_HOMING_MATCH_LIVE_ROUTE=1 currently requires VISUAL_HOMING_USE_ACTIVE_CAMERA_PROFILE=1" >&2
        exit 2
    fi
    "${build_dir}/visual_homing_core" --match-live-route-active-profile \
        "${camera_profile_dir}" \
        "${active_camera_profile}" \
        "${VISUAL_HOMING_CAMERA_FPS:-15}" \
        "${VISUAL_HOMING_CAMERA_FRAMES:-120}" \
        "${route_output}" \
        "${route_warmup_frames}" \
        "${live_route_match_window_radius}" \
        "${live_route_match_min_confidence}" \
        "${live_route_match_max_direction_shift_px}" \
        "${live_route_match_expected_progress}" \
        "${live_route_match_max_progress_regressions}" \
        "${live_route_match_max_progress_rollback}" \
        "${camera_target_override_args[@]}" \
        "${live_route_match_require_endpoint_progress}" \
        "${live_route_match_endpoint_start_progress}" \
        "${live_route_match_endpoint_end_progress}" \
        "${operator_cue_args[@]}" \
        "${live_route_dry_run_command_args[@]}" \
        "${live_route_match_telemetry_args[@]}" \
        "${live_route_session_audit_args[@]}" \
        "${live_route_endpoint_stop_args[@]}"
fi

if [[ "${VISUAL_HOMING_VALIDATE_ROUTE:-0}" == "1" ]]; then
    "${build_dir}/visual_homing_core" --inspect-route "${route_output}"
    "${build_dir}/visual_homing_core" --self-match-route \
        "${route_output}" \
        "${VISUAL_HOMING_SELF_MATCH_MIN_CONFIDENCE:-0.99}"
    "${build_dir}/visual_homing_core" --perturb-route \
        "${route_output}" \
        "${VISUAL_HOMING_PERTURB_MIN_CONFIDENCE:-0.90}"
    if [[ -n "${VISUAL_HOMING_ROUTE_EDGE_TRIM:-}" ]]; then
        "${build_dir}/visual_homing_core" --route-distinctiveness \
            "${route_output}" \
            "${VISUAL_HOMING_ROUTE_EDGE_TRIM}"
    else
        "${build_dir}/visual_homing_core" --route-distinctiveness "${route_output}"
    fi
fi

if [[ "${VISUAL_HOMING_INSPECT_ROUTE:-0}" == "1" ]]; then
    "${build_dir}/visual_homing_core" --inspect-route "${route_output}"
fi

if [[ "${VISUAL_HOMING_EXPORT_ROUTE_KEYFRAMES:-0}" == "1" ]]; then
    "${build_dir}/visual_homing_core" --export-route-keyframes \
        "${route_output}" \
        "${route_keyframe_dir}" \
        "${route_keyframe_scale}"
fi

if [[ "${VISUAL_HOMING_SELF_MATCH_ROUTE:-0}" == "1" ]]; then
    "${build_dir}/visual_homing_core" --self-match-route \
        "${route_output}" \
        "${VISUAL_HOMING_SELF_MATCH_MIN_CONFIDENCE:-0.99}"
fi

if [[ "${VISUAL_HOMING_PERTURB_ROUTE:-0}" == "1" ]]; then
    "${build_dir}/visual_homing_core" --perturb-route \
        "${route_output}" \
        "${VISUAL_HOMING_PERTURB_MIN_CONFIDENCE:-0.90}"
fi

if [[ "${VISUAL_HOMING_ROUTE_DISTINCTIVENESS:-0}" == "1" ]]; then
    if [[ -n "${VISUAL_HOMING_ROUTE_EDGE_TRIM:-}" ]]; then
        "${build_dir}/visual_homing_core" --route-distinctiveness \
            "${route_output}" \
            "${VISUAL_HOMING_ROUTE_EDGE_TRIM}"
    else
        "${build_dir}/visual_homing_core" --route-distinctiveness "${route_output}"
    fi
fi
