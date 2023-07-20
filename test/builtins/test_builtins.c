#include <string.h>

#include "builtins.h"

#include "object.h"
#include "value.h"

#include "unity.h"

static native_function native_abs;
static native_function native_clock;
static native_function native_max;
static native_function native_min;
static native_function native_round;
static native_function native_sqrt;
static native_function native_sum;
static native_function native_table;

static native_function get_native_function(const char *name)
{
    for (struct builtin_function_info *info = builtins; info->name != NULL; info++) {
        if (strcmp(name, info->name) == 0) {
            return info->function;
        }
    }
    return NULL;
}

void setUp(void)
{
}

void tearDown(void)
{
}

void test_abs(void)
{
    native_function builtin = get_native_function("abs");

    TEST_ASSERT_NOT_NULL(builtin);

    value args1[] = {NUMBER_VAL(-1)};
    value v1 = builtin(1, args1);
    TEST_ASSERT_TRUE(IS_NUMBER(v1));

    TEST_ASSERT_EQUAL(1, AS_NUMBER(v1));

    value args2[] = {NUMBER_VAL(321)};
    value v2 = builtin(321, args2);
    TEST_ASSERT_TRUE(IS_NUMBER(v2));
    TEST_ASSERT_EQUAL(321, AS_NUMBER(v2));
}

void test_clock(void)
{
    native_function builtin = get_native_function("clock");

    TEST_ASSERT_NOT_NULL(builtin);

    value v1 = builtin(0, NULL);
    TEST_ASSERT_TRUE(IS_NUMBER(v1));
    TEST_ASSERT_GREATER_THAN_DOUBLE(0, AS_NUMBER(v1));

    value v2 = builtin(0, NULL);
    TEST_ASSERT_GREATER_THAN_DOUBLE(AS_NUMBER(v1), AS_NUMBER(v2));
}

void test_max(void)
{
    native_function builtin = get_native_function("max");

    TEST_ASSERT_NOT_NULL(builtin);

    value args[] = {
        NUMBER_VAL(123),
        NUMBER_VAL(321),
        NUMBER_VAL(-456),
    };

    value v = builtin(3, args);
    TEST_ASSERT_EQUAL(321, AS_NUMBER(v));
}

void test_min(void)
{
    native_function builtin = get_native_function("min");

    TEST_ASSERT_NOT_NULL(builtin);

    value args[] = {
        NUMBER_VAL(123),
        NUMBER_VAL(321),
        NUMBER_VAL(-456),
    };

    value v = builtin(3, args);
    TEST_ASSERT_EQUAL(-456, AS_NUMBER(v));
}

void test_round(void)
{
    native_function builtin = get_native_function("round");

    TEST_ASSERT_NOT_NULL(builtin);

    value args1[] = {
        NUMBER_VAL(123.45678),
    };

    value v1 = builtin(1, args1);
    TEST_ASSERT_EQUAL(123, AS_NUMBER(v1));

    value args2[] = {
        NUMBER_VAL(876.54321),
    };

    value v2 = builtin(1, args2);
    TEST_ASSERT_EQUAL(877, AS_NUMBER(v2));
}

void test_sqrt(void)
{
    native_function builtin = get_native_function("sqrt");

    TEST_ASSERT_NOT_NULL(builtin);

    value args1[] = {
        NUMBER_VAL(10000),
    };

    value v1 = builtin(1, args1);
    TEST_ASSERT_EQUAL(100, AS_NUMBER(v1));
}

void test_sum(void)
{
    native_function builtin = get_native_function("sum");

    TEST_ASSERT_NOT_NULL(builtin);

    value v1 = builtin(0, NULL);
    TEST_ASSERT_EQUAL(0, AS_NUMBER(v1));

    value args2[] = {
        NUMBER_VAL(-100),
        NUMBER_VAL(0),
        NUMBER_VAL(123456),
        NUMBER_VAL(100),
    };

    value v2 = builtin(4, args2);
    TEST_ASSERT_EQUAL(123456, AS_NUMBER(v2));
}

void test_table(void)
{
    native_function builtin = get_native_function("table");

    TEST_ASSERT_NOT_NULL(builtin);

    value v1 = builtin(0, NULL);

    TEST_ASSERT_TRUE(is_object_type(v1, OBJECT_TABLE));
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_abs);
    RUN_TEST(test_clock);
    RUN_TEST(test_max);
    RUN_TEST(test_min);
    RUN_TEST(test_round);
    RUN_TEST(test_sqrt);
    RUN_TEST(test_sum);
    RUN_TEST(test_table);

    return UNITY_END();
}
