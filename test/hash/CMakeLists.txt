add_executable(hash_utest
    test_hash.c
    ../../hash.c
)

target_link_libraries(hash_utest
    Unity
)

add_test(dplang hash_utest)
