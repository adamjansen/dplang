#ifndef DPLANG_PARSER_H
#define DPLANG_PARSER_H

#include <stdbool.h>

#include "scanner.h"

struct parser {
    struct token current;
    struct token previous;
    struct scanner scanner;
    bool had_error;
    bool panic;
};

enum precedence {
    PREC_NONE,
    PREC_ASSIGNMENT,  // =, +=, -=, *=, /=
    PREC_OR,          // or
    PREC_AND,         // and
    PREC_EQUALITY,    // == !=
    PREC_COMPARISON,  // < > <= >=
    PREC_TERM,        // + -
    PREC_FACTOR,      // * /
    PREC_UNARY,       // ! -
    PREC_CALL,        // . () []
    PREC_PRIMARY,
};

typedef void (*parse_function)(struct parser *parser, enum precedence precedence, void *userdata);

struct parse_rule {
    parse_function prefix;
    parse_function infix;
    enum precedence precedence;
};

void parser_error(struct parser *parser, const char *message);
void parser_error_at(struct parser *parser, struct token *token, const char *message);
void parser_error_at_current(struct parser *parser, const char *message);

int parser_init(struct parser *parser, const char *src);
void parser_synchronize(struct parser *parser);
void parser_precedence(struct parser *parser, enum precedence precedence, void *userdata);
bool parser_check(struct parser *parser, enum token_type type);
bool parser_match(struct parser *parser, enum token_type type);
void parser_consume(struct parser *parser, enum token_type type, const char *message);
#endif
