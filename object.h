#ifndef DPLANG_OBJECT_H
#define DPLANG_OBJECT_H

#include "chunk.h"
#include "value.h"
#include "hash.h"
#include <stdint.h>
#include <stddef.h>

enum object_type {
    OBJECT_STRING,
    OBJECT_FUNCTION,
};

struct object {
    enum object_type type;
};

struct object_function {
    struct object object;
    int arity;
    struct chunk chunk;
    struct object_string *name;
};

struct object_string {
    struct object object;
    int length;
    char *data;
    hash_t hash;
};

struct object_function *object_function_new();

#define OBJECT_TYPE(value) (AS_OBJECT(value)->type)

static inline bool is_object_type(value val, enum object_type type)
{
    return IS_OBJECT(val) && AS_OBJECT(val)->type == type;
}

#define IS_STRING(val)   is_object_type(val, OBJECT_STRING)
#define IS_FUNCTION(val) is_object_type(val, OBJECT_FUNCTION)

#define AS_STRING(val)   ((struct object_string *)AS_OBJECT(val))
#define AS_FUNCTION(val) ((struct object_function *)AS_OBJECT(val))
#define AS_CSTRING(val)  (((struct object_string *)AS_OBJECT(val))->data)

struct object_string *object_string_allocate(const char *s, size_t length);
struct object_string *object_string_take(const char *s, size_t length);

int object_print(struct object *obj);
bool object_equal(struct object *a, struct object *b);

#endif
