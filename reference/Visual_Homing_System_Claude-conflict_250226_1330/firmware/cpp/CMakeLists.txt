cmake_minimum_required(VERSION 3.10)
project(visual_homing VERSION 2.1)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find packages
find_package(OpenCV REQUIRED)
find_package(Threads REQUIRED)

# MAVLink headers
set(MAVLINK_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/include)

# Include directories
include_directories(
    ${OpenCV_INCLUDE_DIRS}
    ${MAVLINK_INCLUDE_DIR}
    ${CMAKE_SOURCE_DIR}/src
)

# Source files
set(SOURCES
    src/main.cpp
    src/camera.cpp
    src/feature_tracker.cpp
    src/visual_odometry.cpp
    src/route_memory.cpp
    src/mavlink_interface.cpp
    src/optical_flow.cpp
    src/lidar.cpp
)

# Executable
add_executable(visual_homing ${SOURCES})

# Link libraries
target_link_libraries(visual_homing
    ${OpenCV_LIBS}
    Threads::Threads
)

# Installation
install(TARGETS visual_homing DESTINATION bin)
