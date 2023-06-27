#include <string.h>
#include <stdio.h>

#include "chunk.h"
#include "memory.h"
#include "object.h"
#include "value.h"

#define ARRAY_SIZE(a) sizeof(a) / sizeof(a[0])

int chunk_init(struct chunk *chunk)
{
    chunk->count = 0;
    chunk->capacity = MIN_CHUNK_SIZE;
    chunk->code = reallocate(NULL, 0, MIN_CHUNK_SIZE * sizeof(chunk->code[0]));
    chunk->lines = reallocate(NULL, 0, MIN_CHUNK_SIZE * sizeof(chunk->lines[0]));
    value_array_init(&chunk->constants);
    return 0;
}

int chunk_free(struct chunk *chunk)
{
    chunk->code = reallocate(chunk->code, chunk->capacity, 0);
    chunk->lines = reallocate(chunk->lines, chunk->capacity, 0);
    chunk->capacity = chunk->count = 0;
    value_array_free(&chunk->constants);
    memset(chunk, 0x00, sizeof(*chunk));
    return 0;
}

static int simple_instruction(const char *name, int offset)
{
    printf("%s\n", name);
    return offset + 1;
}

static int constant_instruction(const char *name, struct chunk *chunk, int offset)
{
    uint8_t constant_idx = chunk->code[offset + 1];
    printf("%-16s %4d '", name, constant_idx);
    value_print(chunk->constants.values[constant_idx]);
    printf("\n");
    return offset + 2;
}

static int byte_instruction(const char *name, struct chunk *chunk, int offset)
{
    uint8_t slot = chunk->code[offset + 1];
    printf("%-16s %4d\n", name, slot);
    return offset + 2;
}

static int jump_instruction(const char *name, struct chunk *chunk, int offset, int sign)
{
    uint16_t target = (uint16_t)(chunk->code[offset + 1] << 8);
    target |= chunk->code[offset + 2];
    printf("%-16s %4d -> %d\n", name, offset, offset + 3 + sign * target);
    // 16-bit jump destination, plus implicit pop of condition
    return offset + 3;
}

static const char *opnames[] = {
    [OP_CONSTANT] = "OP_CONSTANT",
    [OP_NIL] = "OP_NIL",
    [OP_FALSE] = "OP_FALSE",
    [OP_POP] = "OP_POP",
    [OP_TRUE] = "OP_TRUE",
    [OP_EQUAL] = "OP_EQUAL",
    [OP_GREATER] = "OP_GREATER",
    [OP_LESS] = "OP_LESS",
    [OP_ADD] = "OP_ADD",
    [OP_SUBTRACT] = "OP_SUBTRACT",
    [OP_MULTIPLY] = "OP_MULTIPLY",
    [OP_DIVIDE] = "OP_DIVIDE",
    [OP_MOD] = "OP_MOD",
    [OP_SHL] = "OP_SHL",
    [OP_SHR] = "OP_SHR",
    [OP_NEGATE] = "OP_NEGATE",
    [OP_NOT] = "OP_NOT",
    [OP_AND] = "OP_AND",
    [OP_OR] = "OP_OR",
    [OP_XOR] = "OP_XOR",
    [OP_DEFINE_GLOBAL] = "OP_DEFINE_GLOBAL",
    [OP_GET_GLOBAL] = "OP_GET_GLOBAL",
    [OP_SET_GLOBAL] = "OP_SET_GLOBAL",
    [OP_GET_LOCAL] = "OP_GET_LOCAL",
    [OP_SET_LOCAL] = "OP_SET_LOCAL",
    [OP_PRINT] = "OP_PRINT",
    [OP_JUMP_IF_FALSE] = "OP_JUMP_IF_FALSE",
    [OP_JUMP_IF_TRUE] = "OP_JUMP_IF_TRUE",
    [OP_LOOP] = "OP_LOOP",
    [OP_JUMP] = "OP_JUMP",
    [OP_RETURN] = "OP_RETURN",
};

static const char *opcode_to_string(enum opcode op)
{
    if (op >= ARRAY_SIZE(opnames)) {
        return "?";
    }
    return opnames[op];
}

static size_t disassemble_instruction(struct chunk *chunk, size_t offset)
{
    printf("%04u ", (int)offset);
    uint8_t opcode = chunk->code[offset];

    const char *opname = opcode_to_string(opcode);
    if (offset > 0 && chunk->lines[offset] == chunk->lines[offset - 1]) {
        printf("   | ");
    } else {
        printf("%4d ", chunk->lines[offset]);
    }
    printf(" %02x ", opcode);
    switch (opcode) {
        case OP_CONSTANT:
        case OP_DEFINE_GLOBAL:
        case OP_GET_GLOBAL:
        case OP_SET_GLOBAL:
            return constant_instruction(opname, chunk, offset);
        case OP_GET_LOCAL:
        case OP_SET_LOCAL:
            return byte_instruction(opname, chunk, offset);

        case OP_JUMP_IF_FALSE:
        case OP_JUMP_IF_TRUE:
        case OP_JUMP:
            return jump_instruction(opname, chunk, offset, 1);
        case OP_LOOP:
            return jump_instruction(opname, chunk, offset, -1);
        case OP_RETURN:
        case OP_NEGATE:
        case OP_ADD:
        case OP_SUBTRACT:
        case OP_MULTIPLY:
        case OP_DIVIDE:
        case OP_NIL:
        case OP_TRUE:
        case OP_FALSE:
        case OP_NOT:
        case OP_XOR:
        case OP_EQUAL:
        case OP_GREATER:
        case OP_LESS:
        case OP_MOD:
        case OP_SHL:
        case OP_SHR:
        case OP_PRINT:
        case OP_POP:
            return simple_instruction(opname, offset);

        default:
            printf("Unknown opcode 0x%02x\n", opcode);
            return offset + 1;
    }
}

int chunk_disassemble(struct chunk *chunk, const char *name)
{
    int count = 0;
    printf("=== %s === [%u bytes]\n", name, chunk->count);
    for (size_t offset = 0; offset < chunk->count;) {
        offset = disassemble_instruction(chunk, offset);
        count++;
    }

    return count;
}

int chunk_write_opcode(struct chunk *chunk, enum opcode op, void *operands, size_t len, int line)
{
    if (chunk->capacity < chunk->count + (1 + len)) {
        size_t prev_cap = chunk->capacity;
        chunk->capacity *= CHUNK_GROWTH_FACTOR;
        chunk->code = reallocate(chunk->code, prev_cap, chunk->capacity * sizeof(chunk->code[0]));
        chunk->lines = reallocate(chunk->lines, prev_cap, chunk->capacity * sizeof(chunk->lines[0]));
    }

    chunk->code[chunk->count] = (uint8_t)op;
    chunk->lines[chunk->count] = line;
    chunk->count++;

    if (len) {
        memcpy(&chunk->code[chunk->count], operands, len);
        for (int i = 0; i < len; i++) { chunk->lines[chunk->count++] = line; }
    }

    return 0;
}

int chunk_add_constant(struct chunk *chunk, value val)
{
    for (int i = 0; i < chunk->constants.count; i++) {
        if (value_equal(chunk->constants.values[i], val)) {
            return i;
        }
        if (IS_OBJECT(val) && IS_OBJECT(chunk->constants.values[i])) {
            if (object_equal(AS_OBJECT(val), AS_OBJECT(chunk->constants.values[i])))
                return i;
        }
    }
    value_array_write(&chunk->constants, val);
    return chunk->constants.count - 1;
}
