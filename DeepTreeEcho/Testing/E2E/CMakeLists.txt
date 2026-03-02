cmake_minimum_required(VERSION 3.14)
project(DeepTreeEchoE2ETests VERSION 1.0.0 LANGUAGES CXX)

# C++ Standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Find dependencies
find_package(GTest REQUIRED)
find_package(Threads REQUIRED)
include(GoogleTest)

# Compiler flags
if(MSVC)
    add_compile_options(/W4 /WX-)
else()
    add_compile_options(-Wall -Wextra -pedantic -Wno-unused-parameter -pthread)
endif()

# Enable testing
enable_testing()

# ============================================================================
# Cognitive Pipeline E2E Tests
# ============================================================================
add_executable(CognitivePipelineE2E
    CognitivePipelineE2E.cpp
)

target_link_libraries(CognitivePipelineE2E
    GTest::gtest
    GTest::gtest_main
    GTest::gmock
    Threads::Threads
)

target_include_directories(CognitivePipelineE2E PRIVATE
    ${CMAKE_SOURCE_DIR}/../../
    ${CMAKE_SOURCE_DIR}/../../../
)

gtest_discover_tests(CognitivePipelineE2E
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "e2e;cognitive;pipeline"
        TIMEOUT 300
)

# ============================================================================
# Avatar Integration E2E Tests
# ============================================================================
add_executable(AvatarIntegrationE2E
    AvatarIntegrationE2E.cpp
)

target_link_libraries(AvatarIntegrationE2E
    GTest::gtest
    GTest::gtest_main
    GTest::gmock
    Threads::Threads
)

target_include_directories(AvatarIntegrationE2E PRIVATE
    ${CMAKE_SOURCE_DIR}/../../
    ${CMAKE_SOURCE_DIR}/../../../
)

gtest_discover_tests(AvatarIntegrationE2E
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "e2e;avatar;integration"
        TIMEOUT 300
)

# ============================================================================
# Reservoir Cognitive E2E Tests
# ============================================================================
add_executable(ReservoirCognitiveE2E
    ReservoirCognitiveE2E.cpp
)

target_link_libraries(ReservoirCognitiveE2E
    GTest::gtest
    GTest::gtest_main
    GTest::gmock
    Threads::Threads
)

target_include_directories(ReservoirCognitiveE2E PRIVATE
    ${CMAKE_SOURCE_DIR}/../../
    ${CMAKE_SOURCE_DIR}/../../../
    ${CMAKE_SOURCE_DIR}/../../../ReservoirEcho/reservoircpp_cpp/include
)

gtest_discover_tests(ReservoirCognitiveE2E
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "e2e;reservoir;cognitive"
        TIMEOUT 300
)

# ============================================================================
# Neural-Symbolic Pipeline E2E Tests (Feature F1.6.2)
# ============================================================================
add_executable(NeuralSymbolicPipelineE2E
    NeuralSymbolicPipelineE2E.cpp
)

target_link_libraries(NeuralSymbolicPipelineE2E
    GTest::gtest
    GTest::gtest_main
    GTest::gmock
    Threads::Threads
)

target_include_directories(NeuralSymbolicPipelineE2E PRIVATE
    ${CMAKE_SOURCE_DIR}/../../
    ${CMAKE_SOURCE_DIR}/../../../
)

gtest_discover_tests(NeuralSymbolicPipelineE2E
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "e2e;neural-symbolic;pipeline;f1.6.2"
        TIMEOUT 600
)

# ============================================================================
# All E2E Tests Target
# ============================================================================
add_custom_target(run_e2e_tests
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure --parallel 2 -L e2e
    DEPENDS CognitivePipelineE2E AvatarIntegrationE2E ReservoirCognitiveE2E NeuralSymbolicPipelineE2E
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Running all DeepTreeEcho E2E tests"
)

# ============================================================================
# Stress Test Target
# ============================================================================
add_custom_target(run_stress_tests
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure -R "Stress"
    DEPENDS CognitivePipelineE2E
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Running stress tests"
)

# ============================================================================
# Performance Test Target
# ============================================================================
add_custom_target(run_performance_tests
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure -R "Performance"
    DEPENDS CognitivePipelineE2E
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Running performance tests"
)

# ============================================================================
# Installation
# ============================================================================
install(TARGETS CognitivePipelineE2E AvatarIntegrationE2E ReservoirCognitiveE2E NeuralSymbolicPipelineE2E
    RUNTIME DESTINATION bin/tests/e2e
)

message(STATUS "DeepTreeEcho E2E Tests configured")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  Tests: CognitivePipelineE2E, AvatarIntegrationE2E, ReservoirCognitiveE2E, NeuralSymbolicPipelineE2E")
