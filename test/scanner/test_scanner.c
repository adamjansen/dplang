#include <string.h>

#include "unity.h"

#include "scanner.h"

static struct scanner s;

static const char *source_empty = "";

void setUp(void)
{
    int ret = scanner_init(&s, source_empty);
    TEST_ASSERT_EQUAL(0, ret);
}

void tearDown(void)
{
}

void test_init(void)
{
    int ret = scanner_init(&s, source_empty);

    TEST_ASSERT_EQUAL(0, ret);
    TEST_ASSERT_EQUAL_PTR(source_empty, s.start);
    TEST_ASSERT_EQUAL(s.start, s.current);

    TEST_ASSERT_EQUAL(1, s.line);
}

void test_scan_token_eof(void)
{
    struct token t = scanner_scan_token(&s);
    TEST_ASSERT_EQUAL_PTR(s.start, t.start);
    TEST_ASSERT_EQUAL(1, t.line);
    TEST_ASSERT_EQUAL(TOKEN_EOF, t.type);
    TEST_ASSERT_EQUAL(0, t.length);
}

void test_scan_token_skips_whitespace(void)
{
    const char *source_white_eof = "     ";
    scanner_init(&s, source_white_eof);
    struct token t = scanner_scan_token(&s);
    TEST_ASSERT_EQUAL(TOKEN_EOF, t.type);
    TEST_ASSERT_EQUAL_PTR(source_white_eof + strlen(source_white_eof), t.start);
    TEST_ASSERT_EQUAL(0, t.length);
    TEST_ASSERT_EQUAL(1, t.line);
}

void test_scan_token_identifiers(void)
{
    scanner_init(&s, "foobar");
    struct token t = scanner_scan_token(&s);
    TEST_ASSERT_EQUAL(TOKEN_IDENTIFIER, t.type);
    TEST_ASSERT_EQUAL_STRING("foobar", t.start);
    TEST_ASSERT_EQUAL(6, t.length);

    scanner_init(&s, "_123");
    t = scanner_scan_token(&s);
    TEST_ASSERT_EQUAL(TOKEN_IDENTIFIER, t.type);
    TEST_ASSERT_EQUAL_STRING("_123", t.start);
    TEST_ASSERT_EQUAL(4, t.length);

    scanner_init(&s, "_t_h_i_s_i_s_a_r_i_d_u_l_o_u_s_l_y_l_o_n_g_I_D_E_N_T_I_F_I_E_R_t_h_a_t_i_s_v_a_l_i_d_b_u_t_p_r_o_"
                     "b_a_b_l_y_s_h_o_u_l_d_n_o_t_b_e");
    t = scanner_scan_token(&s);
    TEST_ASSERT_EQUAL(TOKEN_IDENTIFIER, t.type);
}

void test_scan_token_number(void)
{
    scanner_init(&s, "123456");
    struct token t = scanner_scan_token(&s);
    TEST_ASSERT_EQUAL(TOKEN_NUMBER, t.type);
    TEST_ASSERT_EQUAL_STRING("123456", t.start);

    // Negative numbers get parsed as a positive number and a unary minus prefix operator
    scanner_init(&s, "-123.456");
    t = scanner_scan_token(&s);
    TEST_ASSERT_EQUAL(TOKEN_MINUS, t.type);
    TEST_ASSERT_EQUAL(1, t.length);
    t = scanner_scan_token(&s);
    TEST_ASSERT_EQUAL(TOKEN_NUMBER, t.type);
    TEST_ASSERT_EQUAL_STRING("123.456", t.start);

    // Hexadecimal is valid
    scanner_init(&s, "0xDEADbeef");
    t = scanner_scan_token(&s);
    TEST_ASSERT_EQUAL(TOKEN_NUMBER, t.type);

    scanner_init(&s, "0b1111000011110000");
    t = scanner_scan_token(&s);
    TEST_ASSERT_EQUAL(TOKEN_NUMBER, t.type);
}

void test_scan_token_binary_number_invalid_literal(void)
{
    scanner_init(&s, "0b012");
    struct token t = scanner_scan_token(&s);
    TEST_ASSERT_EQUAL(TOKEN_ERROR, t.type);
    TEST_ASSERT_EQUAL_STRING("Invalid binary literal", t.start);
}

void test_scan_token_number_invalid_literal(void)
{
    scanner_init(&s, "1.abc");
    struct token t = scanner_scan_token(&s);
    TEST_ASSERT_EQUAL(TOKEN_ERROR, t.type);
    TEST_ASSERT_EQUAL_STRING("Invalid numeric literal", t.start);

    scanner_init(&s, "1.23ea");
    t = scanner_scan_token(&s);
    TEST_ASSERT_EQUAL(TOKEN_ERROR, t.type);
    TEST_ASSERT_EQUAL_STRING("Invalid numeric literal", t.start);
}

void test_scan_token_parens(void)
{
    scanner_init(&s, "    (     )");
    struct token t = scanner_scan_token(&s);
    TEST_ASSERT_EQUAL(TOKEN_LEFT_PAREN, t.type);
    t = scanner_scan_token(&s);
    TEST_ASSERT_EQUAL(TOKEN_RIGHT_PAREN, t.type);
}

void test_scan_token_braces(void)
{
    scanner_init(&s, "    {     }");
    struct token t = scanner_scan_token(&s);
    TEST_ASSERT_EQUAL(TOKEN_LEFT_BRACE, t.type);
    t = scanner_scan_token(&s);
    TEST_ASSERT_EQUAL(TOKEN_RIGHT_BRACE, t.type);
}

void test_scan_token_bracket(void)
{
    scanner_init(&s, "    [     ]");
    struct token t = scanner_scan_token(&s);
    TEST_ASSERT_EQUAL(TOKEN_LEFT_BRACKET, t.type);
    t = scanner_scan_token(&s);
    TEST_ASSERT_EQUAL(TOKEN_RIGHT_BRACKET, t.type);
}

void test_scan_token_semicolon(void)
{
    scanner_init(&s, ";");
    struct token t = scanner_scan_token(&s);
    TEST_ASSERT_EQUAL(TOKEN_SEMICOLON, t.type);
    TEST_ASSERT_EQUAL(1, t.length);
}

void test_scan_token_comma(void)
{
    scanner_init(&s, ",");
    struct token t = scanner_scan_token(&s);
    TEST_ASSERT_EQUAL(TOKEN_COMMA, t.type);
}

void test_scan_token_dot(void)
{
    scanner_init(&s, ".");
    struct token t = scanner_scan_token(&s);
    TEST_ASSERT_EQUAL(TOKEN_DOT, t.type);
}

void test_scan_token_minus(void)
{
    scanner_init(&s, "-");
    struct token t = scanner_scan_token(&s);
    TEST_ASSERT_EQUAL(TOKEN_MINUS, t.type);
}

void test_scan_token_plus(void)
{
    scanner_init(&s, "+");
    struct token t = scanner_scan_token(&s);
    TEST_ASSERT_EQUAL(TOKEN_PLUS, t.type);
}

void test_scan_token_slash(void)
{
    scanner_init(&s, "/");
    struct token t = scanner_scan_token(&s);
    TEST_ASSERT_EQUAL(TOKEN_SLASH, t.type);
}

void test_scan_token_star(void)
{
    scanner_init(&s, "*");
    struct token t = scanner_scan_token(&s);
    TEST_ASSERT_EQUAL(TOKEN_STAR, t.type);
}

void test_scan_token_percent(void)
{
    scanner_init(&s, "%");
    struct token t = scanner_scan_token(&s);
    TEST_ASSERT_EQUAL(TOKEN_PERCENT, t.type);
}

void test_scan_token_caret(void)
{
    scanner_init(&s, "^");
    struct token t = scanner_scan_token(&s);
    TEST_ASSERT_EQUAL(TOKEN_CARET, t.type);
}

void test_scan_token_tilde(void)
{
    scanner_init(&s, "~");
    struct token t = scanner_scan_token(&s);
    TEST_ASSERT_EQUAL(TOKEN_TILDE, t.type);
}

void test_scan_token_bang(void)
{
    scanner_init(&s, "!");
    struct token t = scanner_scan_token(&s);
    TEST_ASSERT_EQUAL(TOKEN_BANG, t.type);
}

void test_scan_token_bang_equal(void)
{
    scanner_init(&s, "!=");
    struct token t = scanner_scan_token(&s);
    TEST_ASSERT_EQUAL(TOKEN_BANG_EQUAL, t.type);
}

void test_scan_token_equal(void)
{
    scanner_init(&s, "=");
    struct token t = scanner_scan_token(&s);
    TEST_ASSERT_EQUAL(TOKEN_EQUAL, t.type);
}

void test_scan_token_equal_equal(void)
{
    scanner_init(&s, "==");
    struct token t = scanner_scan_token(&s);
    TEST_ASSERT_EQUAL(TOKEN_EQUAL_EQUAL, t.type);
}

void test_scan_token_less(void)
{
    scanner_init(&s, "<");
    struct token t = scanner_scan_token(&s);
    TEST_ASSERT_EQUAL(TOKEN_LESS, t.type);
}

void test_scan_token_less_equal(void)
{
    scanner_init(&s, "<=");
    struct token t = scanner_scan_token(&s);
    TEST_ASSERT_EQUAL(TOKEN_LESS_EQUAL, t.type);
}

void test_scan_token_less_less(void)
{
    scanner_init(&s, "<<");
    struct token t = scanner_scan_token(&s);
    TEST_ASSERT_EQUAL(TOKEN_LESS_LESS, t.type);
}

void test_scan_token_greater(void)
{
    scanner_init(&s, ">");
    struct token t = scanner_scan_token(&s);
    TEST_ASSERT_EQUAL(TOKEN_GREATER, t.type);
}

void test_scan_token_greater_equal(void)
{
    scanner_init(&s, ">=");
    struct token t = scanner_scan_token(&s);
    TEST_ASSERT_EQUAL(TOKEN_GREATER_EQUAL, t.type);
}

void test_scan_token_greater_greater(void)
{
    scanner_init(&s, ">>");
    struct token t = scanner_scan_token(&s);
    TEST_ASSERT_EQUAL(TOKEN_GREATER_GREATER, t.type);
}

void test_scan_token_string(void)
{
    scanner_init(&s, "\"this is a string\"");
    struct token t = scanner_scan_token(&s);
    TEST_ASSERT_EQUAL(TOKEN_STRING, t.type);
    TEST_ASSERT_EQUAL_PTR(s.start, t.start);
}

void test_scan_token_string_unterminated(void)
{
    scanner_init(&s, "\"This string is not terminated");
    struct token t = scanner_scan_token(&s);
    TEST_ASSERT_EQUAL(TOKEN_ERROR, t.type);
    TEST_ASSERT_EQUAL_STRING("Unterminated string literal", t.start);
}

void test_scan_token_unexpected(void)
{
    scanner_init(&s, "?$@");
    struct token t = scanner_scan_token(&s);
    TEST_ASSERT_EQUAL(TOKEN_ERROR, t.type);
    TEST_ASSERT_EQUAL_STRING("unexpected character", t.start);
}

void test_scan_token_keyword_and(void)
{
    scanner_init(&s, "and");
    struct token t = scanner_scan_token(&s);
    TEST_ASSERT_EQUAL(TOKEN_AND, t.type);

    scanner_init(&s, "sand");
    t = scanner_scan_token(&s);
    TEST_ASSERT_EQUAL(TOKEN_IDENTIFIER, t.type);

    scanner_init(&s, "andy");
    t = scanner_scan_token(&s);
    TEST_ASSERT_EQUAL(TOKEN_IDENTIFIER, t.type);

    scanner_init(&s, "candy");
    t = scanner_scan_token(&s);
    TEST_ASSERT_EQUAL(TOKEN_IDENTIFIER, t.type);
}

void test_scan_token_keyword_break(void)
{
    scanner_init(&s, "break");
    struct token t = scanner_scan_token(&s);
    TEST_ASSERT_EQUAL(TOKEN_BREAK, t.type);

    scanner_init(&s, "breakout");
    t = scanner_scan_token(&s);
    TEST_ASSERT_EQUAL(TOKEN_IDENTIFIER, t.type);

    scanner_init(&s, "_break");
    t = scanner_scan_token(&s);
    TEST_ASSERT_EQUAL(TOKEN_IDENTIFIER, t.type);

    scanner_init(&s, "unbreakable");
    t = scanner_scan_token(&s);
    TEST_ASSERT_EQUAL(TOKEN_IDENTIFIER, t.type);
}

void test_scan_token_keyword_continue(void)
{
    scanner_init(&s, "continue");
    struct token t = scanner_scan_token(&s);
    TEST_ASSERT_EQUAL(TOKEN_CONTINUE, t.type);

    scanner_init(&s, "continue_forever");
    t = scanner_scan_token(&s);
    TEST_ASSERT_EQUAL(TOKEN_IDENTIFIER, t.type);

    scanner_init(&s, "_continue");
    t = scanner_scan_token(&s);
    TEST_ASSERT_EQUAL(TOKEN_IDENTIFIER, t.type);

    scanner_init(&s, "_continue_");
    t = scanner_scan_token(&s);
    TEST_ASSERT_EQUAL(TOKEN_IDENTIFIER, t.type);
}

void test_scan_token_keyword_class(void)
{
    scanner_init(&s, "class");
    struct token t = scanner_scan_token(&s);
    TEST_ASSERT_EQUAL(TOKEN_CLASS, t.type);

    scanner_init(&s, "classy");
    t = scanner_scan_token(&s);
    TEST_ASSERT_EQUAL(TOKEN_IDENTIFIER, t.type);

    scanner_init(&s, "outclass");
    t = scanner_scan_token(&s);
    TEST_ASSERT_EQUAL(TOKEN_IDENTIFIER, t.type);

    scanner_init(&s, "not_classic");
    t = scanner_scan_token(&s);
    TEST_ASSERT_EQUAL(TOKEN_IDENTIFIER, t.type);
}

void test_scan_token_keyword_else(void)
{
    scanner_init(&s, "else");
    struct token t = scanner_scan_token(&s);
    TEST_ASSERT_EQUAL(TOKEN_ELSE, t.type);

    scanner_init(&s, "elsewhere");
    t = scanner_scan_token(&s);
    TEST_ASSERT_EQUAL(TOKEN_IDENTIFIER, t.type);

    scanner_init(&s, "_else");
    t = scanner_scan_token(&s);
    TEST_ASSERT_EQUAL(TOKEN_IDENTIFIER, t.type);

    scanner_init(&s, "nelsen");
    t = scanner_scan_token(&s);
    TEST_ASSERT_EQUAL(TOKEN_IDENTIFIER, t.type);
}

void test_scan_token_keyword_false(void)
{
    scanner_init(&s, "false");
    struct token t = scanner_scan_token(&s);
    TEST_ASSERT_EQUAL(TOKEN_FALSE, t.type);

    scanner_init(&s, "false_beliefs");
    t = scanner_scan_token(&s);
    TEST_ASSERT_EQUAL(TOKEN_IDENTIFIER, t.type);

    scanner_init(&s, "_false");
    t = scanner_scan_token(&s);
    TEST_ASSERT_EQUAL(TOKEN_IDENTIFIER, t.type);

    scanner_init(&s, "unfalsey");
    t = scanner_scan_token(&s);
    TEST_ASSERT_EQUAL(TOKEN_IDENTIFIER, t.type);
}

void test_scan_token_keyword_for(void)
{
    scanner_init(&s, "for");
    struct token t = scanner_scan_token(&s);
    TEST_ASSERT_EQUAL(TOKEN_FOR, t.type);

    scanner_init(&s, "forky");
    t = scanner_scan_token(&s);
    TEST_ASSERT_EQUAL(TOKEN_IDENTIFIER, t.type);

    scanner_init(&s, "therefor");
    t = scanner_scan_token(&s);
    TEST_ASSERT_EQUAL(TOKEN_IDENTIFIER, t.type);

    scanner_init(&s, "workforce");
    t = scanner_scan_token(&s);
    TEST_ASSERT_EQUAL(TOKEN_IDENTIFIER, t.type);
}

void test_scan_token_keyword_func(void)
{
    scanner_init(&s, "func");
    struct token t = scanner_scan_token(&s);
    TEST_ASSERT_EQUAL(TOKEN_FUNC, t.type);

    scanner_init(&s, "function");
    t = scanner_scan_token(&s);
    TEST_ASSERT_EQUAL(TOKEN_IDENTIFIER, t.type);

    scanner_init(&s, "_func");
    t = scanner_scan_token(&s);
    TEST_ASSERT_EQUAL(TOKEN_IDENTIFIER, t.type);

    scanner_init(&s, "perfunctory");
    t = scanner_scan_token(&s);
    TEST_ASSERT_EQUAL(TOKEN_IDENTIFIER, t.type);
}

void test_scan_token_keyword_if(void)
{
    scanner_init(&s, "if");
    struct token t = scanner_scan_token(&s);
    TEST_ASSERT_EQUAL(TOKEN_IF, t.type);

    scanner_init(&s, "iffy");
    t = scanner_scan_token(&s);
    TEST_ASSERT_EQUAL(TOKEN_IDENTIFIER, t.type);

    scanner_init(&s, "serif");
    t = scanner_scan_token(&s);
    TEST_ASSERT_EQUAL(TOKEN_IDENTIFIER, t.type);

    scanner_init(&s, "wife");
    t = scanner_scan_token(&s);
    TEST_ASSERT_EQUAL(TOKEN_IDENTIFIER, t.type);
}

void test_scan_token_keyword_nil(void)
{
    scanner_init(&s, "nil");
    struct token t = scanner_scan_token(&s);
    TEST_ASSERT_EQUAL(TOKEN_NIL, t.type);

    scanner_init(&s, "nilla");
    t = scanner_scan_token(&s);
    TEST_ASSERT_EQUAL(TOKEN_IDENTIFIER, t.type);

    scanner_init(&s, "_nil");
    t = scanner_scan_token(&s);
    TEST_ASSERT_EQUAL(TOKEN_IDENTIFIER, t.type);

    scanner_init(&s, "unilateral");
    t = scanner_scan_token(&s);
    TEST_ASSERT_EQUAL(TOKEN_IDENTIFIER, t.type);
}

void test_scan_token_keyword_or(void)
{
    scanner_init(&s, "or");
    struct token t = scanner_scan_token(&s);
    TEST_ASSERT_EQUAL(TOKEN_OR, t.type);

    scanner_init(&s, "organ");
    t = scanner_scan_token(&s);
    TEST_ASSERT_EQUAL(TOKEN_IDENTIFIER, t.type);

    scanner_init(&s, "nor");
    t = scanner_scan_token(&s);
    TEST_ASSERT_EQUAL(TOKEN_IDENTIFIER, t.type);

    scanner_init(&s, "torque");
    t = scanner_scan_token(&s);
    TEST_ASSERT_EQUAL(TOKEN_IDENTIFIER, t.type);
}

void test_scan_token_keyword_print(void)
{
    scanner_init(&s, "print");
    struct token t = scanner_scan_token(&s);
    TEST_ASSERT_EQUAL(TOKEN_PRINT, t.type);

    scanner_init(&s, "printer");
    t = scanner_scan_token(&s);
    TEST_ASSERT_EQUAL(TOKEN_IDENTIFIER, t.type);

    scanner_init(&s, "reprint");
    t = scanner_scan_token(&s);
    TEST_ASSERT_EQUAL(TOKEN_IDENTIFIER, t.type);

    scanner_init(&s, "unprintable");
    t = scanner_scan_token(&s);
    TEST_ASSERT_EQUAL(TOKEN_IDENTIFIER, t.type);
}

void test_scan_token_keyword_return(void)
{
    scanner_init(&s, "return");
    struct token t = scanner_scan_token(&s);
    TEST_ASSERT_EQUAL(TOKEN_RETURN, t.type);

    scanner_init(&s, "returning");
    t = scanner_scan_token(&s);
    TEST_ASSERT_EQUAL(TOKEN_IDENTIFIER, t.type);

    scanner_init(&s, "no_return");
    t = scanner_scan_token(&s);
    TEST_ASSERT_EQUAL(TOKEN_IDENTIFIER, t.type);

    scanner_init(&s, "unreturnable");
    t = scanner_scan_token(&s);
    TEST_ASSERT_EQUAL(TOKEN_IDENTIFIER, t.type);
}

void test_scan_token_keyword_super(void)
{
    scanner_init(&s, "super");
    struct token t = scanner_scan_token(&s);
    TEST_ASSERT_EQUAL(TOKEN_SUPER, t.type);

    scanner_init(&s, "superbly");
    t = scanner_scan_token(&s);
    TEST_ASSERT_EQUAL(TOKEN_IDENTIFIER, t.type);

    scanner_init(&s, "not_super");
    t = scanner_scan_token(&s);
    TEST_ASSERT_EQUAL(TOKEN_IDENTIFIER, t.type);

    scanner_init(&s, "really_super_duper");
    t = scanner_scan_token(&s);
    TEST_ASSERT_EQUAL(TOKEN_IDENTIFIER, t.type);
}

void test_scan_token_keyword_this(void)
{
    scanner_init(&s, "this");
    struct token t = scanner_scan_token(&s);
    TEST_ASSERT_EQUAL(TOKEN_THIS, t.type);

    scanner_init(&s, "thistle");
    t = scanner_scan_token(&s);
    TEST_ASSERT_EQUAL(TOKEN_IDENTIFIER, t.type);

    scanner_init(&s, "not_this");
    t = scanner_scan_token(&s);
    TEST_ASSERT_EQUAL(TOKEN_IDENTIFIER, t.type);

    scanner_init(&s, "baathist");
    t = scanner_scan_token(&s);
    TEST_ASSERT_EQUAL(TOKEN_IDENTIFIER, t.type);
}

void test_scan_token_keyword_true(void)
{
    scanner_init(&s, "true");
    struct token t = scanner_scan_token(&s);
    TEST_ASSERT_EQUAL(TOKEN_TRUE, t.type);

    scanner_init(&s, "true_or_false");
    t = scanner_scan_token(&s);
    TEST_ASSERT_EQUAL(TOKEN_IDENTIFIER, t.type);

    scanner_init(&s, "big_if_true");
    t = scanner_scan_token(&s);
    TEST_ASSERT_EQUAL(TOKEN_IDENTIFIER, t.type);

    scanner_init(&s, "misconstrued");
    t = scanner_scan_token(&s);
    TEST_ASSERT_EQUAL(TOKEN_IDENTIFIER, t.type);
}

void test_scan_token_keyword_var(void)
{
    scanner_init(&s, "var");
    struct token t = scanner_scan_token(&s);
    TEST_ASSERT_EQUAL(TOKEN_VAR, t.type);

    scanner_init(&s, "varnish");
    t = scanner_scan_token(&s);
    TEST_ASSERT_EQUAL(TOKEN_IDENTIFIER, t.type);

    scanner_init(&s, "_var");
    t = scanner_scan_token(&s);
    TEST_ASSERT_EQUAL(TOKEN_IDENTIFIER, t.type);

    scanner_init(&s, "aardvark");
    t = scanner_scan_token(&s);
    TEST_ASSERT_EQUAL(TOKEN_IDENTIFIER, t.type);
}

void test_scan_token_keyword_while(void)
{
    scanner_init(&s, "while");
    struct token t = scanner_scan_token(&s);
    TEST_ASSERT_EQUAL(TOKEN_WHILE, t.type);

    scanner_init(&s, "while_you_wait");
    t = scanner_scan_token(&s);
    TEST_ASSERT_EQUAL(TOKEN_IDENTIFIER, t.type);

    scanner_init(&s, "awhile");
    t = scanner_scan_token(&s);
    TEST_ASSERT_EQUAL(TOKEN_IDENTIFIER, t.type);

    scanner_init(&s, "worthwhile_stuff");
    t = scanner_scan_token(&s);
    TEST_ASSERT_EQUAL(TOKEN_IDENTIFIER, t.type);
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_init);
    RUN_TEST(test_scan_token_eof);
    RUN_TEST(test_scan_token_skips_whitespace);
    RUN_TEST(test_scan_token_identifiers);
    RUN_TEST(test_scan_token_number);
    RUN_TEST(test_scan_token_binary_number_invalid_literal);
    RUN_TEST(test_scan_token_number_invalid_literal);
    RUN_TEST(test_scan_token_parens);
    RUN_TEST(test_scan_token_braces);
    RUN_TEST(test_scan_token_bracket);
    RUN_TEST(test_scan_token_semicolon);
    RUN_TEST(test_scan_token_comma);
    RUN_TEST(test_scan_token_dot);
    RUN_TEST(test_scan_token_minus);
    RUN_TEST(test_scan_token_plus);
    RUN_TEST(test_scan_token_slash);
    RUN_TEST(test_scan_token_star);
    RUN_TEST(test_scan_token_percent);
    RUN_TEST(test_scan_token_caret);
    RUN_TEST(test_scan_token_tilde);
    RUN_TEST(test_scan_token_bang_equal);
    RUN_TEST(test_scan_token_bang);
    RUN_TEST(test_scan_token_equal_equal);
    RUN_TEST(test_scan_token_equal);
    RUN_TEST(test_scan_token_less);
    RUN_TEST(test_scan_token_less_equal);
    RUN_TEST(test_scan_token_less_less);
    RUN_TEST(test_scan_token_greater);
    RUN_TEST(test_scan_token_greater_equal);
    RUN_TEST(test_scan_token_greater_greater);
    RUN_TEST(test_scan_token_unexpected);
    RUN_TEST(test_scan_token_keyword_and);
    RUN_TEST(test_scan_token_keyword_break);
    RUN_TEST(test_scan_token_keyword_continue);
    RUN_TEST(test_scan_token_keyword_class);
    RUN_TEST(test_scan_token_keyword_else);
    RUN_TEST(test_scan_token_keyword_false);
    RUN_TEST(test_scan_token_keyword_for);
    RUN_TEST(test_scan_token_keyword_func);
    RUN_TEST(test_scan_token_keyword_if);
    RUN_TEST(test_scan_token_keyword_nil);
    RUN_TEST(test_scan_token_keyword_or);
    RUN_TEST(test_scan_token_keyword_print);
    RUN_TEST(test_scan_token_keyword_return);
    RUN_TEST(test_scan_token_keyword_super);
    RUN_TEST(test_scan_token_keyword_this);
    RUN_TEST(test_scan_token_keyword_true);
    RUN_TEST(test_scan_token_keyword_var);
    RUN_TEST(test_scan_token_keyword_while);

    return UNITY_END();
}
