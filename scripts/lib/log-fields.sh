#!/usr/bin/env bash
# Shared field extraction only; callers own their readiness/safety policies.
# Preserve the existing format: literal spaces separate tokens, the first exact
# key wins, and only the value before a second '=' is returned. Missing/empty
# values produce no output; tabs and carriage returns are not normalized.
extract_field() {
    local line="$1"
    local key="$2"
    local token
    token="$(printf '%s\n' "${line}" | tr ' ' '\n' | awk -F= -v key="${key}" '$1 == key { print $2; exit }')"
    printf '%s' "${token}"
}
