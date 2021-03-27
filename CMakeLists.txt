cmake_minimum_required(VERSION 3.12)
project("file_fragmentation" LANGUAGES C CXX VERSION "0.1")

add_executable(file_fragmentation
    ./src/main.cpp
)

target_compile_options(file_fragmentation PRIVATE -Wall -Wextra -pedantic)
target_compile_features(file_fragmentation PRIVATE cxx_std_20)
