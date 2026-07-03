#!/usr/bin/env bash
set -euo pipefail

expected_estimates="${VISUAL_HOMING_EXPECTED_EXTERNAL_NAV_OUTPUT_AUDIT_ESTIMATES:-auto}"
expected_allowed="${VISUAL_HOMING_EXPECTED_EXTERNAL_NAV_OUTPUT_AUDIT_ALLOWED:-0}"
expected_sent="${VISUAL_HOMING_EXPECTED_EXTERNAL_NAV_OUTPUT_AUDIT_SENT:-0}"
expected_blocked="${VISUAL_HOMING_EXPECTED_EXTERNAL_NAV_OUTPUT_AUDIT_BLOCKED:-auto}"
expected_reason="${VISUAL_HOMING_EXPECTED_EXTERNAL_NAV_OUTPUT_AUDIT_REASON:-*}"
expected_stop_reason="${VISUAL_HOMING_EXPECTED_EXTERNAL_NAV_OUTPUT_AUDIT_STOP_REASON:-match_live_route_complete}"
expected_valid_for_fc="${VISUAL_HOMING_EXPECTED_EXTERNAL_NAV_OUTPUT_AUDIT_VALID_FOR_FC:-true}"

usage() {
    echo "usage: $0 <external-nav-output-audit-log> [<external-nav-output-audit-log> ...]" >&2
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
        echo "external_nav_output_audit_log_check path=${audit_path} passed=false field=${name} expected=${expected} actual=${actual}" >&2
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

require_uint() {
    local audit_path="$1"
    local name="$2"
    local value="$3"
    if [[ ! "${value}" =~ ^[0-9]+$ ]]; then
        echo "external_nav_output_audit_log_check path=${audit_path} passed=false field=${name} expected=uint actual=${value:-missing}" >&2
        return 1
    fi
}

check_audit_log() {
    local audit_path="$1"
    if [[ ! -f "${audit_path}" ]]; then
        echo "external_nav_output_audit_log_check path=${audit_path} passed=false error=audit_log_not_found" >&2
        return 1
    fi

    local start_count estimate_count stop_count
    start_count="$(grep -c '^external_nav_output_audit event=start' "${audit_path}" || true)"
    estimate_count="$(grep -c '^external_nav_output_audit event=estimate ' "${audit_path}" || true)"
    stop_count="$(grep -c '^external_nav_output_audit event=stop ' "${audit_path}" || true)"

    local resolved_expected_estimates resolved_expected_allowed resolved_expected_sent resolved_expected_blocked
    resolved_expected_estimates="$(resolve_expected_count "${expected_estimates}" "${estimate_count}")"
    resolved_expected_allowed="${expected_allowed}"
    resolved_expected_sent="${expected_sent}"
    if [[ "${expected_blocked}" == "auto" && "${resolved_expected_allowed}" != "auto" ]]; then
        require_uint "${audit_path}" "allowed" "${resolved_expected_allowed}" || return 1
        resolved_expected_blocked="$((estimate_count - resolved_expected_allowed))"
    else
        resolved_expected_blocked="${expected_blocked}"
    fi

    local failed=0
    require_count "${audit_path}" "start_events" "1" "${start_count}" || failed=1
    require_count "${audit_path}" "estimate_events" "${resolved_expected_estimates}" "${estimate_count}" || failed=1
    require_count "${audit_path}" "stop_events" "1" "${stop_count}" || failed=1

    local stop_line stop_reason
    stop_line="$(grep '^external_nav_output_audit event=stop ' "${audit_path}" | tail -1 || true)"
    stop_reason="$(extract_field "${stop_line}" "reason")"
    if [[ "${stop_reason}" != "${expected_stop_reason}" ]]; then
        echo "external_nav_output_audit_log_check path=${audit_path} passed=false field=stop_reason expected=${expected_stop_reason} actual=${stop_reason:-missing}" >&2
        failed=1
    fi

    local estimate_summary
    estimate_summary="$(awk \
        -v audit_path="${audit_path}" \
        -v expected_reason="${expected_reason}" \
        -v expected_allowed="${resolved_expected_allowed}" \
        -v expected_sent="${resolved_expected_sent}" \
        -v expected_blocked="${resolved_expected_blocked}" \
        -v expected_valid_for_fc="${expected_valid_for_fc}" '
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

        /^external_nav_output_audit event=estimate / {
            allowed = field("allowed")
            sent = field("sent")
            decision = field("decision")
            reason = field("reason")
            time_usec = field("time_usec")
            valid_for_fc = field("valid_for_fc")
            estimate_reason = field("estimate_reason")
            source = field("source")
            progress = field("progress")
            route_index = field("route_index")
            relative_altitude_seen = field("relative_altitude_seen")
            route_match_valid = field("route_match_valid")
            telemetry_fresh = field("telemetry_fresh")
            altitude_valid = field("altitude_valid")
            scale_known = field("scale_known")

            if (allowed == "true") {
                ++allowed_estimates
                if (decision != "" && decision != "allowed") {
                    printf "external_nav_output_audit_log_check path=%s passed=false estimate_index=%d field=decision expected=allowed actual=%s\n", audit_path, estimates + 1, decision > "/dev/stderr"
                    bad = 1
                }
            } else if (allowed == "false") {
                ++blocked_estimates
                if (decision != "" && decision != "blocked") {
                    printf "external_nav_output_audit_log_check path=%s passed=false estimate_index=%d field=decision expected=blocked actual=%s\n", audit_path, estimates + 1, decision > "/dev/stderr"
                    bad = 1
                }
            } else {
                printf "external_nav_output_audit_log_check path=%s passed=false estimate_index=%d field=allowed expected=true_or_false actual=%s\n", audit_path, estimates + 1, allowed > "/dev/stderr"
                bad = 1
            }

            if (sent == "true") {
                ++sent_estimates
            } else if (sent != "false") {
                printf "external_nav_output_audit_log_check path=%s passed=false estimate_index=%d field=sent expected=true_or_false actual=%s\n", audit_path, estimates + 1, sent > "/dev/stderr"
                bad = 1
            }

            if (expected_reason != "*" && reason != expected_reason) {
                printf "external_nav_output_audit_log_check path=%s passed=false estimate_index=%d field=reason expected=%s actual=%s\n", audit_path, estimates + 1, expected_reason, reason > "/dev/stderr"
                bad = 1
            }
            if (expected_valid_for_fc != "any" && valid_for_fc != expected_valid_for_fc) {
                printf "external_nav_output_audit_log_check path=%s passed=false estimate_index=%d field=valid_for_fc expected=%s actual=%s\n", audit_path, estimates + 1, expected_valid_for_fc, valid_for_fc > "/dev/stderr"
                bad = 1
            }
            if (time_usec !~ /^[0-9]+$/) {
                printf "external_nav_output_audit_log_check path=%s passed=false estimate_index=%d field=time_usec expected=uint actual=%s\n", audit_path, estimates + 1, time_usec > "/dev/stderr"
                bad = 1
            }
            if (estimate_reason == "" || source == "" || progress == "" || route_index == "") {
                printf "external_nav_output_audit_log_check path=%s passed=false estimate_index=%d field=estimate_core expected=present actual=missing\n", audit_path, estimates + 1 > "/dev/stderr"
                bad = 1
            }
            if (relative_altitude_seen != "true" || route_match_valid != "true" || telemetry_fresh != "true" || altitude_valid != "true" || scale_known != "true") {
                printf "external_nav_output_audit_log_check path=%s passed=false estimate_index=%d field=readiness_flags expected=true actual=relative_altitude_seen:%s,route_match_valid:%s,telemetry_fresh:%s,altitude_valid:%s,scale_known:%s\n", audit_path, estimates + 1, relative_altitude_seen, route_match_valid, telemetry_fresh, altitude_valid, scale_known > "/dev/stderr"
                bad = 1
            }
            ++estimates
        }

        END {
            if (expected_allowed != "auto" && allowed_estimates != expected_allowed) {
                printf "external_nav_output_audit_log_check path=%s passed=false field=allowed_estimates expected=%s actual=%d\n", audit_path, expected_allowed, allowed_estimates > "/dev/stderr"
                bad = 1
            }
            if (expected_sent != "auto" && sent_estimates != expected_sent) {
                printf "external_nav_output_audit_log_check path=%s passed=false field=sent_estimates expected=%s actual=%d\n", audit_path, expected_sent, sent_estimates > "/dev/stderr"
                bad = 1
            }
            if (expected_blocked != "auto" && blocked_estimates != expected_blocked) {
                printf "external_nav_output_audit_log_check path=%s passed=false field=blocked_estimates expected=%s actual=%d\n", audit_path, expected_blocked, blocked_estimates > "/dev/stderr"
                bad = 1
            }
            printf "%d %d %d", allowed_estimates, sent_estimates, blocked_estimates
            exit bad
        }
    ' "${audit_path}")" || failed=1

    local allowed_estimates sent_estimates blocked_estimates
    read -r allowed_estimates sent_estimates blocked_estimates <<< "${estimate_summary:-0 0 0}"

    if [[ "${failed}" != "0" ]]; then
        return 1
    fi

    echo "external_nav_output_audit_log_check path=${audit_path} passed=true estimates=${estimate_count} allowed=${allowed_estimates} sent=${sent_estimates} blocked=${blocked_estimates} reason=${expected_reason} stop_reason=${expected_stop_reason}"
}

overall_status=0
for audit_path in "$@"; do
    if ! check_audit_log "${audit_path}"; then
        overall_status=2
    fi
done

exit "${overall_status}"
