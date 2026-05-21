cmake_minimum_required(VERSION 3.16)

project(visual_homing_core LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(visual_homing_core
    src/main.cpp
    src/system_clock.cpp
)

target_include_directories(visual_homing_core PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_options(visual_homing_core PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
)
