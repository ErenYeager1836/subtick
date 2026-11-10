cmake_minimum_required(VERSION 3.20)
project(subtick VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler warning flags
if(MSVC)
    add_compile_options(/W4 /permissive- /utf-8)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Library interface
add_library(subtick INTERFACE)
target_include_directories(subtick INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# Test executables
enable_testing()

set(TEST_TARGETS
    test_bitstream
    test_packet
    test_reliability
    test_simulator
    test_subtick
    test_delta
)

foreach(TGT ${TEST_TARGETS})
    add_executable(${TGT} tests/${TGT}.cpp)
    target_link_libraries(${TGT} PRIVATE subtick)
    add_test(NAME ${TGT} COMMAND ${TGT})
endforeach()

# Benchmark example
add_executable(benchmark_server_client examples/benchmark_server_client.cpp)
target_link_libraries(benchmark_server_client PRIVATE subtick)
