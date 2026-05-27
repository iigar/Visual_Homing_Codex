cmake_minimum_required(VERSION 3.16)

project(visual_homing_core LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(visual_homing_core
    src/main.cpp
)

add_library(visual_homing_core_lib
    src/gray8_resize_preprocessor.cpp
    src/health_monitor.cpp
    src/replay_frame_source.cpp
    src/system_clock.cpp
)

target_include_directories(visual_homing_core_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

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
endif()
