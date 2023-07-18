#include "unity.h"
#include "hash.h"
#include "object.h"

void setUp(void)
{
}

void tearDown(void)
{
}

void test_string_empty(void)
{
    TEST_ASSERT_EQUAL(0x811c9dc5, hash_string("", 0));
}

void test_string_hello(void)
{
    TEST_ASSERT_EQUAL(0xed90f094, hash_string("Hello, world!", 13));
}

void test_double(void)
{
    TEST_ASSERT_EQUAL(0xadf048f2, hash_double(1234.5678));
    TEST_ASSERT_EQUAL(0xded13808, hash_double(5678.4321));
}

void test_value_number(void)
{
    TEST_ASSERT_EQUAL(hash_double(1234.5678), hash_value(NUMBER_VAL(1234.5678)));
}

void test_bool_false(void)
{
    TEST_ASSERT_EQUAL(5, hash_value(BOOL_VAL(false)));
}

void test_bool_true(void)
{
    TEST_ASSERT_EQUAL(3, hash_value(BOOL_VAL(true)));
}

void test_empty(void)
{
    TEST_ASSERT_EQUAL(7, hash_value(NIL_VAL));
}

void test_invalid(void)
{
    value v = {
        .type = 111,
        .as.number = 0,
    };

    TEST_ASSERT_EQUAL(-1U, hash_value(v));
}

void test_object(void)
{
    struct object obj = {.type = OBJECT_CLASS, .next = NULL, .marked = false};
    value v = OBJECT_VAL(&obj);

    TEST_ASSERT_EQUAL((hash_t)(uintptr_t)(void *)&obj, hash_value(v));
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_string_empty);
    RUN_TEST(test_string_hello);
    RUN_TEST(test_double);
    RUN_TEST(test_value_number);
    RUN_TEST(test_bool_false);
    RUN_TEST(test_bool_true);
    RUN_TEST(test_empty);
    RUN_TEST(test_invalid);
    RUN_TEST(test_object);

    return UNITY_END();
}
