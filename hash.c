#include "hash.h"
#include "object.h"

#define FNV_OFFSET_BASIS 0x811c9dc5
#define FNV_PRIME        0x01000193

// FNV1A hash
// See http://www.isthe.com/chongo/src/fnv/hash_32.c
//
//
//
// TODO: replace with SipHash?
hash_t hash_string(const char *s, size_t length)
{
    hash_t hash = FNV_OFFSET_BASIS;
    for (int i = 0; i < (int)length; i++) {
        hash ^= (uint8_t)s[i];
        hash *= FNV_PRIME;
    }

    return hash;
}

hash_t hash_double(double d)
{
    union bit_cast {
        double v;
        uint32_t ints[2];
    };
    union bit_cast cast;
    cast.v = d + 1.0;
    return cast.ints[0] + cast.ints[1];
}

static hash_t hash_object(struct object *obj)
{
    switch (obj->type) {
        case OBJECT_STRING:
            return ((struct object_string *)obj)->hash;
        case OBJECT_BOUND_METHOD:
        case OBJECT_CLASS:
        case OBJECT_CLOSURE:
        case OBJECT_FUNCTION:
        case OBJECT_INSTANCE:
        case OBJECT_NATIVE:
        case OBJECT_TABLE:
        case OBJECT_UPVALUE:
        default:
            return (hash_t)(uintptr_t)obj;
    }
}

// NOLINTBEGIN(readability-magic-numbers)
hash_t hash_value(value v)
{
    switch (v.type) {
        case VAL_BOOL:
            return AS_BOOL(v) ? 3 : 5;
        case VAL_NIL:
            return 7;
        case VAL_NUMBER:
            return hash_double(AS_NUMBER(v));
        case VAL_OBJECT:
            return hash_object(v.as.object);
        case VAL_EMPTY:
            return 0;
        default:
            return -1U;
    }
}
// NOLINTEND(readability-magic-numbers)
