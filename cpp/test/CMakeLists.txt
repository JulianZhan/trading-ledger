# Test executables
add_executable(event_parser_test
    event_parser_test.cpp
)

target_link_libraries(event_parser_test
    PRIVATE
        trading_ledger_lib
        GTest::gtest
        GTest::gtest_main
)

gtest_discover_tests(event_parser_test)

# Integration test
add_executable(integration_test
    integration_test.cpp
)

target_link_libraries(integration_test
    PRIVATE
        trading_ledger_lib
        GTest::gtest
        GTest::gtest_main
)

gtest_discover_tests(integration_test)

# Ring buffer test (header-only, no library dependency needed)
add_executable(ring_buffer_test
    ring_buffer_test.cpp
)

target_include_directories(ring_buffer_test
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(ring_buffer_test
    PRIVATE
        GTest::gtest
        GTest::gtest_main
)

# Link threads library for multi-threaded tests
find_package(Threads REQUIRED)
target_link_libraries(ring_buffer_test PRIVATE Threads::Threads)

gtest_discover_tests(ring_buffer_test)

# Event log reader test
add_executable(event_log_reader_test
    event_log_reader_test.cpp
)

target_link_libraries(event_log_reader_test
    PRIVATE
        trading_ledger_lib
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
)

gtest_discover_tests(event_log_reader_test)
