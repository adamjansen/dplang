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

#define BYTECODE_MAGIC 0xDEADBEEF

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
            case OBJECT_BOUND_METHOD: {
                struct object_bound_method *bound = AS_BOUND_METHOD(callee);
                vm->sp[-arg_count - 1] = bound->receiver;  // set 'this' to bound instance
                return call(vm, bound->method, arg_count);
            }
            case OBJECT_CLASS: {
                struct object_class *klass = AS_CLASS(callee);
                vm->sp[-arg_count - 1] = OBJECT_VAL(object_instance_new(klass));
                value initializer;
                if (table_get(&klass->methods, vm->init_string, &initializer)) {
                    return call(vm, AS_CLOSURE(initializer), arg_count);
                } else if (arg_count != 0) {
                    vm_runtime_error(vm, "Expected 0 arguments, but got %d", arg_count);
                    return false;
                }
                return true;
            }
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

static bool invoke_from_class(struct vm *vm, struct object_class *klass, struct object_string *name, int arg_count)
{
    value method;
    if (!table_get(&klass->methods, name, &method)) {
        vm_runtime_error(vm, "Undefined property '%s'", name->data);
        return false;
    }
    return call(vm, AS_CLOSURE(method), arg_count);
}

static bool invoke(struct vm *vm, struct object_string *name, int arg_count)
{
    value receiver = stack_peek(vm, arg_count);

    if (!IS_INSTANCE(receiver)) {
        vm_runtime_error(vm, "Only instances have methods");
        return false;
    }

    struct object_instance *instance = AS_INSTANCE(receiver);

    value value;
    if (table_get(&instance->fields, name, &value)) {
        vm->sp[-arg_count - 1] = value;
        return call_value(vm, value, arg_count);
    }

    return invoke_from_class(vm, instance->klass, name, arg_count);
}

static bool bind_method(struct vm *vm, struct object_class *klass, struct object_string *name)
{
    value method;
    if (!table_get(&klass->methods, name, &method)) {
        vm_runtime_error(vm, "Undefined property '%s'", name->data);
        return false;
    }

    struct object_bound_method *bound = object_bound_method_new(stack_peek(vm, 0), AS_CLOSURE(method));

    stack_pop(vm);
    stack_push(vm, OBJECT_VAL(bound));
    return true;
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

static void define_method(struct vm *vm, struct object_string *name)
{
    value method = stack_peek(vm, 0);
    struct object_class *klass = AS_CLASS(stack_peek(vm, 1));
    table_set(&klass->methods, name, method);
    stack_pop(vm);
}

struct object_string *vm_intern_string(struct vm *vm, const char *s, size_t len)
{
    uint32_t hash = hash_string(s, len);
    struct object_string *interned = table_find_string(&vm->strings, s, len, hash);
    if (interned != NULL) {
        printf("found already interned string at %p\n", interned);
        return interned;
    }
    struct object_string *obj = object_string_take(s, len);
    table_set(&vm->strings, obj, NIL_VAL);
    return obj;
}

int vm_init(struct vm *vm)
{
    gc_init(vm);
    stack_reset(vm);
    vm->objects = NULL;
    table_init(&vm->strings);
    table_init(&vm->globals);

    vm->init_string = NULL;
    vm->init_string = object_string_allocate("init", 4);

#if 0
    for (struct builtin_function_info *builtin = builtins; builtin->function != NULL; builtin++) {
        define_native(vm, builtin->name, builtin->function);
    }

#endif
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
    vm->init_string = NULL;
    // TODO: free_objects();
    return 0;
}

#define READ_U8(vm)       ((uint8_t) * (vm)->frame->ip++)
#define READ_CONSTANT(vm) ((vm)->frame->closure->function->chunk.constants.values[READ_U8(vm)])
#define READ_U16(vm)      ((vm)->frame->ip += 2, (uint16_t)(((vm)->frame->ip[-2] << 8) | (vm)->frame->ip[-1]))
#define READ_OPCODE(vm)   ((enum opcode)READ_U8(vm))
#define READ_STRING(vm)   AS_STRING(READ_CONSTANT(vm))

#define BINARY_OP(valtype, op)                                                \
    do {                                                                      \
        if (!IS_NUMBER(stack_peek(vm, 0)) || !IS_NUMBER(stack_peek(vm, 1))) { \
            vm_runtime_error(vm, "Operands must be numbers");                 \
            return false;                                                     \
        }                                                                     \
        double b = AS_NUMBER(stack_pop(vm));                                  \
        double a = AS_NUMBER(stack_pop(vm));                                  \
        stack_push(vm, valtype(a op b));                                      \
    } while (0)

#define BINARY_FUNC(valtype, func)                                            \
    do {                                                                      \
        if (!IS_NUMBER(stack_peek(vm, 0)) || !IS_NUMBER(stack_peek(vm, 1))) { \
            vm_runtime_error(vm, "Operands must be numbers");                 \
            return false;                                                     \
        }                                                                     \
        double b = AS_NUMBER(stack_pop(vm));                                  \
        double a = AS_NUMBER(stack_pop(vm));                                  \
        stack_push(vm, valtype(func(a, b)));                                  \
    } while (0)

bool vm_op_constant(struct vm *vm)
{
    value constant = READ_CONSTANT(vm);
    stack_push(vm, constant);
    return true;
}

bool vm_op_nil(struct vm *vm)
{
    stack_push(vm, NIL_VAL);
    return true;
}
bool vm_op_true(struct vm *vm)
{
    stack_push(vm, BOOL_VAL(true));
    return true;
}

bool vm_op_false(struct vm *vm)
{
    stack_push(vm, BOOL_VAL(false));
    return true;
}

bool vm_op_pop(struct vm *vm)
{
    stack_pop(vm);
    return true;
}

bool vm_op_get_local(struct vm *vm)
{
    uint8_t slot = READ_U8(vm);
    stack_push(vm, vm->frame->slots[slot]);
    return true;
}

bool vm_op_set_local(struct vm *vm)
{
    uint8_t slot = READ_U8(vm);
    vm->frame->slots[slot] = stack_peek(vm, 0);
    return true;
}

bool vm_op_get_global(struct vm *vm)
{
    struct object_string *name = READ_STRING(vm);
    value value;
    if (!table_get(&vm->globals, name, &value)) {
        vm_runtime_error(vm, "Undefined variable '%s'", name->data);
        return false;
    }
    stack_push(vm, value);
    return true;
}

bool vm_op_define_global(struct vm *vm)
{
    struct object_string *name = READ_STRING(vm);
    table_set(&vm->globals, name, stack_peek(vm, 0));
    stack_pop(vm);
    return true;
}

bool vm_op_set_global(struct vm *vm)
{
    struct object_string *name = READ_STRING(vm);
    if (table_set(&vm->globals, name, stack_peek(vm, 0))) {
        table_delete(&vm->globals, name);
        vm_runtime_error(vm, "Undefined variable '%s'", name->data);
        return false;
    }
    return true;
}

bool vm_op_get_upvalue(struct vm *vm)
{
    uint8_t slot = READ_U8(vm);
    stack_push(vm, *vm->frame->closure->upvalues[slot]->location);
    return true;
}

bool vm_op_set_upvalue(struct vm *vm)
{
    uint8_t slot = READ_U8(vm);
    *vm->frame->closure->upvalues[slot]->location = stack_peek(vm, 0);
    return true;
}

bool vm_op_get_super(struct vm *vm)
{
    struct object_string *name = READ_STRING(vm);
    struct object_class *superclass = AS_CLASS(stack_pop(vm));

    if (!bind_method(vm, superclass, name)) {
        return false;
    }
    return true;
}

bool vm_op_get_property(struct vm *vm)
{
    if (!IS_INSTANCE(stack_peek(vm, 0))) {
        vm_runtime_error(vm, "Only instances have properties");
        return false;
    }
    struct object_instance *instance = AS_INSTANCE(stack_peek(vm, 0));
    struct object_string *name = READ_STRING(vm);
    value value;
    if (table_get(&instance->fields, name, &value)) {
        stack_pop(vm);  // instance
        stack_push(vm, value);
        return true;
    }
    if (!bind_method(vm, instance->klass, name)) {
        return false;
    }
    return false;
}

bool vm_op_set_property(struct vm *vm)
{
    if (!IS_INSTANCE(stack_peek(vm, 1))) {
        vm_runtime_error(vm, "Only instances have fields");
        return false;
    }
    struct object_instance *instance = AS_INSTANCE(stack_peek(vm, 1));
    table_set(&instance->fields, READ_STRING(vm), stack_peek(vm, 0));
    value value = stack_pop(vm);
    stack_pop(vm);
    stack_push(vm, value);
    return true;
}

bool vm_op_equal(struct vm *vm)
{
    value b = stack_pop(vm);
    value a = stack_pop(vm);
    stack_push(vm, BOOL_VAL(value_equal(a, b)));
    return true;
}

bool vm_op_greater(struct vm *vm)
{
    BINARY_OP(BOOL_VAL, >);
    return true;
}

bool vm_op_less(struct vm *vm)
{
    BINARY_OP(BOOL_VAL, <);
    return true;
}

bool vm_op_add(struct vm *vm)
{
    if (IS_STRING(stack_peek(vm, 0)) && IS_STRING(stack_peek(vm, 1))) {
        /* keep s1 and s2 on the stack until s3 is added
         * This helps avoid gc-related issues
         */
        struct object_string *s2 = AS_STRING(stack_peek(vm, 0));
        struct object_string *s1 = AS_STRING(stack_peek(vm, 1));
        size_t new_length = s2->length + s1->length;
        char *data = reallocate(NULL, 0, new_length + 1);
        memcpy(data, s1->data, s1->length);
        memcpy(&data[s1->length], s2->data, s2->length);
        data[new_length] = '\0';
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
        return false;
    }
    return true;
}

bool vm_op_subtract(struct vm *vm)
{
    BINARY_OP(NUMBER_VAL, -);
    return true;
}

bool vm_op_multiply(struct vm *vm)
{
    BINARY_OP(NUMBER_VAL, *);
    return true;
}

bool vm_op_divide(struct vm *vm)
{
    BINARY_OP(NUMBER_VAL, /);
    return true;
}

bool vm_op_mod(struct vm *vm)
{
    BINARY_FUNC(NUMBER_VAL, fmod);
    return true;
}

bool vm_op_shl(struct vm *vm)
{
    BINARY_FUNC(NUMBER_VAL, fshl);
    return true;
}

bool vm_op_shr(struct vm *vm)
{
    BINARY_FUNC(NUMBER_VAL, fshr);
    return true;
}

bool vm_op_not(struct vm *vm)
{
    stack_push(vm, BOOL_VAL(is_falsey(stack_pop(vm))));
    return true;
}

bool vm_op_print(struct vm *vm)
{
    value_print(stack_pop(vm));
    printf("\n");
    return true;
}

bool vm_op_jump(struct vm *vm)
{
    uint16_t offset = READ_U16(vm);
    vm->frame->ip += offset;
    return true;
}

bool vm_op_jump_if_false(struct vm *vm)
{
    uint16_t offset = READ_U16(vm);
    if (is_falsey(stack_peek(vm, 0))) {
        vm->frame->ip += offset;
    }
    return true;
}

bool vm_op_jump_if_true(struct vm *vm)
{
    uint16_t offset = READ_U16(vm);
    if (!is_falsey(stack_peek(vm, 0))) {
        vm->frame->ip += offset;
    }
    return true;
}

bool vm_op_loop(struct vm *vm)
{
    uint16_t offset = READ_U16(vm);
    vm->frame->ip -= offset;
    return true;
}

bool vm_op_call(struct vm *vm)
{
    int arg_count = READ_U8(vm);
    if (!call_value(vm, stack_peek(vm, arg_count), arg_count)) {
        return false;
    }
    vm->frame = &vm->frames[vm->frame_count - 1];
    return true;
}

bool vm_op_invoke(struct vm *vm)
{
    struct object_string *method = READ_STRING(vm);
    int arg_count = READ_U8(vm);
    if (!invoke(vm, method, arg_count)) {
        return false;
    }
    vm->frame = &vm->frames[vm->frame_count - 1];
    return true;
}

bool vm_op_super_invoke(struct vm *vm)
{
    struct object_string *method = READ_STRING(vm);
    int arg_count = READ_U8(vm);
    struct object_class *superclass = AS_CLASS(stack_pop(vm));
    if (!invoke_from_class(vm, superclass, method, arg_count)) {
        return false;
    }
    vm->frame = &vm->frames[vm->frame_count - 1];
    return true;
}

bool vm_op_closure(struct vm *vm)
{
    struct object_function *function = AS_FUNCTION(READ_CONSTANT(vm));
    struct object_closure *closure = object_closure_new(function);
    stack_push(vm, OBJECT_VAL(closure));
    for (int i = 0; i < closure->nupvalues; i++) {
        uint8_t is_local = READ_U8(vm);
        uint8_t index = READ_U8(vm);

        if (is_local) {
            closure->upvalues[i] = capture_upvalue(vm, vm->frame->slots + index);
        } else {
            closure->upvalues[i] = vm->frame->closure->upvalues[index];
        }
    }
    return true;
}

bool vm_op_close_upvalue(struct vm *vm)
{
    close_upvalues(vm, vm->sp - 1);
    stack_pop(vm);
    return true;
}

bool vm_op_return(struct vm *vm)
{
    value result = stack_pop(vm);
    close_upvalues(vm, vm->frame->slots);
    vm->frame_count--;
    if (vm->frame_count == 0) {
        stack_pop(vm);
        return false;
    }
    vm->sp = vm->frame->slots;
    stack_push(vm, result);
    vm->frame = &vm->frames[vm->frame_count - 1];
    return true;
}

bool vm_op_negate(struct vm *vm)
{
    if (!IS_NUMBER(stack_peek(vm, 0))) {
        vm_runtime_error(vm, "Operand must be a number");
        return false;
    }
    stack_push(vm, NUMBER_VAL(-AS_NUMBER(stack_pop(vm))));
    return true;
}

bool vm_op_class(struct vm *vm)
{
    stack_push(vm, OBJECT_VAL(object_class_new(READ_STRING(vm))));
    return true;
}

bool vm_op_method(struct vm *vm)
{
    define_method(vm, READ_STRING(vm));
    return true;
}

bool vm_op_inherit(struct vm *vm)
{
    value superclass = stack_peek(vm, 1);
    if (!IS_CLASS(superclass)) {
        vm_runtime_error(vm, "Superclass must be a class");
        return false;
    }
    struct object_class *subclass = AS_CLASS(stack_peek(vm, 0));
    table_add_all(&AS_CLASS(superclass)->methods, &subclass->methods);
    stack_pop(vm);  // subclass
    return true;
}

typedef bool (*opcode_impl)(struct vm *vm);
static opcode_impl opcode_handlers[UINT8_MAX + 1] = {
    [OP_CONSTANT] = vm_op_constant,
    [OP_NIL] = vm_op_nil,
    [OP_FALSE] = vm_op_false,
    [OP_POP] = vm_op_pop,
    [OP_TRUE] = vm_op_true,
    [OP_GET_PROPERTY] = vm_op_get_property,
    [OP_SET_PROPERTY] = vm_op_set_property,
    [OP_GET_SUPER] = vm_op_get_super,
    [OP_EQUAL] = vm_op_equal,
    [OP_GREATER] = vm_op_greater,
    [OP_LESS] = vm_op_less,
    [OP_ADD] = vm_op_add,
    [OP_SUBTRACT] = vm_op_subtract,
    [OP_MULTIPLY] = vm_op_multiply,
    [OP_DIVIDE] = vm_op_divide,
    [OP_MOD] = vm_op_mod,
    [OP_SHL] = vm_op_shl,
    [OP_SHR] = vm_op_shr,
    [OP_NEGATE] = vm_op_negate,
    [OP_NOT] = vm_op_not,
    [OP_DEFINE_GLOBAL] = vm_op_define_global,
    [OP_GET_GLOBAL] = vm_op_get_global,
    [OP_SET_GLOBAL] = vm_op_set_global,
    [OP_GET_LOCAL] = vm_op_get_local,
    [OP_SET_LOCAL] = vm_op_set_local,
    [OP_GET_UPVALUE] = vm_op_get_upvalue,
    [OP_SET_UPVALUE] = vm_op_set_upvalue,
    [OP_JUMP_IF_FALSE] = vm_op_jump_if_false,
    [OP_JUMP_IF_TRUE] = vm_op_jump_if_true,
    [OP_JUMP] = vm_op_jump,
    [OP_LOOP] = vm_op_loop,
    [OP_PRINT] = vm_op_print,
    [OP_CALL] = vm_op_call,
    [OP_CLOSE_UPVALUE] = vm_op_close_upvalue,
    [OP_CLOSURE] = vm_op_closure,
    [OP_RETURN] = vm_op_return,
    [OP_CLASS] = vm_op_class,
    [OP_METHOD] = vm_op_method,
    [OP_INVOKE] = vm_op_invoke,
    [OP_SUPER_INVOKE] = vm_op_super_invoke,
    [OP_INHERIT] = vm_op_inherit,
};

int vm_run(struct vm *vm)
{
    vm->frame = &vm->frames[vm->frame_count - 1];
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
        disassemble_instruction(&vm->frame->closure->function->chunk,
                                (int)(vm->frame->ip - vm->frame->closure->function->chunk.code));

#endif
        enum opcode inst = READ_OPCODE(vm);
        opcode_impl handler = opcode_handlers[inst];
        if (!handler(vm)) {
            return -1;
        }
    }
    return 0;
}

struct bytecode_header {
    uint32_t magic;
    uint8_t vm_ver_major;
    uint8_t vm_ver_minor;
    uint16_t header_size;
    uint64_t timestamp;
} __attribute__((packed));

int vm_dump_bytecode(struct vm *vm, struct object_function *function)
{
    (void)vm;
    FILE *f = fopen("bytecode.dpc", "wb");

    struct bytecode_header header = {
        .magic = BYTECODE_MAGIC,
        .header_size = sizeof(struct bytecode_header),
        .timestamp = time(NULL),
        .vm_ver_major = 0,
        .vm_ver_minor = 1,
    };

    fwrite(&header, 1, sizeof(header), f);

    printf("Dumping %d bytes of byutecode\n", function->chunk.count);
    fwrite(&function->chunk.count, 1, sizeof(function->chunk.count), f);
    fwrite(function->chunk.code, 1, function->chunk.count, f);

    fwrite(&function->chunk.constants.count, 1, sizeof(function->chunk.constants.count), f);
    printf("Dumping %d constants\n", function->chunk.constants.count);
    for (int i = 0; i < function->chunk.constants.count; i++) {
        value v = function->chunk.constants.values[i];
        uint8_t type = (uint8_t)v.type;
        void *data = NULL;
        size_t dsize = 0;
        switch (v.type) {
            case VAL_BOOL:
                data = &v.as.boolean;
                dsize = sizeof(v.as.boolean);
                break;
            case VAL_NIL:
                data = &v.as.number;
                dsize = sizeof(v.as.number);
                break;
            case VAL_NUMBER:
                data = &v.as.number;
                dsize = sizeof(v.as.number);
                break;
            case VAL_OBJECT:
                break;
        }
        printf("Type %d, size %ld, data=%p\n", type, dsize, data);
        fwrite(&type, 1, sizeof(type), f);
        if (data != NULL) {
            fwrite(data, 1, dsize, f);
        }
    }
    fclose(f);
    return 0;
}

int vm_interpret(struct vm *vm, const char *source)
{
    struct object_function *function = compile(source);
    if (function == NULL) {
        return -1;
    }

    // m_dump_bytecode(vm, function);

    stack_push(vm, OBJECT_VAL(function));

    struct object_closure *closure = object_closure_new(function);
    stack_pop(vm);
    stack_push(vm, OBJECT_VAL(closure));
    call(vm, closure, 0);

    return vm_run(vm);
}
