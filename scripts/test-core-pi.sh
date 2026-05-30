#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
core_dir="${repo_root}/core"
build_dir="${core_dir}/build-pi"

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
    -DCMAKE_BUILD_TYPE=Release \
    -DVISUAL_HOMING_ENABLE_LIBCAMERA=ON

cmake --build "${build_dir}"
ctest --test-dir "${build_dir}" --output-on-failure

if [[ "${VISUAL_HOMING_RUN_CAMERA_SMOKE:-0}" == "1" ]]; then
    "${build_dir}/visual_homing_core" --pi-camera-smoke \
        "${VISUAL_HOMING_CAMERA_WIDTH:-320}" \
        "${VISUAL_HOMING_CAMERA_HEIGHT:-240}" \
        "${VISUAL_HOMING_CAMERA_FPS:-15}" \
        "${VISUAL_HOMING_CAMERA_FRAMES:-30}"
fi
