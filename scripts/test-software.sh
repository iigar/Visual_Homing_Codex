#!/usr/bin/env bash
set -euo pipefail

if (( $# > 1 )); then
    echo "usage: $0 [BUILD_PARENT]" >&2
    exit 2
fi

repo_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
build_parent="${1:-${repo_dir}/core/build-software}"
jobs="${VISUAL_HOMING_TEST_JOBS:-2}"
if [[ ! "${jobs}" =~ ^[1-9][0-9]*$ ]]; then
    echo "VISUAL_HOMING_TEST_JOBS must be a positive integer" >&2
    exit 2
fi
for dependency in cmake ctest ninja python3; do
    if ! command -v "${dependency}" >/dev/null 2>&1; then
        echo "Required software-test dependency is missing: ${dependency}" >&2
        exit 2
    fi
done

flags=(
    -DBUILD_TESTING=ON
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
    -DVISUAL_HOMING_ENABLE_LIBCAMERA=OFF
    -DVISUAL_HOMING_ENABLE_LIVE_MAVLINK_OUTPUT=OFF
    -DVISUAL_HOMING_ENABLE_BENCH_PROPS_OFF_LIVE_OUTPUT=OFF
    -DVISUAL_HOMING_ATTACH_BENCH_PROPS_OFF_SERIAL_WRITER=OFF
    -DVISUAL_HOMING_ENABLE_EXTERNAL_NAV_OUTPUT=OFF
    -DVISUAL_HOMING_ENABLE_BENCH_PROPS_OFF_EXTERNAL_NAV_OUTPUT=OFF
    -DVISUAL_HOMING_ATTACH_BENCH_PROPS_OFF_EXTERNAL_NAV_WRITER=OFF
)

for configuration in Debug Release; do
    build_dir="${build_parent}/${configuration}"
    cmake -S "${repo_dir}/core" -B "${build_dir}" -G Ninja \
        "-DCMAKE_BUILD_TYPE=${configuration}" "${flags[@]}"
    cmake --build "${build_dir}" --parallel "${jobs}"
    ctest --test-dir "${build_dir}" --output-on-failure
done

python3 "${repo_dir}/scripts/tests/test_readiness_logs.py" -v
echo "Software validation passed: Debug, Release and readiness log consumers."
