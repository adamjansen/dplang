#include "unity.h"

void setUp(void)
{
}

void tearDown(void)
{
}

void test_basic(void)
{
    TEST_ASSERT(1);
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_basic);

    return UNITY_END();
}
