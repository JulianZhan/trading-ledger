cmake_minimum_required(VERSION 3.20)
project(TradingLedgerCpp VERSION 1.0.0 LANGUAGES CXX)

# C++20 required
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler warnings
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic -Werror)
endif()

# Fetch Google Test
include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
)
FetchContent_MakeAvailable(googletest)

# Fetch Google Benchmark
FetchContent_Declare(
    googlebenchmark
    GIT_REPOSITORY https://github.com/google/benchmark.git
    GIT_TAG v1.8.3
)
set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googlebenchmark)

# Enable testing
enable_testing()
include(GoogleTest)

# Source files
set(SOURCES
    src/EventParser.cpp
    src/FileReader.cpp
)

# Create library
add_library(trading_ledger_lib ${SOURCES})
target_include_directories(trading_ledger_lib PUBLIC include)

# Link with -lz for CRC32 (zlib)
find_package(ZLIB REQUIRED)
target_link_libraries(trading_ledger_lib PUBLIC ZLIB::ZLIB)

# Tests
add_subdirectory(test)

# Benchmarks
add_subdirectory(benchmark)
