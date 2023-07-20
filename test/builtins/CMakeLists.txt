add_compile_definitions(UNITY_INCLUDE_DOUBLE)

add_executable(builtins_utest
    test_builtins.c
)

target_link_libraries(builtins_utest
    unity
    dplanglib
    m
)

add_test(builtins builtins_utest)
