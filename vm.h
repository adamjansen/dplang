#ifndef DPLANG_VM_H
#define DPLANG_VM_H

#include <stdbool.h>

#include "chunk.h"
#include "table.h"
#include "value.h"

#define STACK_MAX 256

struct vm {
    struct chunk *chunk;
    uint8_t *ip;
    value stack[STACK_MAX];
    value *sp;
    struct object *objects;
    struct table strings;
    struct table globals;
};

int vm_init(struct vm *vm);
int vm_free(struct vm *vm);
int vm_interpret(struct vm *vm, const char *source);

struct object_string *vm_intern_string(struct vm *vm, const char *s, size_t len);
#endif
