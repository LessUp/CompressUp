cmake_minimum_required(VERSION 3.16)

project(CompressUp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(compressup_lib
    src/rle_compressor.cpp
    src/lz77_compressor.cpp
    src/registry.cpp
    src/container.cpp
    src/file_io.cpp
    src/api.cpp
)

target_include_directories(compressup_lib
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

add_executable(compressup_cli
    src/main.cpp
)

target_link_libraries(compressup_cli PRIVATE compressup_lib)

add_executable(compressup_tests
    tests/test_main.cpp
)

target_link_libraries(compressup_tests PRIVATE compressup_lib)

add_executable(compressup_bench
    bench/bench_main.cpp
)

target_link_libraries(compressup_bench PRIVATE compressup_lib)

enable_testing()

add_test(NAME compressup_tests COMMAND compressup_tests)
