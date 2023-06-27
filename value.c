#include "value.h"
#include "memory.h"
#include "object.h"
#include <stdint.h>
#include <stdio.h>

#define MIN_VARRAY_SIZE      1
#define VARRAY_GROWTH_FACTOR 2

int value_array_init(struct value_array *varray)
{
    varray->count = 0;
    varray->capacity = MIN_VARRAY_SIZE;
    varray->values = (value *)reallocate(NULL, 0, MIN_VARRAY_SIZE);
    return 0;
}

int value_array_write(struct value_array *value_array, value val)
{
    if (value_array->capacity < value_array->count + 1) {
        size_t prev_cap = value_array->capacity;
        value_array->capacity *= VARRAY_GROWTH_FACTOR;
        value_array->values = (value *)reallocate(value_array->values, prev_cap * sizeof(value_array->values[0]),
                                                  value_array->capacity * sizeof(value_array->values[0]));
    }

    value_array->values[value_array->count++] = val;

    return 0;
}

int value_array_free(struct value_array *value_array)
{
    value_array->values =
        (value *)reallocate(value_array->values, value_array->capacity * sizeof(value_array->values[0]), 0);
    value_array->capacity = value_array->count = 0;
    return 0;
}

int value_print(value val)
{
    switch (val.type) {
        case VAL_BOOL:
            printf(AS_BOOL(val) ? "true" : "false");
            break;
        case VAL_NIL:
            printf("nil");
            break;
        case VAL_NUMBER:
            printf("%g", AS_NUMBER(val));
            break;
        case VAL_OBJECT:
            object_print(AS_OBJECT(val));
            break;
        default:
            printf("unrecognized type: %d\n", val.type);
    }

    return 0;
}

bool value_equal(value a, value b)
{
    if (a.type != b.type)
        return false;
    switch (a.type) {
        case VAL_BOOL:
            return AS_BOOL(a) == AS_BOOL(b);
        case VAL_NIL:
            return true;
        case VAL_NUMBER:
            return AS_NUMBER(a) == AS_NUMBER(b);
        case VAL_OBJECT:
            return AS_OBJECT(a) == AS_OBJECT(b);
        default:
            return false;
    }
}
