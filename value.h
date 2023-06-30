#ifndef DPLANG_VALUE_H
#define DPLANG_VALUE_H
#include <stdbool.h>

enum value_type {
    VAL_BOOL,
    VAL_NIL,
    VAL_NUMBER,
    VAL_OBJECT,
};

struct _value {
    enum value_type type;
    union {
        bool boolean;
        double number;
        struct object *object;
    } as;
};

typedef struct _value value;

#define IS_TYPE(v, t) ((v).type == t)
#define IS_BOOL(v)    IS_TYPE(v, VAL_BOOL)
#define IS_NIL(v)     IS_TYPE(v, VAL_NIL)
#define IS_NUMBER(v)  IS_TYPE(v, VAL_NUMBER)
#define IS_OBJECT(v)  IS_TYPE(v, VAL_OBJECT)

#define AS_BOOL(v)   ((v).as.boolean)
#define AS_NUMBER(v) ((v).as.number)
#define AS_OBJECT(v) ((v).as.object)

#define BOOL_VAL(v)   ((value){.type = VAL_BOOL, .as = {.boolean = v}})
#define NIL_VAL       ((value){.type = VAL_NIL, .as = {.number = 0}})
#define NUMBER_VAL(v) ((value){.type = VAL_NUMBER, .as = {.number = v}})
#define OBJECT_VAL(v) ((value){.type = VAL_OBJECT, .as = {.object = (struct object *)v}})

struct value_array {
    int capacity;
    int count;
    value *values;
};

int value_array_init(struct value_array *varray);
int value_array_write(struct value_array *varray, value val);
int value_array_free(struct value_array *varray);

int value_print(value val);

bool value_equal(value a, value b);
#endif
