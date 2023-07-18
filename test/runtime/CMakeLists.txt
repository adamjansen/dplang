add_executable(runtime_utest
    test_runtime.c
)

target_link_libraries(runtime_utest
    Unity
    dplanglib
)

add_test(runtime runtime_utest)
