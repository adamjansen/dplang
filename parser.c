#include "parser.h"
#include <stdio.h>
#include <stdbool.h>
#include <stddef.h>

extern struct parse_rule *parser_get_rule(enum token_type type);

void parser_error_at(struct parser *parser, struct token *token, const char *message)
{
    if (parser->panic) {
        return;
    }
    parser->panic = true;
    fprintf(stderr, "[line %u] error", token->line);

    if (token->type == TOKEN_EOF) {
        fprintf(stderr, " at end");
    } else if (token->type == TOKEN_ERROR) {
    } else {
        fprintf(stderr, " at '%.*s'", (int)token->length, token->start);
    }

    fprintf(stderr, ": %s\n", message);
    parser->had_error = true;
}

void parser_error(struct parser *parser, const char *message)
{
    parser_error_at(parser, &parser->previous, message);
}

void parser_error_at_current(struct parser *parser, const char *message)
{
    parser_error_at(parser, &parser->current, message);
}

static void parser_advance(struct parser *parser)
{
    parser->previous = parser->current;

    while (1) {
        parser->current = scanner_scan_token(&parser->scanner);
        if (parser->current.type != TOKEN_ERROR) {
            break;
        }

        parser_error_at_current(parser, parser->current.start);
    }
}

int parser_init(struct parser *parser, const char *src)
{
    scanner_init(&parser->scanner, src);
    parser_advance(parser);
    return 0;
}

void parser_synchronize(struct parser *parser)
{
    parser->panic = false;

    while (parser->current.type != TOKEN_EOF) {
        if (parser->previous.type == TOKEN_SEMICOLON) {
            return;
        }
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
        parser_advance(parser);
    }
}

void parser_precedence(struct parser *parser, enum precedence precedence, void *userdata)
{
    parser_advance(parser);
    parse_function prefix_rule = parser_get_rule(parser->previous.type)->prefix;
    if (prefix_rule == NULL) {
        parser_error(parser, "Expected expression");
        return;
    }

    prefix_rule(parser, precedence, userdata);

    while (precedence <= parser_get_rule(parser->current.type)->precedence) {
        parser_advance(parser);
        parse_function infix_rule = parser_get_rule(parser->previous.type)->infix;
        if (infix_rule == NULL) {
            printf("token_type=%d\n", parser->previous.type);
            parser_error(parser, "Invalid infix rule?");
            return;
        }
        infix_rule(parser, precedence, userdata);
    }

    bool assign_ok = precedence <= PREC_ASSIGNMENT;
    if (assign_ok && parser_match(parser, TOKEN_EQUAL)) {
        parser_error(parser, "Invalid assignment target");
    }
}

void parser_consume(struct parser *parser, enum token_type type, const char *message)
{
    if (parser->current.type == type) {
        parser_advance(parser);
        return;
    }
    parser_error_at_current(parser, message);
}

bool parser_check(struct parser *parser, enum token_type type)
{
    return parser->current.type == type;
}

bool parser_match(struct parser *parser, enum token_type type)
{
    if (!parser_check(parser, type)) {
        return false;
    }
    parser_advance(parser);
    return true;
}
