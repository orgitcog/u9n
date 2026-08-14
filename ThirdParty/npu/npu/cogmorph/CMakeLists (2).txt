# CogMorph - Cognitive State Transformation Framework
# Part of the NPU (Neural Processing Unit) project

cmake_minimum_required(VERSION 3.16)
project(cogmorph VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Options
option(COGMORPH_BUILD_TESTS "Build cogmorph tests" ON)
option(COGMORPH_BUILD_EXAMPLES "Build cogmorph examples" ON)

# Find dependencies
# (Currently header-only, but may need virtual-device lib)

# Header-only library
add_library(cogmorph INTERFACE)
target_include_directories(cogmorph INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

# Link to virtual-device if building as part of NPU
if(TARGET ggnucash-vdev)
    target_link_libraries(cogmorph INTERFACE ggnucash-vdev)
endif()

# Link to opencog-modern if available
if(TARGET opencog-core)
    target_link_libraries(cogmorph INTERFACE opencog-core)
endif()

# Implementation library (when we add .cpp files)
# add_library(cogmorph-impl
#     projections.cpp
#     transforms.cpp
#     assd-driver.cpp
# )
# target_link_libraries(cogmorph-impl PUBLIC cogmorph)

# Examples
if(COGMORPH_BUILD_EXAMPLES)
    add_executable(cogmorph-demo demo.cpp)
    target_link_libraries(cogmorph-demo PRIVATE cogmorph)
    target_compile_features(cogmorph-demo PRIVATE cxx_std_23)
endif()

# Tests
if(COGMORPH_BUILD_TESTS)
    enable_testing()

    # Test executable (placeholder)
    # add_executable(cogmorph-tests tests/test_main.cpp)
    # target_link_libraries(cogmorph-tests PRIVATE cogmorph)
    # add_test(NAME cogmorph-tests COMMAND cogmorph-tests)
endif()

# Installation
install(FILES
    cogmorph.hpp
    projections.hpp
    transforms.hpp
    assd-driver.hpp
    prime-basis.hpp
    matula-numbers.hpp
    grassmannian.hpp
    DESTINATION include/cogmorph
)

# Package config
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/cogmorph-config-version.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(EXPORT cogmorph-targets
    FILE cogmorph-targets.cmake
    NAMESPACE cogmorph::
    DESTINATION lib/cmake/cogmorph
)
