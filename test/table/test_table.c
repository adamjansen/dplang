#include "unity.h"

#include "object.h"
#include "table.h"
#include "value.h"

void setUp(void)
{
}

void tearDown(void)
{
}

void test_table_init(void)
{
    struct table t;

    TEST_ASSERT_EQUAL(0, table_init(&t));
    TEST_ASSERT_EQUAL(0, t.count);
    TEST_ASSERT_EQUAL(0, t.capacity);
    TEST_ASSERT_NULL(t.entries);
}

void test_table_init_null(void)
{
    TEST_ASSERT_EQUAL(-1, table_init(NULL));
}

void test_table_free(void)
{
    // We don't really want to pass non-NULL entries, because this
    // isn't a real table
    struct table t = {.capacity = 123, .count = 0, .entries = NULL};

    table_free(&t);
    TEST_ASSERT_EQUAL(0, t.count);
    TEST_ASSERT_EQUAL(0, t.capacity);
    TEST_ASSERT_NULL(t.entries);
}

void test_table_free_null(void)
{
    table_free(NULL);
}

void test_table_set(void)
{
    struct table t;
    table_init(&t);
    TEST_ASSERT_TRUE(table_set(&t, NUMBER_VAL(123), NUMBER_VAL(8675309)));

    TEST_ASSERT_EQUAL(1, t.count);
    TEST_ASSERT_GREATER_THAN(0, t.capacity);
    TEST_ASSERT_NOT_NULL(t.entries);

    table_free(&t);
}

void test_table_set_null(void)
{
    TEST_ASSERT_FALSE(table_set(NULL, NUMBER_VAL(0), NUMBER_VAL(1)));
}

void test_table_set_exists(void)
{
    struct table t;
    table_init(&t);

    table_set(&t, NUMBER_VAL(123), NUMBER_VAL(8675309));
    TEST_ASSERT_EQUAL(1, t.count);

    TEST_ASSERT_FALSE(table_set(&t, NUMBER_VAL(123), NUMBER_VAL(0xDEADBEEF)));
    TEST_ASSERT_EQUAL(1, t.count);

    table_free(&t);
}

void test_table_get(void)
{
    struct table t;
    table_init(&t);

    table_set(&t, NUMBER_VAL(123), NUMBER_VAL(8675309));

    value val;
    TEST_ASSERT_TRUE(table_get(&t, NUMBER_VAL(123), &val));
    TEST_ASSERT_EQUAL(8675309, val.as.number);

    table_free(&t);
}

void test_table_get_no_such_key(void)
{
    struct table t;
    table_init(&t);

    table_set(&t, NUMBER_VAL(1), NUMBER_VAL(1));
    value val;
    TEST_ASSERT_FALSE(table_get(&t, NUMBER_VAL(999), &val));
    table_free(&t);
}

void test_table_get_null(void)
{
    value val;
    TEST_ASSERT_FALSE(table_get(NULL, NUMBER_VAL(1), &val));
    struct table t;
    table_init(&t);
    TEST_ASSERT_FALSE(table_get(&t, NUMBER_VAL(1), NULL));
}

void test_table_get_empty(void)
{
    struct table t;
    table_init(&t);

    value val = {.as.number = 0, .type = 0};
    TEST_ASSERT_FALSE(table_get(&t, NUMBER_VAL(123), &val));
    TEST_ASSERT_EQUAL(0, val.type);
    TEST_ASSERT_EQUAL(0, val.as.number);

    table_free(&t);
}

void test_table_get_deleted(void)
{
    struct table t;
    table_init(&t);

    table_set(&t, NUMBER_VAL(1), NUMBER_VAL(123456789));
    table_delete(&t, NUMBER_VAL(1));
    TEST_ASSERT_EQUAL(0, t.count);

    value val = NUMBER_VAL(0);
    TEST_ASSERT_FALSE(table_get(&t, NUMBER_VAL(1), &val));
    TEST_ASSERT_EQUAL(0, val.as.number);
    table_free(&t);
}

void test_table_reinsert(void)
{
    struct table t;
    table_init(&t);

    value val = NUMBER_VAL(12345678);
    table_set(&t, NUMBER_VAL(1), val);
    TEST_ASSERT_TRUE(table_delete(&t, NUMBER_VAL(1)));
    TEST_ASSERT_EQUAL(0, t.count);

    TEST_ASSERT_TRUE(table_set(&t, NUMBER_VAL(1), val));
    TEST_ASSERT_EQUAL(1, t.count);
    table_free(&t);
}

void test_table_delete_null(void)
{
    TEST_ASSERT_FALSE(table_delete(NULL, NUMBER_VAL(0)));
}

void test_table_delete_from_empty(void)
{
    struct table t;
    table_init(&t);
    TEST_ASSERT_FALSE(table_delete(&t, NUMBER_VAL(0)));
}

void test_table_delete_no_such_key(void)
{
    struct table t;
    table_init(&t);
    table_set(&t, NUMBER_VAL(1), NUMBER_VAL(1));
    TEST_ASSERT_FALSE(table_delete(&t, NUMBER_VAL(0)));
    table_free(&t);
}

void test_table_grow(void)
{
    struct table t;
    table_init(&t);
    for (int i = 0; i < 100; i++) { table_set(&t, NUMBER_VAL(i), NUMBER_VAL(i)); }
    table_free(&t);
}

void test_table_copy(void)
{
    struct table t1;
    table_init(&t1);
    for (int i = 0; i < 64; i++) { table_set(&t1, NUMBER_VAL(i), NUMBER_VAL(i)); }

    struct table t2;
    table_init(&t2);
    table_add_all(&t1, &t2);

    TEST_ASSERT_EQUAL(t1.count, t2.count);
    for (int i = 0; i < 64; i++) {
        value v2;
        table_get(&t2, NUMBER_VAL(i), &v2);
        TEST_ASSERT_EQUAL(i, AS_NUMBER(v2));
    }

    table_free(&t1);
    table_free(&t2);
}

void test_table_copy_from_null(void)
{
    struct table t2;
    table_add_all(NULL, &t2);
}

void test_table_copy_to_null(void)
{
    struct table t1;
    table_add_all(&t1, NULL);
}

void test_table_copy_ident(void)
{
    struct table t1;

    table_add_all(&t1, &t1);
}

void test_table_find_string(void)
{
    char *key = "key";
    struct object_string obj_key = {
        .data = key,
        .hash = 0x12345678,
        .length = 3,
        .object = {.marked = false, .next = false, .type = OBJECT_STRING},
    };
    struct table t;
    table_init(&t);
    table_set(&t, OBJECT_VAL(&obj_key), BOOL_VAL(true));

    struct object_string *s = table_find_string(&t, key, 3, 0x12345678);
    TEST_ASSERT_EQUAL_PTR(&obj_key, s);

    table_free(&t);
}

void test_table_find_string_many(void)
{
    char *key = "key";
    struct object_string obj_key = {
        .data = key,
        .hash = 0x12345678,
        .length = 3,
        .object = {.marked = false, .next = false, .type = OBJECT_STRING},
    };
    char *abc = "abc";
    struct object_string obj_abc = {
        .data = abc,
        .hash = 0x87654321,
        .length = 3,
        .object = {.marked = false, .next = false, .type = OBJECT_STRING},
    };

    struct object obj_plain = {
        .marked = false,
        .next = NULL,
        .type = -1,
    };
    struct table t;
    table_init(&t);
    for (int i = 0; i < 16; i++) { table_set(&t, NUMBER_VAL(i), BOOL_VAL(true)); }

    table_set(&t, OBJECT_VAL(&obj_abc), BOOL_VAL(true));
    table_set(&t, OBJECT_VAL(&obj_key), BOOL_VAL(true));
    table_set(&t, OBJECT_VAL(&obj_plain), BOOL_VAL(true));

    struct object_string *s = table_find_string(&t, key, 3, 0x12345678);
    TEST_ASSERT_EQUAL_PTR(&obj_key, s);

    table_free(&t);
}

void test_table_find_string_no_such_key(void)
{
    char *key = "key";
    struct object_string obj_key = {
        .data = key,
        .hash = 0x12345678,
        .length = 3,
        .object = {.marked = false, .next = false, .type = OBJECT_STRING},
    };
    struct table t;
    table_init(&t);

    table_set(&t, OBJECT_VAL(&obj_key), BOOL_VAL(true));

    struct object_string *s = table_find_string(&t, "KEY", 3, 0x12345678);
    TEST_ASSERT_NULL(s);

    s = table_find_string(&t, "abc123", 6, 0x12345678);
    TEST_ASSERT_NULL(s);

    table_free(&t);
}

void test_table_find_string_null(void)
{
    TEST_ASSERT_NULL(table_find_string(NULL, NULL, 0, 0));
}

void test_table_find_string_empty(void)
{
    struct table t;
    table_init(&t);
    TEST_ASSERT_NULL(table_find_string(&t, NULL, 0, 0));
    table_free(&t);
}

void test_table_find_string_removed(void)
{
    char *key = "key";
    struct object_string obj_key = {
        .data = key,
        .hash = 0x12345678,
        .length = 3,
        .object = {.marked = false, .next = false, .type = OBJECT_STRING},
    };
    struct table t;
    table_init(&t);
    table_set(&t, OBJECT_VAL(&obj_key), BOOL_VAL(true));
    table_delete(&t, OBJECT_VAL(&obj_key));

    struct object_string *s = table_find_string(&t, key, 3, 0x12345678);
    TEST_ASSERT_NULL(s);

    table_free(&t);
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_table_init);
    RUN_TEST(test_table_init_null);
    RUN_TEST(test_table_free);
    RUN_TEST(test_table_free_null);
    RUN_TEST(test_table_set);
    RUN_TEST(test_table_set_null);
    RUN_TEST(test_table_set_exists);
    RUN_TEST(test_table_reinsert);
    RUN_TEST(test_table_get);
    RUN_TEST(test_table_get_null);
    RUN_TEST(test_table_get_no_such_key);
    RUN_TEST(test_table_delete_null);
    RUN_TEST(test_table_delete_from_empty);
    RUN_TEST(test_table_delete_no_such_key);
    RUN_TEST(test_table_get_deleted);
    RUN_TEST(test_table_get_empty);
    RUN_TEST(test_table_grow);
    RUN_TEST(test_table_copy);
    RUN_TEST(test_table_copy_from_null);
    RUN_TEST(test_table_copy_to_null);
    RUN_TEST(test_table_copy_ident);
    RUN_TEST(test_table_find_string);
    RUN_TEST(test_table_find_string_many);
    RUN_TEST(test_table_find_string_null);
    RUN_TEST(test_table_find_string_empty);
    RUN_TEST(test_table_find_string_no_such_key);
    RUN_TEST(test_table_find_string_removed);

    return UNITY_END();
}
