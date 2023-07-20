add_executable(hash_utest
    test_hash.c
)

target_link_libraries(hash_utest
    dplanglib
    unity
)

add_test(hash hash_utest)
