cmake_minimum_required(VERSION 3.14)
project(DeepTreeEchoTests VERSION 1.0.0 LANGUAGES CXX)

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

# Find or fetch GTest
find_package(GTest QUIET)
if(NOT GTest_FOUND)
    message(STATUS "GTest not found, fetching from GitHub...")
    include(FetchContent)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.14.0
    )
    # Prevent overriding the parent project's compiler/linker settings on Windows
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
    set(GTEST_LIBS gtest gtest_main gmock pthread)
else()
    set(GTEST_LIBS GTest::gtest GTest::gtest_main GTest::gmock pthread)
endif()
include(GoogleTest)

# Compiler flags
if(MSVC)
    add_compile_options(/W4 /WX-)
else()
    add_compile_options(-Wall -Wextra -pedantic -Wno-unused-parameter)
endif()

# Enable testing
enable_testing()

# ============================================================================
# Core Tests
# ============================================================================
add_executable(CoreTests
    CoreTests.cpp
)

target_link_libraries(CoreTests ${GTEST_LIBS})

target_include_directories(CoreTests PRIVATE
    ${CMAKE_SOURCE_DIR}/../../
    ${CMAKE_SOURCE_DIR}/../../../
)

gtest_discover_tests(CoreTests
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;core"
        TIMEOUT 60
)

# ============================================================================
# Memory Tests
# ============================================================================
add_executable(MemoryTests
    MemoryTests.cpp
)

target_link_libraries(MemoryTests ${GTEST_LIBS})

target_include_directories(MemoryTests PRIVATE
    ${CMAKE_SOURCE_DIR}/../../
    ${CMAKE_SOURCE_DIR}/../../../
)

gtest_discover_tests(MemoryTests
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;memory"
        TIMEOUT 60
)

# ============================================================================
# Avatar Cognition Tests
# ============================================================================
add_executable(AvatarCognitionTests
    AvatarCognitionTests.cpp
)

target_link_libraries(AvatarCognitionTests ${GTEST_LIBS})

target_include_directories(AvatarCognitionTests PRIVATE
    ${CMAKE_SOURCE_DIR}/../../
    ${CMAKE_SOURCE_DIR}/../../../
)

gtest_discover_tests(AvatarCognitionTests
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;avatar;cognition"
        TIMEOUT 60
)

# ============================================================================
# Reservoir Integration Tests
# ============================================================================
add_executable(ReservoirIntegrationTests
    ReservoirIntegrationTests.cpp
)

target_link_libraries(ReservoirIntegrationTests ${GTEST_LIBS})

target_include_directories(ReservoirIntegrationTests PRIVATE
    ${CMAKE_SOURCE_DIR}/../../
    ${CMAKE_SOURCE_DIR}/../../../
    ${CMAKE_SOURCE_DIR}/../../../ReservoirEcho/reservoircpp_cpp/include
    ${CMAKE_SOURCE_DIR}/../../../ReservoirEcho/external/taskflow-3.8.0/3rd-party/eigen-3.3.7
)

gtest_discover_tests(ReservoirIntegrationTests
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;reservoir;integration"
        TIMEOUT 120
)

# ============================================================================
# Echo State Network Tests (Feature F1.2.1)
# ============================================================================
add_executable(EchoStateNetworkTests
    EchoStateNetworkTests.cpp
)

target_link_libraries(EchoStateNetworkTests ${GTEST_LIBS})

target_include_directories(EchoStateNetworkTests PRIVATE
    ${CMAKE_SOURCE_DIR}/../../
    ${CMAKE_SOURCE_DIR}/../../../
    ${CMAKE_SOURCE_DIR}/../../../ReservoirEcho/reservoircpp_cpp/include
    ${CMAKE_SOURCE_DIR}/../../../ReservoirEcho/external/eigen-3.4.0
)

gtest_discover_tests(EchoStateNetworkTests
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;esn;reservoir;f1.2.1"
        TIMEOUT 120
)

# ============================================================================
# Active Inference Tests
# ============================================================================
add_executable(ActiveInferenceTests
    ActiveInferenceTests.cpp
)

target_link_libraries(ActiveInferenceTests ${GTEST_LIBS})

target_include_directories(ActiveInferenceTests PRIVATE
    ${CMAKE_SOURCE_DIR}/../../
    ${CMAKE_SOURCE_DIR}/../../../
)

gtest_discover_tests(ActiveInferenceTests
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;active-inference"
        TIMEOUT 60
)

# ============================================================================
# DNA Integration Tests
# ============================================================================
add_executable(DNAIntegrationTests
    DNAIntegrationTests.cpp
)

target_link_libraries(DNAIntegrationTests ${GTEST_LIBS})

target_include_directories(DNAIntegrationTests PRIVATE
    ${CMAKE_SOURCE_DIR}/../../
    ${CMAKE_SOURCE_DIR}/../../../
    ${CMAKE_SOURCE_DIR}/../../../UnrealEcho/Avatar
)

gtest_discover_tests(DNAIntegrationTests
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;dna;integration;metahuman"
        TIMEOUT 120
)

# ============================================================================
# Cognitive Loop Tests (12-Step Cycle)
# ============================================================================
add_executable(CognitiveLoopTests
    CognitiveLoopTests.cpp
)

target_link_libraries(CognitiveLoopTests ${GTEST_LIBS})

target_include_directories(CognitiveLoopTests PRIVATE
    ${CMAKE_SOURCE_DIR}/../../
    ${CMAKE_SOURCE_DIR}/../../../
)

gtest_discover_tests(CognitiveLoopTests
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;cognitive-loop;12-step"
        TIMEOUT 120
)

# ============================================================================
# Wisdom & Entelechy Tests
# ============================================================================
add_executable(WisdomEntelechyTests
    WisdomEntelechyTests.cpp
)

target_link_libraries(WisdomEntelechyTests ${GTEST_LIBS})

target_include_directories(WisdomEntelechyTests PRIVATE
    ${CMAKE_SOURCE_DIR}/../../
    ${CMAKE_SOURCE_DIR}/../../../
)

gtest_discover_tests(WisdomEntelechyTests
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;wisdom;entelechy;relevance"
        TIMEOUT 120
)

# ============================================================================
# Sys6 Operad Tests
# ============================================================================
add_executable(Sys6OperadTests
    Sys6OperadTests.cpp
)

target_link_libraries(Sys6OperadTests ${GTEST_LIBS})

target_include_directories(Sys6OperadTests PRIVATE
    ${CMAKE_SOURCE_DIR}/../../
    ${CMAKE_SOURCE_DIR}/../../../
)

gtest_discover_tests(Sys6OperadTests
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;sys6;operad;clock30"
        TIMEOUT 120
)

# ============================================================================
# Behavioral Framework Tests
# ============================================================================
add_executable(BehavioralFrameworkTests
    BehavioralFrameworkTests.cpp
)

target_link_libraries(BehavioralFrameworkTests ${GTEST_LIBS})

target_include_directories(BehavioralFrameworkTests PRIVATE
    ${CMAKE_SOURCE_DIR}/../../
    ${CMAKE_SOURCE_DIR}/../../../
)

gtest_discover_tests(BehavioralFrameworkTests
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;behavioral;personality;emotion"
        TIMEOUT 120
)

# ============================================================================
# Tensor Logic Tests
# ============================================================================
add_executable(TensorLogicTests
    TensorLogicTests.cpp
)

target_link_libraries(TensorLogicTests ${GTEST_LIBS})

target_include_directories(TensorLogicTests PRIVATE
    ${CMAKE_SOURCE_DIR}/../../
    ${CMAKE_SOURCE_DIR}/../../../
)

gtest_discover_tests(TensorLogicTests
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;tensor-logic;neural-symbolic"
        TIMEOUT 180
)

# ============================================================================
# Reservoir Topology Generator Tests
# ============================================================================
add_executable(ReservoirTopologyGeneratorTests
    ReservoirTopologyGeneratorTests.cpp
)

target_link_libraries(ReservoirTopologyGeneratorTests ${GTEST_LIBS})

target_include_directories(ReservoirTopologyGeneratorTests PRIVATE
    ${CMAKE_SOURCE_DIR}/../../
    ${CMAKE_SOURCE_DIR}/../../../
    ${CMAKE_SOURCE_DIR}/../../../ReservoirEcho/reservoircpp_cpp/include
    ${CMAKE_SOURCE_DIR}/../../../ReservoirEcho/external/taskflow-3.8.0/3rd-party/eigen-3.3.7
)

gtest_discover_tests(ReservoirTopologyGeneratorTests
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;reservoir;topology;F1.2.3"
        TIMEOUT 120
)

# ============================================================================
# Membrane Lifecycle Tests (Feature F1.3.3)
# ============================================================================
add_executable(MembraneLifecycleTests
    MembraneLifecycleTests.cpp
)

target_link_libraries(MembraneLifecycleTests ${GTEST_LIBS})

target_include_directories(MembraneLifecycleTests PRIVATE
    ${CMAKE_SOURCE_DIR}/../../
    ${CMAKE_SOURCE_DIR}/../../../
)

gtest_discover_tests(MembraneLifecycleTests
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;membrane;lifecycle;F1.3.3"
        TIMEOUT 120
)

# ============================================================================
# All Tests Target
# ============================================================================
add_custom_target(run_all_tests
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure --parallel 4
    DEPENDS CoreTests MemoryTests AvatarCognitionTests ReservoirIntegrationTests EchoStateNetworkTests ActiveInferenceTests DNAIntegrationTests CognitiveLoopTests WisdomEntelechyTests Sys6OperadTests BehavioralFrameworkTests TensorLogicTests ReservoirTopologyGeneratorTests MembraneLifecycleTests
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Running all DeepTreeEcho unit tests"
)

# ============================================================================
# Coverage Target (GCC/Clang only)
# ============================================================================
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    option(ENABLE_COVERAGE "Enable code coverage" OFF)
    
    if(ENABLE_COVERAGE)
        add_compile_options(--coverage -fprofile-arcs -ftest-coverage)
        add_link_options(--coverage)
        
        add_custom_target(coverage
            COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
            COMMAND gcov -r ${CMAKE_SOURCE_DIR}/*.cpp
            COMMAND lcov --capture --directory . --output-file coverage.info
            COMMAND lcov --remove coverage.info '/usr/*' --output-file coverage.info
            COMMAND genhtml coverage.info --output-directory coverage_report
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            COMMENT "Generating code coverage report"
        )
    endif()
endif()

# ============================================================================
# Installation
# ============================================================================
install(TARGETS CoreTests MemoryTests AvatarCognitionTests ReservoirIntegrationTests EchoStateNetworkTests ActiveInferenceTests DNAIntegrationTests CognitiveLoopTests WisdomEntelechyTests Sys6OperadTests BehavioralFrameworkTests TensorLogicTests ReservoirTopologyGeneratorTests MembraneLifecycleTests
    RUNTIME DESTINATION bin/tests
)

message(STATUS "DeepTreeEcho Tests configured")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  Tests: CoreTests, MemoryTests, AvatarCognitionTests, ReservoirIntegrationTests, EchoStateNetworkTests, ActiveInferenceTests, DNAIntegrationTests, CognitiveLoopTests, WisdomEntelechyTests, Sys6OperadTests, BehavioralFrameworkTests, TensorLogicTests, ReservoirTopologyGeneratorTests, MembraneLifecycleTests")
