#ifndef DPLANG_OBJECT_H
#define DPLANG_OBJECT_H

#include "chunk.h"
#include "value.h"
#include "hash.h"
#include <stdint.h>
#include <stddef.h>

typedef value (*native_function)(int arg_count, value *args);

enum object_type {
    OBJECT_CLOSURE,
    OBJECT_FUNCTION,
    OBJECT_NATIVE,
    OBJECT_STRING,
    OBJECT_UPVALUE,
};

struct object {
    enum object_type type;
};

struct object_function {
    struct object object;
    int arity;
    int nupvalues;
    struct chunk chunk;
    struct object_string *name;
};

struct object_native {
    struct object object;
    native_function function;
};

struct object_string {
    struct object object;
    int length;
    char *data;
    hash_t hash;
};

struct object_upvalue {
    struct object object;
    value *location;
    value closed;
    struct object_upvalue *next;
};

struct object_closure {
    struct object object;
    struct object_function *function;
    struct object_upvalue **upvalues;
    int nupvalues;
};

struct object_native *object_native_new(native_function function);
struct object_function *object_function_new();
struct object_closure *object_closure_new(struct object_function *function);
struct object_upvalue *object_upvalue_new(value *slot);

#define OBJECT_TYPE(value) (AS_OBJECT(value)->type)

static inline bool is_object_type(value val, enum object_type type)
{
    return IS_OBJECT(val) && AS_OBJECT(val)->type == type;
}

#define IS_STRING(val)   is_object_type(val, OBJECT_STRING)
#define IS_FUNCTION(val) is_object_type(val, OBJECT_FUNCTION)
#define IS_NATIVE(val)   is_object_type(val, OBJECT_NATIVE)
#define IS_CLOSURE(val)  is_object_type(val, OBJECT_CLOSURE)

#define AS_STRING(val)   ((struct object_string *)AS_OBJECT(val))
#define AS_FUNCTION(val) ((struct object_function *)AS_OBJECT(val))
#define AS_CLOSURE(val)  ((struct object_closure *)AS_OBJECT(val))
#define AS_CSTRING(val)  (((struct object_string *)AS_OBJECT(val))->data)
#define AS_NATIVE(val)   (((struct object_native *)AS_OBJECT(val))->function)

struct object_string *object_string_allocate(const char *s, size_t length);
struct object_string *object_string_take(const char *s, size_t length);

int object_print(struct object *obj);
bool object_equal(struct object *a, struct object *b);

#endif
