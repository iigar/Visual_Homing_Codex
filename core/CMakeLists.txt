cmake_minimum_required(VERSION 3.16)

project(visual_homing_core LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(visual_homing_core
    src/main.cpp
)

add_library(visual_homing_core_lib
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
    add_executable(visual_homing_core_tests
        tests/replay_frame_source_test.cpp
    )

    target_link_libraries(visual_homing_core_tests PRIVATE visual_homing_core_lib)

    target_compile_options(visual_homing_core_tests PRIVATE
        $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
        $<$<CXX_COMPILER_ID:MSVC>:/W4>
    )

    add_test(NAME replay_frame_source COMMAND visual_homing_core_tests)
endif()
