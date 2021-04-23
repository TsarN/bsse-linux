cmake_minimum_required(VERSION 3.12)
project("transform_mp" LANGUAGES C CXX VERSION "0.1")

add_library(transform_mp INTERFACE)
target_include_directories(transform_mp INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_compile_features(transform_mp INTERFACE cxx_std_20)

add_executable(transform_mp_test
    ./test/test.cpp
)
target_link_libraries(transform_mp_test PRIVATE transform_mp)
