#include <string.h>

#include "unity.h"

#include "util.h"
#include "value.h"
#include "object.h"

void setUp(void)
{
}

void tearDown(void)
{
}

void test_nil_equal(void)
{
    TEST_ASSERT(value_equal(NIL_VAL, NIL_VAL));
}

void test_bool_equal(void)
{
    TEST_ASSERT(value_equal(BOOL_VAL(true), BOOL_VAL(true)));
    TEST_ASSERT(value_equal(BOOL_VAL(false), BOOL_VAL(false)));
    TEST_ASSERT_FALSE(value_equal(BOOL_VAL(true), BOOL_VAL(false)));
}

void test_different_types_equal(void)
{
    TEST_ASSERT_FALSE(value_equal(BOOL_VAL(true), NIL_VAL));
}

#define FMT_BUFFER_NUM_CHARS 64
char buf[FMT_BUFFER_NUM_CHARS];
void test_format_bool(void)
{
    int len = value_format(buf, sizeof(buf), BOOL_VAL(false));
    TEST_ASSERT_EQUAL(5, len);
    TEST_ASSERT_EQUAL_STRING("false", buf);

    len = value_format(buf, sizeof(buf), BOOL_VAL(true));
    TEST_ASSERT_EQUAL(4, len);
    TEST_ASSERT_EQUAL_STRING("true", buf);
}

void test_format_nil(void)
{
    int len = value_format(buf, sizeof(buf), NIL_VAL);
    TEST_ASSERT_EQUAL(3, len);
    TEST_ASSERT_EQUAL_STRING("nil", buf);
}

void test_format_number(void)
{
    int len = value_format(buf, sizeof(buf), NUMBER_VAL(1.2345));
    TEST_ASSERT_EQUAL(6, len);
    TEST_ASSERT_EQUAL_STRING("1.2345", buf);

    len = value_format(buf, sizeof(buf), NUMBER_VAL(0.0));
    TEST_ASSERT_EQUAL(1, len);
    TEST_ASSERT_EQUAL_STRING("0", buf);

    len = value_format(buf, sizeof(buf), NUMBER_VAL(-123.456e-7));
    TEST_ASSERT_EQUAL(12, len);
    TEST_ASSERT_EQUAL_STRING("-1.23456e-05", buf);
}

void test_format_object(void)
{
    // Objects do their own formatting that is validated separately.
    // Here we just verify *something* gets returned that looks valid.
    struct object o = {
        .marked = false,
        .next = NULL,
        .type = OBJECT_STRING,
    };
    struct object_string s = {
        .data = "Hello, world!",
        .hash = 123,
        .length = strlen("Hello, world!"),
        .object = {
                   .marked = false,
                   .next = NULL,
                   .type = OBJECT_STRING,
                   }
    };

    int len = value_format(buf, sizeof(buf), OBJECT_VAL(&o));
    TEST_ASSERT(len > 0);
    TEST_ASSERT(strlen(buf) > 0);
}

void test_format_unknown(void)
{
    value v = {.type = 123, .as.object = NULL};
    int len = value_format(buf, sizeof(buf), v);
    TEST_ASSERT_EQUAL(22, len);
    TEST_ASSERT_EQUAL_STRING("unrecognized type: 123", buf);
}

void test_format_empty(void)
{
    int len = value_format(buf, sizeof(buf), EMPTY_VAL);
    TEST_ASSERT_EQUAL(7, len);
    TEST_ASSERT_EQUAL_STRING("<empty>", buf);
}

void test_array_null_fails(void)
{
    TEST_ASSERT(value_array_init(NULL) < 0);
}

void test_array_basic(void)
{
    struct value_array varray;
    TEST_ASSERT_EQUAL(0, value_array_init(&varray));

    TEST_ASSERT_NOT_NULL(varray.values);
    TEST_ASSERT(varray.capacity > 0);

    value_array_write(&varray, NUMBER_VAL(12345));

    TEST_ASSERT_EQUAL(1, varray.count);

    TEST_ASSERT_EQUAL(0, value_array_free(&varray));

    TEST_ASSERT_EQUAL(0, varray.count);
    TEST_ASSERT_EQUAL(0, varray.capacity);
    TEST_ASSERT_NULL(varray.values);
}

void test_array_grow(void)
{
    struct value_array varray;
    value_array_init(&varray);

    size_t initial_capacity = varray.capacity;

    for (int i = 0; i < initial_capacity + 1; i++) { value_array_write(&varray, NUMBER_VAL(i)); }

    TEST_ASSERT_EQUAL(initial_capacity + 1, varray.count);
    TEST_ASSERT_EQUAL(2 * initial_capacity, varray.capacity);
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_nil_equal);
    RUN_TEST(test_different_types_equal);
    RUN_TEST(test_bool_equal);

    RUN_TEST(test_format_bool);
    RUN_TEST(test_format_nil);
    RUN_TEST(test_format_number);
    RUN_TEST(test_format_object);
    RUN_TEST(test_format_empty);
    RUN_TEST(test_format_unknown);

    RUN_TEST(test_array_null_fails);
    RUN_TEST(test_array_basic);
    RUN_TEST(test_array_grow);

    return UNITY_END();
}
