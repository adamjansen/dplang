add_executable(scanner_utest
    test_scanner.c
)

target_link_libraries(scanner_utest
    Unity
    dplanglib
)

add_test(scanner scanner_utest)
