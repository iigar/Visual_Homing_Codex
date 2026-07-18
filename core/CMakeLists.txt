cmake_minimum_required(VERSION 3.16)

project(visual_homing_core LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(VISUAL_HOMING_ENABLE_LIBCAMERA "Build the Raspberry Pi libcamera capture backend" OFF)
option(VISUAL_HOMING_ENABLE_LIVE_MAVLINK_OUTPUT "Request live MAVLink command output build scope" OFF)
option(VISUAL_HOMING_ENABLE_BENCH_PROPS_OFF_LIVE_OUTPUT "Request the reviewed bench props-off live-output boundary scope" OFF)
option(VISUAL_HOMING_ATTACH_BENCH_PROPS_OFF_SERIAL_WRITER "Attach the reviewed bench props-off serial MAVLink writer" OFF)
option(VISUAL_HOMING_ENABLE_EXTERNAL_NAV_OUTPUT "Request external-nav provider output build scope" OFF)
option(VISUAL_HOMING_ENABLE_BENCH_PROPS_OFF_EXTERNAL_NAV_OUTPUT "Request the reviewed bench props-off external-nav provider output scope" OFF)
option(VISUAL_HOMING_ATTACH_BENCH_PROPS_OFF_EXTERNAL_NAV_WRITER "Attach the reviewed bench props-off external-nav writer" OFF)

if(VISUAL_HOMING_ENABLE_LIVE_MAVLINK_OUTPUT AND NOT VISUAL_HOMING_ENABLE_BENCH_PROPS_OFF_LIVE_OUTPUT)
    message(FATAL_ERROR "Live MAVLink command output requires VISUAL_HOMING_ENABLE_BENCH_PROPS_OFF_LIVE_OUTPUT=ON and remains blocked until the bench props-off writer is implemented and reviewed")
endif()

if(VISUAL_HOMING_ENABLE_BENCH_PROPS_OFF_LIVE_OUTPUT AND NOT VISUAL_HOMING_ENABLE_LIVE_MAVLINK_OUTPUT)
    message(FATAL_ERROR "VISUAL_HOMING_ENABLE_BENCH_PROPS_OFF_LIVE_OUTPUT is only valid with VISUAL_HOMING_ENABLE_LIVE_MAVLINK_OUTPUT=ON")
endif()

if(VISUAL_HOMING_ATTACH_BENCH_PROPS_OFF_SERIAL_WRITER AND NOT (VISUAL_HOMING_ENABLE_LIVE_MAVLINK_OUTPUT AND VISUAL_HOMING_ENABLE_BENCH_PROPS_OFF_LIVE_OUTPUT))
    message(FATAL_ERROR "VISUAL_HOMING_ATTACH_BENCH_PROPS_OFF_SERIAL_WRITER requires both VISUAL_HOMING_ENABLE_LIVE_MAVLINK_OUTPUT=ON and VISUAL_HOMING_ENABLE_BENCH_PROPS_OFF_LIVE_OUTPUT=ON")
endif()

if(VISUAL_HOMING_ENABLE_EXTERNAL_NAV_OUTPUT AND NOT VISUAL_HOMING_ENABLE_BENCH_PROPS_OFF_EXTERNAL_NAV_OUTPUT)
    message(FATAL_ERROR "External-nav provider output requires VISUAL_HOMING_ENABLE_BENCH_PROPS_OFF_EXTERNAL_NAV_OUTPUT=ON and remains unattached until the reviewed bench props-off writer path is implemented")
endif()

if(VISUAL_HOMING_ENABLE_BENCH_PROPS_OFF_EXTERNAL_NAV_OUTPUT AND NOT VISUAL_HOMING_ENABLE_EXTERNAL_NAV_OUTPUT)
    message(FATAL_ERROR "VISUAL_HOMING_ENABLE_BENCH_PROPS_OFF_EXTERNAL_NAV_OUTPUT is only valid with VISUAL_HOMING_ENABLE_EXTERNAL_NAV_OUTPUT=ON")
endif()

if(VISUAL_HOMING_ATTACH_BENCH_PROPS_OFF_EXTERNAL_NAV_WRITER AND NOT (VISUAL_HOMING_ENABLE_EXTERNAL_NAV_OUTPUT AND VISUAL_HOMING_ENABLE_BENCH_PROPS_OFF_EXTERNAL_NAV_OUTPUT))
    message(FATAL_ERROR "VISUAL_HOMING_ATTACH_BENCH_PROPS_OFF_EXTERNAL_NAV_WRITER requires both VISUAL_HOMING_ENABLE_EXTERNAL_NAV_OUTPUT=ON and VISUAL_HOMING_ENABLE_BENCH_PROPS_OFF_EXTERNAL_NAV_OUTPUT=ON")
endif()

add_executable(visual_homing_core
    src/main.cpp
)

add_library(visual_homing_core_lib
    src/bounded_navigator.cpp
    src/camera_profile.cpp
    src/camera_smoke.cpp
    src/coordinate_frames.cpp
    src/dry_run_command_sink.cpp
    src/dry_run_mavlink_bridge.cpp
    src/external_nav_estimator.cpp
    src/gray8_resize_preprocessor.cpp
    src/gray8_route_matcher.cpp
    src/health_monitor.cpp
    src/inflight_home_reset_safety_gate.cpp
    src/live_external_nav_output_audit_log.cpp
    src/live_external_nav_output_session.cpp
    src/live_mavlink_bridge.cpp
    src/live_mavlink_external_nav_writer.cpp
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
    src/rc_switch_trigger_decoder.cpp
    src/route_local_odometry.cpp
    src/route_local_odometry_estimator.cpp
    src/route_artifact_check.cpp
    src/route_signature_recorder.cpp
    src/route_signature.cpp
    src/system_clock.cpp
)

target_include_directories(visual_homing_core_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if(VISUAL_HOMING_ATTACH_BENCH_PROPS_OFF_SERIAL_WRITER)
    target_compile_definitions(visual_homing_core_lib PUBLIC
        VISUAL_HOMING_LIVE_MAVLINK_OUTPUT_BLOCKED=0
        VISUAL_HOMING_LIVE_MAVLINK_OUTPUT_AVAILABLE=1
        VISUAL_HOMING_BENCH_PROPS_OFF_SERIAL_WRITER_ATTACHED=1
    )
else()
    target_compile_definitions(visual_homing_core_lib PUBLIC
        VISUAL_HOMING_LIVE_MAVLINK_OUTPUT_BLOCKED=1
        VISUAL_HOMING_LIVE_MAVLINK_OUTPUT_AVAILABLE=0
        VISUAL_HOMING_BENCH_PROPS_OFF_SERIAL_WRITER_ATTACHED=0
    )
endif()

if(VISUAL_HOMING_ATTACH_BENCH_PROPS_OFF_EXTERNAL_NAV_WRITER)
    target_compile_definitions(visual_homing_core_lib PUBLIC
        VISUAL_HOMING_EXTERNAL_NAV_OUTPUT_BLOCKED=0
        VISUAL_HOMING_EXTERNAL_NAV_OUTPUT_AVAILABLE=1
        VISUAL_HOMING_BENCH_PROPS_OFF_EXTERNAL_NAV_WRITER_ATTACHED=1
    )
else()
    target_compile_definitions(visual_homing_core_lib PUBLIC
        VISUAL_HOMING_EXTERNAL_NAV_OUTPUT_BLOCKED=1
        VISUAL_HOMING_EXTERNAL_NAV_OUTPUT_AVAILABLE=0
        VISUAL_HOMING_BENCH_PROPS_OFF_EXTERNAL_NAV_WRITER_ATTACHED=0
    )
endif()

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

if(VISUAL_HOMING_ENABLE_EXTERNAL_NAV_OUTPUT)
    target_compile_definitions(visual_homing_core_lib PUBLIC
        VISUAL_HOMING_EXTERNAL_NAV_OUTPUT_BUILD_REQUESTED=1
        VISUAL_HOMING_BENCH_PROPS_OFF_EXTERNAL_NAV_OUTPUT_SCOPE=1
    )
else()
    target_compile_definitions(visual_homing_core_lib PUBLIC
        VISUAL_HOMING_EXTERNAL_NAV_OUTPUT_BUILD_REQUESTED=0
        VISUAL_HOMING_BENCH_PROPS_OFF_EXTERNAL_NAV_OUTPUT_SCOPE=0
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

add_executable(rc12_local_reset_dry_run
    tools/rc12_local_reset_dry_run.cpp
)

target_link_libraries(rc12_local_reset_dry_run PRIVATE visual_homing_core_lib)

target_compile_options(rc12_local_reset_dry_run PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
)

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

    add_executable(coordinate_frames_test
        tests/coordinate_frames_test.cpp
    )

    target_link_libraries(coordinate_frames_test PRIVATE visual_homing_core_lib)

    target_compile_options(coordinate_frames_test PRIVATE
        $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
        $<$<CXX_COMPILER_ID:MSVC>:/W4>
    )

    add_test(NAME coordinate_frames COMMAND coordinate_frames_test)

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

    add_executable(external_nav_estimator_test
        tests/external_nav_estimator_test.cpp
    )

    target_link_libraries(external_nav_estimator_test PRIVATE visual_homing_core_lib)

    target_compile_options(external_nav_estimator_test PRIVATE
        $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
        $<$<CXX_COMPILER_ID:MSVC>:/W4>
    )

    add_test(NAME external_nav_estimator COMMAND external_nav_estimator_test)

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

    add_executable(inflight_home_reset_safety_gate_test
        tests/inflight_home_reset_safety_gate_test.cpp
    )

    target_link_libraries(inflight_home_reset_safety_gate_test PRIVATE visual_homing_core_lib)

    target_compile_options(inflight_home_reset_safety_gate_test PRIVATE
        $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
        $<$<CXX_COMPILER_ID:MSVC>:/W4>
    )

    add_test(NAME inflight_home_reset_safety_gate COMMAND inflight_home_reset_safety_gate_test)

    add_executable(rc_switch_trigger_decoder_test
        tests/rc_switch_trigger_decoder_test.cpp
    )

    target_link_libraries(rc_switch_trigger_decoder_test PRIVATE visual_homing_core_lib)

    target_compile_options(rc_switch_trigger_decoder_test PRIVATE
        $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
        $<$<CXX_COMPILER_ID:MSVC>:/W4>
    )

    add_test(NAME rc_switch_trigger_decoder COMMAND rc_switch_trigger_decoder_test)

    add_test(
        NAME rc12_local_reset_dry_run_trace
        COMMAND rc12_local_reset_dry_run
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/data/rc12_trigger_trace.txt
            1
            1
    )

    add_test(
        NAME rc12_local_reset_dry_run_rejects_negative_count
        COMMAND rc12_local_reset_dry_run
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/data/rc12_trigger_trace.txt
            -1
            1
    )
    set_tests_properties(
        rc12_local_reset_dry_run_rejects_negative_count
        PROPERTIES WILL_FAIL TRUE
    )

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

    add_executable(live_mavlink_external_nav_writer_test
        tests/live_mavlink_external_nav_writer_test.cpp
    )

    target_link_libraries(live_mavlink_external_nav_writer_test PRIVATE visual_homing_core_lib)

    target_compile_options(live_mavlink_external_nav_writer_test PRIVATE
        $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
        $<$<CXX_COMPILER_ID:MSVC>:/W4>
    )

    add_test(NAME live_mavlink_external_nav_writer COMMAND live_mavlink_external_nav_writer_test)

    add_executable(route_local_odometry_test
        tests/route_local_odometry_test.cpp
    )

    target_link_libraries(route_local_odometry_test PRIVATE visual_homing_core_lib)

    target_compile_options(route_local_odometry_test PRIVATE
        $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
        $<$<CXX_COMPILER_ID:MSVC>:/W4>
    )

    add_test(NAME route_local_odometry COMMAND route_local_odometry_test)

    add_executable(route_local_odometry_estimator_test
        tests/route_local_odometry_estimator_test.cpp
    )

    target_link_libraries(route_local_odometry_estimator_test PRIVATE visual_homing_core_lib)

    target_compile_options(route_local_odometry_estimator_test PRIVATE
        $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
        $<$<CXX_COMPILER_ID:MSVC>:/W4>
    )

    add_test(NAME route_local_odometry_estimator COMMAND route_local_odometry_estimator_test)

    add_executable(route_local_odometry_sitl_producer
        tools/route_local_odometry_sitl_producer.cpp
    )

    target_link_libraries(route_local_odometry_sitl_producer PRIVATE visual_homing_core_lib)

    target_compile_options(route_local_odometry_sitl_producer PRIVATE
        $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
        $<$<CXX_COMPILER_ID:MSVC>:/W4>
    )

    add_test(
        NAME route_local_odometry_sitl_producer_self_test
        COMMAND route_local_odometry_sitl_producer --self-test
    )

    add_executable(live_external_nav_output_audit_log_test
        tests/live_external_nav_output_audit_log_test.cpp
    )

    target_link_libraries(live_external_nav_output_audit_log_test PRIVATE visual_homing_core_lib)

    target_compile_options(live_external_nav_output_audit_log_test PRIVATE
        $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
        $<$<CXX_COMPILER_ID:MSVC>:/W4>
    )

    add_test(NAME live_external_nav_output_audit_log COMMAND live_external_nav_output_audit_log_test)

    add_executable(live_external_nav_output_session_test
        tests/live_external_nav_output_session_test.cpp
    )

    target_link_libraries(live_external_nav_output_session_test PRIVATE visual_homing_core_lib)

    target_compile_options(live_external_nav_output_session_test PRIVATE
        $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
        $<$<CXX_COMPILER_ID:MSVC>:/W4>
    )

    add_test(NAME live_external_nav_output_session COMMAND live_external_nav_output_session_test)

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
