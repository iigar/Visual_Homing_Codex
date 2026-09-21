cmake_minimum_required(VERSION 3.16)

# Exercise the real CLI without opening a camera or telemetry device. Valid
# arguments reach a deliberately absent route; invalid ones must fail earlier.
foreach(required CORE_EXE PROFILE_DIR TEST_DIR)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "Missing ${required}")
    endif()
endforeach()
file(MAKE_DIRECTORY "${TEST_DIR}")
set(active_profile "${TEST_DIR}/active profile.txt")
file(WRITE "${active_profile}" "ov9281-160-wide\n")
set(missing_route "${TEST_DIR}/absent route.vhrs")
if(EXISTS "${missing_route}")
    message(FATAL_ERROR "The numeric-argument test requires an absent route")
endif()

# Do not let a developer's runtime environment affect this offline test.
execute_process(COMMAND "${CMAKE_COMMAND}" -E environment
    OUTPUT_VARIABLE inherited_environment RESULT_VARIABLE environment_result)
if(NOT environment_result STREQUAL "0")
    message(FATAL_ERROR "Could not enumerate the test environment")
endif()
string(REGEX MATCHALL "VISUAL_HOMING_[A-Za-z0-9_]+=" inherited_settings "${inherited_environment}")
foreach(setting IN LISTS inherited_settings)
    string(REGEX REPLACE "=$" "" setting "${setting}")
    unset(ENV{${setting}})
endforeach()
set(ENV{LC_ALL} C)
set(route_error "Could not open route signature file for read:")
set_property(GLOBAL PROPERTY numeric_case_count 0)

function(check_arguments label expected regressions rollback start end)
    execute_process(
        COMMAND "${CORE_EXE}" --match-live-route-active-profile
            "${PROFILE_DIR}" "${active_profile}" 10 1 "${missing_route}"
            0 1 0.8 0 forward "${regressions}" "${rollback}" false "${start}" "${end}"
        RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error
        TIMEOUT 10)
    string(FIND "${error}" "${expected}" found)
    get_property(count GLOBAL PROPERTY numeric_case_count)
    math(EXPR count "${count} + 1")
    set_property(GLOBAL PROPERTY numeric_case_count "${count}")
    if(NOT result STREQUAL "1" OR found EQUAL -1)
        set_property(GLOBAL APPEND PROPERTY numeric_failures "${label}")
        message(STATUS "FAIL ${label}: exit=${result}, expected=${expected}, stderr=${error}")
    endif()
endfunction()

function(check_environment name value expected)
    set(ENV{${name}} "${value}")
    check_arguments("${name}=[${value}]" "${expected}" 5 0.25 0.15 0.85)
    unset(ENV{${name}})
endfunction()

# Preserve existing valid syntax, including leading whitespace and plus signs.
foreach(value "0" "5" "+5" " 5" "\t+5" "18446744073709551615")
    check_arguments("uint64 valid [${value}]" "${route_error}" "${value}" 0.25 0.15 0.85)
endforeach()
foreach(value "-1" " -1" "\t-1" "\n-1" "\r-1" " -0")
    check_arguments("uint64 negative [${value}]" "max_progress_regressions must be a non-negative integer"
        "${value}" 0.25 0.15 0.85)
endforeach()
foreach(value "5x" "5 " "1.5")
    check_arguments("uint64 trailing [${value}]" "max_progress_regressions must be a complete non-negative integer"
        "${value}" 0.25 0.15 0.85)
endforeach()

foreach(suffix MAX_KEYFRAMES DESCRIPTOR_DIMENSIONS)
    set(name "VISUAL_HOMING_LIVE_ROUTE_VERIFICATION_${suffix}")
    foreach(value 0 1 4294967295)
        check_environment("${name}" "${value}" "${route_error}")
    endforeach()
    foreach(value 4294967296 4294967297 18446744073709551615)
        check_environment("${name}" "${value}" "${name} is outside uint32_t range")
    endforeach()
endforeach()

foreach(suffix MIN_INTERVAL_MS MAX_INTERVAL_MS)
    set(name "VISUAL_HOMING_LIVE_ROUTE_VERIFICATION_${suffix}")
    foreach(value 0 1 18446744073709)
        check_environment("${name}" "${value}" "${route_error}")
    endforeach()
    foreach(value 18446744073710 18446744073709551615)
        check_environment("${name}" "${value}" "${name} is too large to convert to nanoseconds")
    endforeach()
endforeach()

foreach(value nan NaN inf -inf Infinity)
    check_arguments("rollback ${value}" "max_progress_rollback must be finite" 5 "${value}" 0.15 0.85)
    check_arguments("start ${value}" "endpoint_start_progress must be finite" 5 0.25 "${value}" 0.85)
    check_arguments("end ${value}" "endpoint_end_progress must be finite" 5 0.25 0.15 "${value}")
    check_environment(VISUAL_HOMING_LIVE_ROUTE_VERIFICATION_ROUTE_LENGTH_M "${value}"
        "VISUAL_HOMING_LIVE_ROUTE_VERIFICATION_ROUTE_LENGTH_M must be finite")
endforeach()
check_arguments("finite scientific notation" "${route_error}" 5 " 2.5e-1" 0 1)
check_arguments("finite negative rollback" "max_progress_rollback" 5 -0.25 0.15 0.85)
check_arguments("inverted endpoints" "endpoint_start_progress must be less than endpoint_end_progress" 5 0.25 0.9 0.1)

get_property(count GLOBAL PROPERTY numeric_case_count)
get_property(failures GLOBAL PROPERTY numeric_failures)
if(failures)
    list(LENGTH failures failed_count)
    message(FATAL_ERROR "${failed_count}/${count} numeric CLI cases failed: ${failures}")
endif()
message(STATUS "All ${count} numeric CLI cases passed")
