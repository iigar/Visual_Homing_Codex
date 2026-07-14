#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
log_dir="${VISUAL_HOMING_LOG_DIR:-${repo_root}/artifacts/logs}"
stamp="$(date -u +"%Y%m%dT%H%M%SZ")"
readiness_log="${VISUAL_HOMING_FIELD_READINESS_LOG:-${log_dir}/pi-field-readiness-${stamp}.log}"

device="${VISUAL_HOMING_MAVLINK_TELEMETRY_DEVICE:-/dev/serial0}"
require_route="${VISUAL_HOMING_FIELD_READINESS_REQUIRE_ROUTE:-1}"
route_path="${VISUAL_HOMING_ROUTE_OUTPUT:-${repo_root}/artifacts/visual_homing_live_route.vhrs}"
active_camera_profile="${VISUAL_HOMING_ACTIVE_CAMERA_PROFILE:-${repo_root}/artifacts/active_camera_profile.txt}"
run_telemetry_sanity="${VISUAL_HOMING_FIELD_READINESS_RUN_TELEMETRY_SANITY:-1}"
telemetry_duration_ms="${VISUAL_HOMING_FIELD_READINESS_TELEMETRY_DURATION_MS:-5000}"
git_fsck_enabled="${VISUAL_HOMING_FIELD_READINESS_GIT_FSCK:-1}"

mkdir -p "${log_dir}"

passed=true
first_reason="valid"

record() {
    printf '%s\n' "$*" | tee -a "${readiness_log}"
}

block() {
    local reason="$1"
    local detail="$2"
    if [[ "${passed}" == "true" ]]; then
        first_reason="${reason}"
    fi
    passed=false
    record "field_readiness_check name=${reason} passed=false ${detail}"
}

pass() {
    local name="$1"
    local detail="$2"
    record "field_readiness_check name=${name} passed=true ${detail}"
}

require_bool() {
    local name="$1"
    local value="$2"
    case "${value}" in
        0|1) ;;
        *)
            echo "${name} must be 0 or 1, got '${value}'" >&2
            exit 2
            ;;
    esac
}

require_bool "VISUAL_HOMING_FIELD_READINESS_REQUIRE_ROUTE" "${require_route}"
require_bool "VISUAL_HOMING_FIELD_READINESS_RUN_TELEMETRY_SANITY" "${run_telemetry_sanity}"
require_bool "VISUAL_HOMING_FIELD_READINESS_GIT_FSCK" "${git_fsck_enabled}"

record "visual_homing_pi_field_readiness_start wall_time_utc=$(date -u +"%Y-%m-%dT%H:%M:%SZ") repo_root=${repo_root} log_path=${readiness_log}"

if [[ "${EUID}" -eq 0 ]]; then
    block "running_as_root" "hint=run_visual_homing_wrappers_as_pi_not_sudo"
else
    pass "running_as_non_root" "user=$(id -un)"
fi

if git -C "${repo_root}" rev-parse --is-inside-work-tree >/dev/null 2>&1; then
    head_commit="$(git -C "${repo_root}" rev-parse --short HEAD 2>/dev/null || true)"
    pass "git_worktree" "head=${head_commit:-unknown}"
else
    block "git_worktree_invalid" "hint=run_from_valid_visual_homing_checkout"
fi

if [[ "${git_fsck_enabled}" == "1" ]]; then
    if git -C "${repo_root}" fsck --connectivity-only --no-dangling >/dev/null 2>&1; then
        pass "git_fsck" "mode=connectivity_only"
    else
        block "git_fsck_failed" "hint=repair_or_reclone_checkout_before_field_tests"
    fi
else
    pass "git_fsck" "skipped=true"
fi

if [[ -e "${device}" ]]; then
    device_target="${device}"
    if [[ -L "${device}" ]]; then
        link_target="$(readlink "${device}")"
        device_target="$(readlink -f "${device}" 2>/dev/null || printf '%s' "${device}")"
        pass "serial_device_exists" "device=${device} symlink_target=${link_target} resolved=${device_target}"
    else
        pass "serial_device_exists" "device=${device} resolved=${device_target}"
    fi
else
    block "serial_device_missing" "device=${device} hint=check_uart_overlay_and_fc_serial_connection"
    device_target="${device}"
fi

if [[ -e "${device_target}" ]]; then
    device_group="$(stat -c '%G' "${device_target}" 2>/dev/null || echo missing)"
    device_mode="$(stat -c '%a' "${device_target}" 2>/dev/null || echo missing)"
    if [[ "${device_group}" == "dialout" ]]; then
        pass "serial_group" "target=${device_target} group=${device_group} mode=${device_mode}"
    else
        block "serial_group_wrong" "target=${device_target} group=${device_group} mode=${device_mode} fix='sudo chgrp dialout ${device_target}'"
    fi
    if [[ -r "${device_target}" && -w "${device_target}" ]]; then
        pass "serial_access" "target=${device_target} readable=true writable=true"
    else
        block "serial_access_denied" "target=${device_target} fix='sudo chmod 660 ${device_target}; ensure user is in dialout; reboot or relogin after usermod'"
    fi
fi

if id -nG | tr ' ' '\n' | grep -qx 'dialout'; then
    pass "user_group_dialout" "user=$(id -un)"
else
    block "user_not_in_dialout" "user=$(id -un) fix='sudo usermod -aG dialout $(id -un); relogin_or_reboot'"
fi

stale_tmp="$(find /tmp -maxdepth 1 -name 'visual_homing_*' ! -user "$(id -un)" -print -quit 2>/dev/null || true)"
if [[ -n "${stale_tmp}" ]]; then
    block "visual_homing_tmp_owned_by_other_user" "path=${stale_tmp} hint=remove_reviewed_stale_tmp_files_and_avoid_sudo_wrappers"
else
    pass "visual_homing_tmp_ownership" "status=ok"
fi

if [[ "${require_route}" == "1" ]]; then
    if [[ -f "${route_path}" ]]; then
        pass "route_file" "path=${route_path}"
    else
        block "route_file_missing" "path=${route_path} hint='export VISUAL_HOMING_ROUTE_OUTPUT=/home/pi/Visual_Homing_Codex/artifacts/field_routes/<route>.vhrs'"
    fi
else
    pass "route_file" "skipped=true path=${route_path}"
fi

if [[ -f "${active_camera_profile}" ]]; then
    profile_id="$(grep -E '^id=' "${active_camera_profile}" | head -1 | cut -d= -f2- || true)"
    pass "active_camera_profile" "path=${active_camera_profile} id=${profile_id:-unknown}"
else
    block "active_camera_profile_missing" "path=${active_camera_profile} hint='select camera profile before field tests'"
fi

if [[ "${run_telemetry_sanity}" == "1" ]]; then
    record "field_readiness_check name=telemetry_sanity started=true duration_ms=${telemetry_duration_ms}"
    if VISUAL_HOMING_MAVLINK_TELEMETRY_DEVICE="${device}" \
        VISUAL_HOMING_MAVLINK_TELEMETRY_DURATION_MS="${telemetry_duration_ms}" \
        "${repo_root}/scripts/check-external-nav-telemetry-sanity-pi.sh" | tee -a "${readiness_log}"; then
        pass "telemetry_sanity" "status=passed"
    else
        block "telemetry_sanity_failed" "hint=inspect_external_nav_telemetry_sanity_line_above"
    fi
else
    pass "telemetry_sanity" "skipped=true"
fi

record "visual_homing_pi_field_readiness_done passed=${passed} reason=${first_reason} log_path=${readiness_log} route=${route_path} device=${device}"

if [[ "${passed}" == "true" ]]; then
    exit 0
fi
exit 2
