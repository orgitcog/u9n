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
# Cortisol Dynamics Tests
# ============================================================================
add_executable(CortisolDynamicsTests
    CortisolDynamicsTests.cpp
)

target_link_libraries(CortisolDynamicsTests
    GTest::gtest
    GTest::gtest_main
    pthread
)

target_include_directories(CortisolDynamicsTests PRIVATE
    ${CMAKE_SOURCE_DIR}/../../
    ${CMAKE_SOURCE_DIR}/../../../
)

gtest_discover_tests(CortisolDynamicsTests
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;neurochemical;cortisol"
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
# Membrane Communication Channels Tests (Feature F1.3.4)
# ============================================================================
add_executable(MembraneCommChannelsTests
    MembraneCommChannelsTests.cpp
)

target_link_libraries(MembraneCommChannelsTests ${GTEST_LIBS})

target_include_directories(MembraneCommChannelsTests PRIVATE
    ${CMAKE_SOURCE_DIR}/../../
    ${CMAKE_SOURCE_DIR}/../../../
)

gtest_discover_tests(MembraneCommChannelsTests
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;membrane;p-system;channels;F1.3.4"
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
# Object Transport Rules Tests (Feature F1.3.2)
# ============================================================================
add_executable(ObjectTransportRulesTests
    ObjectTransportRulesTests.cpp
)

target_link_libraries(ObjectTransportRulesTests ${GTEST_LIBS})

target_include_directories(ObjectTransportRulesTests PRIVATE
    ${CMAKE_SOURCE_DIR}/../../
    ${CMAKE_SOURCE_DIR}/../../../
    ${CMAKE_SOURCE_DIR}/../../../DeepTreeEcho/Membrane
)

gtest_discover_tests(ObjectTransportRulesTests
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;membrane;transport;F1.3.2"
        TIMEOUT 120
)

# ============================================================================
# Hypergraph Pattern Matcher Tests (Feature F1.4.2)
# ============================================================================
add_executable(HypergraphPatternMatcherTests
    HypergraphPatternMatcherTests.cpp
)

target_link_libraries(HypergraphPatternMatcherTests ${GTEST_LIBS})

target_include_directories(HypergraphPatternMatcherTests PRIVATE
    ${CMAKE_SOURCE_DIR}/../../
    ${CMAKE_SOURCE_DIR}/../../../
    ${CMAKE_SOURCE_DIR}/../../../DeepTreeEcho/Memory
)

gtest_discover_tests(HypergraphPatternMatcherTests
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;hypergraph;pattern-matching;F1.4.2"
        TIMEOUT 120
)

# ============================================================================
# Future State Prediction Tests (Feature F1.5.3)
# ============================================================================
add_executable(FutureStatePredictionTests
    FutureStatePredictionTests.cpp
)

target_link_libraries(FutureStatePredictionTests ${GTEST_LIBS})

target_include_directories(FutureStatePredictionTests PRIVATE
    ${CMAKE_SOURCE_DIR}/../../
    ${CMAKE_SOURCE_DIR}/../../../
    ${CMAKE_SOURCE_DIR}/../../../DeepTreeEcho/Planning
)

gtest_discover_tests(FutureStatePredictionTests
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;prediction;planning;F1.5.3"
        TIMEOUT 120
)

# ============================================================================
# Virtual Controller Driver Tests
# ============================================================================
add_executable(VirtualControllerDriverTests
    VirtualControllerDriverTests.cpp
)

target_link_libraries(VirtualControllerDriverTests ${GTEST_LIBS})

target_include_directories(VirtualControllerDriverTests PRIVATE
    ${CMAKE_SOURCE_DIR}/../../
    ${CMAKE_SOURCE_DIR}/../../../
)

gtest_discover_tests(VirtualControllerDriverTests
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;gaming;controller;motor-cortex"
        TIMEOUT 60
)

# ============================================================================
# Gaming Mastery System Tests
# ============================================================================
add_executable(GamingMasterySystemTests
    GamingMasterySystemTests.cpp
)

target_link_libraries(GamingMasterySystemTests ${GTEST_LIBS})

target_include_directories(GamingMasterySystemTests PRIVATE
    ${CMAKE_SOURCE_DIR}/../../
    ${CMAKE_SOURCE_DIR}/../../../
)

gtest_discover_tests(GamingMasterySystemTests
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;gaming;mastery;dreyfus"
        TIMEOUT 60
)

# ============================================================================
# DTE Avatar Agent Tests
# ============================================================================
add_executable(DTEAvatarAgentTests
    DTEAvatarAgentTests.cpp
)

target_link_libraries(DTEAvatarAgentTests ${GTEST_LIBS})

target_include_directories(DTEAvatarAgentTests PRIVATE
    ${CMAKE_SOURCE_DIR}/../../
    ${CMAKE_SOURCE_DIR}/../../../
)

gtest_discover_tests(DTEAvatarAgentTests
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;avatar;agent;sima;4e-cognition"
        TIMEOUT 120
)

# ============================================================================
# Embodied Autonomy Pipeline Tests
# ============================================================================
add_executable(EmbodiedAutonomyPipelineTests
    EmbodiedAutonomyPipelineTests.cpp
)

target_link_libraries(EmbodiedAutonomyPipelineTests ${GTEST_LIBS})

target_include_directories(EmbodiedAutonomyPipelineTests PRIVATE
    ${CMAKE_SOURCE_DIR}/../../
    ${CMAKE_SOURCE_DIR}/../../../
)

gtest_discover_tests(EmbodiedAutonomyPipelineTests
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;autonomy;pipeline;embodied"
        TIMEOUT 120
)

# ============================================================================
# Dopaminergic Reward System Tests (Feature F4.1.2)
# ============================================================================
add_executable(DopaminergicRewardSystemTests
    DopaminergicRewardSystemTests.cpp
)

target_link_libraries(DopaminergicRewardSystemTests ${GTEST_LIBS})

target_include_directories(DopaminergicRewardSystemTests PRIVATE
    ${CMAKE_SOURCE_DIR}/../../
    ${CMAKE_SOURCE_DIR}/../../../
)

gtest_discover_tests(DopaminergicRewardSystemTests
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;neurochemical;dopamine;reward;F4.1.2"
        TIMEOUT 60
)

# ============================================================================
# Liquid State Machine Tests (Feature F1.2.2)
# ============================================================================
add_executable(LiquidStateMachineTests
    LiquidStateMachineTests.cpp
)

target_link_libraries(LiquidStateMachineTests ${GTEST_LIBS})

target_include_directories(LiquidStateMachineTests PRIVATE
    ${CMAKE_SOURCE_DIR}/../../
    ${CMAKE_SOURCE_DIR}/../../../
    ${CMAKE_SOURCE_DIR}/../../Reservoir
)

gtest_discover_tests(LiquidStateMachineTests
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;reservoir;lsm;spiking;F1.2.2"
        TIMEOUT 120
)

# ============================================================================
# Symbolic To Neural Encoder Tests
# ============================================================================
add_executable(SymbolicToNeuralEncoderTests
    SymbolicToNeuralEncoderTests.cpp
)

target_link_libraries(SymbolicToNeuralEncoderTests ${GTEST_LIBS})

target_include_directories(SymbolicToNeuralEncoderTests PRIVATE
    ${CMAKE_SOURCE_DIR}/../../
    ${CMAKE_SOURCE_DIR}/../../../
)

gtest_discover_tests(SymbolicToNeuralEncoderTests
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;neural-symbolic;encoder"
        TIMEOUT 60
)

# ============================================================================
# Neural To Symbolic Translator Tests (Feature F1.1.1)
# ============================================================================
add_executable(NeuralToSymbolicTranslatorTests
    NeuralToSymbolicTranslatorTests.cpp
)

target_link_libraries(NeuralToSymbolicTranslatorTests ${GTEST_LIBS})

target_include_directories(NeuralToSymbolicTranslatorTests PRIVATE
    ${CMAKE_SOURCE_DIR}/../../
    ${CMAKE_SOURCE_DIR}/../../../
)

gtest_discover_tests(NeuralToSymbolicTranslatorTests
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;neural-symbolic;translator;F1.1.1"
        TIMEOUT 60
)

# ============================================================================
# Lock-Free Message Queue Tests (Feature F1.1.2)
# ============================================================================
add_executable(LockFreeMessageQueueTests
    LockFreeMessageQueueTests.cpp
)

target_link_libraries(LockFreeMessageQueueTests ${GTEST_LIBS})

target_include_directories(LockFreeMessageQueueTests PRIVATE
    ${CMAKE_SOURCE_DIR}/../../
    ${CMAKE_SOURCE_DIR}/../../../
)

gtest_discover_tests(LockFreeMessageQueueTests
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;message-protocol;lock-free-queue;F1.1.2"
        TIMEOUT 60
)

# ============================================================================
# Bidirectional Message Protocol Tests (Feature F1.1.2)
# ============================================================================
add_executable(MessageProtocolTests
    MessageProtocolTests.cpp
)

target_link_libraries(MessageProtocolTests ${GTEST_LIBS})

target_include_directories(MessageProtocolTests PRIVATE
    ${CMAKE_SOURCE_DIR}/../../
    ${CMAKE_SOURCE_DIR}/../../../
)

gtest_discover_tests(MessageProtocolTests
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;message-protocol;bidirectional;F1.1.2"
        TIMEOUT 60
)

# ============================================================================
# Inference Module Tests (Gap A — DTE reservoir core, ridge readout, MLP encoder)
# ============================================================================
add_executable(InferenceModuleTests
    InferenceModuleTests.cpp
)

target_link_libraries(InferenceModuleTests ${GTEST_LIBS})

target_include_directories(InferenceModuleTests PRIVATE
    ${CMAKE_SOURCE_DIR}/../../
    ${CMAKE_SOURCE_DIR}/../../../
    ${CMAKE_SOURCE_DIR}/../../../ReservoirEcho/external/taskflow-3.8.0/3rd-party/eigen-3.3.7
)

gtest_discover_tests(InferenceModuleTests
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;inference;reservoir;gap-a"
        TIMEOUT 120
)

# ============================================================================
# Executive Module Tests (Gap B — cog0 task subsystem, reasoning, message bus)
# ============================================================================
add_executable(ExecutiveModuleTests
    ExecutiveModuleTests.cpp
)

target_link_libraries(ExecutiveModuleTests ${GTEST_LIBS})

target_include_directories(ExecutiveModuleTests PRIVATE
    ${CMAKE_SOURCE_DIR}/../../
    ${CMAKE_SOURCE_DIR}/../../../
)

gtest_discover_tests(ExecutiveModuleTests
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;executive;cog0;gap-b"
        TIMEOUT 120
)

# ============================================================================
# Endocrine Avatar Tests (Gap E — endocrine bus + avatar expression adapter)
# ============================================================================
add_executable(EndocrineAvatarTests
    EndocrineAvatarTests.cpp
)

target_link_libraries(EndocrineAvatarTests ${GTEST_LIBS})

target_include_directories(EndocrineAvatarTests PRIVATE
    ${CMAKE_SOURCE_DIR}/../../
    ${CMAKE_SOURCE_DIR}/../../../
)

gtest_discover_tests(EndocrineAvatarTests
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;endocrine;avatar;gap-e"
        TIMEOUT 120
)

# ============================================================================
# All Tests Target
# ============================================================================
add_custom_target(run_all_tests
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure --parallel 4
    DEPENDS CoreTests MemoryTests AvatarCognitionTests ReservoirIntegrationTests EchoStateNetworkTests ActiveInferenceTests CortisolDynamicsTests DNAIntegrationTests CognitiveLoopTests WisdomEntelechyTests Sys6OperadTests BehavioralFrameworkTests TensorLogicTests ReservoirTopologyGeneratorTests MembraneCommChannelsTests MembraneLifecycleTests ObjectTransportRulesTests HypergraphPatternMatcherTests FutureStatePredictionTests VirtualControllerDriverTests GamingMasterySystemTests DTEAvatarAgentTests EmbodiedAutonomyPipelineTests DopaminergicRewardSystemTests LiquidStateMachineTests SymbolicToNeuralEncoderTests NeuralToSymbolicTranslatorTests LockFreeMessageQueueTests MessageProtocolTests InferenceModuleTests ExecutiveModuleTests EndocrineAvatarTests
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
install(TARGETS CoreTests MemoryTests AvatarCognitionTests ReservoirIntegrationTests EchoStateNetworkTests ActiveInferenceTests CortisolDynamicsTests DNAIntegrationTests CognitiveLoopTests WisdomEntelechyTests Sys6OperadTests BehavioralFrameworkTests TensorLogicTests ReservoirTopologyGeneratorTests MembraneCommChannelsTests MembraneLifecycleTests ObjectTransportRulesTests HypergraphPatternMatcherTests FutureStatePredictionTests VirtualControllerDriverTests GamingMasterySystemTests DTEAvatarAgentTests EmbodiedAutonomyPipelineTests DopaminergicRewardSystemTests LiquidStateMachineTests SymbolicToNeuralEncoderTests NeuralToSymbolicTranslatorTests LockFreeMessageQueueTests MessageProtocolTests InferenceModuleTests ExecutiveModuleTests EndocrineAvatarTests
    RUNTIME DESTINATION bin/tests
)

message(STATUS "DeepTreeEcho Tests configured")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  Tests: CoreTests, MemoryTests, AvatarCognitionTests, ReservoirIntegrationTests, EchoStateNetworkTests, ActiveInferenceTests, CortisolDynamicsTests, DNAIntegrationTests, CognitiveLoopTests, WisdomEntelechyTests, Sys6OperadTests, BehavioralFrameworkTests, TensorLogicTests, ReservoirTopologyGeneratorTests, MembraneCommChannelsTests, MembraneLifecycleTests, ObjectTransportRulesTests, HypergraphPatternMatcherTests, FutureStatePredictionTests, VirtualControllerDriverTests, GamingMasterySystemTests, DTEAvatarAgentTests, EmbodiedAutonomyPipelineTests, DopaminergicRewardSystemTests, LiquidStateMachineTests, SymbolicToNeuralEncoderTests, NeuralToSymbolicTranslatorTests, LockFreeMessageQueueTests, MessageProtocolTests")
