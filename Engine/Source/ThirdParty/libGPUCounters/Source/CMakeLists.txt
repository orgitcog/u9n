cmake_minimum_required(VERSION 3.22.1)
project(libGPUCounters)

message("Using NDK version ${CMAKE_ANDROID_NDK_VERSION}")

file(GLOB_RECURSE SRC
  ${CMAKE_SOURCE_DIR}/**/*.cpp
  ${CMAKE_SOURCE_DIR}/**/*.hpp
  ${CMAKE_SOURCE_DIR}/*.cpp
  ${CMAKE_SOURCE_DIR}/../Public/*.h
)

add_library(GPUCounters SHARED ${SRC})
target_include_directories(GPUCounters PRIVATE
  ${CMAKE_SOURCE_DIR}/src
  ${CMAKE_SOURCE_DIR}/include
  ${CMAKE_SOURCE_DIR}/../Public
)

set_property(TARGET GPUCounters PROPERTY CXX_STANDARD 20)

set_target_properties(GPUCounters PROPERTIES COMPILE_FLAGS "-fno-exceptions -fno-rtti -fno-unwind-tables -g -O3")
set_target_properties(GPUCounters PROPERTIES LINK_FLAGS "-z max-page-size=16384")
