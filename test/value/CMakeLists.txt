add_executable(value_utest
    test_value.c
)

target_link_libraries(value_utest
    Unity
    dplanglib
)

add_test(value value_utest)
