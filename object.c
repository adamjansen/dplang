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
    strncpy(data, s, length);
    return object_string_take(data, length);
}

struct object_function *object_function_new()
{
    struct object_function *func = reallocate(NULL, 0, sizeof(struct object_function));
    func->object.type = OBJECT_FUNCTION;
    func->arity = 0;
    func->name = NULL;
    chunk_init(&func->chunk);
    return func;
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
            struct object_function *func = (struct object_function *)func;
            printf("<fn %s>", func->name->data);
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
    }
}
