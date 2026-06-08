#!/usr/bin/env bash
set -euo pipefail

expected_commands="${VISUAL_HOMING_EXPECTED_LIVE_SESSION_AUDIT_COMMANDS:-150}"
expected_reason="${VISUAL_HOMING_EXPECTED_LIVE_SESSION_AUDIT_REASON:-vehicle_not_armed}"
expected_stop_reason="${VISUAL_HOMING_EXPECTED_LIVE_SESSION_AUDIT_STOP_REASON:-match_live_route_complete}"
expected_allowed_commands="${VISUAL_HOMING_EXPECTED_LIVE_SESSION_AUDIT_ALLOWED_COMMANDS:-0}"
expected_blocked_commands="${VISUAL_HOMING_EXPECTED_LIVE_SESSION_AUDIT_BLOCKED_COMMANDS:-${expected_commands}}"

usage() {
    echo "usage: $0 <live-output-session-audit-log> [<live-output-session-audit-log> ...]" >&2
}

if [[ "$#" -lt 1 ]]; then
    usage
    exit 2
fi

extract_field() {
    local line="$1"
    local key="$2"
    local token
    token="$(printf '%s\n' "${line}" | tr ' ' '\n' | awk -F= -v key="${key}" '$1 == key { print $2; exit }')"
    printf '%s' "${token}"
}

require_count() {
    local audit_path="$1"
    local name="$2"
    local expected="$3"
    local actual="$4"
    if [[ "${actual}" != "${expected}" ]]; then
        echo "session_audit_log_check path=${audit_path} passed=false field=${name} expected=${expected} actual=${actual}" >&2
        return 1
    fi
}

resolve_expected_count() {
    local expected="$1"
    local actual="$2"
    if [[ "${expected}" == "auto" ]]; then
        printf '%s' "${actual}"
    else
        printf '%s' "${expected}"
    fi
}

check_audit_log() {
    local audit_path="$1"
    if [[ ! -f "${audit_path}" ]]; then
        echo "session_audit_log_check path=${audit_path} passed=false error=audit_log_not_found" >&2
        return 1
    fi

    local start_count command_count stop_count
    start_count="$(grep -c '^live_output_audit event=start' "${audit_path}" || true)"
    command_count="$(grep -c '^live_output_audit event=command ' "${audit_path}" || true)"
    stop_count="$(grep -c '^live_output_audit event=stop ' "${audit_path}" || true)"
    local resolved_expected_commands resolved_expected_allowed resolved_expected_blocked
    resolved_expected_commands="$(resolve_expected_count "${expected_commands}" "${command_count}")"
    resolved_expected_allowed="$(resolve_expected_count "${expected_allowed_commands}" "0")"
    if [[ "${expected_blocked_commands}" == "auto" ]]; then
        if [[ ! "${resolved_expected_allowed}" =~ ^[0-9]+$ ]]; then
            echo "session_audit_log_check path=${audit_path} passed=false field=allowed_commands expected=uint actual=${resolved_expected_allowed}" >&2
            return 1
        fi
        resolved_expected_blocked="$((command_count - resolved_expected_allowed))"
    else
        resolved_expected_blocked="${expected_blocked_commands}"
    fi

    local failed=0
    require_count "${audit_path}" "start_events" "1" "${start_count}" || failed=1
    require_count "${audit_path}" "command_events" "${resolved_expected_commands}" "${command_count}" || failed=1
    require_count "${audit_path}" "stop_events" "1" "${stop_count}" || failed=1

    local stop_line stop_reason
    stop_line="$(grep '^live_output_audit event=stop ' "${audit_path}" | tail -1 || true)"
    stop_reason="$(extract_field "${stop_line}" "reason")"
    if [[ "${stop_reason}" != "${expected_stop_reason}" ]]; then
        echo "session_audit_log_check path=${audit_path} passed=false field=stop_reason expected=${expected_stop_reason} actual=${stop_reason:-missing}" >&2
        failed=1
    fi

    local command_summary
    command_summary="$(awk \
        -v audit_path="${audit_path}" \
        -v expected_reason="${expected_reason}" \
        -v expected_allowed_commands="${resolved_expected_allowed}" \
        -v expected_blocked_commands="${resolved_expected_blocked}" '
        function field(key, parts, n, i, kv) {
            n = split($0, parts, " ")
            for (i = 1; i <= n; ++i) {
                split(parts[i], kv, "=")
                if (kv[1] == key) {
                    return kv[2]
                }
            }
            return ""
        }

        /^live_output_audit event=command / {
            allowed = field("allowed")
            reason = field("reason")
            valid = field("valid")
            vx = field("vx_mps")
            decision = field("decision")
            if (allowed == "true") {
                ++allowed_commands
                if (decision != "" && decision != "allowed") {
                    printf "session_audit_log_check path=%s passed=false command_index=%d field=decision expected=allowed actual=%s\n", audit_path, commands + 1, decision > "/dev/stderr"
                    bad = 1
                }
            } else if (allowed == "false") {
                ++blocked_commands
                if (decision != "" && decision != "blocked") {
                    printf "session_audit_log_check path=%s passed=false command_index=%d field=decision expected=blocked actual=%s\n", audit_path, commands + 1, decision > "/dev/stderr"
                    bad = 1
                }
            } else {
                printf "session_audit_log_check path=%s passed=false command_index=%d field=allowed expected=true_or_false actual=%s\n", audit_path, commands + 1, allowed > "/dev/stderr"
                bad = 1
            }
            if (expected_reason != "*" && reason != expected_reason) {
                printf "session_audit_log_check path=%s passed=false command_index=%d field=reason expected=%s actual=%s\n", audit_path, commands + 1, expected_reason, reason > "/dev/stderr"
                bad = 1
            }
            if (valid != "true") {
                printf "session_audit_log_check path=%s passed=false command_index=%d field=valid expected=true actual=%s\n", audit_path, commands + 1, valid > "/dev/stderr"
                bad = 1
            }
            if (vx != "0") {
                printf "session_audit_log_check path=%s passed=false command_index=%d field=vx_mps expected=0 actual=%s\n", audit_path, commands + 1, vx > "/dev/stderr"
                bad = 1
            }
            ++commands
        }

        END {
            if (allowed_commands != expected_allowed_commands) {
                printf "session_audit_log_check path=%s passed=false field=allowed_commands expected=%s actual=%d\n", audit_path, expected_allowed_commands, allowed_commands > "/dev/stderr"
                bad = 1
            }
            if (blocked_commands != expected_blocked_commands) {
                printf "session_audit_log_check path=%s passed=false field=blocked_commands expected=%s actual=%d\n", audit_path, expected_blocked_commands, blocked_commands > "/dev/stderr"
                bad = 1
            }
            printf "%d %d", allowed_commands, blocked_commands
            exit bad
        }
    ' "${audit_path}")" || failed=1

    local allowed_commands blocked_commands
    read -r allowed_commands blocked_commands <<< "${command_summary:-0 0}"

    if [[ "${failed}" != "0" ]]; then
        failed=1
    fi

    if [[ "${failed}" != "0" ]]; then
        return 1
    fi

    echo "session_audit_log_check path=${audit_path} passed=true commands=${command_count} allowed=${allowed_commands} blocked=${blocked_commands} reason=${expected_reason} stop_reason=${expected_stop_reason}"
}

overall_status=0
for audit_path in "$@"; do
    if ! check_audit_log "${audit_path}"; then
        overall_status=2
    fi
done

exit "${overall_status}"
