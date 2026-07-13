# ============================================================================
# OpenCogEcho - Modern OpenCog core (ported from 9-o9/u9)
#
# Builds the opencog_echo library: SoA AtomSpace, 8-byte TruthValue,
# lock-free ECAN attention bank, coroutine pattern matcher, SIMD PLN
# formulas, URE forward/backward chaining, and Atomese loader.
#
# This module requires C++23 (falls back to C++20 if the toolchain does
# not advertise cxx_std_23). The global project standard stays C++17 —
# the newer standard applies only to targets in this directory.
# ============================================================================

find_package(Threads REQUIRED)

# Pick the newest language standard the compiler advertises (>= C++20)
if("cxx_std_23" IN_LIST CMAKE_CXX_COMPILE_FEATURES)
    set(OPENCOG_ECHO_CXX_STD cxx_std_23)
else()
    set(OPENCOG_ECHO_CXX_STD cxx_std_20)
    message(STATUS "OpenCogEcho: compiler lacks cxx_std_23, falling back to cxx_std_20")
endif()

# ----------------------------------------------------------------------------
# Library
# ----------------------------------------------------------------------------
add_library(opencog_echo STATIC
    src/core/types.cpp
    src/core/memory.cpp
    src/atomspace/atom_table.cpp
    src/atomspace/atomspace.cpp
    src/atomspace/index.cpp
    src/attention/attention_bank.cpp
    src/attention/ecan.cpp
    src/pattern/matcher.cpp
    src/pattern/pattern.cpp
    src/pln/formulas.cpp
    src/pln/inference.cpp
    src/pln/truth_value.cpp
    src/ure/engine.cpp
    src/ure/rule.cpp
    src/atomese/loader.cpp

    # Bio-cognitive modules (layer 2, ported from 9-o9/u9)
    src/endocrine/types.cpp
    src/endocrine/hormone_bus.cpp
    src/endocrine/gland.cpp
    src/endocrine/valence.cpp
    src/endocrine/affect.cpp
    src/endocrine/moral.cpp
    src/endocrine/connector.cpp
    src/endocrine/guidance_connector.cpp
    src/endocrine/marduk_adapter.cpp
    src/endocrine/npu_adapter.cpp
    src/endocrine/o9c2_adapter.cpp
    src/endocrine/touchpad_adapter.cpp
    src/endocrine/endocrine_system.cpp

    src/nervous/nerve_bus.cpp
    src/nervous/nucleus.cpp
    src/nervous/nervous_system.cpp

    src/temporal/crystal_bus.cpp
    src/temporal/temporal_system.cpp

    src/entelechy/adapter.cpp
    src/entelechy/civic_angel.cpp
    src/entelechy/cloninger.cpp
    src/entelechy/developmental.cpp
    src/entelechy/district.cpp
    src/entelechy/interoceptive.cpp
    src/entelechy/narrative.cpp
    src/entelechy/social.cpp

    src/afi/adapter.cpp
    src/afi/blanket.cpp
    src/afi/free_energy.cpp
    src/afi/precision.cpp

    # Type Conversion Bridge (F1.1.3) — tensor ↔ atom conversions
    src/bridge/tensor_to_atom.cpp
    src/bridge/atom_to_tensor.cpp
    src/bridge/type_conversion_bridge.cpp
)

target_compile_features(opencog_echo PUBLIC ${OPENCOG_ECHO_CXX_STD})

# Preserve #include <opencog/...> paths for this module and its consumers
target_include_directories(opencog_echo PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(opencog_echo PUBLIC Threads::Threads Eigen3::Eigen)

# ----------------------------------------------------------------------------
# Tests (custom lightweight harness in tests/test_main.cpp, ctest-registered)
# ----------------------------------------------------------------------------
if(BUILD_TESTING)
    set(OPENCOG_ECHO_TEST_SOURCES
        tests/test_main.cpp
        tests/test_types.cpp
        tests/test_atomspace.cpp
        tests/test_attention.cpp
        tests/test_pattern.cpp
        tests/test_pln.cpp
        tests/test_atomese.cpp
    )

    # test_integration.cpp exercises the layer-2 endocrine adapters; it is
    # included automatically once the bio-cognitive modules land.
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/include/opencog/endocrine/endocrine_system.hpp")
        list(APPEND OPENCOG_ECHO_TEST_SOURCES tests/test_integration.cpp)
        message(STATUS "OpenCogEcho: endocrine headers present, enabling test_integration.cpp")

        # Bio-cognitive module tests (layer 2, ported from 9-o9/u9)
        list(APPEND OPENCOG_ECHO_TEST_SOURCES
            tests/test_endocrine.cpp
            tests/test_nervous.cpp
            tests/test_neuroendocrine.cpp
            tests/test_entelechy_integration.cpp
            tests/test_civic_angel.cpp
            tests/test_cloninger.cpp
            tests/test_developmental.cpp
            tests/test_interoceptive.cpp
            tests/test_narrative.cpp
            tests/test_social.cpp
            tests/test_afi.cpp
            tests/test_temporal.cpp
            tests/test_marduk.cpp
            tests/test_touchpad.cpp
        )
    else()
        message(STATUS "OpenCogEcho: endocrine headers absent (layer 2), skipping test_integration.cpp")
    endif()

    # Type Conversion Bridge tests (F1.1.3) — enabled when bridge headers present
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/include/opencog/bridge/type_conversion_bridge.hpp")
        list(APPEND OPENCOG_ECHO_TEST_SOURCES tests/test_type_conversion_bridge.cpp)
        message(STATUS "OpenCogEcho: bridge headers present, enabling test_type_conversion_bridge.cpp")
    endif()

    add_executable(opencog_echo_tests ${OPENCOG_ECHO_TEST_SOURCES})
    target_link_libraries(opencog_echo_tests PRIVATE opencog_echo)

    add_test(NAME OpenCogEchoTests COMMAND opencog_echo_tests)
    set_tests_properties(OpenCogEchoTests PROPERTIES
        LABELS "unit;opencog"
        TIMEOUT 300
    )
endif()

# ----------------------------------------------------------------------------
# Benchmarks
# ----------------------------------------------------------------------------
if(BUILD_BENCHMARKS)
    add_executable(opencog_echo_benchmarks benchmarks/bench_main.cpp)
    target_link_libraries(opencog_echo_benchmarks PRIVATE opencog_echo)
endif()

message(STATUS "OpenCogEcho: opencog_echo configured (${OPENCOG_ECHO_CXX_STD})")
