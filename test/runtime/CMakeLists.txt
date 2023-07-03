add_executable(runtime_utest
    test_runtime.c
)

target_link_libraries(runtime_utest
    Unity
)

add_test(dplang runtime_utest)
