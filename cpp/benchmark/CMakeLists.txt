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
