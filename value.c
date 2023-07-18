#include "value.h"
#include "memory.h"
#include "object.h"
#include "util.h"

#include <math.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#define MIN_VARRAY_SIZE      2
#define VARRAY_GROWTH_FACTOR 2

int value_array_init(struct value_array *varray)
{
    if (unlikely(varray == NULL)) {
        return -1;
    }
    varray->count = 0;
    varray->capacity = MIN_VARRAY_SIZE;
    varray->values = (value *)reallocate(NULL, 0, MIN_VARRAY_SIZE * sizeof(value));
    // TODO: simulate memory allocation failure
    if (unlikely(varray->values == NULL)) {
        return -1;
    }

    return 0;
}

int value_array_write(struct value_array *varray, value val)
{
    if (unlikely(varray == NULL)) {
        return -1;
    }

    if (varray->capacity < varray->count + 1) {
        int prev_cap = varray->capacity;
        varray->capacity = (prev_cap < MIN_VARRAY_SIZE) ? MIN_VARRAY_SIZE : (VARRAY_GROWTH_FACTOR * prev_cap);
        varray->values =
            (value *)reallocate(varray->values, prev_cap * sizeof(value), varray->capacity * sizeof(value));
        if (unlikely(varray->values == NULL)) {
            // TODO: There's a memory leak here if values has already been allocated and reallocation fails
            return -1;
        }
    }

    varray->values[varray->count] = val;
    varray->count++;

    return 0;
}

int value_array_free(struct value_array *varray)
{
    if (unlikely(varray == NULL)) {
        return -1;
    }
    varray->values = (value *)reallocate(varray->values, varray->capacity * sizeof(value), 0);
    varray->capacity = varray->count = 0;
    return 0;
}

#define VALUE_FORMAT_MAX_CHARS 64
int value_format(char *s, size_t maxlen, value val)
{
    // Some of these cases use "%s" as a format string even though it
    // isn't strictly necessary.  This ensures that any % or other special
    // characters in a user-controlled string aren't interpretted as
    // format specifiers.

    switch (val.type) {
        case VAL_BOOL:
            return snprintf(s, maxlen, "%s", AS_BOOL(val) ? "true" : "false");
        case VAL_NIL:
            return snprintf(s, maxlen, "%s", "nil");
        case VAL_NUMBER: {
            double intpart;
            if (modf(AS_NUMBER(val), &intpart) == 0) {
                return snprintf(s, maxlen, "%d", (int)intpart);
            } else {
                return snprintf(s, maxlen, "%g", AS_NUMBER(val));
            }
        }
        case VAL_OBJECT:
            return object_format(s, maxlen, AS_OBJECT(val));
        case VAL_EMPTY:
            return snprintf(s, maxlen, "%s", "<empty>");
        default:
            return snprintf(s, maxlen, "unrecognized type: %d", val.type);
    }
}

int value_print(value val)
{
    char buf[VALUE_FORMAT_MAX_CHARS];
    // TODO: check for format string overflow
    value_format(buf, sizeof(buf), val);
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
