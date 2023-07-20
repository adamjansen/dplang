add_executable(runtime_utest
    test_runtime.c
)

target_link_libraries(runtime_utest
    unity
    dplanglib
)

add_test(runtime runtime_utest)
