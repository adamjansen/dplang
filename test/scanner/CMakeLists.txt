add_executable(scanner_utest
    test_scanner.c
)

target_link_libraries(scanner_utest
    unity
    dplanglib
)

add_test(scanner scanner_utest)
