cmake_minimum_required(VERSION 3.14)

# Find Google Test (check if already found by parent)
if(NOT TARGET GTest::gtest)
    find_package(GTest QUIET)
    
    if(NOT GTest_FOUND)
        # GTest not found, skip tests
        message(WARNING "GTest not found. Skipping ReservoirCpp tests.")
        return()
    endif()
endif()

include(GoogleTest)

# Node test
add_executable(node_test node_test.cpp)
target_link_libraries(node_test PRIVATE reservoircpp_core GTest::GTest GTest::Main)
target_include_directories(node_test PRIVATE ${CMAKE_SOURCE_DIR}/include)
gtest_discover_tests(node_test)

# Model test
add_executable(model_test model_test.cpp)
target_link_libraries(model_test PRIVATE reservoircpp_core GTest::GTest GTest::Main)
target_include_directories(model_test PRIVATE ${CMAKE_SOURCE_DIR}/include)
gtest_discover_tests(model_test)
