cmake_minimum_required(VERSION 3.13.0)
project(dr_libs CXX)
add_library(dr_libs STATIC drmp3.cpp)

target_include_directories(dr_libs 
	PUBLIC dr_libs-master
	PUBLIC ../dr_libs/include
)
