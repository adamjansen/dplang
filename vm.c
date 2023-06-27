#include "vm.h"
#include "compiler.h"
#include "object.h"
#include "value.h"
#include "table.h"
#include "memory.h"
#include <math.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>

#define READ_BYTE(vm)     ((uint8_t)(*(vm)->ip++))
#define READ_U16(vm)      ((vm)->ip += 2, (uint16_t)(((vm)->ip[-2] << 8) | (vm)->ip[-1]))
#define READ_OPCODE(vm)   ((enum opcode)READ_BYTE(vm))
#define READ_CONSTANT(vm) ((value)((vm)->chunk->constants.values[READ_BYTE(vm)]))
#define READ_STRING(vm)   AS_STRING(READ_CONSTANT(vm))

static inline double fshl(double a, double b)
{
    return (int)a << (int)b;
}

static inline double fshr(double a, double b)
{
    return (int)a >> (int)b;
}

static int stack_reset(struct vm *vm)
{
    vm->sp = vm->stack;
    return 0;
}

static void vm_runtime_error(struct vm *vm, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fputs("\n", stderr);

    size_t instruction = vm->ip - vm->chunk->code - 1;
    int line = vm->chunk->lines[instruction];
    fprintf(stderr, "[line %d] in script\n", line);
    stack_reset(vm);
}

static int stack_push(struct vm *vm, value val)
{
    *vm->sp++ = val;
    return 0;
}

static value stack_pop(struct vm *vm)
{
    vm->sp--;
    return *vm->sp;
}

static value stack_peek(struct vm *vm, int distance)
{
    return vm->sp[-1 - distance];
}

struct object_string *vm_intern_string(struct vm *vm, const char *s, size_t len)
{
    uint32_t hash = hash_string(s, len);
    struct object_string *interned = table_find_string(&vm->strings, s, len, hash);
    if (interned != NULL)
        return interned;
    struct object_string *obj = object_string_take(s, len);
    table_set(&vm->strings, obj, NIL_VAL);
    return obj;
}

int vm_init(struct vm *vm)
{
    stack_reset(vm);
    vm->objects = NULL;
    table_init(&vm->strings);
    table_init(&vm->globals);
    return 0;
}

static bool is_falsey(value val)
{
    return IS_NIL(val) || (IS_BOOL(val) && !AS_BOOL(val));
}

int vm_free(struct vm *vm)
{
    table_free(&vm->globals);
    table_free(&vm->strings);
    // TODO: free_objects();
    return 0;
}

#define BINARY_OP(vm, valtype, op)                                            \
    do {                                                                      \
        if (!IS_NUMBER(stack_peek(vm, 0)) || !IS_NUMBER(stack_peek(vm, 1))) { \
            vm_runtime_error(vm, "Operands must be numbers");                 \
            return -1;                                                        \
        }                                                                     \
        double b = AS_NUMBER(stack_pop(vm));                                  \
        double a = AS_NUMBER(stack_pop(vm));                                  \
        stack_push(vm, valtype(a op b));                                      \
    } while (0)

#define BINARY_FUNC(vm, valtype, func)                                        \
    do {                                                                      \
        if (!IS_NUMBER(stack_peek(vm, 0)) || !IS_NUMBER(stack_peek(vm, 1))) { \
            vm_runtime_error(vm, "Operands must be numbers");                 \
            return -1;                                                        \
        }                                                                     \
        double b = AS_NUMBER(stack_pop(vm));                                  \
        double a = AS_NUMBER(stack_pop(vm));                                  \
        stack_push(vm, valtype(func(a, b)));                                  \
    } while (0)

int vm_run(struct vm *vm)
{
    while (1) {
        enum opcode inst = READ_OPCODE(vm);
        switch (inst) {
            case OP_JUMP_IF_FALSE: {
                uint16_t offset = READ_U16(vm);
                if (is_falsey(stack_peek(vm, 0)))
                    vm->ip += offset;
                break;
            }
            case OP_JUMP_IF_TRUE: {
                uint16_t offset = READ_U16(vm);
                if (!is_falsey(stack_peek(vm, 0)))
                    vm->ip += offset;
                break;
            }
            case OP_JUMP: {
                uint16_t offset = READ_U16(vm);
                vm->ip += offset;
                break;
            }
            case OP_LOOP: {
                uint16_t offset = READ_U16(vm);
                vm->ip -= offset;
                break;
            }
            case OP_RETURN:
                return 0;
            case OP_CONSTANT:
                stack_push(vm, READ_CONSTANT(vm));
                break;
            case OP_NIL:
                stack_push(vm, NIL_VAL);
                break;
            case OP_TRUE:
                stack_push(vm, BOOL_VAL(true));
                break;
            case OP_FALSE:
                stack_push(vm, BOOL_VAL(false));
                break;
            case OP_NEGATE:
                if (!IS_NUMBER(stack_peek(vm, 0))) {
                    vm_runtime_error(vm, "Operand must be a number");
                    return -1;
                }
                stack_push(vm, NUMBER_VAL(-AS_NUMBER(stack_pop(vm))));
                break;
            case OP_ADD:
                if (IS_STRING(stack_peek(vm, 0)) && IS_STRING(stack_peek(vm, 1))) {
                    struct object_string *s2 = AS_STRING(stack_pop(vm));
                    struct object_string *s1 = AS_STRING(stack_pop(vm));
                    size_t new_length = s2->length + s1->length;
                    char *data = reallocate(NULL, 0, new_length + 1);
                    strncpy(data, s1->data, s1->length);
                    strncpy(&data[s1->length], s2->data, s2->length);
                    struct object_string *s3 = vm_intern_string(vm, data, new_length);
                    stack_push(vm, OBJECT_VAL(s3));
                } else if (IS_NUMBER(stack_peek(vm, 0)) && IS_NUMBER(stack_peek(vm, 1))) {
                    double b = AS_NUMBER(stack_pop(vm));
                    double a = AS_NUMBER(stack_pop(vm));
                    stack_push(vm, NUMBER_VAL(a + b));
                } else {
                    vm_runtime_error(vm, "Operands must be two numbers or two strings");
                    return -1;
                }
                break;
            case OP_SUBTRACT:
                BINARY_OP(vm, NUMBER_VAL, -);
                break;
            case OP_MULTIPLY:
                BINARY_OP(vm, NUMBER_VAL, *);
                break;
            case OP_DIVIDE:
                BINARY_OP(vm, NUMBER_VAL, /);
                break;
            case OP_MOD:
                BINARY_FUNC(vm, NUMBER_VAL, fmod);
                break;
            case OP_SHL:
                BINARY_FUNC(vm, NUMBER_VAL, fshl);
                break;
            case OP_SHR:
                BINARY_FUNC(vm, NUMBER_VAL, fshr);
                break;
            case OP_NOT:
                stack_push(vm, BOOL_VAL(is_falsey(stack_pop(vm))));
                break;
            case OP_POP:
                stack_pop(vm);
                break;
            case OP_GET_LOCAL: {
                uint8_t slot = READ_BYTE(vm);
                stack_push(vm, vm->stack[slot]);
                break;
            }
            case OP_SET_LOCAL: {
                uint8_t slot = READ_BYTE(vm);
                vm->stack[slot] = stack_peek(vm, 0);
                break;
            }
            case OP_GET_GLOBAL: {
                struct object_string *name = READ_STRING(vm);
                value value;
                if (!table_get(&vm->globals, name, &value)) {
                    vm_runtime_error(vm, "Undefined variable '%s'", name->data);
                    return -1;
                }
                stack_push(vm, value);
            } break;
            case OP_SET_GLOBAL: {
                struct object_string *name = READ_STRING(vm);
                if (table_set(&vm->globals, name, stack_peek(vm, 0))) {
                    table_delete(&vm->globals, name);
                    vm_runtime_error(vm, "Undefined variable '%s'", name->data);
                    return -1;
                }
            } break;
            case OP_DEFINE_GLOBAL: {
                struct object_string *name = READ_STRING(vm);
                table_set(&vm->globals, name, stack_peek(vm, 0));
                stack_pop(vm);
            } break;
            case OP_EQUAL: {
                value b = stack_pop(vm);
                value a = stack_pop(vm);
                stack_push(vm, BOOL_VAL(value_equal(a, b)));
                break;
            }
            case OP_GREATER:
                BINARY_OP(vm, BOOL_VAL, >);
                break;
            case OP_LESS:
                BINARY_OP(vm, BOOL_VAL, <);
                break;
            case OP_PRINT:
                value_print(stack_pop(vm));
                printf("\n");
                break;
        }
    }

    return 0;
}

int vm_interpret(struct vm *vm, const char *source)
{
    struct chunk chunk;
    chunk_init(&chunk);

    if (compile(source, &chunk)) {
        chunk_free(&chunk);
        return -1;
    }

    chunk_disassemble(&chunk, "repl");

    vm->chunk = &chunk;
    vm->ip = vm->chunk->code;
    int result = vm_run(vm);
    chunk_free(&chunk);
    return result;
}
