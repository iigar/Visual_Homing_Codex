#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
core_dir="${repo_root}/core"
build_dir="${core_dir}/build-pi"
build_type="${VISUAL_HOMING_PI_BUILD_TYPE:-MinSizeRel}"
build_jobs="${VISUAL_HOMING_BUILD_JOBS:-1}"
artifact_dir="${repo_root}/artifacts"
log_dir="${VISUAL_HOMING_LOG_DIR:-${artifact_dir}/logs}"
route_output="${VISUAL_HOMING_ROUTE_OUTPUT:-${artifact_dir}/visual_homing_live_route.vhrs}"
route_warmup_frames="${VISUAL_HOMING_ROUTE_WARMUP_FRAMES:-3}"
camera_profile_dir="${VISUAL_HOMING_CAMERA_PROFILE_DIR:-${repo_root}/config/camera_profiles}"
camera_profile="${VISUAL_HOMING_CAMERA_PROFILE:-${repo_root}/config/camera_profiles/imx219-visible-wide.profile}"
active_camera_profile="${VISUAL_HOMING_ACTIVE_CAMERA_PROFILE:-${artifact_dir}/active_camera_profile.txt}"
run_started_wall_time_utc="$(date -u +"%Y-%m-%dT%H:%M:%SZ")"
run_log_stamp="$(date -u +"%Y%m%dT%H%M%SZ")"
run_started_epoch="$(date +%s)"
run_log_file="${VISUAL_HOMING_RUN_LOG:-${log_dir}/test-core-pi-${run_log_stamp}.log}"

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

echo "pi_test_run_start wall_time_utc=${run_started_wall_time_utc} log_path=${run_log_file} repo_root=${repo_root} route_output=${route_output} route_warmup_frames=${route_warmup_frames}"

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
    -DVISUAL_HOMING_ENABLE_LIBCAMERA=ON

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

if [[ "${VISUAL_HOMING_RECORD_LIVE_ROUTE:-0}" == "1" || "${VISUAL_HOMING_VALIDATE_ROUTE:-0}" == "1" || "${VISUAL_HOMING_INSPECT_ROUTE:-0}" == "1" || "${VISUAL_HOMING_SELF_MATCH_ROUTE:-0}" == "1" || "${VISUAL_HOMING_PERTURB_ROUTE:-0}" == "1" || "${VISUAL_HOMING_ROUTE_DISTINCTIVENESS:-0}" == "1" ]]; then
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
    if [[ "${VISUAL_HOMING_USE_ACTIVE_CAMERA_PROFILE:-0}" == "1" ]]; then
        "${build_dir}/visual_homing_core" --record-live-route-active-profile \
            "${camera_profile_dir}" \
            "${active_camera_profile}" \
            "${VISUAL_HOMING_CAMERA_FPS:-15}" \
            "${VISUAL_HOMING_CAMERA_FRAMES:-120}" \
            "${route_output}" \
            "${VISUAL_HOMING_ROUTE_ALTITUDE_M:-0.0}" \
            "${VISUAL_HOMING_ROUTE_HEADING_HINT_RAD:-0.0}" \
            "${route_warmup_frames}"
    elif [[ "${VISUAL_HOMING_USE_CAMERA_PROFILE:-0}" == "1" ]]; then
        "${build_dir}/visual_homing_core" --record-live-route-profile \
            "${camera_profile}" \
            "${VISUAL_HOMING_CAMERA_FPS:-15}" \
            "${VISUAL_HOMING_CAMERA_FRAMES:-120}" \
            "${route_output}" \
            "${VISUAL_HOMING_ROUTE_ALTITUDE_M:-0.0}" \
            "${VISUAL_HOMING_ROUTE_HEADING_HINT_RAD:-0.0}" \
            "${route_warmup_frames}"
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
            "${route_warmup_frames}"
    fi

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
