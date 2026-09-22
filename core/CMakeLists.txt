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
    src/artifact_digest.cpp
    src/bounded_verification_publisher.cpp
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
    src/live_verification_capture.cpp
    src/live_route_verification.cpp
    src/live_external_nav_output_audit_log.cpp
    src/live_external_nav_output_session.cpp
    src/live_mavlink_bridge.cpp
    src/live_mavlink_external_nav_writer.cpp
    src/live_mavlink_output_audit_log.cpp
    src/live_mavlink_output_session.cpp
    src/live_mavlink_output_safety_gate.cpp
    src/live_mavlink_serial_writer.cpp
    src/local_reset_dry_run_session.cpp
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
    src/route_descriptor_index.cpp
    src/route_package_builder.cpp
    src/route_package_manifest.cpp
    src/route_signature_recorder.cpp
    src/route_signature_entry.cpp
    src/route_signature.cpp
    src/route_signature_stream_writer.cpp
    src/route_signature_streaming_recorder.cpp
    src/system_clock.cpp
    src/verification_keyframe_selector.cpp
    src/verification_package_writer.cpp
)

target_include_directories(visual_homing_core_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
target_link_libraries(visual_homing_core_lib PUBLIC Threads::Threads)

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

add_executable(route_descriptor_index_builder
    tools/route_descriptor_index_builder.cpp
)

target_link_libraries(route_descriptor_index_builder PRIVATE visual_homing_core_lib)

target_compile_options(route_descriptor_index_builder PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
)

add_executable(gray8_matcher_benchmark tools/gray8_matcher_benchmark.cpp)
target_link_libraries(gray8_matcher_benchmark PRIVATE visual_homing_core_lib)
target_compile_options(gray8_matcher_benchmark PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_executable(gray8_edge_cache_benchmark tools/gray8_edge_cache_benchmark.cpp)
    target_link_libraries(gray8_edge_cache_benchmark PRIVATE visual_homing_core_lib)
    target_compile_options(gray8_edge_cache_benchmark PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(live_verification_capture_benchmark
    tools/live_verification_capture_benchmark.cpp
)

target_link_libraries(live_verification_capture_benchmark PRIVATE visual_homing_core_lib)

target_compile_options(live_verification_capture_benchmark PRIVATE
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

    # Standard one-source unit tests; special command-line tests stay explicit.
    function(add_visual_homing_test name)
        set(test_target "${name}_test")
        add_executable(${test_target} tests/${test_target}.cpp)
        target_link_libraries(${test_target} PRIVATE visual_homing_core_lib)
        target_compile_options(${test_target} PRIVATE
            $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
            $<$<CXX_COMPILER_ID:MSVC>:/W4>
        )
        add_test(NAME ${name} COMMAND ${test_target})
    endfunction()

    add_visual_homing_test(coordinate_frames)
    add_visual_homing_test(camera_smoke)
    add_visual_homing_test(live_route_progress)
    add_visual_homing_test(live_route_endpoint)
    add_visual_homing_test(live_route_readiness)
    add_visual_homing_test(live_route_config)
    add_test(
        NAME cli_numeric_arguments
        COMMAND ${CMAKE_COMMAND}
            "-DCORE_EXE=$<TARGET_FILE:visual_homing_core>"
            "-DPROFILE_DIR=${CMAKE_CURRENT_SOURCE_DIR}/../config/camera_profiles"
            "-DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/cli numeric arguments"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_numeric_arguments_test.cmake"
    )
    set_tests_properties(cli_numeric_arguments PROPERTIES TIMEOUT 120)
    add_visual_homing_test(camera_profile)
    add_visual_homing_test(bounded_navigator)
    add_visual_homing_test(dry_run_command_sink)
    add_visual_homing_test(dry_run_mavlink_bridge)
    add_visual_homing_test(external_nav_estimator)
    add_visual_homing_test(gray8_resize_preprocessor)
    add_visual_homing_test(replay_frame_source)
    add_visual_homing_test(health_monitor)
    add_visual_homing_test(inflight_home_reset_safety_gate)
    add_visual_homing_test(local_reset_dry_run_session)
    add_visual_homing_test(rc_switch_trigger_decoder)

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

    add_visual_homing_test(mavlink_telemetry_adapter)
    add_visual_homing_test(mavlink_telemetry_inspector)
    add_visual_homing_test(mavlink_telemetry_stream)
    add_visual_homing_test(live_mavlink_bridge)
    add_visual_homing_test(live_mavlink_serial_writer)
    add_visual_homing_test(live_mavlink_external_nav_writer)
    add_visual_homing_test(route_local_odometry)
    add_visual_homing_test(route_local_odometry_estimator)

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

    add_visual_homing_test(live_external_nav_output_audit_log)
    add_visual_homing_test(live_external_nav_output_session)
    add_visual_homing_test(live_mavlink_output_audit_log)
    add_visual_homing_test(live_mavlink_output_session)
    add_visual_homing_test(live_mavlink_output_session_audit_file)
    add_visual_homing_test(live_mavlink_output_safety_gate)
    add_visual_homing_test(pipeline_harness)
    add_visual_homing_test(pi_camera_source)
    add_visual_homing_test(route_signature)
    add_visual_homing_test(route_signature_stream_writer)
    add_visual_homing_test(route_signature_streaming_recorder)
    add_visual_homing_test(route_package_manifest)
    add_visual_homing_test(route_package_builder)
    add_visual_homing_test(route_descriptor_index)
    add_visual_homing_test(verification_keyframe_selector)
    add_visual_homing_test(verification_package_writer)
    add_visual_homing_test(live_verification_capture)
    add_visual_homing_test(bounded_verification_publisher)
    add_visual_homing_test(live_route_verification)
    add_visual_homing_test(progress_only_route_verification_wiring)
    add_visual_homing_test(route_signature_recorder)
    add_visual_homing_test(route_artifact_check)
    add_visual_homing_test(gray8_route_matcher)
    add_visual_homing_test(gray8_edge_cache)
    add_test(NAME gray8_matcher_benchmark_self_test COMMAND gray8_matcher_benchmark --self-test)
endif()
