#ifndef DPLANG_VM_H
#define DPLANG_VM_H

#include <stdbool.h>

#include "object.h"
#include "chunk.h"
#include "table.h"
#include "value.h"

#define FRAMES_MAX 64

#define STACK_MAX 256

struct call_frame {
    struct object_closure *closure;
    uint8_t *ip;
    value *slots;
};

struct vm {
    struct call_frame frames[FRAMES_MAX];
    struct call_frame *frame;
    int frame_count;
    value stack[STACK_MAX];
    value *sp;
    struct table globals;
    struct table strings;
    struct object_upvalue *open_upvalues;
    struct object *objects;
    struct object_string *init_string;
};

int vm_init(struct vm *vm);
int vm_free(struct vm *vm);
int vm_interpret(struct vm *vm, const char *source);

struct object_string *vm_intern_string(struct vm *vm, const char *s, size_t len);
#endif
