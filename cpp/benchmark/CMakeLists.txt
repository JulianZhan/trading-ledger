# Benchmark executables
add_executable(event_parser_bench
    event_parser_bench.cpp
)

target_link_libraries(event_parser_bench
    PRIVATE
        trading_ledger_lib
        benchmark::benchmark
        benchmark::benchmark_main
)

# Ring buffer benchmark
add_executable(ring_buffer_bench
    ring_buffer_bench.cpp
)

target_include_directories(ring_buffer_bench
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(ring_buffer_bench
    PRIVATE
        benchmark::benchmark
)

# Link threads library for multi-threaded benchmarks
find_package(Threads REQUIRED)
target_link_libraries(ring_buffer_bench PRIVATE Threads::Threads)
