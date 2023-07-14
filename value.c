#include "value.h"
#include "memory.h"
#include "object.h"
#include <math.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#define MIN_VARRAY_SIZE      2
#define VARRAY_GROWTH_FACTOR 2

int value_array_init(struct value_array *varray)
{
    varray->count = 0;
    varray->capacity = 0;
    varray->values = NULL;
    return 0;
}

int value_array_write(struct value_array *varray, value val)
{
    if (varray->capacity < varray->count + 1) {
        int prev_cap = varray->capacity;
        varray->capacity = (prev_cap < MIN_VARRAY_SIZE) ? MIN_VARRAY_SIZE : (VARRAY_GROWTH_FACTOR * prev_cap);
        varray->values =
            (value *)reallocate(varray->values, prev_cap * sizeof(value), varray->capacity * sizeof(value));
    }

    varray->values[varray->count] = val;
    varray->count++;

    return 0;
}

int value_array_free(struct value_array *varray)
{
    varray->values = (value *)reallocate(varray->values, varray->capacity * sizeof(value), 0);
    varray->capacity = varray->count = 0;
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
        case VAL_NUMBER: {
            double intpart;
            if (modf(AS_NUMBER(val), &intpart) == 0) {
                printf("%d", (int)intpart);
            } else {
                printf("%g", AS_NUMBER(val));
            }
            break;
        }
        case VAL_OBJECT:
            object_print(AS_OBJECT(val));
            break;
        case VAL_EMPTY:
            printf("<empty");
            break;
        default:
            printf("unrecognized type: %d\n", val.type);
    }

    return 0;
}

bool value_equal(value a, value b)
{
    if (a.type != b.type) {
        return false;
    }
    switch (a.type) {
        case VAL_BOOL:
            return AS_BOOL(a) == AS_BOOL(b);
        case VAL_NIL:
            return true;
        case VAL_NUMBER:
            return AS_NUMBER(a) == AS_NUMBER(b);
        case VAL_OBJECT:
            return object_equal(AS_OBJECT(a), AS_OBJECT(b));
        case VAL_EMPTY:
            return true;
        default:
            return false;
    }
}
