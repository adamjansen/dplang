#include "unity.h"

#include "util.h"

void setUp(void)
{
}

void tearDown(void)
{
}

void test_U16LSB(void)
{
    TEST_ASSERT_EQUAL(123, U16LSB(0xaf7b));
    TEST_ASSERT_EQUAL(0, U16LSB(0xFF00));
    TEST_ASSERT_EQUAL(0xFF, U16LSB(0x00FF));
}

void test_U16MSB(void)
{
    TEST_ASSERT_EQUAL(123, U16MSB(0x7baf));
    TEST_ASSERT_EQUAL(0, U16MSB(0x00FF));
    TEST_ASSERT_EQUAL(0xFF, U16MSB(0xFF00));
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_U16LSB);
    RUN_TEST(test_U16MSB);

    return UNITY_END();
}
