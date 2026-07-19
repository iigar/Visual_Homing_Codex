#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
core_dir="${repo_root}/core"
build_dir="${VISUAL_HOMING_PI_BUILD_DIR:-${core_dir}/build-pi-verification-benchmark}"
artifact_root="${VISUAL_HOMING_ARTIFACT_DIR:-${repo_root}/artifacts}"
stamp="$(date -u +"%Y%m%dT%H%M%SZ")"
benchmark_parent="${VISUAL_HOMING_VERIFICATION_BENCHMARK_DIR:-${artifact_root}/verification_benchmarks}"
output_dir="${benchmark_parent}/verification-benchmark-${stamp}"
log_dir="${VISUAL_HOMING_LOG_DIR:-${artifact_root}/logs}"
run_log="${VISUAL_HOMING_RUN_LOG:-${log_dir}/verification-capture-benchmark-${stamp}.log}"
system_log="${VISUAL_HOMING_SYSTEM_LOG:-${log_dir}/verification-capture-system-${stamp}.csv}"
build_log="${VISUAL_HOMING_BUILD_LOG:-${log_dir}/verification-capture-build-${stamp}.log}"

profile_id="${VISUAL_HOMING_CAMERA_PROFILE_ID:-ov9281-160-wide}"
width="${VISUAL_HOMING_CAMERA_WIDTH:-1280}"
height="${VISUAL_HOMING_CAMERA_HEIGHT:-800}"
fps="${VISUAL_HOMING_CAMERA_FPS:-10}"
duration_seconds="${VISUAL_HOMING_BENCHMARK_DURATION_SECONDS:-600}"
warmup_frames="${VISUAL_HOMING_ROUTE_WARMUP_FRAMES:-5}"
capture_interval_seconds="${VISUAL_HOMING_VERIFICATION_CAPTURE_INTERVAL_SECONDS:-10}"
altitude_m="${VISUAL_HOMING_BENCHMARK_ALTITUDE_M:-1.0}"
build_jobs="${VISUAL_HOMING_BUILD_JOBS:-1}"

mkdir -p "${benchmark_parent}" "${log_dir}"

cat <<EOF
###############################################################################
### NATIVE VERIFICATION CAPTURE BENCHMARK
### Camera and SD-card load test only.
### FC/UART/MAVLink/ODOMETRY/command output are not opened or used.
### output_dir=${output_dir}
### run_log=${run_log}
### system_log=${system_log}
### requested_capture=${width}x${height}@${fps}
### duration_seconds=${duration_seconds}
### sparse_capture_interval_seconds=${capture_interval_seconds}
###############################################################################
EOF

cmake -S "${core_dir}" -B "${build_dir}" -G Ninja \
    -DCMAKE_BUILD_TYPE=MinSizeRel \
    -DBUILD_TESTING=ON \
    -DVISUAL_HOMING_ENABLE_LIBCAMERA=ON \
    -DVISUAL_HOMING_ENABLE_LIVE_MAVLINK_OUTPUT=OFF \
    -DVISUAL_HOMING_ENABLE_BENCH_PROPS_OFF_LIVE_OUTPUT=OFF \
    -DVISUAL_HOMING_ATTACH_BENCH_PROPS_OFF_SERIAL_WRITER=OFF \
    -DVISUAL_HOMING_ENABLE_EXTERNAL_NAV_OUTPUT=OFF \
    -DVISUAL_HOMING_ENABLE_BENCH_PROPS_OFF_EXTERNAL_NAV_OUTPUT=OFF \
    -DVISUAL_HOMING_ATTACH_BENCH_PROPS_OFF_EXTERNAL_NAV_WRITER=OFF \
    >"${build_log}" 2>&1
cmake --build "${build_dir}" --parallel "${build_jobs}" >>"${build_log}" 2>&1
ctest --test-dir "${build_dir}" --output-on-failure >>"${build_log}" 2>&1

disk_before="$(df -B1 --output=avail "${benchmark_parent}" | tail -1 | tr -d ' ')"
temperature_before="$(cat /sys/class/thermal/thermal_zone0/temp 2>/dev/null || printf 'unavailable')"
throttled_before="$(vcgencmd get_throttled 2>/dev/null || printf 'unavailable')"

"${build_dir}/live_verification_capture_benchmark" \
    "${output_dir}" \
    "${profile_id}" \
    "${width}" \
    "${height}" \
    "${fps}" \
    "${duration_seconds}" \
    "${warmup_frames}" \
    "${capture_interval_seconds}" \
    "${altitude_m}" \
    >"${run_log}" 2>&1 &
benchmark_pid=$!

cleanup_child() {
    if kill -0 "${benchmark_pid}" 2>/dev/null; then
        kill "${benchmark_pid}" 2>/dev/null || true
        wait "${benchmark_pid}" 2>/dev/null || true
    fi
}
trap cleanup_child EXIT INT TERM

printf 'wall_time_utc,rss_kib,temperature_millic,arm_clock_hz,get_throttled,disk_available_bytes\n' >"${system_log}"
while kill -0 "${benchmark_pid}" 2>/dev/null; do
    rss_kib="$(awk '/^VmRSS:/ {print $2}' "/proc/${benchmark_pid}/status" 2>/dev/null || true)"
    temperature="$(cat /sys/class/thermal/thermal_zone0/temp 2>/dev/null || printf 'unavailable')"
    arm_clock="$(vcgencmd measure_clock arm 2>/dev/null | awk -F= '{print $2}' || true)"
    throttled="$(vcgencmd get_throttled 2>/dev/null || printf 'unavailable')"
    disk_available="$(df -B1 --output=avail "${benchmark_parent}" | tail -1 | tr -d ' ')"
    printf '%s,%s,%s,%s,%s,%s\n' \
        "$(date -u +"%Y-%m-%dT%H:%M:%SZ")" \
        "${rss_kib:-unavailable}" \
        "${temperature:-unavailable}" \
        "${arm_clock:-unavailable}" \
        "${throttled:-unavailable}" \
        "${disk_available:-unavailable}" \
        >>"${system_log}"
    sleep 1
done

set +e
wait "${benchmark_pid}"
benchmark_status=$?
set -e
trap - EXIT INT TERM

if [[ "${benchmark_status}" -ne 0 ]]; then
    tail -40 "${run_log}" >&2 || true
    echo "verification_capture_benchmark_failed status=${benchmark_status} run_log=${run_log} system_log=${system_log}" >&2
    exit "${benchmark_status}"
fi

if ! grep -q '^live_verification_benchmark_done passed=true ' "${run_log}"; then
    tail -40 "${run_log}" >&2 || true
    echo "verification_capture_benchmark_failed reason=missing_pass_summary run_log=${run_log}" >&2
    exit 1
fi

disk_after="$(df -B1 --output=avail "${benchmark_parent}" | tail -1 | tr -d ' ')"
temperature_after="$(cat /sys/class/thermal/thermal_zone0/temp 2>/dev/null || printf 'unavailable')"
throttled_after="$(vcgencmd get_throttled 2>/dev/null || printf 'unavailable')"
summary="$(grep '^live_verification_benchmark_done ' "${run_log}" | tail -1)"

echo "${summary}"
echo "verification_capture_system_summary disk_before=${disk_before} disk_after=${disk_after} temperature_before=${temperature_before} temperature_after=${temperature_after} throttled_before=${throttled_before} throttled_after=${throttled_after}"
echo "verification_capture_benchmark_ready output=${output_dir} run_log=${run_log} system_log=${system_log} build_log=${build_log}"
