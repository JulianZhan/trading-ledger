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
