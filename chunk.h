#ifndef DPLANG_CHUNK_H
#define DPLANG_CHUNK_H

#include <stddef.h>
#include <stdint.h>
#include "value.h"

#define MIN_CHUNK_SIZE      8
#define CHUNK_GROWTH_FACTOR 2

enum opcode {
    OP_CONSTANT,
    OP_NIL,
    OP_FALSE,
    OP_POP,
    OP_TRUE,
    OP_EQUAL,
    OP_GREATER,
    OP_LESS,
    OP_ADD,
    OP_SUBTRACT,
    OP_MULTIPLY,
    OP_DIVIDE,
    OP_MOD,
    OP_SHL,
    OP_SHR,
    OP_NEGATE,
    OP_NOT,
    OP_AND,
    OP_OR,
    OP_XOR,
    OP_DEFINE_GLOBAL,
    OP_GET_GLOBAL,
    OP_SET_GLOBAL,
    OP_GET_LOCAL,
    OP_SET_LOCAL,
    OP_JUMP_IF_FALSE,
    OP_JUMP_IF_TRUE,
    OP_JUMP,
    OP_LOOP,
    OP_PRINT,
    OP_RETURN,
};

struct chunk {
    size_t count;
    size_t capacity;
    uint8_t *code;
    int *lines;
    struct value_array constants;
};

int chunk_init(struct chunk *chunk);
int chunk_write_opcode(struct chunk *chunk, enum opcode op, void *operands, size_t len, int line);
int chunk_add_constant(struct chunk *chunk, value val);
int chunk_free(struct chunk *chunk);
int chunk_disassemble(struct chunk *chunk, const char *name);
#endif
