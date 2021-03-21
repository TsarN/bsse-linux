cmake_minimum_required(VERSION 3.12)
project("file_read_benchmark" LANGUAGES C CXX VERSION "0.1")
include(cmake/CPM.cmake)

add_executable(file_read_benchmark
    ./src/main.cpp
)

target_compile_options(file_read_benchmark PRIVATE -Wall -Wextra -pedantic)
target_compile_features(file_read_benchmark PRIVATE cxx_std_20)

CPMAddPackage("gh:jarro2783/cxxopts#dd45a0801c99d62109aaa29f8c410ba8def2fbf2")

target_link_libraries(file_read_benchmark cxxopts::cxxopts)
