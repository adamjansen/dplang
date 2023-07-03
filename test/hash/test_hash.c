#include "unity.h"
#include "hash.h"

void setUp(void)
{
}

void tearDown(void)
{
}

void test_empty(void)
{
    TEST_ASSERT_EQUAL(hash_string("", 0), 0x811c9dc5);
}

void test_hello(void)
{
    TEST_ASSERT_EQUAL(hash_string("Hello, world!", 13), 0xed90f094);
}

    int main(void)
    {
        UNITY_BEGIN();

        RUN_TEST(test_empty);
        RUN_TEST(test_hello);

        return UNITY_END();
    }
