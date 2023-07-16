#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include "object.h"
#include "memory.h"

// #define DEBUG_LOG_GC

extern struct object *gc_objects;

#define ALLOCATE_OBJECT(type, id) (type *)object_allocate(sizeof(type), id)

#ifdef DEBUG_LOG_GC
static inline const char *object_type_name(enum object_type type)
{
    switch (type) {
        case OBJECT_BOUND_METHOD:
            return "BOUND_METHOD";
        case OBJECT_CLASS:
            return "CLASS";
        case OBJECT_CLOSURE:
            return "CLOSURE";
        case OBJECT_FUNCTION:
            return "FUNCTION";
        case OBJECT_INSTANCE:
            return "INSTANCE";
        case OBJECT_NATIVE:
            return "NATIVE";
        case OBJECT_STRING:
            return "STRING";
        case OBJECT_TABLE:
            return "TABLE";
        case OBJECT_UPVALUE:
            return "UPVALUE";
        default:
            return "INVALID";
    };
}
#endif

void object_enable_gc(struct object *obj)
{
#ifdef DEBUG_LOG_GC
    printf("%p Enable gc\n", obj);
#endif
    obj->next = gc_objects;
    gc_objects = obj;
}

void object_disable_gc(struct object *obj)
{
#ifdef DEBUG_LOG_GC
    printf("%p Disable gc\n", obj);
#endif
    if (gc_objects == obj) {
        gc_objects = gc_objects->next;
        return;
    }

    struct object *next = gc_objects;

    while (next->next != NULL) {
        if (next->next == obj) {
            next->next = obj->next;
        }
        next = next->next;
    }
}

// NOLINTBEGIN(bugprone-easily-swappable-parameters)
static struct object *object_allocate(size_t size, enum object_type type)
{
    struct object *object = (struct object *)reallocate(NULL, 0, size);
    object->type = type;
    object->marked = false;

#ifdef DEBUG_LOG_GC
    printf("%p object allocate %s [%zu bytes]\n", (void *)object, object_type_name(type), size);
#endif
    return object;
}
// NOLINTEND(bugprone-easily-swappable-parameters)

struct object_string *object_string_take(const char *s, size_t length)
{
    struct object_string *string = ALLOCATE_OBJECT(struct object_string, OBJECT_STRING);
    string->length = length;
    string->data = (char *)s;
    string->hash = hash_string(s, length);

    object_enable_gc((struct object *)string);
    return string;
}

struct object_string *object_string_allocate(const char *s, size_t length)
{
    char *data = (char *)reallocate(NULL, 0, length + 1);
    memcpy(data, s, length);
    data[length] = '\0';
    return object_string_take(data, length);
}

struct object_string *object_string_vformat(const char *fmt, va_list ap)
{
    va_list aq;
    va_copy(aq, ap);
    int count = vsnprintf(NULL, 0, fmt, ap);
    if (count < 0) {
        va_end(aq);
        return NULL;
    }
    size_t length = count + 1;
    char *s = (char *)reallocate(NULL, 0, length);
    vsnprintf(s, length, fmt, aq);

    struct object_string *obj = object_string_take(s, length);
    va_end(aq);
    return obj;
}

struct object_string *object_string_format(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    struct object_string *s = object_string_vformat(fmt, args);
    va_end(args);
    return s;
}
struct object_class *object_class_new(struct object_string *name)
{
    struct object_class *klass = ALLOCATE_OBJECT(struct object_class, OBJECT_CLASS);
    klass->name = name;
    table_init(&klass->methods);
    object_enable_gc((struct object *)klass);
    return klass;
}

struct object_bound_method *object_bound_method_new(value receiver, struct object_closure *method)
{
    struct object_bound_method *bound = ALLOCATE_OBJECT(struct object_bound_method, OBJECT_BOUND_METHOD);
    bound->receiver = receiver;
    bound->method = method;
    object_enable_gc((struct object *)bound);
    return bound;
}

struct object_table *object_table_new()
{
    struct object_table *table = ALLOCATE_OBJECT(struct object_table, OBJECT_TABLE);
    table_init(&table->table);
    object_enable_gc((struct object *)table);
    return table;
}

struct object_upvalue *object_upvalue_new(value *slot)
{
    struct object_upvalue *upvalue = ALLOCATE_OBJECT(struct object_upvalue, OBJECT_UPVALUE);
    upvalue->next = NULL;
    upvalue->closed = NIL_VAL;
    upvalue->location = slot;
    object_enable_gc((struct object *)upvalue);
    return upvalue;
}

struct object_function *object_function_new(struct object_string *name)
{
    struct object_function *func = ALLOCATE_OBJECT(struct object_function, OBJECT_FUNCTION);
    func->arity = 0;
    func->nupvalues = 0;
    func->name = name;
    chunk_init(&func->chunk);
    object_enable_gc((struct object *)func);
    return func;
}

struct object_instance *object_instance_new(struct object_class *klass)
{
    struct object_instance *instance = ALLOCATE_OBJECT(struct object_instance, OBJECT_INSTANCE);
    instance->klass = klass;
    table_init(&instance->fields);
    object_enable_gc((struct object *)instance);
    return instance;
}

struct object_closure *object_closure_new(struct object_function *function)
{
    struct object_upvalue **upvalues = reallocate(NULL, 0, function->nupvalues * sizeof(struct object_upvalue *));
    for (int i = 0; i < function->nupvalues; i++) { upvalues[i] = NULL; }
    struct object_closure *closure = ALLOCATE_OBJECT(struct object_closure, OBJECT_CLOSURE);

    closure->function = function;
    closure->upvalues = upvalues;
    closure->nupvalues = function->nupvalues;
    object_enable_gc((struct object *)closure);
    return closure;
}

struct object_native *object_native_new(native_function function)
{
    struct object_native *native = ALLOCATE_OBJECT(struct object_native, OBJECT_NATIVE);
    native->function = function;
    object_enable_gc((struct object *)native);
    return native;
}

static void function_print(struct object_function *function)
{
    printf("<%s%s>", (function->name == NULL) ? "script" : "fn ", (function->name == NULL) ? "" : function->name->data);
}

int object_print(struct object *obj)
{
    switch (obj->type) {
        case OBJECT_BOUND_METHOD: {
            struct object_bound_method *bound = (struct object_bound_method *)obj;
            function_print(bound->method->function);
            break;
        }
        case OBJECT_CLASS: {
            struct object_class *klass = (struct object_class *)obj;
            printf("class %s", klass->name->data);
            break;
        }
        case OBJECT_INSTANCE: {
            struct object_instance *instance = (struct object_instance *)obj;
            printf("%s instance %p", instance->klass->name->data, (void *)instance);
            break;
        }
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
            printf("upvalue %p", (void *)obj);
            break;
        }
        case OBJECT_NATIVE: {
            printf("<native fn> %p", (void *)obj);
            break;
        }
        case OBJECT_TABLE: {
            printf("table %p", (void *)obj);
            break;
        }
        default:
            printf("Unsupported object type");
            break;
    }
    return 0;
}

bool object_equal(struct object *a, struct object *b)
{
    if (a->type != b->type) {
        return false;
    }

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
        case OBJECT_BOUND_METHOD: {
            struct object_bound_method *bound = (struct object_bound_method *)obj;
            reallocate(obj, sizeof(*bound), 0);
            break;
        }
        case OBJECT_CLASS: {
            struct object_class *klass = (struct object_class *)obj;
            table_free(&klass->methods);
            reallocate(obj, sizeof(*klass), 0);
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
        case OBJECT_FUNCTION: {
            struct object_function *func = (struct object_function *)obj;
            chunk_free(&func->chunk);
            reallocate(func, sizeof(*func), 0);
            break;
        }
        case OBJECT_INSTANCE: {
            struct object_instance *instance = (struct object_instance *)obj;
            table_free(&instance->fields);
            reallocate(obj, sizeof(*instance), 0);
            break;
        }
        case OBJECT_NATIVE: {
            struct object_native *native = (struct object_native *)obj;
            reallocate(obj, sizeof(*native), 0);
            break;
        }
        case OBJECT_STRING: {
            struct object_string *str = (struct object_string *)obj;
            str->data = reallocate(str->data, str->length + 1, 0);
            reallocate(str, sizeof(*str), 0);
            break;
        }
        case OBJECT_TABLE: {
            struct object_table *table = (struct object_table *)obj;
            table_free(&table->table);
            reallocate(table, sizeof(*table), 0);
            break;
        }
        case OBJECT_UPVALUE: {
            struct object_upvalue *upvalue = (struct object_upvalue *)obj;
            reallocate(obj, sizeof(*upvalue), 0);
            break;
        }
    }
}
