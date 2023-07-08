#ifndef DPLANG_OBJECT_H
#define DPLANG_OBJECT_H

#include "chunk.h"
#include "value.h"
#include "hash.h"
#include "table.h"
#include <stdarg.h>
#include <stdint.h>
#include <stddef.h>

typedef value (*native_function)(int arg_count, value *args);

enum object_type {
    OBJECT_BOUND_METHOD,
    OBJECT_CLASS,
    OBJECT_CLOSURE,
    OBJECT_FUNCTION,
    OBJECT_INSTANCE,
    OBJECT_NATIVE,
    OBJECT_STRING,
    OBJECT_UPVALUE,
};

struct object {
    struct object *next;
    enum object_type type;
    bool marked;
};

struct object_class {
    struct object object;
    struct object_string *name;
    struct table methods;
};

struct object_instance {
    struct object object;
    struct object_class *klass;
    struct table fields;
};

struct object_bound_method {
    struct object object;
    value receiver;
    struct object_closure *method;
};

struct object_closure {
    struct object object;
    struct object_function *function;
    struct object_upvalue **upvalues;
    int nupvalues;
};

struct object_function {
    struct object object;
    struct object_string *name;
    struct chunk chunk;
    int arity;
    int nupvalues;
};

struct object_native {
    struct object object;
    native_function function;
};

struct object_string {
    struct object object;
    char *data;
    hash_t hash;
    size_t length;
};

struct object_upvalue {
    struct object object;
    value *location;
    value closed;
    struct object_upvalue *next;
};

struct object_bound_method *object_bound_method_new(value receiver, struct object_closure *method);
struct object_class *object_class_new(struct object_string *name);
struct object_closure *object_closure_new(struct object_function *function);
struct object_instance *object_instance_new(struct object_class *klass);
struct object_function *object_function_new(struct object_string *name);
struct object_native *object_native_new(native_function function);
struct object_upvalue *object_upvalue_new(value *slot);

#define OBJECT_TYPE(value) (AS_OBJECT(value)->type)

static inline bool is_object_type(value val, enum object_type type)
{
    return IS_OBJECT(val) && AS_OBJECT(val)->type == type;
}

#define IS_BOUND_METHOD(val) is_object_type(val, OBJECT_BOUND_METHOD)
#define IS_CLASS(val)        is_object_type(val, OBJECT_CLASS)
#define IS_CLOSURE(val)      is_object_type(val, OBJECT_CLOSURE)
#define IS_FUNCTION(val)     is_object_type(val, OBJECT_FUNCTION)
#define IS_INSTANCE(val)     is_object_type(val, OBJECT_INSTANCE)
#define IS_NATIVE(val)       is_object_type(val, OBJECT_NATIVE)
#define IS_STRING(val)       is_object_type(val, OBJECT_STRING)

#define AS_BOUND_METHOD(val) ((struct object_bound_method *)AS_OBJECT(val))
#define AS_CLASS(val)        ((struct object_class *)AS_OBJECT(val))
#define AS_CLOSURE(val)      ((struct object_closure *)AS_OBJECT(val))
#define AS_FUNCTION(val)     ((struct object_function *)AS_OBJECT(val))
#define AS_INSTANCE(val)     ((struct object_instance *)AS_OBJECT(val))
#define AS_CSTRING(val)      (((struct object_string *)AS_OBJECT(val))->data)
#define AS_NATIVE(val)       (((struct object_native *)AS_OBJECT(val))->function)
#define AS_STRING(val)       ((struct object_string *)AS_OBJECT(val))

struct object_string *object_string_allocate(const char *s, size_t length);
struct object_string *object_string_take(const char *s, size_t length);
struct object_string *object_string_format(const char *fmt, ...);
struct object_string *object_string_vformat(const char *fmt, va_list ap);

void object_free(struct object *object);

int object_print(struct object *obj);
bool object_equal(struct object *a, struct object *b);

void object_enable_gc(struct object *obj);
void object_disable_gc(struct object *obj);

#endif
