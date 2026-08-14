cmake_minimum_required(VERSION 3.14)

# Basic example
add_executable(basic_example basic_example.cpp)
target_link_libraries(basic_example PRIVATE reservoircpp)
