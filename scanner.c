#include "scanner.h"
#include <stdlib.h>
#include <math.h>
#include <errno.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>

int scanner_init(struct scanner *scanner, const char *source)
{
    scanner->start = source;
    scanner->current = source;
    scanner->line = 1;
    return 0;
}

static bool is_at_end(struct scanner *scanner)
{
    return *scanner->current == '\0';
}

static char advance(struct scanner *scanner)
{
    char c = *scanner->current;
    scanner->current++;
    return c;
}

static char peek(struct scanner *scanner)
{
    return *scanner->current;
}

static char peek_next(struct scanner *scanner)
{
    if (is_at_end(scanner)) {
        return '\0';
    }
    return scanner->current[1];
}

static bool match(struct scanner *scanner, char expected)
{
    if (is_at_end(scanner)) {
        return false;
    }
    if (*scanner->current != expected) {
        return false;
    }
    scanner->current++;
    return true;
}

static struct token make_token(struct scanner *scanner, enum token_type type)
{
    struct token token = {
        .type = type,
        .start = scanner->start,
        .length = (int)(scanner->current - scanner->start),
        .line = scanner->line,
    };

    return token;
}

static struct token error_token(struct scanner *scanner, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    int len = vsnprintf(NULL, 0, format, args);
    char *message = malloc(len);
    vsnprintf(message, len, format, args);
    va_end(args);
    struct token token = {
        .type = TOKEN_ERROR,
        .start = message,
        .length = len,
        .line = scanner->line,
    };
    return token;
}

/**
 * Ignores all whitespace in input.
 * Also keeps track of input line number.
 *
 * Treats comments as whitespace.  As a side-effect,
 * they never make it to the compiler or into the bytecode.
 */
static void skip_whitespace(struct scanner *scanner)
{
    while (1) {
        char c = peek(scanner);
        switch (c) {
            case ' ':
            case '\r':
            case '\t':
                advance(scanner);
                break;
            case '\n':
                scanner->line++;
                advance(scanner);
                break;
            case '/':
                if (peek_next(scanner) == '/') { /* comment until end of line */
                    while (peek(scanner) != '\n' && !is_at_end(scanner)) { advance(scanner); }
                } else if (peek_next(scanner) == '*') {  // comment until */ sequence
                    while (!is_at_end(scanner) && !(peek(scanner) == '*' && peek_next(scanner) == '/')) {
                        advance(scanner);
                    }
                    advance(scanner);
                    advance(scanner);
                } else {
                    return;
                }
                break;
            default:
                return;
        }
    }
}

static enum token_type check_keyword(struct scanner *scanner, int start, int length, const char *rest,
                                     enum token_type type)
{
    if (scanner->current - scanner->start == start + length && memcmp(scanner->start + start, rest, length) == 0) {
        return type;
    }
    return TOKEN_IDENTIFIER;
}

// NOLINTBEGIN(readability-magic-numbers)
static enum token_type identifier_type(struct scanner *scanner)
{
    switch (scanner->start[0]) {
        case 'a':
            return check_keyword(scanner, 1, 2, "nd", TOKEN_AND);
        case 'b':
            return check_keyword(scanner, 1, 4, "reak", TOKEN_BREAK);
        case 'c':
            if (scanner->current - scanner->start > 1) {
                switch (scanner->start[1]) {
                    case 'o':
                        return check_keyword(scanner, 2, 6, "ntinue", TOKEN_CONTINUE);
                        break;
                    case 'l':
                        return check_keyword(scanner, 2, 3, "ass", TOKEN_CLASS);
                }
            }
            break;
        case 'e':
            return check_keyword(scanner, 1, 3, "lse", TOKEN_ELSE);
        case 'f':
            if (scanner->current - scanner->start > 1) {
                switch (scanner->start[1]) {
                    case 'a':
                        return check_keyword(scanner, 2, 3, "lse", TOKEN_FALSE);
                    case 'o':
                        return check_keyword(scanner, 2, 1, "r", TOKEN_FOR);
                    case 'u':
                        return check_keyword(scanner, 2, 2, "nc", TOKEN_FUNC);
                }
            }
            break;
        case 'i':
            return check_keyword(scanner, 1, 1, "f", TOKEN_IF);
        case 'n':
            return check_keyword(scanner, 1, 2, "il", TOKEN_NIL);
        case 'o':
            return check_keyword(scanner, 1, 1, "r", TOKEN_OR);
        case 'p':
            return check_keyword(scanner, 1, 4, "rint", TOKEN_PRINT);
        case 'r':
            return check_keyword(scanner, 1, 5, "eturn", TOKEN_RETURN);
        case 's':
            return check_keyword(scanner, 1, 4, "uper", TOKEN_SUPER);
        case 't':
            if (scanner->current - scanner->start > 1) {
                switch (scanner->start[1]) {
                    case 'h':
                        return check_keyword(scanner, 2, 2, "is", TOKEN_THIS);
                    case 'r':
                        return check_keyword(scanner, 2, 2, "ue", TOKEN_TRUE);
                }
            }
            break;
        case 'v':
            return check_keyword(scanner, 1, 2, "ar", TOKEN_VAR);
        case 'w':
            return check_keyword(scanner, 1, 4, "hile", TOKEN_WHILE);
    }
    return TOKEN_IDENTIFIER;
}
// NOLINTEND(readability-magic-numbers)END

struct token identifier(struct scanner *scanner)
{
    while (isalnum(peek(scanner)) || peek(scanner) == '_') { advance(scanner); }
    return make_token(scanner, identifier_type(scanner));
}

struct token binary_number(struct scanner *scanner)
{
    while (peek(scanner) == '1' || peek(scanner) == '0') { advance(scanner); }
    if (isdigit(peek(scanner))) {
        return error_token(scanner, "Invalid binary literal %.*s", scanner->current - scanner->start, scanner->start);
    }
    return make_token(scanner, TOKEN_NUMBER);
}

struct token hexadecimal_number(struct scanner *scanner)
{
    // Base 16 (hex) literal
    while (isxdigit(peek(scanner))) { advance(scanner); }
    return make_token(scanner, TOKEN_NUMBER);
}

struct token number(struct scanner *scanner)
{
    if (scanner->current[-1] == '0') {
        if (match(scanner, 'b') || match(scanner, 'B')) {
            return binary_number(scanner);
        } else if (match(scanner, 'x') || match(scanner, 'X')) {
            return hexadecimal_number(scanner);
        }
    }

    while (isdigit(peek(scanner))) { advance(scanner); }

    if (match(scanner, '.')) {
        if (isdigit(peek(scanner))) {
            while (isdigit(peek(scanner))) { advance(scanner); }
        } else {
            return error_token(scanner, "Invalid numeric literal");
        }
    }

    if (match(scanner, 'e')) {
        if (peek(scanner) == '+' || peek(scanner) == '-') {
            advance(scanner);
        }
        if (isdigit(peek(scanner))) {
            while (isdigit(peek(scanner))) { advance(scanner); }
        } else {
            return error_token(scanner, "Invalid numeric literal");
        }
    }

    return make_token(scanner, TOKEN_NUMBER);
}

struct token string(struct scanner *scanner)
{
    while (peek(scanner) != '"' && !is_at_end(scanner)) {
        if (peek(scanner) == '\n') {
            scanner->line++;
        }
        advance(scanner);
    }
    if (is_at_end(scanner)) {
        return error_token(scanner, "Unterminated string literal");
    }

    advance(scanner);
    return make_token(scanner, TOKEN_STRING);
}

struct token scanner_scan_token(struct scanner *scanner)
{
    skip_whitespace(scanner);
    scanner->start = scanner->current;
    if (is_at_end(scanner)) {
        return make_token(scanner, TOKEN_EOF);
    }

    char c = advance(scanner);
    if (isalpha(c) || c == '_') {
        return identifier(scanner);
    } else if (isdigit(c)) {
        return number(scanner);
    }
    switch (c) {
        case '(':
            return make_token(scanner, TOKEN_LEFT_PAREN);
        case ')':
            return make_token(scanner, TOKEN_RIGHT_PAREN);
        case '{':
            return make_token(scanner, TOKEN_LEFT_BRACE);
        case '}':
            return make_token(scanner, TOKEN_RIGHT_BRACE);
        case '[':
            return make_token(scanner, TOKEN_LEFT_BRACKET);
        case ']':
            return make_token(scanner, TOKEN_RIGHT_BRACKET);
        case ';':
            return make_token(scanner, TOKEN_SEMICOLON);
        case ',':
            return make_token(scanner, TOKEN_COMMA);
        case '.':
            return make_token(scanner, TOKEN_DOT);
        case '-':
            return make_token(scanner, TOKEN_MINUS);
        case '+':
            return make_token(scanner, TOKEN_PLUS);
        case '/':
            return make_token(scanner, TOKEN_SLASH);
        case '*':
            return make_token(scanner, TOKEN_STAR);
        case '%':
            return make_token(scanner, TOKEN_PERCENT);
        case '^':
            return make_token(scanner, TOKEN_CARET);
        case '~':
            return make_token(scanner, TOKEN_TILDE);
        case '!':
            return make_token(scanner, match(scanner, '=') ? TOKEN_BANG_EQUAL : TOKEN_BANG);
        case '=':
            return make_token(scanner, match(scanner, '=') ? TOKEN_EQUAL_EQUAL : TOKEN_EQUAL);
        case '<':
            if (match(scanner, '=')) {
                return make_token(scanner, TOKEN_LESS_EQUAL);
            } else if (match(scanner, '<')) {
                return make_token(scanner, TOKEN_LESS_LESS);
            } else {
                return make_token(scanner, TOKEN_LESS);
            }
        case '>':
            if (match(scanner, '=')) {
                return make_token(scanner, TOKEN_GREATER_EQUAL);
            } else if (match(scanner, '>')) {
                return make_token(scanner, TOKEN_GREATER_GREATER);
            } else {
                return make_token(scanner, TOKEN_GREATER);
            }
        case '"':
            return string(scanner);
    }

    return error_token(scanner, "unexpected character");
}
