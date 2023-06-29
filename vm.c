#include "vm.h"
#include "chunk.h"
#include "compiler.h"
#include "object.h"
#include "value.h"
#include "table.h"
#include "memory.h"
#include "builtins.h"
#include <math.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

// #define DEBUG_TRACE_EXEC

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
    vm->frame_count = 0;
    vm->open_upvalues = NULL;
    return 0;
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

static void vm_runtime_error(struct vm *vm, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fputs("\n", stderr);

    for (int i = vm->frame_count - 1; i >= 0; i--) {
        struct call_frame *frame = &vm->frames[i];
        struct object_function *function = frame->closure->function;

        size_t instruction = frame->ip - function->chunk.code - 1;

        bool named = function->name != NULL;

        fprintf(stderr, "[line %d] in %s%s\n", function->chunk.lines[instruction],
                named ? function->name->data : "script", named ? "()" : "");
    }

    stack_reset(vm);
}

static void define_native(struct vm *vm, const char *name, native_function function)
{
    struct object_string *s = object_string_allocate(name, strlen(name));
    stack_push(vm, OBJECT_VAL(s));
    stack_push(vm, OBJECT_VAL(object_native_new(function)));
    table_set(&vm->globals, AS_STRING(vm->stack[0]), vm->stack[1]);
    stack_pop(vm);
    stack_pop(vm);
}

static bool call(struct vm *vm, struct object_closure *closure, int arg_count)
{
    if (arg_count != closure->function->arity) {
        vm_runtime_error(vm, "Expected %d arguments but got %d", closure->function->arity, arg_count);
        return false;
    }

    if (vm->frame_count == FRAMES_MAX) {
        vm_runtime_error(vm, "Stack overflow");
        return false;
    }

    struct call_frame *frame = &vm->frames[vm->frame_count++];
    frame->closure = closure;
    frame->ip = closure->function->chunk.code;
    frame->slots = vm->sp - arg_count - 1;
    return true;
}

static bool call_value(struct vm *vm, value callee, int arg_count)
{
    if (IS_OBJECT(callee)) {
        switch (OBJECT_TYPE(callee)) {
            case OBJECT_NATIVE: {
                native_function native = AS_NATIVE(callee);
                value result = native(arg_count, vm->sp - arg_count);
                vm->sp -= arg_count + 1;
                stack_push(vm, result);
                return true;
            }
            case OBJECT_CLOSURE:
                return call(vm, AS_CLOSURE(callee), arg_count);
            default:
                printf("what kind of object?\n");
                break;
        }
    }
    vm_runtime_error(vm, "Object not callable");
    return false;
}

static struct object_upvalue *capture_upvalue(struct vm *vm, value *local)
{
    struct object_upvalue *prev = NULL;
    struct object_upvalue *upvalue = vm->open_upvalues;
    while (upvalue != NULL && upvalue->location > local) {
        prev = upvalue;
        upvalue = upvalue->next;
    }

    if (upvalue != NULL && upvalue->location == local) {
        return upvalue;
    }

    struct object_upvalue *created = object_upvalue_new(local);
    created->next = upvalue;
    if (prev == NULL) {
        vm->open_upvalues = created;
    } else {
        prev->next = created;
    }
    return created;
}

static void close_upvalues(struct vm *vm, value *last)
{
    while (vm->open_upvalues != NULL && vm->open_upvalues->location >= last) {
        struct object_upvalue *upvalue = vm->open_upvalues;
        upvalue->closed = *upvalue->location;
        upvalue->location = &upvalue->closed;
        vm->open_upvalues = upvalue->next;
    }
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

    for (struct builtin_function_info *builtin = builtins; builtin->function != NULL; builtin++) {
        define_native(vm, builtin->name, builtin->function);
    }

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

int vm_run(struct vm *vm)
{
    struct call_frame *frame = &vm->frames[vm->frame_count - 1];

#define READ_U8()       ((uint8_t)*frame->ip++)
#define READ_U16()      (frame->ip += 2, (uint16_t)((frame->ip[-2] << 8) | frame->ip[-1]))
#define READ_CONSTANT() (frame->closure->function->chunk.constants.values[READ_U8()])
#define READ_OPCODE()   ((enum opcode)READ_U8())
#define READ_STRING()   AS_STRING(READ_CONSTANT())

#define BINARY_OP(valtype, op)                                                \
    do {                                                                      \
        if (!IS_NUMBER(stack_peek(vm, 0)) || !IS_NUMBER(stack_peek(vm, 1))) { \
            vm_runtime_error(vm, "Operands must be numbers");                 \
            return -1;                                                        \
        }                                                                     \
        double b = AS_NUMBER(stack_pop(vm));                                  \
        double a = AS_NUMBER(stack_pop(vm));                                  \
        stack_push(vm, valtype(a op b));                                      \
    } while (0)

#define BINARY_FUNC(valtype, func)                                            \
    do {                                                                      \
        if (!IS_NUMBER(stack_peek(vm, 0)) || !IS_NUMBER(stack_peek(vm, 1))) { \
            vm_runtime_error(vm, "Operands must be numbers");                 \
            return -1;                                                        \
        }                                                                     \
        double b = AS_NUMBER(stack_pop(vm));                                  \
        double a = AS_NUMBER(stack_pop(vm));                                  \
        stack_push(vm, valtype(func(a, b)));                                  \
    } while (0)

#ifdef DEBUG_TRACE_EXEC
    printf("++++ TRACE ++++\n");
#endif

    while (1) {
#ifdef DEBUG_TRACE_EXEC
        printf("            ");
        for (value *slot = vm->stack; slot < vm->sp; slot++) {
            printf("[ ");
            value_print(*slot);
            printf(" ]");
        }
        printf("\n");
        disassemble_instruction(&frame->closure->function->chunk,
                                (int)(frame->ip - frame->closure->function->chunk.code));

#endif
        enum opcode inst = READ_OPCODE();
        switch (inst) {
            case OP_CONSTANT: {
                value constant = READ_CONSTANT();
                stack_push(vm, constant);
                break;
            }
            case OP_NIL:
                stack_push(vm, NIL_VAL);
                break;
            case OP_TRUE:
                stack_push(vm, BOOL_VAL(true));
                break;
            case OP_FALSE:
                stack_push(vm, BOOL_VAL(false));
                break;
            case OP_POP:
                stack_pop(vm);
                break;
            case OP_GET_LOCAL: {
                uint8_t slot = READ_U8();
                stack_push(vm, frame->slots[slot]);
                break;
            }
            case OP_SET_LOCAL: {
                uint8_t slot = READ_U8();
                frame->slots[slot] = stack_peek(vm, 0);
                break;
            }
            case OP_GET_GLOBAL: {
                struct object_string *name = READ_STRING();
                value value;
                if (!table_get(&vm->globals, name, &value)) {
                    vm_runtime_error(vm, "Undefined variable '%s'", name->data);
                    return -1;
                }
                stack_push(vm, value);
                break;
            }
            case OP_DEFINE_GLOBAL: {
                struct object_string *name = READ_STRING();
                table_set(&vm->globals, name, stack_peek(vm, 0));
                stack_pop(vm);
                break;
            }
            case OP_SET_GLOBAL: {
                struct object_string *name = READ_STRING();
                if (table_set(&vm->globals, name, stack_peek(vm, 0))) {
                    table_delete(&vm->globals, name);
                    vm_runtime_error(vm, "Undefined variable '%s'", name->data);
                    return -1;
                }
                break;
            }
            case OP_GET_UPVALUE: {
                uint8_t slot = READ_U8();
                stack_push(vm, *frame->closure->upvalues[slot]->location);
                break;
            }
            case OP_SET_UPVALUE: {
                uint8_t slot = READ_U8();
                *frame->closure->upvalues[slot]->location = stack_peek(vm, 0);
                break;
            }
            case OP_EQUAL: {
                value b = stack_pop(vm);
                value a = stack_pop(vm);
                stack_push(vm, BOOL_VAL(value_equal(a, b)));
                break;
            }
            case OP_GREATER:
                BINARY_OP(BOOL_VAL, >);
                break;
            case OP_LESS:
                BINARY_OP(BOOL_VAL, <);
                break;
            case OP_ADD:
                if (IS_STRING(stack_peek(vm, 0)) && IS_STRING(stack_peek(vm, 1))) {
                    /* keep s1 and s2 on the stack until s3 is added
                     * This helps avoid gc-related issues
                     */
                    struct object_string *s2 = AS_STRING(stack_peek(vm, 0));
                    struct object_string *s1 = AS_STRING(stack_peek(vm, 1));
                    size_t new_length = s2->length + s1->length;
                    char *data = reallocate(NULL, 0, new_length + 1);
                    strncpy(data, s1->data, s1->length);
                    strncpy(&data[s1->length], s2->data, s2->length);
                    struct object_string *s3 = vm_intern_string(vm, data, new_length);
                    stack_pop(vm);
                    stack_pop(vm);
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
                BINARY_OP(NUMBER_VAL, -);
                break;
            case OP_MULTIPLY:
                BINARY_OP(NUMBER_VAL, *);
                break;
            case OP_DIVIDE:
                BINARY_OP(NUMBER_VAL, /);
                break;
            case OP_MOD:
                BINARY_FUNC(NUMBER_VAL, fmod);
                break;
            case OP_SHL:
                BINARY_FUNC(NUMBER_VAL, fshl);
                break;
            case OP_SHR:
                BINARY_FUNC(NUMBER_VAL, fshr);
                break;
            case OP_NOT:
                stack_push(vm, BOOL_VAL(is_falsey(stack_pop(vm))));
                break;
            case OP_PRINT:
                value_print(stack_pop(vm));
                printf("\n");
                break;
            case OP_JUMP: {
                uint16_t offset = READ_U16();
                frame->ip += offset;
                break;
            }
            case OP_JUMP_IF_FALSE: {
                uint16_t offset = READ_U16();
                if (is_falsey(stack_peek(vm, 0)))
                    frame->ip += offset;
                break;
            }
            case OP_JUMP_IF_TRUE: {
                uint16_t offset = READ_U16();
                if (!is_falsey(stack_peek(vm, 0)))
                    frame->ip += offset;
                break;
            }
            case OP_LOOP: {
                uint16_t offset = READ_U16();
                frame->ip -= offset;
                break;
            }
            case OP_CALL: {
                int arg_count = READ_U8();
                if (!call_value(vm, stack_peek(vm, arg_count), arg_count)) {
                    return -1;
                }
                frame = &vm->frames[vm->frame_count - 1];
                break;
            }
            case OP_CLOSURE: {
                struct object_function *function = AS_FUNCTION(READ_CONSTANT());
                struct object_closure *closure = object_closure_new(function);
                stack_push(vm, OBJECT_VAL(closure));
                for (int i = 0; i < closure->nupvalues; i++) {
                    uint8_t is_local = READ_U8();
                    uint8_t index = READ_U8();

                    if (is_local) {
                        closure->upvalues[i] = capture_upvalue(vm, frame->slots + index);
                    } else {
                        closure->upvalues[i] = frame->closure->upvalues[index];
                    }
                }
                break;
            }
            case OP_CLOSE_UPVALUE:
                close_upvalues(vm, vm->sp - 1);
                stack_pop(vm);
                break;
            case OP_RETURN: {
                value result = stack_pop(vm);
                close_upvalues(vm, frame->slots);
                vm->frame_count--;
                if (vm->frame_count == 0) {
                    stack_pop(vm);
                    return 0;
                }
                vm->sp = frame->slots;
                stack_push(vm, result);
                frame = &vm->frames[vm->frame_count - 1];
                break;
            }
            case OP_NEGATE:
                if (!IS_NUMBER(stack_peek(vm, 0))) {
                    vm_runtime_error(vm, "Operand must be a number");
                    return -1;
                }
                stack_push(vm, NUMBER_VAL(-AS_NUMBER(stack_pop(vm))));
                break;
        }
    }

    return 0;
}

int vm_dump_bytecode(struct vm *vm, struct object_function *function)
{
    FILE *f = fopen("bytecode.dpc", "wb");
    fwrite(function->chunk.code, 1, function->chunk.count, f);
    for (int i = 0; i < function->chunk.constants.count; i++) {
        // TODO: dump constants
    }
    fclose(f);
}

int vm_interpret(struct vm *vm, const char *source)
{
    struct object_function *function = compile(source);
    if (function == NULL)
        return -1;

    vm_dump_bytecode(vm, function);

    gc_init(vm);

    stack_push(vm, OBJECT_VAL(function));

    struct object_closure *closure = object_closure_new(function);
    stack_pop(vm);
    stack_push(vm, OBJECT_VAL(closure));
    call(vm, closure, 0);

    return vm_run(vm);
}
