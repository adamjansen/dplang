#include "compiler.h"
#include "chunk.h"
#include "scanner.h"
#include "object.h"
#include "parser.h"
#include "memory.h"
#include "util.h"

#include <ctype.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

// #define DEBUG_BYTECODE

static void *memmem(const void *haystack, size_t haystack_len, const void *needle, size_t needle_len)
{
    if (needle_len == 0) {
        return (void *)haystack;
    }

    if (haystack_len < needle_len) {
        return NULL;
    }

    const uint8_t *const end = (const uint8_t *)haystack + haystack_len - needle_len;
    for (const uint8_t *begin = (const uint8_t *)haystack; begin <= end; begin++) {
        if (!memcmp((const void *)begin, (const void *)needle, needle_len)) {
            return (void *)begin;
        }
    }
    return NULL;
}

#define SCRIPT_NAME        "<script>"
#define SCRIPT_NAME_LENGTH strlen(SCRIPT_NAME)

#define BINARY_LITERAL_MAX_LENGTH 32

#define ARG_MAX UINT8_MAX

#define DUMMY_JUMP_TARGET 0xFFFF

#define _PLACEHOLDER_JUMP_INST(op)                                 \
    {                                                              \
        (op), U16LSB(DUMMY_JUMP_TARGET), U16MSB(DUMMY_JUMP_TARGET) \
    }

#define PLACEHOLDER_JUMP_INST          _PLACEHOLDER_JUMP_INST(OP_JUMP)
#define PLACEHOLDER_JUMP_IF_FALSE_INST _PLACEHOLDER_JUMP_INST(OP_JUMP_IF_FALSE)

struct local {
    struct token name;
    int level;
    bool is_captured;
};

struct upvalue {
    uint8_t index;
    bool is_local;
};

enum function_type {
    TYPE_FUNCTION,
    TYPE_INITIALIZER,
    TYPE_METHOD,
    TYPE_SCRIPT,
};

struct class_compiler {
    struct class_compiler *enclosing;
    bool has_superclass;
};

struct block {
    int loop_top;
    int loop_bottom;
    int loop_scope_level;
    struct block *previous;
};

struct compiler {
    struct object_function *function;
    enum function_type type;
    struct local locals[UINT8_MAX + 1];
    struct parser *parser;
    int nlocals;
    struct upvalue upvalues[UINT8_MAX + 1];
    int scope_level;
    struct compiler *enclosing;
    struct class_compiler *current_class;
    struct block *block;
};

static struct compiler *current = NULL;

static void grouping(struct parser *parser, enum precedence precedence, void *userdata);
static void binary(struct parser *parser, enum precedence precedence, void *userdata);
static void unary(struct parser *parser, enum precedence precedence, void *userdata);
static void number(struct parser *parser, enum precedence precedence, void *userdata);
static void literal(struct parser *parser, enum precedence precedence, void *userdata);
static void string(struct parser *parser, enum precedence precedence, void *userdata);
static void variable(struct parser *parser, enum precedence precedence, void *userdata);
static void and_(struct parser *parser, enum precedence precedence, void *userdata);
static void or_(struct parser *parser, enum precedence precedence, void *userdata);
static void call(struct parser *parser, enum precedence precedence, void *userdata);
static void dot(struct parser *parser, enum precedence precedence, void *userdata);
static void this_(struct parser *parser, enum precedence precedence, void *userdata);
static void super_(struct parser *parser, enum precedence precedence, void *userdata);
static void index_(struct parser *parser, enum precedence precedence, void *userdata);

struct parse_rule rules[] = {
    [TOKEN_LEFT_PAREN] = {grouping, call,   PREC_CALL      },
    [TOKEN_RIGHT_PAREN] = {NULL,     NULL,   PREC_NONE      },
    [TOKEN_LEFT_BRACE] = {NULL,     NULL,   PREC_NONE      },
    [TOKEN_RIGHT_BRACE] = {NULL,     NULL,   PREC_NONE      },
    [TOKEN_LEFT_BRACKET] = {NULL,     index_, PREC_CALL      },
    [TOKEN_RIGHT_BRACKET] = {NULL,     NULL,   PREC_NONE      },
    [TOKEN_COMMA] = {NULL,     NULL,   PREC_NONE      },
    [TOKEN_DOT] = {NULL,     dot,    PREC_CALL      },
    [TOKEN_MINUS] = {unary,    binary, PREC_TERM      },
    [TOKEN_PLUS] = {NULL,     binary, PREC_TERM      },
    [TOKEN_SEMICOLON] = {NULL,     NULL,   PREC_NONE      },
    [TOKEN_SLASH] = {NULL,     binary, PREC_FACTOR    },
    [TOKEN_STAR] = {NULL,     binary, PREC_FACTOR    },
    [TOKEN_PERCENT] = {NULL,     binary, PREC_FACTOR    },
    [TOKEN_TILDE] = {unary,    NULL,   PREC_NONE      },
    [TOKEN_BANG] = {unary,    NULL,   PREC_NONE      },
    [TOKEN_BANG_EQUAL] = {NULL,     binary, PREC_EQUALITY  },
    [TOKEN_EQUAL] = {NULL,     NULL,   PREC_NONE      },
    [TOKEN_EQUAL_EQUAL] = {NULL,     binary, PREC_EQUALITY  },
    [TOKEN_GREATER] = {NULL,     binary, PREC_COMPARISON},
    [TOKEN_GREATER_GREATER] = {NULL,     binary, PREC_TERM      },
    [TOKEN_GREATER_EQUAL] = {NULL,     binary, PREC_COMPARISON},
    [TOKEN_LESS] = {NULL,     binary, PREC_COMPARISON},
    [TOKEN_LESS_LESS] = {NULL,     binary, PREC_TERM      },
    [TOKEN_LESS_EQUAL] = {NULL,     binary, PREC_COMPARISON},
    [TOKEN_IDENTIFIER] = {variable, NULL,   PREC_NONE      },
    [TOKEN_STRING] = {string,   NULL,   PREC_NONE      },
    [TOKEN_NUMBER] = {number,   NULL,   PREC_NONE      },
    [TOKEN_AND] = {NULL,     and_,   PREC_AND       },
    [TOKEN_CLASS] = {NULL,     NULL,   PREC_NONE      },
    [TOKEN_ELSE] = {NULL,     NULL,   PREC_NONE      },
    [TOKEN_FALSE] = {literal,  NULL,   PREC_NONE      },
    [TOKEN_FOR] = {NULL,     NULL,   PREC_NONE      },
    [TOKEN_FUNC] = {NULL,     NULL,   PREC_NONE      },
    [TOKEN_IF] = {NULL,     NULL,   PREC_NONE      },
    [TOKEN_NIL] = {literal,  NULL,   PREC_NONE      },
    [TOKEN_OR] = {NULL,     or_,    PREC_OR        },
    [TOKEN_CARET] = {NULL,     binary, PREC_NONE      },
    [TOKEN_PRINT] = {NULL,     NULL,   PREC_NONE      },
    [TOKEN_RETURN] = {NULL,     NULL,   PREC_NONE      },
    [TOKEN_SUPER] = {super_,   NULL,   PREC_NONE      },
    [TOKEN_THIS] = {this_,    NULL,   PREC_NONE      },
    [TOKEN_TRUE] = {literal,  NULL,   PREC_NONE      },
    [TOKEN_VAR] = {NULL,     NULL,   PREC_NONE      },
    [TOKEN_WHILE] = {NULL,     NULL,   PREC_NONE      },
    [TOKEN_ERROR] = {NULL,     NULL,   PREC_NONE      },
    [TOKEN_EOF] = {NULL,     NULL,   PREC_NONE      },
};

struct parse_rule *parser_get_rule(enum token_type type)
{
    return &rules[type];
}

static void declaration(struct compiler *compiler);
static void expression(struct compiler *compiler);
static void statement(struct compiler *compiler);
static void expression_statement(struct compiler *compiler);
static void if_statement(struct compiler *compiler);
static void while_statement(struct compiler *compiler);
static void continue_statement(struct compiler *compiler);
static void break_statement(struct compiler *compiler);
static void for_statement(struct compiler *compiler);
static void var_declaration(struct compiler *compiler);

static void named_variable(struct compiler *compiler, struct token name, bool assign_ok);

static void scope_enter(struct compiler *compiler);
static void scope_exit(struct compiler *compiler);

static struct token synthetic_token(struct compiler *compiler, const char *text);

static struct object_function *end(struct compiler *compiler);

static inline void emit_opcode_args(struct compiler *compiler, enum opcode op, void *operands, size_t length)
{
    chunk_write_opcode(&compiler->function->chunk, op, operands, length, compiler->parser->previous.line);
}

static inline void emit_opcode(struct compiler *compiler, enum opcode op)
{
    chunk_write_byte(&compiler->function->chunk, op, compiler->parser->previous.line);
}

static inline void emit_byte(struct compiler *compiler, uint8_t b)
{
    chunk_write_byte(&compiler->function->chunk, b, compiler->parser->previous.line);
}

static inline void emit_loop(struct compiler *compiler, int loop_start)
{
    int offset = compiler->function->chunk.count - loop_start + 3;
    if (offset > UINT16_MAX) {
        parser_error(compiler->parser, "Loop body too large");
        return;
    }

    uint16_t u16_offset = (uint16_t)offset;

    emit_opcode_args(compiler, OP_LOOP, &u16_offset, sizeof(u16_offset));
}

static inline int emit_jump(struct compiler *compiler, enum opcode jmp)
{
    uint16_t target = DUMMY_JUMP_TARGET;
    emit_opcode_args(compiler, jmp, &target, sizeof(target));
    return (int)compiler->function->chunk.count - 2;  // offset of to-be-patched jump destination
}

static void patch_jump(struct compiler *compiler, int offset)
{
    // account for byte of the jump offset itself
    size_t size = compiler->function->chunk.count - (size_t)offset - 2;

    if (size > UINT16_MAX) {
        parser_error(compiler->parser, "Too much code for jump");
    }

    compiler->function->chunk.code[offset] = U16LSB(size);
    compiler->function->chunk.code[offset + 1] = U16MSB(size);
}

static void emit_return(struct compiler *compiler)
{
    // Class initializers implicitly return initialized object
    if (compiler->type == TYPE_INITIALIZER) {
        uint8_t zero = 0;
        emit_opcode_args(compiler, OP_GET_LOCAL, &zero, sizeof(zero));
    } else {
        emit_opcode(compiler, OP_NIL);
    }
    emit_opcode(compiler, OP_RETURN);
}

static uint8_t make_constant(struct compiler *compiler, value value)
{
    gc_mark_value(value);
    int ret = chunk_add_constant(&compiler->function->chunk, value);
    if (ret > UINT8_MAX) {
        parser_error(compiler->parser, "Too many constants in one chunk");
        return 0;
    }
    return (uint8_t)ret;
}

static void compiler_init(struct compiler *compiler, struct parser *parser, enum function_type type)
{
    compiler->function = NULL;
    compiler->type = type;
    compiler->parser = parser;
    compiler->enclosing = current;

    compiler->nlocals = 0;
    compiler->scope_level = 0;

    compiler->block = NULL;

    current = compiler;

    compiler->function = object_function_new(NULL);
    if (type != TYPE_SCRIPT) {
        compiler->function->name =
            object_string_allocate(compiler->parser->previous.start, compiler->parser->previous.length);
    } else {
        compiler->function->name = object_string_allocate(SCRIPT_NAME, SCRIPT_NAME_LENGTH);
    }
    memset(compiler->upvalues, 0x00, sizeof(compiler->upvalues));

    struct local *local = &compiler->locals[compiler->nlocals++];
    local->level = 0;
    local->is_captured = false;
    if (type != TYPE_FUNCTION) {
        local->name.start = "this";
        local->name.length = 4;
    } else {
        local->name.start = "";
        local->name.length = 0;
    }
}

static uint8_t identifier_constant(struct compiler *compiler, struct token *name)
{
    struct object_string *s = object_string_allocate(name->start, name->length);
    return make_constant(compiler, OBJECT_VAL(s));
}

static void add_local(struct compiler *compiler, struct token name)
{
    if (compiler->nlocals > UINT8_MAX) {
        parser_error(compiler->parser, "Too many local variables in function");
        return;
    }
    struct local *local = &compiler->locals[compiler->nlocals++];
    local->name = name;
    local->level = -1;  // -1 indicates uninitialized
    local->is_captured = false;
}

static bool identifiers_equal(struct token *a, struct token *b)
{
    if (a->length != b->length) {
        return false;
    }
    return memcmp(a->start, b->start, a->length) == 0;
}

static int resolve_local(struct compiler *compiler, struct token *name)
{
    for (int i = compiler->nlocals - 1; i >= 0; i--) {
        struct local *local = &compiler->locals[i];
        if (identifiers_equal(name, &local->name)) {
            if (local->level == -1) {
                parser_error(compiler->parser, "Can't read local in its own initializer");
            }
            return i;
        }
    }
    return -1;
}

static int add_upvalue(struct compiler *compiler, uint8_t index, bool is_local)
{
    int nupvalues = compiler->function->nupvalues;
    // Check to see if upvalue was already added for same variable
    for (int i = 0; i < nupvalues; i++) {
        struct upvalue *upvalue = &compiler->upvalues[i];
        if (upvalue->index == index && upvalue->is_local == is_local) {
            return i;
        }
    }

    if (nupvalues > UINT8_MAX) {
        parser_error(compiler->parser, "Too many closure variables in function");
        return 0;
    }
    compiler->upvalues[nupvalues].is_local = is_local;
    compiler->upvalues[nupvalues].index = index;
    return compiler->function->nupvalues++;
}

// NOLINTNEXTLINE(misc-no-recursion)
static int resolve_upvalue(struct compiler *compiler, struct token *name)
{
    if (compiler->enclosing == NULL) {
        return -1;  // no upvalues at top-level scope
    }

    int local = resolve_local(compiler->enclosing, name);
    if (local != -1) {
        compiler->enclosing->locals[local].is_captured = true;
        return add_upvalue(compiler, (uint8_t)local, true);
    }

    int upvalue = resolve_upvalue(compiler->enclosing, name);
    if (upvalue != -1) {
        return add_upvalue(compiler, (uint8_t)upvalue, false);
    }

    return -1;
}

static void declare_variable(struct compiler *compiler)
{
    if (compiler->scope_level == 0) {
        return;
    }

    struct token *name = &compiler->parser->previous;

    for (int i = compiler->nlocals - 1; i >= 0; i--) {
        struct local *local = &compiler->locals[i];
        if (local->level != -1 && local->level < compiler->scope_level) {
            break;
        }

        if (identifiers_equal(name, &local->name)) {
            parser_error(compiler->parser, "Variable is already defined in this scope");
        }
    }
    add_local(compiler, *name);
}

static uint8_t parse_variable(struct compiler *compiler, const char *errmsg)
{
    parser_consume(compiler->parser, TOKEN_IDENTIFIER, errmsg);
    declare_variable(compiler);

    /* Locals aren't looked up by name at runtime, so
     * they don't need to be entered into the constant table
     */
    if (compiler->scope_level > 0) {
        return 0;
    }

    return identifier_constant(compiler, &compiler->parser->previous);
}

static void mark_initialized(struct compiler *compiler)
{
    if (compiler->scope_level == 0) {
        return;
    }
    compiler->locals[compiler->nlocals - 1].level = compiler->scope_level;
}

static void define_variable(struct compiler *compiler, uint8_t global)
{
    // Local variables don't have associated runtime code for declaration
    if (compiler->scope_level > 0) {
        mark_initialized(compiler);
        return;
    }
    emit_opcode_args(compiler, OP_DEFINE_GLOBAL, &global, sizeof(global));
}

static uint8_t argument_list(struct compiler *compiler)
{
    uint8_t arg_count = 0;
    if (!parser_check(compiler->parser, TOKEN_RIGHT_PAREN)) {
        do {
            expression(compiler);
            if (arg_count == ARG_MAX) {
                parser_error(compiler->parser, "Can't pass more than 255 arguments");
            }
            arg_count++;
        } while (parser_match(compiler->parser, TOKEN_COMMA));
    }
    parser_consume(compiler->parser, TOKEN_RIGHT_PAREN, "Expect ')' after arguments");
    return arg_count;
}

static void expression(struct compiler *compiler)
{
    parser_precedence(compiler->parser, PREC_ASSIGNMENT, compiler);
}

// NOLINTNEXTLINE(misc-no-recursion)
static void block(struct compiler *compiler)
{
    while (!parser_check(compiler->parser, TOKEN_RIGHT_BRACE) && !parser_check(compiler->parser, TOKEN_EOF)) {
        declaration(compiler);
    }

    parser_consume(compiler->parser, TOKEN_RIGHT_BRACE, "Expect '}' after block");
}

// NOLINTNEXTLINE(misc-no-recursion)
static void function(struct compiler *compiler, enum function_type type)
{
    struct compiler inner;
    compiler_init(&inner, compiler->parser, type);
    current = &inner;
    scope_enter(&inner);

    parser_consume(inner.parser, TOKEN_LEFT_PAREN, "Expect '(' after function name");
    // gather arguments, if any
    if (!parser_check(inner.parser, TOKEN_RIGHT_PAREN)) {
        do {
            inner.function->arity++;
            if (inner.function->arity > ARG_MAX) {
                parser_error_at_current(inner.parser, "Can't have more than 255 parameters");
            }
            uint8_t constant = parse_variable(&inner, "Expect parameter name");
            define_variable(&inner, constant);
        } while (parser_match(inner.parser, TOKEN_COMMA));
    }
    parser_consume(inner.parser, TOKEN_RIGHT_PAREN, "Expect ')' after parameters");
    parser_consume(inner.parser, TOKEN_LEFT_BRACE, "Expect '{' before function body");
    block(&inner);

    struct object_function *function = end(&inner);

    uint8_t constant = make_constant(compiler, OBJECT_VAL(function));
    emit_opcode_args(compiler, OP_CLOSURE, &constant, sizeof(constant));

    for (int i = 0; i < function->nupvalues; i++) {
        emit_byte(compiler, inner.upvalues[i].is_local ? 1 : 0);
        emit_byte(compiler, inner.upvalues[i].index);
    }
}

// NOLINTNEXTLINE(misc-no-recursion)
static void method(struct compiler *compiler)
{
    parser_consume(compiler->parser, TOKEN_IDENTIFIER, "Expect method name");
    uint8_t constant = identifier_constant(compiler, &compiler->parser->previous);
    enum function_type type = TYPE_METHOD;
    if (compiler->parser->previous.length == 4 && memcmp(compiler->parser->previous.start, "init", 4) == 0) {
        type = TYPE_INITIALIZER;
    }
    function(compiler, type);
    emit_opcode_args(compiler, OP_METHOD, &constant, sizeof(constant));
}

// NOLINTNEXTLINE(misc-no-recursion)
static void class_declaration(struct compiler *compiler)
{
    parser_consume(compiler->parser, TOKEN_IDENTIFIER, "Expect class name");
    struct token class_name = compiler->parser->previous;
    uint8_t name_constant = identifier_constant(compiler, &compiler->parser->previous);
    declare_variable(compiler);

    emit_opcode_args(compiler, OP_CLASS, &name_constant, sizeof(name_constant));
    define_variable(compiler, name_constant);

    struct class_compiler class_compiler = {
        .enclosing = compiler->current_class,
        .has_superclass = false,
    };
    compiler->current_class = &class_compiler;

    if (parser_match(compiler->parser, TOKEN_LESS)) {
        parser_consume(compiler->parser, TOKEN_IDENTIFIER, "Expect superclass name");
        variable(compiler->parser, PREC_PRIMARY, (void *)compiler);

        if (identifiers_equal(&class_name, &compiler->parser->previous)) {
            parser_error(compiler->parser, "A class cannot inherit from itself");
        }

        scope_enter(compiler);
        add_local(compiler, synthetic_token(compiler, "super"));
        define_variable(compiler, 0);

        named_variable(compiler, class_name, false);
        emit_opcode(compiler, OP_INHERIT);
        class_compiler.has_superclass = true;
    }

    named_variable(compiler, class_name, false);

    parser_consume(compiler->parser, TOKEN_LEFT_BRACE, "Expect '{' before class body");
    while (!parser_check(compiler->parser, TOKEN_RIGHT_BRACE) && !parser_check(compiler->parser, TOKEN_EOF)) {
        method(compiler);
    }
    parser_consume(compiler->parser, TOKEN_RIGHT_BRACE, "Expect '}' after class body");

    emit_opcode(compiler, OP_POP);

    if (class_compiler.has_superclass) {
        scope_exit(compiler);
    }

    compiler->current_class = compiler->current_class->enclosing;
}

// NOLINTNEXTLINE(misc-no-recursion)
static void func_declaration(struct compiler *compiler)
{
    uint8_t global = parse_variable(compiler, "Expected function name");
    mark_initialized(compiler);
    function(compiler, TYPE_FUNCTION);
    define_variable(compiler, global);
}

static void var_declaration(struct compiler *compiler)
{
    uint8_t global = parse_variable(compiler, "Expect variable name");

    if (parser_match(compiler->parser, TOKEN_EQUAL)) {
        expression(compiler);
    } else {
        emit_opcode(compiler, OP_NIL);
    }
    parser_consume(compiler->parser, TOKEN_SEMICOLON, "Expect ';' after variable declaration");

    define_variable(compiler, global);
}

static void expression_statement(struct compiler *compiler)
{
    expression(compiler);
    parser_consume(compiler->parser, TOKEN_SEMICOLON, "Expect ';' after expression");
    emit_opcode(compiler, OP_POP);
}

static void continue_statement(struct compiler *compiler)
{
    parser_consume(compiler->parser, TOKEN_SEMICOLON, "Expect ';' after continue");
    if (compiler->block == NULL) {
        parser_error(compiler->parser, "Continue cannot be used outside of a loop");
        return;
    }
    // Discard loop's locals
    for (int i = compiler->nlocals - 1; i >= 0 && compiler->locals[i].level > compiler->block->loop_scope_level; i--) {
        emit_opcode(compiler, OP_POP);
    }

    emit_loop(compiler, compiler->block->loop_top);
}

static void break_statement(struct compiler *compiler)
{
    parser_consume(compiler->parser, TOKEN_SEMICOLON, "Expect ';' after break");
    if (compiler->block == NULL) {
        parser_error(compiler->parser, "Break cannot be used outside of a loop");
        return;
    }

    emit_jump(compiler, OP_JUMP);
}

// NOLINTNEXTLINE(misc-no-recursion)
static void for_statement(struct compiler *compiler)
{
    scope_enter(compiler);
    struct block block = {
        .previous = compiler->block,
        .loop_scope_level = compiler->scope_level,
        .loop_top = -1,
        .loop_bottom = -1,
    };

    /* due to recursive parsing, we can get away with
     * keeping a pointer to a stack variable. Once we
     * leave this scope, we won't have a reference to it
     * any more.  If it causes problems, we will need
     * to allocate from the heap.
     */
    compiler->block = &block;

    /* Initializer */
    parser_consume(compiler->parser, TOKEN_LEFT_PAREN, "Expect '(' after 'for'");
    if (parser_match(compiler->parser, TOKEN_VAR)) {
        var_declaration(compiler);
    } else if (parser_match(compiler->parser, TOKEN_SEMICOLON)) {
        // no initializer
    } else {
        expression_statement(compiler);
    }

    block.loop_top = compiler->function->chunk.count;

    /* Condition */
    if (!parser_match(compiler->parser, TOKEN_SEMICOLON)) {
        expression(compiler);
        parser_consume(compiler->parser, TOKEN_SEMICOLON, "Expect ';' after loop condition");
        block.loop_bottom = emit_jump(compiler, OP_JUMP_IF_FALSE);
        emit_opcode(compiler, OP_POP);  // don't leave the condition on the stack
    }

    /* Increment */
    if (!parser_match(compiler->parser, TOKEN_RIGHT_PAREN)) {
        int body_jump = emit_jump(compiler, OP_JUMP);
        int increment_start = compiler->function->chunk.count;
        expression(compiler);
        parser_consume(compiler->parser, TOKEN_RIGHT_PAREN, "Expect ')' after for clauses");
        emit_opcode(compiler, OP_POP);
        emit_loop(compiler, block.loop_top);
        block.loop_top = increment_start;
        patch_jump(compiler, body_jump);
    }

    statement(compiler);

    emit_loop(compiler, block.loop_top);

    block.loop_bottom = compiler->function->chunk.count;

    uint8_t jmpz[] = PLACEHOLDER_JUMP_IF_FALSE_INST;
    uint8_t i = 0;
    while (i < (compiler->function->chunk.count - sizeof(jmpz))) {
        uint8_t *p = (uint8_t *)memmem(&compiler->function->chunk.code[i], compiler->function->chunk.count - i, jmpz,
                                       sizeof(jmpz));
        if (p == NULL) {
            break;
        }
        int jump_distance = block.loop_bottom - (int)(p - compiler->function->chunk.code) - i - 3;
        p[1] = U16LSB(jump_distance);
        p[2] = U16MSB(jump_distance);
        i += (p - compiler->function->chunk.code);
    }

    uint8_t jmp[] = PLACEHOLDER_JUMP_INST;
    uint8_t j = 0;
    while (j < (compiler->function->chunk.count - sizeof(jmp))) {
        uint8_t *p = (uint8_t *)memmem(&compiler->function->chunk.code[j], compiler->function->chunk.count - j, jmp,
                                       sizeof(jmp));
        if (p == NULL) {
            break;
        }
        int jump_distance = block.loop_bottom - (int)(p - compiler->function->chunk.code) - j;
        p[1] = U16LSB(jump_distance);
        p[2] = U16MSB(jump_distance);
        j += (p - compiler->function->chunk.code);
    }

    compiler->block = block.previous;

    scope_exit(compiler);
    emit_opcode(compiler, OP_POP);
}

// NOLINTNEXTLINE(misc-no-recursion)
static void if_statement(struct compiler *compiler)
{
    parser_consume(compiler->parser, TOKEN_LEFT_PAREN, "Expect '(' after 'if'");
    expression(compiler);
    parser_consume(compiler->parser, TOKEN_RIGHT_PAREN, "Expect ')' after condition'");

    int then_jump = emit_jump(compiler, OP_JUMP_IF_FALSE);
    emit_opcode(compiler, OP_POP);
    statement(compiler);

    int else_jump = emit_jump(compiler, OP_JUMP);
    patch_jump(compiler, then_jump);
    emit_opcode(compiler, OP_POP);

    if (parser_match(compiler->parser, TOKEN_ELSE)) {
        statement(compiler);
    }
    patch_jump(compiler, else_jump);
}

static void print_statement(struct compiler *compiler)
{
    expression(compiler);
    parser_consume(compiler->parser, TOKEN_SEMICOLON, "Expect ';' after value");
    emit_opcode(compiler, OP_PRINT);
}

static void return_statement(struct compiler *compiler)
{
    if (compiler->type == TYPE_SCRIPT) {
        parser_error(compiler->parser, "Can't return from top-level code");
    }
    if (parser_match(compiler->parser, TOKEN_SEMICOLON)) {
        emit_return(compiler);
    } else {
        // Initializers can return early, but can't return a value
        if (compiler->type == TYPE_INITIALIZER) {
            parser_error(compiler->parser, "Can't return a value from an initializer");
        }
        expression(compiler);
        parser_consume(compiler->parser, TOKEN_SEMICOLON, "Expect ';' after return value");
        emit_opcode(compiler, OP_RETURN);
    }
}

// NOLINTNEXTLINE(misc-no-recursion)
static void while_statement(struct compiler *compiler)
{
    struct block block = {
        .previous = compiler->block,
        .loop_scope_level = compiler->scope_level,
        .loop_top = compiler->function->chunk.count,
        .loop_bottom = -1,
    };

    // Keeping stack pointer is ok due to how this field is used.
    compiler->block = &block;

    scope_enter(compiler);

    parser_consume(compiler->parser, TOKEN_LEFT_PAREN, "Expect '(' after 'while'");
    expression(compiler);
    parser_consume(compiler->parser, TOKEN_RIGHT_PAREN, "Expect ')' after condition");

    // test the condition
    emit_jump(compiler, OP_JUMP_IF_FALSE);
    // Condition is true; pop it and resume body
    emit_opcode(compiler, OP_POP);

    // loop body
    statement(compiler);

    // jump back to the top of the loop
    emit_loop(compiler, block.loop_top);

    block.loop_bottom = compiler->function->chunk.count;
    emit_opcode(compiler, OP_POP);

    uint8_t jmpz[] = PLACEHOLDER_JUMP_IF_FALSE_INST;
    uint8_t i = 0;
    while (i < (compiler->function->chunk.count - sizeof(jmpz))) {
        uint8_t *p = (uint8_t *)memmem(&compiler->function->chunk.code[i], compiler->function->chunk.count - i, jmpz,
                                       sizeof(jmpz));
        if (p == NULL) {
            break;
        }
        int jump_distance = block.loop_bottom - (int)(p - compiler->function->chunk.code) - i - 3;
        p[1] = U16LSB(jump_distance);
        p[2] = U16MSB(jump_distance);
        i += (p - compiler->function->chunk.code);
    }

    uint8_t jmp[] = PLACEHOLDER_JUMP_INST;
    uint8_t j = 0;
    while (j < (compiler->function->chunk.count - sizeof(jmp))) {
        uint8_t *p = (uint8_t *)memmem(&compiler->function->chunk.code[j], compiler->function->chunk.count - j, jmp,
                                       sizeof(jmp));
        if (p == NULL) {
            break;
        }
        int jump_distance = block.loop_bottom - (int)(p - compiler->function->chunk.code) - j - 3 + 1;
        p[1] = U16LSB(jump_distance);
        p[2] = U16MSB(jump_distance);
        j += (p - compiler->function->chunk.code);
    }

    compiler->block = block.previous;
    scope_exit(compiler);
}

// NOLINTNEXTLINE(misc-no-recursion)
static void declaration(struct compiler *compiler)
{
    if (parser_match(compiler->parser, TOKEN_CLASS)) {
        class_declaration(compiler);
    } else if (parser_match(compiler->parser, TOKEN_FUNC)) {
        func_declaration(compiler);
    } else if (parser_match(compiler->parser, TOKEN_VAR)) {
        var_declaration(compiler);
    } else {
        statement(compiler);
    }

    if (compiler->parser->panic) {
        parser_synchronize(compiler->parser);
    }
}

// NOLINTNEXTLINE(misc-no-recursion)
static void statement(struct compiler *compiler)
{
    if (parser_match(compiler->parser, TOKEN_PRINT)) {
        print_statement(compiler);
    } else if (parser_match(compiler->parser, TOKEN_FOR)) {
        for_statement(compiler);
    } else if (parser_match(compiler->parser, TOKEN_IF)) {
        if_statement(compiler);
    } else if (parser_match(compiler->parser, TOKEN_RETURN)) {
        return_statement(compiler);
    } else if (parser_match(compiler->parser, TOKEN_WHILE)) {
        while_statement(compiler);
    } else if (parser_match(compiler->parser, TOKEN_BREAK)) {
        break_statement(compiler);
    } else if (parser_match(compiler->parser, TOKEN_CONTINUE)) {
        continue_statement(compiler);
    } else if (parser_match(compiler->parser, TOKEN_LEFT_BRACE)) {
        scope_enter(compiler);
        block(compiler);
        scope_exit(compiler);
    } else {
        expression_statement(compiler);
    }
}

static void named_variable(struct compiler *compiler, struct token name, bool assign_ok)
{
    enum opcode op_get;
    enum opcode op_set;
    uint8_t arg;

    /*
     * Resolution order is Local -> Upvalue -> Global
     */
    int ret = resolve_local(compiler, &name);
    if (ret != -1) {
        op_get = OP_GET_LOCAL;
        op_set = OP_SET_LOCAL;
        arg = (uint8_t)ret;
    } else {
        ret = resolve_upvalue(compiler, &name);
        if (ret != -1) {
            op_get = OP_GET_UPVALUE;
            op_set = OP_SET_UPVALUE;
            arg = (uint8_t)ret;
        } else {
            // Assume global will be available at runtime
            ret = identifier_constant(compiler, &name);
            op_get = OP_GET_GLOBAL;
            op_set = OP_SET_GLOBAL;
            arg = (uint8_t)ret;
        }
    }

    if (assign_ok && parser_match(compiler->parser, TOKEN_EQUAL)) {
        expression(compiler);
        emit_opcode_args(compiler, op_set, &arg, sizeof(arg));
    } else {
        emit_opcode_args(compiler, op_get, &arg, sizeof(arg));
    }
}

static void index_(struct parser *parser, enum precedence precedence, void *userdata)
{
    (void)precedence;
    struct compiler *compiler = (struct compiler *)userdata;
    if (parser_check(compiler->parser, TOKEN_RIGHT_BRACKET)) {
        parser_error(parser, "Exression required");
    }
    expression(compiler);
    parser_consume(parser, TOKEN_RIGHT_BRACKET, "Expect ']' after index");

    bool assign_ok = precedence <= PREC_ASSIGNMENT;

    if (assign_ok && parser_match(parser, TOKEN_EQUAL)) {
        expression(compiler);
        emit_opcode(compiler, OP_TABLE_SET);
    } else {
        emit_opcode(compiler, OP_TABLE_GET);
    }
}

static void grouping(struct parser *parser, enum precedence precedence, void *userdata)
{
    (void)precedence;
    struct compiler *compiler = (struct compiler *)userdata;
    expression(compiler);
    parser_consume(parser, TOKEN_RIGHT_PAREN, "Expect ')' after expression");
}

static void binary(struct parser *parser, enum precedence precedence, void *userdata)
{
    (void)precedence;
    struct compiler *compiler = (struct compiler *)userdata;
    enum token_type operator_type = parser->previous.type;

    struct parse_rule *rule = parser_get_rule(operator_type);

    parser_precedence(parser, (enum precedence)(rule->precedence + 1), compiler);

    switch (operator_type) {
        case TOKEN_PLUS:
            emit_opcode(compiler, OP_ADD);
            break;
        case TOKEN_MINUS:
            emit_opcode(compiler, OP_SUBTRACT);
            break;
        case TOKEN_STAR:
            emit_opcode(compiler, OP_MULTIPLY);
            break;
        case TOKEN_PERCENT:
            emit_opcode(compiler, OP_MOD);
            break;
        case TOKEN_SLASH:
            emit_opcode(compiler, OP_DIVIDE);
            break;
        case TOKEN_BANG_EQUAL:
            emit_opcode(compiler, OP_EQUAL);
            emit_opcode(compiler, OP_NOT);
            break;
        case TOKEN_EQUAL_EQUAL:
            emit_opcode(compiler, OP_EQUAL);
            break;
        case TOKEN_GREATER:
            emit_opcode(compiler, OP_GREATER);
            break;
        case TOKEN_GREATER_GREATER:
            emit_opcode(compiler, OP_SHR);
            break;
        case TOKEN_GREATER_EQUAL:
            emit_opcode(compiler, OP_LESS);
            emit_opcode(compiler, OP_NOT);
            break;
        case TOKEN_LESS:
            emit_opcode(compiler, OP_LESS);
            break;
        case TOKEN_LESS_LESS:
            emit_opcode(compiler, OP_SHL);
            break;
        case TOKEN_LESS_EQUAL:
            emit_opcode(compiler, OP_GREATER);
            emit_opcode(compiler, OP_NOT);
            break;
        default:
            return;
    }
}

static void call(struct parser *parser, enum precedence precedence, void *userdata)
{
    (void)parser;
    (void)precedence;
    struct compiler *compiler = (struct compiler *)userdata;

    uint8_t arg_count = argument_list(compiler);
    emit_opcode_args(compiler, OP_CALL, &arg_count, sizeof(arg_count));
}

static void dot(struct parser *parser, enum precedence precedence, void *userdata)
{
    struct compiler *compiler = (struct compiler *)userdata;

    parser_consume(parser, TOKEN_IDENTIFIER, "Expect property name after '.'");
    uint8_t name = identifier_constant(compiler, &parser->previous);

    bool assign_ok = precedence <= PREC_ASSIGNMENT;

    if (assign_ok && parser_match(parser, TOKEN_EQUAL)) {
        expression(compiler);
        emit_opcode_args(compiler, OP_SET_PROPERTY, &name, sizeof(name));
    } else if (parser_match(parser, TOKEN_LEFT_PAREN)) {
        /*
         * Take advantage of an optimization opportunity.
         *
         * Instead of creating a bound method object that can be called later --
         * usually in the next instruction -- fuse the steps together
         * into a superinstruction that is more efficient for the
         * typical case.
         */
        uint8_t arg_count = argument_list(compiler);
        uint8_t args[2] = {
            name,
            arg_count,
        };
        emit_opcode_args(compiler, OP_INVOKE, args, sizeof(args));
    } else {
        emit_opcode_args(compiler, OP_GET_PROPERTY, &name, sizeof(name));
    }
}

static void unary(struct parser *parser, enum precedence precedence, void *userdata)
{
    (void)precedence;
    struct compiler *compiler = (struct compiler *)userdata;
    enum token_type operator_type = parser->previous.type;

    parser_precedence(parser, PREC_UNARY, compiler);

    switch (operator_type) {
        case TOKEN_MINUS:
            emit_opcode(compiler, OP_NEGATE);
            break;
        case TOKEN_BANG:
            emit_opcode(compiler, OP_NOT);
            break;
        default:
            break;
    }
}

static void number(struct parser *parser, enum precedence precedence, void *userdata)
{
    (void)precedence;
    struct compiler *compiler = (struct compiler *)userdata;
    double val;
    if (parser->previous.start[0] == '0' && tolower(parser->previous.start[1]) == 'b') {
        if (parser->previous.length > BINARY_LITERAL_MAX_LENGTH) {
            parser_error_at(parser, &parser->previous, "Invalid binary literal");
        }
        uint32_t u32 = 0;
        for (uint32_t i = 0; i < parser->previous.length; i++) {
            u32 <<= 1;
            u32 |= (parser->previous.start[i] & 1);
        }
        val = (double)u32;

    } else {
        val = strtod(parser->previous.start, NULL);
    }
    uint8_t constant = make_constant(compiler, NUMBER_VAL(val));

    emit_opcode_args(compiler, OP_CONSTANT, &constant, sizeof(constant));
}

static void literal(struct parser *parser, enum precedence precedence, void *userdata)
{
    (void)precedence;
    struct compiler *compiler = (struct compiler *)userdata;
    switch (parser->previous.type) {
        case TOKEN_FALSE:
            emit_opcode(compiler, OP_FALSE);
            break;
        case TOKEN_TRUE:
            emit_opcode(compiler, OP_TRUE);
            break;
        case TOKEN_NIL:
            emit_opcode(compiler, OP_NIL);
            break;
        default:
            break;
    }
}

static inline uint8_t a2h(char c)
{
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    c = (char)tolower(c);
    if (c >= 'a' && c <= 'f') {
        return 0xa + (c - 'a');  // NOLINT(readability-magic-numbers)
    }
    return 0;
}

static size_t string_escape(char *dst, const char *src, size_t escaped_length)
{
    const char *s;
    char *d;
    for (s = src, d = dst; s < (src + escaped_length); s++) {
        switch (*s) {
            case '\\':
                s++;
                switch (*s) {
                    case 'a':
                        *d++ = '\a';  // Alert (bell)
                        break;
                    case 'b':
                        *d++ = '\b';  // backspace
                        break;
                    case 'e':
                        *d++ = '\x1b';  // Escape (\e works, but is non-standard)
                        break;
                    case 'f':
                        *d++ = '\f';  // form feed
                        break;
                    case 'n':
                        *d++ = '\n';  // new line
                        break;
                    case 'r':
                        *d++ = '\r';  // carriage return
                        break;
                    case 't':
                        *d++ = '\t';  // (horizontal) tab
                        break;
                    case 'v':
                        *d++ = '\v';  // (vertical) tab
                        break;
                    case '\\':
                        *d++ = '\\';  // backslash
                        break;
                    case '\'':
                        *d++ = '\\';  // single quote
                        break;
                    case '"':
                        *d++ = '"';  // double quote
                        break;
                    case 'x':
                        *d++ = (char)((a2h(s[1]) << 4) + a2h(s[2]));
                        s += 3;
                        break;
                    default:
                        *d++ = '\\';
                        break;
                }
                break;
            default:
                *d++ = *s;
                break;
        }
    }

    *d++ = '\0';

    return (size_t)(d - dst);
}

static void string(struct parser *parser, enum precedence precedence, void *userdata)
{
    (void)precedence;
    struct compiler *compiler = (struct compiler *)userdata;

    // TODO: Handle escape sequences
    size_t escaped_length = parser->previous.length - 2;
    char *unescaped = reallocate(NULL, 0, escaped_length);

    size_t unescaped_length = string_escape(unescaped, parser->previous.start + 1, escaped_length);

    struct object_string *s = object_string_allocate(unescaped, unescaped_length);
    uint8_t constant = make_constant(compiler, OBJECT_VAL(s));
    emit_opcode_args(compiler, OP_CONSTANT, &constant, sizeof(constant));
}

static void variable(struct parser *parser, enum precedence precedence, void *userdata)
{
    struct compiler *compiler = (struct compiler *)userdata;
    bool assign_ok = precedence <= PREC_ASSIGNMENT;
    named_variable(compiler, parser->previous, assign_ok);
}

static struct token synthetic_token(struct compiler *compiler, const char *text)
{
    struct token token = {
        .type = TOKEN_STRING,
        .line = compiler->parser->current.line,
        .start = text,
        .length = strlen(text),
    };
    return token;
}

static void super_(struct parser *parser, enum precedence precedence, void *userdata)
{
    (void)precedence;
    struct compiler *compiler = (struct compiler *)userdata;

    if (compiler->current_class == NULL) {
        parser_error(parser, "Can't use 'super' outside of a class");
    } else if (!compiler->current_class->has_superclass) {
        parser_error(parser, "Can't use 'super' in a class with no superclass");
    }

    parser_consume(parser, TOKEN_DOT, "Expect '.' after 'super'");
    parser_consume(parser, TOKEN_IDENTIFIER, "Expect superclass method name");
    uint8_t name = identifier_constant(compiler, &parser->previous);

    named_variable(compiler, synthetic_token(compiler, "this"), false);

    if (parser_match(parser, TOKEN_LEFT_PAREN)) {
        uint8_t arg_count = argument_list(compiler);
        uint8_t args[2] = {
            name,
            arg_count,
        };
        named_variable(compiler, synthetic_token(compiler, "super"), false);
        emit_opcode_args(compiler, OP_SUPER_INVOKE, args, sizeof(args));
    } else {
        named_variable(compiler, synthetic_token(compiler, "super"), false);
        emit_opcode_args(compiler, OP_GET_SUPER, &name, sizeof(name));
    }
}

static void this_(struct parser *parser, enum precedence precedence, void *userdata)
{
    (void)precedence;
    struct compiler *compiler = (struct compiler *)userdata;
    if (compiler->current_class == NULL) {
        parser_error(parser, "Cannot use 'this' outside of a class");
        return;
    }
    variable(parser, PREC_PRIMARY, userdata);
}

static void and_(struct parser *parser, enum precedence precedence, void *userdata)
{
    (void)precedence;
    struct compiler *compiler = (struct compiler *)userdata;
    int end_jump = emit_jump(compiler, OP_JUMP_IF_FALSE);
    emit_opcode(compiler, OP_POP);
    parser_precedence(parser, PREC_AND, compiler);

    patch_jump(compiler, end_jump);
}

static void or_(struct parser *parser, enum precedence precedence, void *userdata)
{
    (void)precedence;
    struct compiler *compiler = (struct compiler *)userdata;
    int end_jump = emit_jump(compiler, OP_JUMP_IF_TRUE);
    emit_opcode(compiler, OP_POP);
    parser_precedence(parser, PREC_OR, compiler);

    patch_jump(compiler, end_jump);
}

static struct object_function *end(struct compiler *compiler)
{
    emit_return(compiler);
    struct object_function *func = compiler->function;
#ifdef DEBUG_BYTECODE
    if (!compiler->parser->had_error) {
        chunk_disassemble(&compiler->function->chunk, func->name != NULL ? func->name->data : "<script>");
    }
#endif

    current = current->enclosing;

    return func;
}

static void scope_enter(struct compiler *compiler)
{
    compiler->scope_level++;
}

static void scope_exit(struct compiler *compiler)
{
    compiler->scope_level--;

    while (compiler->nlocals > 0 && compiler->locals[compiler->nlocals - 1].level > compiler->scope_level) {
        if (compiler->locals[compiler->nlocals - 1].is_captured) {
            emit_opcode(compiler, OP_CLOSE_UPVALUE);
        } else {
            emit_opcode(compiler, OP_POP);
        }
        compiler->nlocals--;
    }
}

struct object_function *compile(const char *source)
{
    struct parser parser = {
        .had_error = false,
        .panic = false,
    };
    parser_init(&parser, source);

    struct compiler compiler;

    compiler_init(&compiler, &parser, TYPE_SCRIPT);

    while (!parser_match(&parser, TOKEN_EOF)) { declaration(&compiler); }

    struct object_function *func = end(&compiler);

    return parser.had_error ? NULL : func;
}

void compiler_gc_roots()
{
    struct compiler *compiler = current;
    while (compiler != NULL) {
        gc_mark_object(&compiler->function->object);
        compiler = compiler->enclosing;
    }
}
