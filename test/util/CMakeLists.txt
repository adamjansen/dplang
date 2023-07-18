add_executable(util_utest
    test_util.c
)

target_link_libraries(util_utest
    Unity
    dplanglib
)

add_test(util util_utest)
