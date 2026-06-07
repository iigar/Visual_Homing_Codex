cmake_minimum_required(VERSION 3.16)

project(visual_homing_core LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(VISUAL_HOMING_ENABLE_LIBCAMERA "Build the Raspberry Pi libcamera capture backend" OFF)
option(VISUAL_HOMING_ENABLE_LIVE_MAVLINK_OUTPUT "Request live MAVLink command output build scope" OFF)
option(VISUAL_HOMING_ENABLE_BENCH_PROPS_OFF_LIVE_OUTPUT "Request the reviewed bench props-off live-output boundary scope" OFF)

if(VISUAL_HOMING_ENABLE_LIVE_MAVLINK_OUTPUT AND NOT VISUAL_HOMING_ENABLE_BENCH_PROPS_OFF_LIVE_OUTPUT)
    message(FATAL_ERROR "Live MAVLink command output requires VISUAL_HOMING_ENABLE_BENCH_PROPS_OFF_LIVE_OUTPUT=ON and remains blocked until the bench props-off writer is implemented and reviewed")
endif()

if(VISUAL_HOMING_ENABLE_BENCH_PROPS_OFF_LIVE_OUTPUT AND NOT VISUAL_HOMING_ENABLE_LIVE_MAVLINK_OUTPUT)
    message(FATAL_ERROR "VISUAL_HOMING_ENABLE_BENCH_PROPS_OFF_LIVE_OUTPUT is only valid with VISUAL_HOMING_ENABLE_LIVE_MAVLINK_OUTPUT=ON")
endif()

add_executable(visual_homing_core
    src/main.cpp
)

add_library(visual_homing_core_lib
    src/bounded_navigator.cpp
    src/camera_profile.cpp
    src/camera_smoke.cpp
    src/dry_run_command_sink.cpp
    src/dry_run_mavlink_bridge.cpp
    src/gray8_resize_preprocessor.cpp
    src/gray8_route_matcher.cpp
    src/health_monitor.cpp
    src/live_mavlink_bridge.cpp
    src/live_mavlink_output_audit_log.cpp
    src/live_mavlink_output_session.cpp
    src/live_mavlink_output_safety_gate.cpp
    src/live_mavlink_serial_writer.cpp
    src/mavlink_telemetry_adapter.cpp
    src/mavlink_telemetry_capture.cpp
    src/mavlink_telemetry_inspector.cpp
    src/mavlink_telemetry_stream.cpp
    src/pi_camera_source.cpp
    src/pipeline_harness.cpp
    src/replay_frame_source.cpp
    src/route_artifact_check.cpp
    src/route_signature_recorder.cpp
    src/route_signature.cpp
    src/system_clock.cpp
)

target_include_directories(visual_homing_core_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_definitions(visual_homing_core_lib PUBLIC
    VISUAL_HOMING_LIVE_MAVLINK_OUTPUT_BLOCKED=1
    VISUAL_HOMING_LIVE_MAVLINK_OUTPUT_AVAILABLE=0
)

if(VISUAL_HOMING_ENABLE_LIVE_MAVLINK_OUTPUT)
    target_compile_definitions(visual_homing_core_lib PUBLIC
        VISUAL_HOMING_LIVE_MAVLINK_OUTPUT_BUILD_REQUESTED=1
        VISUAL_HOMING_BENCH_PROPS_OFF_LIVE_OUTPUT_SCOPE=1
    )
else()
    target_compile_definitions(visual_homing_core_lib PUBLIC
        VISUAL_HOMING_LIVE_MAVLINK_OUTPUT_BUILD_REQUESTED=0
        VISUAL_HOMING_BENCH_PROPS_OFF_LIVE_OUTPUT_SCOPE=0
    )
endif()

if(VISUAL_HOMING_ENABLE_LIBCAMERA)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(LIBCAMERA REQUIRED IMPORTED_TARGET libcamera)
    target_compile_definitions(visual_homing_core_lib PUBLIC VISUAL_HOMING_ENABLE_LIBCAMERA)
    target_link_libraries(visual_homing_core_lib PUBLIC PkgConfig::LIBCAMERA)
endif()

target_compile_options(visual_homing_core_lib PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
)

target_link_libraries(visual_homing_core PRIVATE visual_homing_core_lib)

target_compile_options(visual_homing_core PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
)

include(CTest)

if(BUILD_TESTING)
    add_compile_options(
        $<$<CXX_COMPILER_ID:GNU,Clang>:-UNDEBUG>
        $<$<CXX_COMPILER_ID:MSVC>:/UNDEBUG>
    )

    add_executable(camera_smoke_test
        tests/camera_smoke_test.cpp
    )

    target_link_libraries(camera_smoke_test PRIVATE visual_homing_core_lib)

    target_compile_options(camera_smoke_test PRIVATE
        $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
        $<$<CXX_COMPILER_ID:MSVC>:/W4>
    )

    add_test(NAME camera_smoke COMMAND camera_smoke_test)

    add_executable(camera_profile_test
        tests/camera_profile_test.cpp
    )

    target_link_libraries(camera_profile_test PRIVATE visual_homing_core_lib)

    target_compile_options(camera_profile_test PRIVATE
        $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
        $<$<CXX_COMPILER_ID:MSVC>:/W4>
    )

    add_test(NAME camera_profile COMMAND camera_profile_test)

    add_executable(bounded_navigator_test
        tests/bounded_navigator_test.cpp
    )

    target_link_libraries(bounded_navigator_test PRIVATE visual_homing_core_lib)

    target_compile_options(bounded_navigator_test PRIVATE
        $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
        $<$<CXX_COMPILER_ID:MSVC>:/W4>
    )

    add_test(NAME bounded_navigator COMMAND bounded_navigator_test)

    add_executable(dry_run_command_sink_test
        tests/dry_run_command_sink_test.cpp
    )

    target_link_libraries(dry_run_command_sink_test PRIVATE visual_homing_core_lib)

    target_compile_options(dry_run_command_sink_test PRIVATE
        $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
        $<$<CXX_COMPILER_ID:MSVC>:/W4>
    )

    add_test(NAME dry_run_command_sink COMMAND dry_run_command_sink_test)

    add_executable(dry_run_mavlink_bridge_test
        tests/dry_run_mavlink_bridge_test.cpp
    )

    target_link_libraries(dry_run_mavlink_bridge_test PRIVATE visual_homing_core_lib)

    target_compile_options(dry_run_mavlink_bridge_test PRIVATE
        $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
        $<$<CXX_COMPILER_ID:MSVC>:/W4>
    )

    add_test(NAME dry_run_mavlink_bridge COMMAND dry_run_mavlink_bridge_test)

    add_executable(gray8_resize_preprocessor_test
        tests/gray8_resize_preprocessor_test.cpp
    )

    target_link_libraries(gray8_resize_preprocessor_test PRIVATE visual_homing_core_lib)

    target_compile_options(gray8_resize_preprocessor_test PRIVATE
        $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
        $<$<CXX_COMPILER_ID:MSVC>:/W4>
    )

    add_test(NAME gray8_resize_preprocessor COMMAND gray8_resize_preprocessor_test)

    add_executable(replay_frame_source_test
        tests/replay_frame_source_test.cpp
    )

    target_link_libraries(replay_frame_source_test PRIVATE visual_homing_core_lib)

    target_compile_options(replay_frame_source_test PRIVATE
        $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
        $<$<CXX_COMPILER_ID:MSVC>:/W4>
    )

    add_test(NAME replay_frame_source COMMAND replay_frame_source_test)

    add_executable(health_monitor_test
        tests/health_monitor_test.cpp
    )

    target_link_libraries(health_monitor_test PRIVATE visual_homing_core_lib)

    target_compile_options(health_monitor_test PRIVATE
        $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
        $<$<CXX_COMPILER_ID:MSVC>:/W4>
    )

    add_test(NAME health_monitor COMMAND health_monitor_test)

    add_executable(mavlink_telemetry_adapter_test
        tests/mavlink_telemetry_adapter_test.cpp
    )

    target_link_libraries(mavlink_telemetry_adapter_test PRIVATE visual_homing_core_lib)

    target_compile_options(mavlink_telemetry_adapter_test PRIVATE
        $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
        $<$<CXX_COMPILER_ID:MSVC>:/W4>
    )

    add_test(NAME mavlink_telemetry_adapter COMMAND mavlink_telemetry_adapter_test)

    add_executable(mavlink_telemetry_inspector_test
        tests/mavlink_telemetry_inspector_test.cpp
    )

    target_link_libraries(mavlink_telemetry_inspector_test PRIVATE visual_homing_core_lib)

    target_compile_options(mavlink_telemetry_inspector_test PRIVATE
        $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
        $<$<CXX_COMPILER_ID:MSVC>:/W4>
    )

    add_test(NAME mavlink_telemetry_inspector COMMAND mavlink_telemetry_inspector_test)

    add_executable(mavlink_telemetry_stream_test
        tests/mavlink_telemetry_stream_test.cpp
    )

    target_link_libraries(mavlink_telemetry_stream_test PRIVATE visual_homing_core_lib)

    target_compile_options(mavlink_telemetry_stream_test PRIVATE
        $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
        $<$<CXX_COMPILER_ID:MSVC>:/W4>
    )

    add_test(NAME mavlink_telemetry_stream COMMAND mavlink_telemetry_stream_test)

    add_executable(live_mavlink_bridge_test
        tests/live_mavlink_bridge_test.cpp
    )

    target_link_libraries(live_mavlink_bridge_test PRIVATE visual_homing_core_lib)

    target_compile_options(live_mavlink_bridge_test PRIVATE
        $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
        $<$<CXX_COMPILER_ID:MSVC>:/W4>
    )

    add_test(NAME live_mavlink_bridge COMMAND live_mavlink_bridge_test)

    add_executable(live_mavlink_serial_writer_test
        tests/live_mavlink_serial_writer_test.cpp
    )

    target_link_libraries(live_mavlink_serial_writer_test PRIVATE visual_homing_core_lib)

    target_compile_options(live_mavlink_serial_writer_test PRIVATE
        $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
        $<$<CXX_COMPILER_ID:MSVC>:/W4>
    )

    add_test(NAME live_mavlink_serial_writer COMMAND live_mavlink_serial_writer_test)

    add_executable(live_mavlink_output_audit_log_test
        tests/live_mavlink_output_audit_log_test.cpp
    )

    target_link_libraries(live_mavlink_output_audit_log_test PRIVATE visual_homing_core_lib)

    target_compile_options(live_mavlink_output_audit_log_test PRIVATE
        $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
        $<$<CXX_COMPILER_ID:MSVC>:/W4>
    )

    add_test(NAME live_mavlink_output_audit_log COMMAND live_mavlink_output_audit_log_test)

    add_executable(live_mavlink_output_session_test
        tests/live_mavlink_output_session_test.cpp
    )

    target_link_libraries(live_mavlink_output_session_test PRIVATE visual_homing_core_lib)

    target_compile_options(live_mavlink_output_session_test PRIVATE
        $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
        $<$<CXX_COMPILER_ID:MSVC>:/W4>
    )

    add_test(NAME live_mavlink_output_session COMMAND live_mavlink_output_session_test)

    add_executable(live_mavlink_output_session_audit_file_test
        tests/live_mavlink_output_session_audit_file_test.cpp
    )

    target_link_libraries(live_mavlink_output_session_audit_file_test PRIVATE visual_homing_core_lib)

    target_compile_options(live_mavlink_output_session_audit_file_test PRIVATE
        $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
        $<$<CXX_COMPILER_ID:MSVC>:/W4>
    )

    add_test(NAME live_mavlink_output_session_audit_file COMMAND live_mavlink_output_session_audit_file_test)

    add_executable(live_mavlink_output_safety_gate_test
        tests/live_mavlink_output_safety_gate_test.cpp
    )

    target_link_libraries(live_mavlink_output_safety_gate_test PRIVATE visual_homing_core_lib)

    target_compile_options(live_mavlink_output_safety_gate_test PRIVATE
        $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
        $<$<CXX_COMPILER_ID:MSVC>:/W4>
    )

    add_test(NAME live_mavlink_output_safety_gate COMMAND live_mavlink_output_safety_gate_test)

    add_executable(pipeline_harness_test
        tests/pipeline_harness_test.cpp
    )

    target_link_libraries(pipeline_harness_test PRIVATE visual_homing_core_lib)

    target_compile_options(pipeline_harness_test PRIVATE
        $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
        $<$<CXX_COMPILER_ID:MSVC>:/W4>
    )

    add_test(NAME pipeline_harness COMMAND pipeline_harness_test)

    add_executable(pi_camera_source_test
        tests/pi_camera_source_test.cpp
    )

    target_link_libraries(pi_camera_source_test PRIVATE visual_homing_core_lib)

    target_compile_options(pi_camera_source_test PRIVATE
        $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
        $<$<CXX_COMPILER_ID:MSVC>:/W4>
    )

    add_test(NAME pi_camera_source COMMAND pi_camera_source_test)

    add_executable(route_signature_test
        tests/route_signature_test.cpp
    )

    target_link_libraries(route_signature_test PRIVATE visual_homing_core_lib)

    target_compile_options(route_signature_test PRIVATE
        $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
        $<$<CXX_COMPILER_ID:MSVC>:/W4>
    )

    add_test(NAME route_signature COMMAND route_signature_test)

    add_executable(route_signature_recorder_test
        tests/route_signature_recorder_test.cpp
    )

    target_link_libraries(route_signature_recorder_test PRIVATE visual_homing_core_lib)

    target_compile_options(route_signature_recorder_test PRIVATE
        $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
        $<$<CXX_COMPILER_ID:MSVC>:/W4>
    )

    add_test(NAME route_signature_recorder COMMAND route_signature_recorder_test)

    add_executable(route_artifact_check_test
        tests/route_artifact_check_test.cpp
    )

    target_link_libraries(route_artifact_check_test PRIVATE visual_homing_core_lib)

    target_compile_options(route_artifact_check_test PRIVATE
        $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
        $<$<CXX_COMPILER_ID:MSVC>:/W4>
    )

    add_test(NAME route_artifact_check COMMAND route_artifact_check_test)

    add_executable(gray8_route_matcher_test
        tests/gray8_route_matcher_test.cpp
    )

    target_link_libraries(gray8_route_matcher_test PRIVATE visual_homing_core_lib)

    target_compile_options(gray8_route_matcher_test PRIVATE
        $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
        $<$<CXX_COMPILER_ID:MSVC>:/W4>
    )

    add_test(NAME gray8_route_matcher COMMAND gray8_route_matcher_test)
endif()
