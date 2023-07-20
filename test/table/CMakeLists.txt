add_executable(table_utest
    test_table.c
)

target_link_libraries(table_utest
    unity
    dplanglib
)

add_test(table table_utest)
