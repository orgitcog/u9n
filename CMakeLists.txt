cmake_minimum_required(VERSION 3.14)
project(un9n
    VERSION 1.0.0
    DESCRIPTION "Deep Tree Echo - Unified Cognitive Framework for 4E Embodied AGI"
    LANGUAGES CXX
)

# Version string for release builds
set(VERSION_STRING "${PROJECT_VERSION}" CACHE STRING "Version string for builds")

# ============================================================================
# Build Options
# ============================================================================
option(BUILD_SHARED_LIBS "Build shared libraries" ON)
option(BUILD_TESTING "Build unit tests" ON)
option(BUILD_E2E_TESTS "Build end-to-end tests" ON)
option(BUILD_BENCHMARKS "Build performance benchmarks" OFF)
option(ENABLE_COVERAGE "Enable code coverage" OFF)

# Specify C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build type defaults to Release
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Output directories
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Find Eigen - check multiple locations
# Priority 1: System-installed Eigen3
find_package(Eigen3 3.3 QUIET)

if(EIGEN3_FOUND)
    # System Eigen3 found - target Eigen3::Eigen already exists
    message(STATUS "Using system Eigen3 from ${EIGEN3_INCLUDE_DIR}")
else()
    # Priority 2: Bundled Eigen 3.4.0 (check if complete)
    set(EIGEN_3_4_DIR "${CMAKE_SOURCE_DIR}/ReservoirEcho/external/eigen-3.4.0")
    if(EXISTS "${EIGEN_3_4_DIR}/Eigen/Core")
        set(EIGEN3_INCLUDE_DIR "${EIGEN_3_4_DIR}")
        set(EIGEN3_FOUND TRUE)
        message(STATUS "Using bundled Eigen 3.4.0 from ${EIGEN3_INCLUDE_DIR}")
    endif()

    if(NOT EIGEN3_FOUND)
        # Priority 3: Taskflow bundled Eigen 3.3.7 (known to be complete)
        set(EIGEN_3_3_DIR "${CMAKE_SOURCE_DIR}/ReservoirEcho/external/taskflow-3.8.0/3rd-party/eigen-3.3.7")
        if(EXISTS "${EIGEN_3_3_DIR}/Eigen/Core")
            set(EIGEN3_INCLUDE_DIR "${EIGEN_3_3_DIR}")
            set(EIGEN3_FOUND TRUE)
            message(STATUS "Using bundled Eigen 3.3.7 from ${EIGEN3_INCLUDE_DIR}")
        endif()
    endif()

    if(NOT EIGEN3_FOUND)
        message(FATAL_ERROR "Eigen library not found. Install libeigen3-dev or ensure bundled Eigen has header files.")
    endif()

    # Create an interface library for Eigen only if not already defined by find_package
    if(NOT TARGET Eigen3::Eigen)
        add_library(Eigen3::Eigen INTERFACE IMPORTED)
        set_target_properties(Eigen3::Eigen PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${EIGEN3_INCLUDE_DIR}"
        )
    endif()
endif()

# Set compiler flags
if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -pedantic)
    # Suppress deprecated-copy warnings from Eigen 3.3.7 with modern GCC
    add_compile_options(-Wno-deprecated-copy)
endif()

# Add ReservoirCpp subdirectory
add_subdirectory(ReservoirEcho/reservoircpp_cpp)

# ============================================================================
# Dependencies for Testing
# ============================================================================
# Find Threads package for test targets
find_package(Threads REQUIRED)

# ============================================================================
# Testing Infrastructure
# ============================================================================
if(BUILD_TESTING OR BUILD_E2E_TESTS)
    # Try to find system GTest with GMock support
    find_package(GTest QUIET)
    
    # Check if GMock target exists (system GTest may not include it)
    if(GTest_FOUND AND NOT TARGET GTest::gmock)
        message(STATUS "System GTest found but GMock not available, fetching GoogleTest")
        set(GTest_FOUND FALSE)
    endif()
    
    if(NOT GTest_FOUND)
        include(FetchContent)
        FetchContent_Declare(
            googletest
            GIT_REPOSITORY https://github.com/google/googletest.git
            GIT_TAG v1.14.0
        )
        set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
        FetchContent_MakeAvailable(googletest)
        message(STATUS "Using fetched GoogleTest with GMock support")
    else()
        message(STATUS "Using system GoogleTest with GMock support")
    endif()
    
    include(GoogleTest)
    enable_testing()
endif()

# Coverage flags
if(ENABLE_COVERAGE AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(--coverage -fprofile-arcs -ftest-coverage)
    add_link_options(--coverage)
endif()

# ============================================================================
# Unit Tests
# ============================================================================
if(BUILD_TESTING)
    message(STATUS "Building unit tests")
    
    # DeepTreeEcho Unit Tests - exclude UE-dependent tests
    file(GLOB UNIT_TEST_SOURCES "DeepTreeEcho/Testing/UnitTests/*.cpp")
    
    # Exclude Unreal Engine dependent tests (need CoreMinimal.h, AutomationTest.h, UCLASS)
    list(FILTER UNIT_TEST_SOURCES EXCLUDE REGEX "BidirectionalMessageProtocolTests.cpp")
    list(FILTER UNIT_TEST_SOURCES EXCLUDE REGEX "TypeConversionTests.cpp")
    list(FILTER UNIT_TEST_SOURCES EXCLUDE REGEX "ReadoutLayerTrainingTests.cpp")
    list(FILTER UNIT_TEST_SOURCES EXCLUDE REGEX "ReservoirTopologyGeneratorTests.cpp")
    list(FILTER UNIT_TEST_SOURCES EXCLUDE REGEX "ReservoirIntegrationTests.cpp")
    list(FILTER UNIT_TEST_SOURCES EXCLUDE REGEX "SymbolicToNeuralEncoderTests.cpp")
    
    if(UNIT_TEST_SOURCES)
        add_executable(DeepTreeEchoUnitTests ${UNIT_TEST_SOURCES})
        target_link_libraries(DeepTreeEchoUnitTests
            GTest::gtest
            GTest::gtest_main
            GTest::gmock
            Threads::Threads
            Eigen3::Eigen
        )
        target_include_directories(DeepTreeEchoUnitTests PRIVATE
            ${CMAKE_SOURCE_DIR}/DeepTreeEcho
            ${CMAKE_SOURCE_DIR}/ReservoirEcho/reservoircpp_cpp/include
        )
        gtest_discover_tests(DeepTreeEchoUnitTests
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            PROPERTIES LABELS "unit"
        )
        message(STATUS "Standalone unit tests configured (UE-dependent tests excluded for standalone build)")
        message(STATUS "  NOTE: Full test suite runs within Unreal Engine environment")
    endif()
endif()

# ============================================================================
# E2E Tests
# ============================================================================
if(BUILD_E2E_TESTS)
    message(STATUS "Building E2E tests")
    
    file(GLOB E2E_TEST_SOURCES "DeepTreeEcho/Testing/E2E/*.cpp")
    
    foreach(E2E_SOURCE ${E2E_TEST_SOURCES})
        get_filename_component(E2E_NAME ${E2E_SOURCE} NAME_WE)
        
        add_executable(${E2E_NAME} ${E2E_SOURCE})
        target_link_libraries(${E2E_NAME}
            GTest::gtest
            GTest::gtest_main
            GTest::gmock
            Threads::Threads
            Eigen3::Eigen
        )
        target_include_directories(${E2E_NAME} PRIVATE
            ${CMAKE_SOURCE_DIR}/DeepTreeEcho
            ${CMAKE_SOURCE_DIR}/ReservoirEcho/reservoircpp_cpp/include
        )
        gtest_discover_tests(${E2E_NAME}
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            PROPERTIES
                LABELS "e2e"
                TIMEOUT 300
        )
    endforeach()
endif()

# ============================================================================
# Installation
# ============================================================================
include(GNUInstallDirs)

# Install headers
install(DIRECTORY DeepTreeEcho/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/DeepTreeEcho
    FILES_MATCHING
    PATTERN "*.h"
    PATTERN "*.hpp"
    PATTERN "Testing" EXCLUDE
)

# ============================================================================
# CPack Configuration
# ============================================================================
set(CPACK_PACKAGE_NAME "un9n")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${PROJECT_DESCRIPTION}")
set(CPACK_PACKAGE_VENDOR "DeepTreeEcho")

if(WIN32)
    set(CPACK_GENERATOR "ZIP")
elseif(APPLE)
    set(CPACK_GENERATOR "TGZ")
else()
    set(CPACK_GENERATOR "TGZ;DEB")
endif()

include(CPack)

message(STATUS "")
message(STATUS "=== un9n Build Configuration ===")
message(STATUS "Version: ${PROJECT_VERSION}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Eigen location: ${EIGEN3_INCLUDE_DIR}")
message(STATUS "")
message(STATUS "Testing:")
message(STATUS "  Unit Tests: ${BUILD_TESTING}")
message(STATUS "  E2E Tests: ${BUILD_E2E_TESTS}")
message(STATUS "  Benchmarks: ${BUILD_BENCHMARKS}")
message(STATUS "  Coverage: ${ENABLE_COVERAGE}")
message(STATUS "================================")
