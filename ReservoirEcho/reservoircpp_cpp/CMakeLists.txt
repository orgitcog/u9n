cmake_minimum_required(VERSION 3.14)
project(ReservoirCpp VERSION 0.1.0 LANGUAGES CXX)

# Specify C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Find Eigen if not already found by parent project
if(NOT EIGEN3_FOUND)
    # Priority 1: Check eigen-3.4.0 (if complete with headers)
    set(EIGEN_3_4_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../external/eigen-3.4.0")
    if(EXISTS "${EIGEN_3_4_DIR}/Eigen/Core")
        set(EIGEN3_INCLUDE_DIR "${EIGEN_3_4_DIR}")
        set(EIGEN3_FOUND TRUE)
        message(STATUS "ReservoirCpp: Using Eigen 3.4.0 from ${EIGEN3_INCLUDE_DIR}")
    endif()
endif()

if(NOT EIGEN3_FOUND)
    # Priority 2: Check taskflow bundled eigen-3.3.7
    set(EIGEN_3_3_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../external/taskflow-3.8.0/3rd-party/eigen-3.3.7")
    if(EXISTS "${EIGEN_3_3_DIR}/Eigen/Core")
        set(EIGEN3_INCLUDE_DIR "${EIGEN_3_3_DIR}")
        set(EIGEN3_FOUND TRUE)
        message(STATUS "ReservoirCpp: Using Eigen 3.3.7 from ${EIGEN3_INCLUDE_DIR}")
    endif()
endif()

if(NOT EIGEN3_FOUND)
    message(FATAL_ERROR "Eigen library not found. Install libeigen3-dev or ensure bundled Eigen has header files.")
endif()

# Set compiler flags
if(MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4")
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -pedantic")
endif()

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${EIGEN3_INCLUDE_DIR}
)

# Core source files (only the ones that compile cleanly)
set(CORE_SOURCES
    src/_base.cpp
    src/_version.cpp
    src/activationsfunc.cpp
    src/node.cpp
    src/model.cpp
)

# Deep Tree Echo source files
set(DEEP_TREE_ECHO_SOURCES
    src/deep_tree_echo/core.cpp
    src/deep_tree_echo/unreal_integration.cpp
    src/deep_tree_echo/avatar_4e.cpp
)

# Add library with core sources only
add_library(reservoircpp_core ${CORE_SOURCES})
target_include_directories(reservoircpp_core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# Deep Tree Echo integration library
add_library(deep_tree_echo ${DEEP_TREE_ECHO_SOURCES})
target_include_directories(deep_tree_echo PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_link_libraries(deep_tree_echo PUBLIC reservoircpp_core)

# Add tests
# Support both BUILD_TESTS and BUILD_TESTING for compatibility
option(BUILD_TESTS "Build the tests" OFF)
option(BUILD_TESTING "Build the tests (CMake standard)" OFF)
if(BUILD_TESTS OR BUILD_TESTING)
    enable_testing()
    add_subdirectory(test)
endif()

# Add examples
option(BUILD_EXAMPLES "Build the examples" OFF)
if(BUILD_EXAMPLES)
    # add_subdirectory(examples)
endif()

# Installation
install(TARGETS reservoircpp_core deep_tree_echo
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
    INCLUDES DESTINATION include
)
install(DIRECTORY include/ DESTINATION include)

message(STATUS "ReservoirCpp Core configured successfully")
message(STATUS "Deep Tree Echo integration enabled")
