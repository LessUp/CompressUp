cmake_minimum_required(VERSION 3.16)

project(CompressUp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 编译选项
add_compile_options(-Wall -Wextra -Wpedantic)

# 可选：启用LTO
option(ENABLE_LTO "Enable Link Time Optimization" OFF)
if(ENABLE_LTO)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()

# 核心库
add_library(compressup_lib
    # 基础设施
    src/registry.cpp
    src/container.cpp
    src/file_io.cpp
    src/api.cpp
    
    # 压缩算法
    src/rle_compressor.cpp
    src/lz77_compressor.cpp
    src/huffman_compressor.cpp
    src/lzw_compressor.cpp
    src/lzss_compressor.cpp
    src/delta_compressor.cpp
    src/bwt_compressor.cpp
    
    # 并行和高级IO
    src/parallel_compressor.cpp
    src/advanced_io.cpp
)

target_include_directories(compressup_lib
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# 链接pthread
find_package(Threads REQUIRED)
target_link_libraries(compressup_lib PUBLIC Threads::Threads)

# CLI工具
add_executable(compressup_cli
    src/main.cpp
)
target_link_libraries(compressup_cli PRIVATE compressup_lib)

# 测试程序
add_executable(compressup_tests
    tests/test_main.cpp
)
target_link_libraries(compressup_tests PRIVATE compressup_lib)

# 基准测试程序
add_executable(compressup_bench
    bench/bench_main.cpp
)
target_link_libraries(compressup_bench PRIVATE compressup_lib)

# 高级基准测试程序
add_executable(compressup_advanced_bench
    bench/advanced_bench.cpp
)
target_link_libraries(compressup_advanced_bench PRIVATE compressup_lib)

enable_testing()
add_test(NAME compressup_tests COMMAND compressup_tests)

# 安装规则
install(TARGETS compressup_cli compressup_lib
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

install(DIRECTORY src/
    DESTINATION include/compressup
    FILES_MATCHING PATTERN "*.h"
)
