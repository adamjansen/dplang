#include "compiler.h"
#include "chunk.h"
#include "scanner.h"
#include "object.h"
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

struct parser {
    struct token current;
    struct token previous;
    struct chunk *chunk;
    struct scanner scanner;
    struct compiler *compiler;
    bool had_error;
    bool panic;
};

enum precedence {
    PREC_NONE,
    PREC_ASSIGNMENT,  // =
    PREC_OR,          // or
    PREC_AND,         // and
    PREC_EQUALITY,    // == !=
    PREC_COMPARISON,  // < > <= >=
    PREC_TERM,        // + -
    PREC_FACTOR,      // * /
    PREC_UNARY,       // ! -
    PREC_CALL,        // . ()
    PREC_PRIMARY,
};

typedef void (*parse_function)(struct parser *parser, bool assign_ok);

struct parse_rule {
    parse_function prefix;
    parse_function infix;
    enum precedence precedence;
};

struct local {
    struct token name;
    int level;
};

struct compiler {
    struct object_function *function;
    struct local locals[UINT8_MAX + 1];
    size_t nlocals;
    int scope_level;
    struct parser parser;
};

static void grouping(struct parser *parser, bool assign_ok);
static void binary(struct parser *parser, bool assign_ok);
static void unary(struct parser *parser, bool assign_ok);
static void number(struct parser *parser, bool assign_ok);
static void literal(struct parser *parser, bool assign_ok);
static void string(struct parser *parser, bool assign_ok);
static void variable(struct parser *parser, bool assign_ok);
static void and_(struct parser *parser, bool assign_ok);
static void or_(struct parser *parser, bool assign_ok);

static void declaration(struct parser *parser);
static void expression(struct parser *parser);
static void statement(struct parser *parser);
static void expression_statement(struct parser *parser);
static void if_statement(struct parser *parser);
static void while_statement(struct parser *parser);
static void for_statement(struct parser *parser);
static void var_declaration(struct parser *parser);

static void named_variable(struct parser *parser, struct token name, bool assign_ok);
static bool match(struct parser *parser, enum token_type type);

static void scope_enter(struct compiler *compiler);
static void scope_exit(struct compiler *compiler);

struct parse_rule rules[] = {
    [TOKEN_LEFT_PAREN] = {grouping, NULL,   PREC_NONE      },
    [TOKEN_RIGHT_PAREN] = {NULL,     NULL,   PREC_NONE      },
    [TOKEN_LEFT_BRACE] = {NULL,     NULL,   PREC_NONE      },
    [TOKEN_RIGHT_BRACE] = {NULL,     NULL,   PREC_NONE      },
    [TOKEN_COMMA] = {NULL,     NULL,   PREC_NONE      },
    [TOKEN_DOT] = {NULL,     NULL,   PREC_NONE      },
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
    [TOKEN_CARET] = {NULL,     binary, PREC_TERM      },
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
    [TOKEN_SUPER] = {NULL,     NULL,   PREC_NONE      },
    [TOKEN_THIS] = {NULL,     NULL,   PREC_NONE      },
    [TOKEN_TRUE] = {literal,  NULL,   PREC_NONE      },
    [TOKEN_VAR] = {NULL,     NULL,   PREC_NONE      },
    [TOKEN_WHILE] = {NULL,     NULL,   PREC_NONE      },
    [TOKEN_ERROR] = {NULL,     NULL,   PREC_NONE      },
    [TOKEN_EOF] = {NULL,     NULL,   PREC_NONE      },
};
static struct parse_rule *get_rule(enum token_type type);
static void parse_precedence(struct parser *parser, enum precedence precedence);

static void error_at(struct parser *parser, struct token *token, const char *message)
{
    if (parser->panic)
        return;
    parser->panic = true;
    fprintf(stderr, "[line %u] error", token->line);

    if (token->type == TOKEN_EOF) {
        fprintf(stderr, " at end");
    } else if (token->type == TOKEN_ERROR) {
    } else {
        fprintf(stderr, " at '%.*s'", token->length, token->start);
    }

    fprintf(stderr, ": %s\n", message);
    parser->had_error = true;
}

static void error(struct parser *parser, const char *message)
{
    error_at(parser, &parser->previous, message);
}

static void error_at_current(struct parser *parser, const char *message)
{
    error_at(parser, &parser->current, message);
}

static void advance(struct parser *parser)
{
    parser->previous = parser->current;

    while (1) {
        parser->current = scanner_scan_token(&parser->scanner);
        if (parser->current.type != TOKEN_ERROR)
            break;

        error_at_current(parser, parser->current.start);
    }
}

static void consume(struct parser *parser, enum token_type type, const char *message)
{
    if (parser->current.type == type) {
        advance(parser);
        return;
    }
    error_at_current(parser, message);
}

static bool check(struct parser *parser, enum token_type type)
{
    return parser->current.type == type;
}

static bool match(struct parser *parser, enum token_type type)
{
    if (!check(parser, type))
        return false;
    advance(parser);
    return true;
}

static inline void emit_opcode_args(struct parser *parser, enum opcode op, void *operands, size_t length)
{
    chunk_write_opcode(parser->chunk, op, operands, length, parser->previous.line);
}

static inline void emit_opcode(struct parser *parser, enum opcode op)
{
    emit_opcode_args(parser, op, NULL, 0);
}

static inline void emit_loop(struct parser *parser, int loop_start)
{
    uint16_t offset = parser->chunk->count - loop_start + 3;
    if (offset > UINT16_MAX)
        error(parser, "Loop body too large");

    emit_opcode_args(parser, OP_LOOP, &offset, sizeof(offset));
}

static inline int emit_jump(struct parser *parser, enum opcode jmp)
{
    uint16_t dummy = 0xFFFF;
    emit_opcode_args(parser, jmp, &dummy, sizeof(dummy));
    return parser->chunk->count - 2;  // offset of to-be-patched jump destination
}

static void patch_jump(struct parser *parser, int offset)
{
    // account for byte of the jump offset itsefl
    int size = parser->chunk->count - offset - 2;

    if (size > UINT16_MAX) {
        error(parser, "Too much code for jump");
    }

    parser->chunk->code[offset] = (size >> 8) & 0xFF;
    parser->chunk->code[offset + 1] = size & 0xFF;
}

static void compiler_init(struct compiler *compiler, struct chunk *chunk, const char *source)
{
    compiler->parser.chunk = chunk;
    compiler->parser.had_error = false;
    compiler->parser.panic = false;
    compiler->parser.compiler = compiler;

    chunk_init(chunk);

    scanner_init(&compiler->parser.scanner, source);

    compiler->nlocals = 0;
    compiler->scope_level = 0;
}

static void parse_precedence(struct parser *parser, enum precedence precedence)
{
    advance(parser);
    parse_function prefix_rule = get_rule(parser->previous.type)->prefix;
    if (prefix_rule == NULL) {
        error(parser, "Expected expression");
        return;
    }

    // only allow assignment in low-precedence expressions
    bool assign_ok = precedence <= PREC_ASSIGNMENT;

    prefix_rule(parser, assign_ok);

    while (precedence <= get_rule(parser->current.type)->precedence) {
        advance(parser);
        parse_function infix_rule = get_rule(parser->previous.type)->infix;
        infix_rule(parser, assign_ok);
    }

    if (assign_ok && match(parser, TOKEN_EQUAL)) {
        error(parser, "Invalid assignment target");
    }
}

static uint8_t identifier_constant(struct parser *parser, struct token *name)
{
    int ret = chunk_add_constant(parser->chunk, OBJECT_VAL(object_string_allocate(name->start, name->length)));
    return (uint8_t)ret;
}

static void add_local(struct parser *parser, struct token name)
{
    if (parser->compiler->nlocals > UINT8_MAX) {
        error(parser, "Too many local variables in function");
        return;
    }
    struct local *local = &parser->compiler->locals[parser->compiler->nlocals++];
    local->name = name;
    local->level = -1;  // -1 indicates uninitialized
}

static bool identifiers_equal(struct token *a, struct token *b)
{
    if (a->length != b->length)
        return false;
    return memcmp(a->start, b->start, a->length) == 0;
}

static int resolve_local(struct compiler *compiler, struct token *name)
{
    for (int i = compiler->nlocals - 1; i >= 0; i--) {
        struct local *local = &compiler->locals[i];
        if (identifiers_equal(name, &local->name)) {
            if (local->level == -1)
                error(&compiler->parser, "Can't read local in its own initializer");
            return i;
        }
    }
    return -1;
}

static void declare_variable(struct parser *parser)
{
    if (parser->compiler->scope_level == 0)
        return;

    struct token *name = &parser->previous;

    for (int i = parser->compiler->nlocals - 1; i >= 0; i--) {
        struct local *local = &parser->compiler->locals[i];
        if (local->level != -1 && local->level < parser->compiler->scope_level) {
            break;
        }

        if (identifiers_equal(name, &local->name)) {
            error(parser, "Variable is already defined in this scope");
        }
    }
    add_local(parser, *name);
}

static uint8_t parse_variable(struct parser *parser, const char *errmsg)
{
    consume(parser, TOKEN_IDENTIFIER, errmsg);
    declare_variable(parser);

    /* Locals aren't looked up by name at runtime, so
     * they don't need to be entered into the constant table
     */
    if (parser->compiler->scope_level > 0)
        return 0;

    return identifier_constant(parser, &parser->previous);
}

static void define_variable(struct parser *parser, uint8_t global)
{
    // Local variables don't have associated runtime code for declaration
    if (parser->compiler->scope_level > 0) {
        // Mark as initialized
        parser->compiler->locals[parser->compiler->nlocals - 1].level = parser->compiler->scope_level;
        return;
    }
    emit_opcode_args(parser, OP_DEFINE_GLOBAL, &global, sizeof(global));
}

static void and_(struct parser *parser, bool assign_ok)
{
    int end_jump = emit_jump(parser, OP_JUMP_IF_FALSE);
    emit_opcode(parser, OP_POP);
    parse_precedence(parser, PREC_AND);

    patch_jump(parser, end_jump);
}

static struct parse_rule *get_rule(enum token_type type)
{
    return &rules[type];
}

static void expression(struct parser *parser)
{
    parse_precedence(parser, PREC_ASSIGNMENT);
}

static void block(struct parser *parser)
{
    while (!check(parser, TOKEN_RIGHT_BRACE) && !check(parser, TOKEN_EOF)) { declaration(parser); }

    consume(parser, TOKEN_RIGHT_BRACE, "Expect '}' after block");
}

static void var_declaration(struct parser *parser)
{
    uint8_t global = parse_variable(parser, "Expect variable name");

    if (match(parser, TOKEN_EQUAL)) {
        expression(parser);
    } else {
        emit_opcode(parser, OP_NIL);
    }
    consume(parser, TOKEN_SEMICOLON, "Expect ';' after variable declaration");

    define_variable(parser, global);
}

static void expression_statement(struct parser *parser)
{
    expression(parser);
    consume(parser, TOKEN_SEMICOLON, "Expect ';' after expression");
    emit_opcode(parser, OP_POP);
}

static void for_statement(struct parser *parser)
{
    scope_enter(parser->compiler);
    consume(parser, TOKEN_LEFT_PAREN, "Expect '(' after 'for'");
    if (match(parser, TOKEN_SEMICOLON)) {
        // no initializer
    } else if (match(parser, TOKEN_VAR)) {
        var_declaration(parser);
    } else {
        expression_statement(parser);
    }

    int loop_start = parser->chunk->count;
    int exit_jump = -1;

    if (!match(parser, TOKEN_SEMICOLON)) {
        expression(parser);
        consume(parser, TOKEN_SEMICOLON, "Expect ';' after loop condition");
        exit_jump = emit_jump(parser, OP_JUMP_IF_FALSE);
        emit_opcode(parser, OP_POP);
    }

    if (!match(parser, TOKEN_RIGHT_PAREN)) {
        int body_jump = emit_jump(parser, OP_JUMP);
        int increment_start = parser->chunk->count;
        expression(parser);
        emit_opcode(parser, OP_POP);
        consume(parser, TOKEN_RIGHT_PAREN, "Expect ')' after for clauses");
        emit_loop(parser, loop_start);
        loop_start = increment_start;
        patch_jump(parser, body_jump);
    }

    statement(parser);
    emit_loop(parser, loop_start);

    if (exit_jump != -1) {
        patch_jump(parser, exit_jump);
        emit_opcode(parser, OP_POP);
    }

    scope_exit(parser->compiler);
}

static void if_statement(struct parser *parser)
{
    consume(parser, TOKEN_LEFT_PAREN, "Expect '(' after 'if'");
    expression(parser);
    consume(parser, TOKEN_RIGHT_PAREN, "Expect ')' after condition'");

    int then_jump = emit_jump(parser, OP_JUMP_IF_FALSE);
    emit_opcode(parser, OP_POP);
    statement(parser);

    int else_jump = emit_jump(parser, OP_JUMP);
    patch_jump(parser, then_jump);
    emit_opcode(parser, OP_POP);

    if (match(parser, TOKEN_ELSE))
        statement(parser);
    patch_jump(parser, else_jump);
}

static void print_statement(struct parser *parser)
{
    expression(parser);
    consume(parser, TOKEN_SEMICOLON, "Expect ';' after value");
    emit_opcode(parser, OP_PRINT);
}

static void while_statement(struct parser *parser)
{
    int loop_start = parser->chunk->count;
    consume(parser, TOKEN_LEFT_PAREN, "Expect '(' after 'while'");
    expression(parser);
    consume(parser, TOKEN_RIGHT_PAREN, "Expect ')' after condition");

    int exit_jump = emit_jump(parser, OP_JUMP_IF_FALSE);
    emit_opcode(parser, OP_POP);
    statement(parser);

    emit_loop(parser, loop_start);

    patch_jump(parser, exit_jump);
    emit_opcode(parser, OP_POP);
}

static void synchronize(struct parser *parser)
{
    parser->panic = false;

    while (parser->current.type != TOKEN_EOF) {
        if (parser->previous.type == TOKEN_SEMICOLON)
            return;
        switch (parser->current.type) {
            case TOKEN_CLASS:
            case TOKEN_FUNC:
            case TOKEN_VAR:
            case TOKEN_FOR:
            case TOKEN_IF:
            case TOKEN_WHILE:
            case TOKEN_PRINT:
            case TOKEN_RETURN:
                return;
            default:
                break;
        }
        advance(parser);
    }
}

static void declaration(struct parser *parser)
{
    if (match(parser, TOKEN_VAR)) {
        var_declaration(parser);
    } else {
        statement(parser);
    }

    if (parser->panic)
        synchronize(parser);
}

static void statement(struct parser *parser)
{
    if (match(parser, TOKEN_PRINT)) {
        print_statement(parser);
    } else if (match(parser, TOKEN_FOR)) {
        for_statement(parser);
    } else if (match(parser, TOKEN_IF)) {
        if_statement(parser);
    } else if (match(parser, TOKEN_WHILE)) {
        while_statement(parser);
    } else if (match(parser, TOKEN_LEFT_BRACE)) {
        scope_enter(parser->compiler);
        block(parser);
        scope_exit(parser->compiler);
    } else {
        expression_statement(parser);
    }
}

static void number(struct parser *parser, bool assign_ok)
{
    double val = strtod(parser->previous.start, NULL);
    int ret = chunk_add_constant(parser->chunk, NUMBER_VAL(val));
    if (ret > UINT8_MAX) {
        error(parser, "too many constants in chunk");
        ret = 0;
    }
    uint8_t constant = (uint8_t)ret;

    emit_opcode_args(parser, OP_CONSTANT, &constant, sizeof(constant));
}

static void or_(struct parser *parser, bool assign_ok)
{
    int end_jump = emit_jump(parser, OP_JUMP_IF_TRUE);
    emit_opcode(parser, OP_POP);
    parse_precedence(parser, PREC_OR);

    patch_jump(parser, end_jump);
}

static void string(struct parser *parser, bool assign_ok)
{
    // TODO: Handle escape sequences
    struct object_string *s = object_string_allocate(parser->previous.start + 1, parser->previous.length - 2);
    int ret = chunk_add_constant(parser->chunk, OBJECT_VAL(s));
    if (ret > UINT8_MAX) {
        error(parser, "too many constants in chunk");
        ret = 0;
    }
    uint8_t constant = (uint8_t)ret;
    emit_opcode_args(parser, OP_CONSTANT, &constant, sizeof(constant));
}

static void named_variable(struct parser *parser, struct token name, bool assign_ok)
{
    uint8_t arg = resolve_local(parser->compiler, &name);
    bool global = arg == -1;

    if (assign_ok && match(parser, TOKEN_EQUAL)) {
        expression(parser);
        emit_opcode_args(parser, global ? OP_SET_GLOBAL : OP_SET_LOCAL, &arg, sizeof(arg));
    } else {
        emit_opcode_args(parser, global ? OP_GET_GLOBAL : OP_GET_LOCAL, &arg, sizeof(arg));
    }
}

static void variable(struct parser *parser, bool assign_ok)
{
    named_variable(parser, parser->previous, assign_ok);
}

static void literal(struct parser *parser, bool assign_ok)
{
    switch (parser->previous.type) {
        case TOKEN_FALSE:
            emit_opcode(parser, OP_FALSE);
            break;
        case TOKEN_TRUE:
            emit_opcode(parser, OP_TRUE);
            break;
        case TOKEN_NIL:
            emit_opcode(parser, OP_NIL);
            break;
        default:
            break;
    }
}

static void grouping(struct parser *parser, bool assign_ok)
{
    expression(parser);
    consume(parser, TOKEN_RIGHT_PAREN, "Expect ')' after expression");
}

static void binary(struct parser *parser, bool assign_ok)
{
    enum token_type operator_type = parser->previous.type;

    struct parse_rule *rule = get_rule(operator_type);

    parse_precedence(parser, (enum precedence)(rule->precedence + 1));

    switch (operator_type) {
        case TOKEN_PLUS:
            emit_opcode(parser, OP_ADD);
            break;
        case TOKEN_MINUS:
            emit_opcode(parser, OP_SUBTRACT);
            break;
        case TOKEN_STAR:
            emit_opcode(parser, OP_MULTIPLY);
            break;
        case TOKEN_PERCENT:
            emit_opcode(parser, OP_MOD);
            break;
        case TOKEN_SLASH:
            emit_opcode(parser, OP_DIVIDE);
            break;
        case TOKEN_BANG_EQUAL:
            emit_opcode(parser, OP_EQUAL);
            emit_opcode(parser, OP_NOT);
            break;
        case TOKEN_EQUAL_EQUAL:
            emit_opcode(parser, OP_EQUAL);
            break;
        case TOKEN_GREATER:
            emit_opcode(parser, OP_GREATER);
            break;
        case TOKEN_GREATER_GREATER:
            emit_opcode(parser, OP_SHR);
            break;
        case TOKEN_GREATER_EQUAL:
            emit_opcode(parser, OP_LESS);
            emit_opcode(parser, OP_NOT);
            break;
        case TOKEN_LESS:
            emit_opcode(parser, OP_LESS);
            break;
        case TOKEN_LESS_LESS:
            emit_opcode(parser, OP_SHL);
            break;
        case TOKEN_LESS_EQUAL:
            emit_opcode(parser, OP_GREATER);
            emit_opcode(parser, OP_NOT);
            break;
        case TOKEN_AND:
            emit_opcode(parser, OP_AND);
            break;
        case TOKEN_OR:
            emit_opcode(parser, OP_OR);
            break;
        case TOKEN_CARET:
            emit_opcode(parser, OP_XOR);
            break;
        default:
            return;
    }
}

static void unary(struct parser *parser, bool assign_ok)
{
    enum token_type operator_type = parser->previous.type;

    parse_precedence(parser, PREC_UNARY);

    switch (operator_type) {
        case TOKEN_MINUS:
            emit_opcode(parser, OP_NEGATE);
            break;
        case TOKEN_BANG:
            emit_opcode(parser, OP_NOT);
            break;
        default:
            break;
    }
}

static void end(struct parser *parser)
{
    emit_opcode(parser, OP_RETURN);
}

static void scope_enter(struct compiler *compiler)
{
    compiler->scope_level++;
}

static void scope_exit(struct compiler *compiler)
{
    compiler->scope_level--;

    while (compiler->nlocals > 0 && compiler->locals[compiler->nlocals - 1].level > compiler->scope_level) {
        emit_opcode(&compiler->parser, OP_POP);
        compiler->nlocals--;
    }
}

int compile(const char *source, struct chunk *chunk)
{
    struct compiler compiler;
    compiler_init(&compiler, chunk, source);

    advance(&compiler.parser);

    while (!match(&compiler.parser, TOKEN_EOF)) { declaration(&compiler.parser); }

    end(&compiler.parser);

    return compiler.parser.had_error ? -1 : 0;
}
