cmake_minimum_required(VERSION 3.14)

project(PyDNA)

set(CMAKE_CXX_STANDARD 11)

find_package(SWIG 4.0.0)

if(NOT SWIG_FOUND)
    message(WARNING "SWIG 4.0.0 or later not found. Python bindings for DNA will not be built.")
    return()
endif()

include(UseSWIG)

add_subdirectory(python3)
