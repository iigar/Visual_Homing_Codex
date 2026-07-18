#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="${VISUAL_HOMING_PI_BUILD_DIR:-${repo_root}/core/build-pi}"
decoder="${build_dir}/rc12_local_reset_dry_run"
python_bin="${VISUAL_HOMING_DIAGNOSTICS_PYTHON:-/home/pi/.venvs/visual-homing-diagnostics/bin/python}"
capture_s="${VISUAL_HOMING_RC12_DRY_RUN_CAPTURE_S:-30}"
minimum_events="${VISUAL_HOMING_RC12_DRY_RUN_MIN_EVENTS:-1}"
maximum_events="${VISUAL_HOMING_RC12_DRY_RUN_MAX_EVENTS:-10}"
confirmation="${VISUAL_HOMING_RC12_DRY_RUN_CONFIRMATION:-}"
expected_confirmation="I_CONFIRM_RC12_DRY_RUN_NO_RESET_OR_HOME"

if [[ "${confirmation}" != "${expected_confirmation}" ]]; then
    echo "Set VISUAL_HOMING_RC12_DRY_RUN_CONFIRMATION=${expected_confirmation}" >&2
    exit 2
fi
if [[ ! -x "${python_bin}" ]]; then
    echo "Diagnostics Python is not executable: ${python_bin}" >&2
    exit 2
fi
if [[ ! -x "${decoder}" ]]; then
    echo "Dry-run decoder is not built: ${decoder}; run scripts/test-core-pi.sh first" >&2
    exit 2
fi

stamp="$(date -u +"%Y%m%dT%H%M%SZ")"
artifact_dir="${repo_root}/artifacts/fc_baseline"
log_dir="${repo_root}/artifacts/logs"
trace_path="${artifact_dir}/rc12-pwm-trace-${stamp}.txt"
log_path="${log_dir}/rc12-local-reset-dry-run-${stamp}.log"
mkdir -p "${artifact_dir}" "${log_dir}"

echo "rc12_local_reset_dry_run_start capture_s=${capture_s} trace=${trace_path} log=${log_path} request_only=true executor_attached=false fc_home_change_attached=false"
echo "Operator: move only RC12 between LOW and HIGH several times, then return it to LOW."

"${python_bin}" "${repo_root}/scripts/capture-fc-rc-baseline-pi.py" \
    --device /dev/serial0 \
    --baud 115200 \
    --output-dir "${artifact_dir}" \
    --capture-s "${capture_s}" \
    --request-period-s 0.25 \
    --trace-output "${trace_path}" \
    --require-rc12-option-zero

"${decoder}" "${trace_path}" "${minimum_events}" "${maximum_events}" | tee "${log_path}"

echo "rc12_local_reset_dry_run_complete trace=${trace_path} log=${log_path} executor_attached=false fc_home_change_attached=false"
