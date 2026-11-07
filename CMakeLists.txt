cmake_minimum_required(VERSION 3.20)
project(WindowsPerformanceInvestigationLab
    VERSION 1.0.0
    LANGUAGES CXX
    DESCRIPTION "Native Windows C++20 Multithreaded Performance and ETW Investigation Lab"
)

# Standard C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Export compile commands for IDE tooling
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Threading support
find_package(Threads REQUIRED)

# Common include directory
set(PERF_LAB_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)

# Core performance lab static library
add_library(perf_lab_lib STATIC
    src/diagnostics/etw_provider.cpp
    src/workload/contended_workload.cpp
    src/workload/optimized_workload.cpp
    src/benchmark/benchmark_runner.cpp
    src/common/cli_parser.cpp
)

target_include_directories(perf_lab_lib PUBLIC
    ${PERF_LAB_INCLUDE_DIR}
)

target_link_libraries(perf_lab_lib PUBLIC
    Threads::Threads
)

# Platform-specific configurations and warnings
if(MSVC)
    target_compile_options(perf_lab_lib PUBLIC
        /W4
        /permissive-
        /EHsc
        /utf-8
    )
    # Link Advapi32 on Windows for ETW / TraceLogging runtime APIs
    target_link_libraries(perf_lab_lib PUBLIC
        Advapi32
    )
else()
    target_compile_options(perf_lab_lib PUBLIC
        -Wall
        -Wextra
        -Wpedantic
    )
endif()

# Main executable: perf_lab
add_executable(perf_lab src/main.cpp)
target_link_libraries(perf_lab PRIVATE perf_lab_lib)

# Testing configuration with CTest
enable_testing()

# 1. Workload unit tests
add_executable(perf_lab_workload_tests tests/workload_tests.cpp)
target_link_libraries(perf_lab_workload_tests PRIVATE perf_lab_lib)
add_test(NAME WorkloadTests COMMAND perf_lab_workload_tests)

# 2. CLI parser unit tests
add_executable(perf_lab_cli_tests tests/cli_tests.cpp)
target_link_libraries(perf_lab_cli_tests PRIVATE perf_lab_lib)
add_test(NAME CliTests COMMAND perf_lab_cli_tests)

# 3. Benchmark engine and statistics unit tests
add_executable(perf_lab_benchmark_tests tests/benchmark_tests.cpp)
target_link_libraries(perf_lab_benchmark_tests PRIVATE perf_lab_lib)
add_test(NAME BenchmarkTests COMMAND perf_lab_benchmark_tests)
