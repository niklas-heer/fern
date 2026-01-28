/* Lexer Tests
 * 
 * Test-driven development: Write tests FIRST, then implement lexer.
 * Based on DESIGN.md specification.
 */

#include "test.h"
#include "arena.h"
#include "token.h"
#include "lexer.h"

/* Test: Lex simple integer */
void test_lex_integer(void) {
    Arena* arena = arena_create(4096);
    Lexer* lex = lexer_new(arena, "42");
    
    Token tok = lexer_next(lex);
    ASSERT_EQ(tok.type, TOKEN_INT);
    ASSERT_STR_EQ(string_cstr(tok.text), "42");
    
    Token eof = lexer_next(lex);
    ASSERT_EQ(eof.type, TOKEN_EOF);
    
    arena_destroy(arena);
}

/* Test: Lex identifier */
void test_lex_identifier(void) {
    Arena* arena = arena_create(4096);
    Lexer* lex = lexer_new(arena, "hello_world");
    
    Token tok = lexer_next(lex);
    ASSERT_EQ(tok.type, TOKEN_IDENT);
    ASSERT_STR_EQ(string_cstr(tok.text), "hello_world");
    
    arena_destroy(arena);
}

/* Test: Lex keywords */
void test_lex_keywords(void) {
    Arena* arena = arena_create(4096);
    
    // Test 'let' keyword
    Lexer* lex1 = lexer_new(arena, "let");
    Token tok1 = lexer_next(lex1);
    ASSERT_EQ(tok1.type, TOKEN_LET);
    
    // Test 'fn' keyword
    Lexer* lex2 = lexer_new(arena, "fn");
    Token tok2 = lexer_next(lex2);
    ASSERT_EQ(tok2.type, TOKEN_FN);
    
    // Test 'if' keyword
    Lexer* lex3 = lexer_new(arena, "if");
    Token tok3 = lexer_next(lex3);
    ASSERT_EQ(tok3.type, TOKEN_IF);
    
    // Test 'match' keyword
    Lexer* lex4 = lexer_new(arena, "match");
    Token tok4 = lexer_next(lex4);
    ASSERT_EQ(tok4.type, TOKEN_MATCH);
    
    // Test 'true' keyword
    Lexer* lex5 = lexer_new(arena, "true");
    Token tok5 = lexer_next(lex5);
    ASSERT_EQ(tok5.type, TOKEN_TRUE);
    
    // Test 'false' keyword
    Lexer* lex6 = lexer_new(arena, "false");
    Token tok6 = lexer_next(lex6);
    ASSERT_EQ(tok6.type, TOKEN_FALSE);
    
    arena_destroy(arena);
}

/* Test: Lex <- bind operator (critical for error handling) */
void test_lex_bind_operator(void) {
    Arena* arena = arena_create(4096);
    Lexer* lex = lexer_new(arena, "content <- read_file");
    
    Token tok1 = lexer_next(lex);
    ASSERT_EQ(tok1.type, TOKEN_IDENT);
    ASSERT_STR_EQ(string_cstr(tok1.text), "content");
    
    Token tok2 = lexer_next(lex);
    ASSERT_EQ(tok2.type, TOKEN_BIND);
    ASSERT_STR_EQ(string_cstr(tok2.text), "<-");
    
    Token tok3 = lexer_next(lex);
    ASSERT_EQ(tok3.type, TOKEN_IDENT);
    ASSERT_STR_EQ(string_cstr(tok3.text), "read_file");
    
    arena_destroy(arena);
}

/* Test: Lex string literal */
void test_lex_string(void) {
    Arena* arena = arena_create(4096);
    Lexer* lex = lexer_new(arena, "\"hello world\"");
    
    Token tok = lexer_next(lex);
    ASSERT_EQ(tok.type, TOKEN_STRING);
    ASSERT_STR_EQ(string_cstr(tok.text), "hello world");
    
    arena_destroy(arena);
}

/* Test: Lex operators */
void test_lex_operators(void) {
    Arena* arena = arena_create(4096);
    Lexer* lex = lexer_new(arena, "+ - * / == != < <= > >= ->");
    
    Token tok1 = lexer_next(lex);
    ASSERT_EQ(tok1.type, TOKEN_PLUS);
    
    Token tok2 = lexer_next(lex);
    ASSERT_EQ(tok2.type, TOKEN_MINUS);
    
    Token tok3 = lexer_next(lex);
    ASSERT_EQ(tok3.type, TOKEN_STAR);
    
    Token tok4 = lexer_next(lex);
    ASSERT_EQ(tok4.type, TOKEN_SLASH);
    
    Token tok5 = lexer_next(lex);
    ASSERT_EQ(tok5.type, TOKEN_EQ);
    
    Token tok6 = lexer_next(lex);
    ASSERT_EQ(tok6.type, TOKEN_NE);
    
    Token tok7 = lexer_next(lex);
    ASSERT_EQ(tok7.type, TOKEN_LT);
    
    Token tok8 = lexer_next(lex);
    ASSERT_EQ(tok8.type, TOKEN_LE);
    
    Token tok9 = lexer_next(lex);
    ASSERT_EQ(tok9.type, TOKEN_GT);
    
    Token tok10 = lexer_next(lex);
    ASSERT_EQ(tok10.type, TOKEN_GE);
    
    Token tok11 = lexer_next(lex);
    ASSERT_EQ(tok11.type, TOKEN_ARROW);
    
    arena_destroy(arena);
}

/* Test: Lex delimiters */
void test_lex_delimiters(void) {
    Arena* arena = arena_create(4096);
    Lexer* lex = lexer_new(arena, "( ) [ ] { } , : .");
    
    ASSERT_EQ(lexer_next(lex).type, TOKEN_LPAREN);
    ASSERT_EQ(lexer_next(lex).type, TOKEN_RPAREN);
    ASSERT_EQ(lexer_next(lex).type, TOKEN_LBRACKET);
    ASSERT_EQ(lexer_next(lex).type, TOKEN_RBRACKET);
    ASSERT_EQ(lexer_next(lex).type, TOKEN_LBRACE);
    ASSERT_EQ(lexer_next(lex).type, TOKEN_RBRACE);
    ASSERT_EQ(lexer_next(lex).type, TOKEN_COMMA);
    ASSERT_EQ(lexer_next(lex).type, TOKEN_COLON);
    ASSERT_EQ(lexer_next(lex).type, TOKEN_DOT);
    
    arena_destroy(arena);
}

/* Test: Lex simple assignment */
void test_lex_assignment(void) {
    Arena* arena = arena_create(4096);
    Lexer* lex = lexer_new(arena, "let x = 42");
    
    ASSERT_EQ(lexer_next(lex).type, TOKEN_LET);
    ASSERT_EQ(lexer_next(lex).type, TOKEN_IDENT);
    ASSERT_EQ(lexer_next(lex).type, TOKEN_ASSIGN);
    
    Token num = lexer_next(lex);
    ASSERT_EQ(num.type, TOKEN_INT);
    ASSERT_STR_EQ(string_cstr(num.text), "42");
    
    arena_destroy(arena);
}

/* Test: Lex single-line comment */
void test_lex_comment(void) {
    Arena* arena = arena_create(4096);
    Lexer* lex = lexer_new(arena, "42 # this is a comment");
    
    Token tok1 = lexer_next(lex);
    ASSERT_EQ(tok1.type, TOKEN_INT);
    
    // Comments should be skipped
    Token tok2 = lexer_next(lex);
    ASSERT_EQ(tok2.type, TOKEN_EOF);
    
    arena_destroy(arena);
}

/* Test: Lex function definition */
void test_lex_function(void) {
    Arena* arena = arena_create(4096);
    Lexer* lex = lexer_new(arena, "fn add(a: Int, b: Int) -> Int:");
    
    ASSERT_EQ(lexer_next(lex).type, TOKEN_FN);
    ASSERT_EQ(lexer_next(lex).type, TOKEN_IDENT);  // add
    ASSERT_EQ(lexer_next(lex).type, TOKEN_LPAREN);
    ASSERT_EQ(lexer_next(lex).type, TOKEN_IDENT);  // a
    ASSERT_EQ(lexer_next(lex).type, TOKEN_COLON);
    ASSERT_EQ(lexer_next(lex).type, TOKEN_IDENT);  // Int
    ASSERT_EQ(lexer_next(lex).type, TOKEN_COMMA);
    ASSERT_EQ(lexer_next(lex).type, TOKEN_IDENT);  // b
    ASSERT_EQ(lexer_next(lex).type, TOKEN_COLON);
    ASSERT_EQ(lexer_next(lex).type, TOKEN_IDENT);  // Int
    ASSERT_EQ(lexer_next(lex).type, TOKEN_RPAREN);
    ASSERT_EQ(lexer_next(lex).type, TOKEN_ARROW);
    ASSERT_EQ(lexer_next(lex).type, TOKEN_IDENT);  // Int
    ASSERT_EQ(lexer_next(lex).type, TOKEN_COLON);
    
    arena_destroy(arena);
}

/* Test: Distinguish < from <- from <= */
void test_lex_lt_bind_le(void) {
    Arena* arena = arena_create(4096);
    Lexer* lex = lexer_new(arena, "x < y <- z <= w");
    
    lexer_next(lex);  // x
    ASSERT_EQ(lexer_next(lex).type, TOKEN_LT);      // <
    lexer_next(lex);  // y
    ASSERT_EQ(lexer_next(lex).type, TOKEN_BIND);    // <-
    lexer_next(lex);  // z
    ASSERT_EQ(lexer_next(lex).type, TOKEN_LE);      // <=
    
    arena_destroy(arena);
}

/* Test: Lex simple float literal */
void test_lex_float_simple(void) {
    Arena* arena = arena_create(4096);
    Lexer* lex = lexer_new(arena, "3.14");

    Token tok = lexer_next(lex);
    ASSERT_EQ(tok.type, TOKEN_FLOAT);
    ASSERT_STR_EQ(string_cstr(tok.text), "3.14");

    Token eof = lexer_next(lex);
    ASSERT_EQ(eof.type, TOKEN_EOF);

    arena_destroy(arena);
}

/* Test: Lex float with leading zero */
void test_lex_float_leading_zero(void) {
    Arena* arena = arena_create(4096);
    Lexer* lex = lexer_new(arena, "0.5");

    Token tok = lexer_next(lex);
    ASSERT_EQ(tok.type, TOKEN_FLOAT);
    ASSERT_STR_EQ(string_cstr(tok.text), "0.5");

    arena_destroy(arena);
}

/* Test: Lex float with trailing zero */
void test_lex_float_trailing_zero(void) {
    Arena* arena = arena_create(4096);
    Lexer* lex = lexer_new(arena, "1.0");

    Token tok = lexer_next(lex);
    ASSERT_EQ(tok.type, TOKEN_FLOAT);
    ASSERT_STR_EQ(string_cstr(tok.text), "1.0");

    arena_destroy(arena);
}

/* Test: Lex loop keywords */
void test_lex_loop_keywords(void) {
    Arena* arena = arena_create(4096);
    Lexer* lex = lexer_new(arena, "for while loop break continue in");

    Token t1 = lexer_next(lex);
    ASSERT_EQ(t1.type, TOKEN_FOR);

    Token t2 = lexer_next(lex);
    ASSERT_EQ(t2.type, TOKEN_WHILE);

    Token t3 = lexer_next(lex);
    ASSERT_EQ(t3.type, TOKEN_LOOP);

    Token t4 = lexer_next(lex);
    ASSERT_EQ(t4.type, TOKEN_BREAK);

    Token t5 = lexer_next(lex);
    ASSERT_EQ(t5.type, TOKEN_CONTINUE);

    Token t6 = lexer_next(lex);
    ASSERT_EQ(t6.type, TOKEN_IN);

    arena_destroy(arena);
}

/* Test: String interpolation produces BEGIN, expr tokens, END */
void test_lex_string_interpolation(void) {
    Arena* arena = arena_create(4096);
    Lexer* lex = lexer_new(arena, "\"Hello, {name}!\"");

    Token t1 = lexer_next(lex);
    ASSERT_EQ(t1.type, TOKEN_STRING_BEGIN);
    ASSERT_STR_EQ(string_cstr(t1.text), "Hello, ");

    Token t2 = lexer_next(lex);
    ASSERT_EQ(t2.type, TOKEN_IDENT);
    ASSERT_STR_EQ(string_cstr(t2.text), "name");

    Token t3 = lexer_next(lex);
    ASSERT_EQ(t3.type, TOKEN_STRING_END);
    ASSERT_STR_EQ(string_cstr(t3.text), "!");

    arena_destroy(arena);
}

/* Test: String without interpolation stays TOKEN_STRING */
void test_lex_string_no_interpolation(void) {
    Arena* arena = arena_create(4096);
    Lexer* lex = lexer_new(arena, "\"hello world\"");

    Token t1 = lexer_next(lex);
    ASSERT_EQ(t1.type, TOKEN_STRING);
    ASSERT_STR_EQ(string_cstr(t1.text), "hello world");

    arena_destroy(arena);
}

void run_lexer_tests(void) {
    printf("\n=== Lexer Tests ===\n");
    TEST_RUN(test_lex_integer);
    TEST_RUN(test_lex_identifier);
    TEST_RUN(test_lex_keywords);
    TEST_RUN(test_lex_bind_operator);
    TEST_RUN(test_lex_string);
    TEST_RUN(test_lex_operators);
    TEST_RUN(test_lex_delimiters);
    TEST_RUN(test_lex_assignment);
    TEST_RUN(test_lex_comment);
    TEST_RUN(test_lex_function);
    TEST_RUN(test_lex_lt_bind_le);
    TEST_RUN(test_lex_float_simple);
    TEST_RUN(test_lex_float_leading_zero);
    TEST_RUN(test_lex_float_trailing_zero);
    TEST_RUN(test_lex_loop_keywords);
    TEST_RUN(test_lex_string_interpolation);
    TEST_RUN(test_lex_string_no_interpolation);
}
