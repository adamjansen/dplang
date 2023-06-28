#include <stdio.h>
#include <string.h>
#include "object.h"
#include "memory.h"

struct object_string *object_string_take(const char *s, size_t length)
{
    struct object_string *obj = (struct object_string *)reallocate(NULL, 0, sizeof(struct object_string));
    obj->length = length;
    obj->data = (char *)s;
    obj->object.type = OBJECT_STRING;
    obj->hash = hash_string(s, length);
    return obj;
}

struct object_string *object_string_allocate(const char *s, size_t length)
{
    char *data = (char *)reallocate(NULL, 0, length + 1);
    memcpy(data, s, length);
    data[length] = '\0';
    return object_string_take(data, length);
}

struct object_upvalue *object_upvalue_new(value *slot)
{
    struct object_upvalue *upvalue = reallocate(NULL, 0, sizeof(struct object_upvalue));
    upvalue->object.type = OBJECT_UPVALUE;
    upvalue->next = NULL;
    upvalue->closed = NIL_VAL;
    upvalue->location = slot;
    return upvalue;
}

struct object_function *object_function_new()
{
    struct object_function *func = reallocate(NULL, 0, sizeof(struct object_function));
    func->object.type = OBJECT_FUNCTION;
    func->arity = 0;
    func->nupvalues = 0;
    func->name = NULL;
    chunk_init(&func->chunk);
    return func;
}

struct object_closure *object_closure_new(struct object_function *function)
{
    struct object_upvalue **upvalues = reallocate(NULL, 0, function->nupvalues * sizeof(struct object_upvalue *));
    for (int i = 0; i < function->nupvalues; i++) { upvalues[i] = NULL; }
    struct object_closure *closure = reallocate(NULL, 0, sizeof(struct object_closure));

    closure->object.type = OBJECT_CLOSURE;
    closure->function = function;
    closure->upvalues = upvalues;
    closure->nupvalues = function->nupvalues;
    return closure;
}

struct object_native *object_native_new(native_function function)
{
    struct object_native *native = reallocate(NULL, 0, sizeof(struct object_native));
    native->function = function;
    native->object.type = OBJECT_NATIVE;
    return native;
}

static void function_print(struct object_function *function)
{
    printf("<%s%s>", (function->name == NULL) ? "script" : "fn ", (function->name == NULL) ? "" : function->name->data);
}

int object_print(struct object *obj)
{
    switch (obj->type) {
        case OBJECT_STRING: {
            struct object_string *s = (struct object_string *)obj;
            printf("%s", s->data);
            break;
        }
        case OBJECT_FUNCTION: {
            struct object_function *func = (struct object_function *)obj;
            function_print(func);
            break;
        }
        case OBJECT_CLOSURE: {
            struct object_closure *closure = (struct object_closure *)obj;
            function_print(closure->function);
            break;
        }
        case OBJECT_UPVALUE: {
            struct object_upvalue *upvalue = (struct object_upvalue *)obj;
            printf("upvalue");
            break;
        }
        case OBJECT_NATIVE: {
            printf("<native fn>");
            break;
        }
        default:
            printf("Unsupported object type");
            break;
    }
}

bool object_equal(struct object *a, struct object *b)
{
    if (a->type != b->type)
        return false;

    switch (a->type) {
        case OBJECT_STRING: {
            struct object_string *s1 = (struct object_string *)a;
            struct object_string *s2 = (struct object_string *)b;
            return (s1->length == s2->length) && memcmp(s1->data, s2->data, s1->length) == 0;

        }

        break;
        default:
            return false;
    }
}

void object_free(struct object *obj)
{
    switch (obj->type) {
        case OBJECT_FUNCTION: {
            struct object_function *func = (struct object_function *)obj;
            chunk_free(&func->chunk);
            reallocate(obj, sizeof(*func), 0);
            break;
        }
        case OBJECT_STRING: {
            struct object_string *str = (struct object_string *)obj;
            str->data = reallocate(str->data, str->length + 1, 0);
            reallocate(obj, sizeof(*str), 0);
            break;
        }
        case OBJECT_UPVALUE: {
            struct object_upvalue *upvalue = (struct object_upvalue *)obj;
            reallocate(obj, sizeof(*upvalue), 0);
            break;
        }
        case OBJECT_NATIVE: {
            struct object_native *native = (struct object_native *)obj;
            reallocate(obj, sizeof(*native), 0);
            break;
        }
        case OBJECT_CLOSURE: {
            struct object_closure *closure = (struct object_closure *)obj;
            closure->upvalues = reallocate(closure->upvalues, closure->nupvalues * sizeof(struct object_closure *), 0);
            /* Don't free the function here, other closures may reference
             * the same function
             */
            reallocate(obj, sizeof(*closure), 0);
            break;
        }
    }
}
