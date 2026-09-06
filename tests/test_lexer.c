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
    Lexer* lex = lexer_new(arena, "for break continue in");

    Token t1 = lexer_next(lex);
    ASSERT_EQ(t1.type, TOKEN_FOR);

    Token t2 = lexer_next(lex);
    ASSERT_EQ(t2.type, TOKEN_BREAK);

    Token t3 = lexer_next(lex);
    ASSERT_EQ(t3.type, TOKEN_CONTINUE);

    Token t4 = lexer_next(lex);
    ASSERT_EQ(t4.type, TOKEN_IN);

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

void test_lex_string_escape(void) {
    Arena* arena = arena_create(4096);
    Lexer* lex = lexer_new(arena, "\"hello\\nworld\"");

    Token t1 = lexer_next(lex);
    ASSERT_EQ(t1.type, TOKEN_STRING);
    /* Escape sequences are processed: \n becomes actual newline */
    ASSERT_STR_EQ(string_cstr(t1.text), "hello\nworld");

    arena_destroy(arena);
}

void test_lex_string_escaped_quote(void) {
    Arena* arena = arena_create(4096);
    Lexer* lex = lexer_new(arena, "\"say \\\"hi\\\"\"");

    Token t1 = lexer_next(lex);
    ASSERT_EQ(t1.type, TOKEN_STRING);
    /* Escape sequences are processed: \" becomes actual quote */
    ASSERT_STR_EQ(string_cstr(t1.text), "say \"hi\"");

    arena_destroy(arena);
}

void test_lex_string_escaped_brace(void) {
    Arena* arena = arena_create(4096);
    Lexer* lex = lexer_new(arena, "\"\\{not interp}\"");

    Token t1 = lexer_next(lex);
    ASSERT_EQ(t1.type, TOKEN_STRING);
    /* Escape sequences are processed: \{ becomes literal { without starting interpolation */
    ASSERT_STR_EQ(string_cstr(t1.text), "{not interp}");

    arena_destroy(arena);
}

void test_lex_string_escaped_brace_only(void) {
    Arena* arena = arena_create(4096);
    /* Test string containing only an escaped brace */
    Lexer* lex = lexer_new(arena, "\"\\{\"");

    Token t1 = lexer_next(lex);
    ASSERT_EQ(t1.type, TOKEN_STRING);
    ASSERT_STR_EQ(string_cstr(t1.text), "{");

    arena_destroy(arena);
}

void test_lex_string_escaped_closing_brace(void) {
    Arena* arena = arena_create(4096);
    /* Test escaping closing brace */
    Lexer* lex = lexer_new(arena, "\"\\{hello\\}\"");

    Token t1 = lexer_next(lex);
    ASSERT_EQ(t1.type, TOKEN_STRING);
    ASSERT_STR_EQ(string_cstr(t1.text), "{hello}");

    arena_destroy(arena);
}

void test_lex_string_mixed_escapes(void) {
    Arena* arena = arena_create(4096);
    /* Test mix of escape sequences */
    Lexer* lex = lexer_new(arena, "\"line1\\nline2\\t\\{brace\\}\"");

    Token t1 = lexer_next(lex);
    ASSERT_EQ(t1.type, TOKEN_STRING);
    ASSERT_STR_EQ(string_cstr(t1.text), "line1\nline2\t{brace}");

    arena_destroy(arena);
}

void test_lex_string_trailing_brace(void) {
    Arena* arena = arena_create(4096);
    /* Test { at end of string - should NOT start interpolation */
    Lexer* lex = lexer_new(arena, "\"dfsdfs {\"");

    Token t1 = lexer_next(lex);
    ASSERT_EQ(t1.type, TOKEN_STRING);
    ASSERT_STR_EQ(string_cstr(t1.text), "dfsdfs {");

    arena_destroy(arena);
}

void test_lex_string_trailing_brace_interp(void) {
    Arena* arena = arena_create(4096);
    /* Test { at end of interpolated string - the trailing { should be literal */
    Lexer* lex = lexer_new(arena, "\"hello {name} world {\"");

    Token t1 = lexer_next(lex);
    ASSERT_EQ(t1.type, TOKEN_STRING_BEGIN);
    ASSERT_STR_EQ(string_cstr(t1.text), "hello ");

    /* Next should be identifier 'name' */
    Token t2 = lexer_next(lex);
    ASSERT_EQ(t2.type, TOKEN_IDENT);
    ASSERT_STR_EQ(string_cstr(t2.text), "name");

    /* Then STRING_END with " world {" - the trailing { is literal, not new interp */
    Token t3 = lexer_next(lex);
    ASSERT_EQ(t3.type, TOKEN_STRING_END);
    ASSERT_STR_EQ(string_cstr(t3.text), " world {");

    arena_destroy(arena);
}

void test_lex_hex_literal(void) {
    Arena* arena = arena_create(4096);
    Lexer* lex = lexer_new(arena, "0xFF");

    Token t1 = lexer_next(lex);
    ASSERT_EQ(t1.type, TOKEN_INT);
    ASSERT_STR_EQ(string_cstr(t1.text), "0xFF");

    arena_destroy(arena);
}

void test_lex_binary_literal(void) {
    Arena* arena = arena_create(4096);
    Lexer* lex = lexer_new(arena, "0b1010");

    Token t1 = lexer_next(lex);
    ASSERT_EQ(t1.type, TOKEN_INT);
    ASSERT_STR_EQ(string_cstr(t1.text), "0b1010");

    arena_destroy(arena);
}

void test_lex_octal_literal(void) {
    Arena* arena = arena_create(4096);
    Lexer* lex = lexer_new(arena, "0o755");

    Token t1 = lexer_next(lex);
    ASSERT_EQ(t1.type, TOKEN_INT);
    ASSERT_STR_EQ(string_cstr(t1.text), "0o755");

    arena_destroy(arena);
}

void test_lex_question_mark(void) {
    Arena* arena = arena_create(4096);
    Lexer* lex = lexer_new(arena, "value?");

    Token t1 = lexer_next(lex);
    ASSERT_EQ(t1.type, TOKEN_IDENT);
    ASSERT_STR_EQ(string_cstr(t1.text), "value");

    Token t2 = lexer_next(lex);
    ASSERT_EQ(t2.type, TOKEN_QUESTION);
    ASSERT_STR_EQ(string_cstr(t2.text), "?");

    arena_destroy(arena);
}

/* Test: Lex block comment */
void test_lex_block_comment(void) {
    Arena* arena = arena_create(4096);
    Lexer* lex = lexer_new(arena, "42 /* this is a comment */ 7");
    
    Token tok1 = lexer_next(lex);
    ASSERT_EQ(tok1.type, TOKEN_INT);
    ASSERT_STR_EQ(string_cstr(tok1.text), "42");
    
    // Block comment should be skipped
    Token tok2 = lexer_next(lex);
    ASSERT_EQ(tok2.type, TOKEN_INT);
    ASSERT_STR_EQ(string_cstr(tok2.text), "7");
    
    arena_destroy(arena);
}

/* Test: Lex multiline block comment */
void test_lex_block_comment_multiline(void) {
    Arena* arena = arena_create(4096);
    Lexer* lex = lexer_new(arena, "x /* comment\nspans\nlines */ y");
    
    Token tok1 = lexer_next(lex);
    ASSERT_EQ(tok1.type, TOKEN_IDENT);
    ASSERT_STR_EQ(string_cstr(tok1.text), "x");
    
    Token tok2 = lexer_next(lex);
    ASSERT_EQ(tok2.type, TOKEN_IDENT);
    ASSERT_STR_EQ(string_cstr(tok2.text), "y");
    
    arena_destroy(arena);
}

/* Test: Lex block comment at end of input */
void test_lex_block_comment_end(void) {
    Arena* arena = arena_create(4096);
    Lexer* lex = lexer_new(arena, "42 /* comment */");
    
    Token tok1 = lexer_next(lex);
    ASSERT_EQ(tok1.type, TOKEN_INT);
    
    Token tok2 = lexer_next(lex);
    ASSERT_EQ(tok2.type, TOKEN_EOF);
    
    arena_destroy(arena);
}

/* Test: Unicode identifier - Greek letter pi */
void test_lex_unicode_greek(void) {
    Arena* arena = arena_create(4096);
    Lexer* lex = lexer_new(arena, "π");
    
    Token tok = lexer_next(lex);
    ASSERT_EQ(tok.type, TOKEN_IDENT);
    ASSERT_STR_EQ(string_cstr(tok.text), "π");
    
    arena_destroy(arena);
}

/* Test: Unicode identifier - Japanese */
void test_lex_unicode_japanese(void) {
    Arena* arena = arena_create(4096);
    Lexer* lex = lexer_new(arena, "日本語");
    
    Token tok = lexer_next(lex);
    ASSERT_EQ(tok.type, TOKEN_IDENT);
    ASSERT_STR_EQ(string_cstr(tok.text), "日本語");
    
    arena_destroy(arena);
}

/* Test: Unicode identifier - emoji */
void test_lex_unicode_emoji(void) {
    Arena* arena = arena_create(4096);
    Lexer* lex = lexer_new(arena, "🚀");
    
    Token tok = lexer_next(lex);
    ASSERT_EQ(tok.type, TOKEN_IDENT);
    ASSERT_STR_EQ(string_cstr(tok.text), "🚀");
    
    arena_destroy(arena);
}

/* Test: Unicode identifier - mixed with ASCII */
void test_lex_unicode_mixed(void) {
    Arena* arena = arena_create(4096);
    Lexer* lex = lexer_new(arena, "calculate_π_value");
    
    Token tok = lexer_next(lex);
    ASSERT_EQ(tok.type, TOKEN_IDENT);
    ASSERT_STR_EQ(string_cstr(tok.text), "calculate_π_value");
    
    arena_destroy(arena);
}

/* Test: Unicode in let statement */
void test_lex_unicode_let(void) {
    Arena* arena = arena_create(4096);
    Lexer* lex = lexer_new(arena, "let π = 3.14159");
    
    Token tok1 = lexer_next(lex);
    ASSERT_EQ(tok1.type, TOKEN_LET);
    
    Token tok2 = lexer_next(lex);
    ASSERT_EQ(tok2.type, TOKEN_IDENT);
    ASSERT_STR_EQ(string_cstr(tok2.text), "π");
    
    Token tok3 = lexer_next(lex);
    ASSERT_EQ(tok3.type, TOKEN_ASSIGN);
    
    Token tok4 = lexer_next(lex);
    ASSERT_EQ(tok4.type, TOKEN_FLOAT);
    
    arena_destroy(arena);
}

/* Test: Simple indentation - single indent level */
void test_lex_indent_simple(void) {
    Arena* arena = arena_create(4096);
    // Note: Using 4 spaces for indentation
    Lexer* lex = lexer_new(arena, "if true:\n    42");
    
    Token tok1 = lexer_next(lex);
    ASSERT_EQ(tok1.type, TOKEN_IF);
    
    Token tok2 = lexer_next(lex);
    ASSERT_EQ(tok2.type, TOKEN_TRUE);
    
    Token tok3 = lexer_next(lex);
    ASSERT_EQ(tok3.type, TOKEN_COLON);
    
    Token tok4 = lexer_next(lex);
    ASSERT_EQ(tok4.type, TOKEN_NEWLINE);
    
    Token tok5 = lexer_next(lex);
    ASSERT_EQ(tok5.type, TOKEN_INDENT);
    
    Token tok6 = lexer_next(lex);
    ASSERT_EQ(tok6.type, TOKEN_INT);
    
    Token tok7 = lexer_next(lex);
    ASSERT_EQ(tok7.type, TOKEN_DEDENT);
    
    Token tok8 = lexer_next(lex);
    ASSERT_EQ(tok8.type, TOKEN_EOF);
    
    arena_destroy(arena);
}

/* Test: Indentation with dedent at end */
void test_lex_indent_dedent(void) {
    Arena* arena = arena_create(4096);
    Lexer* lex = lexer_new(arena, "fn foo():\n    x\ny");
    
    Token tok;
    
    tok = lexer_next(lex);
    ASSERT_EQ(tok.type, TOKEN_FN);
    
    tok = lexer_next(lex);
    ASSERT_EQ(tok.type, TOKEN_IDENT);  // foo
    
    tok = lexer_next(lex);
    ASSERT_EQ(tok.type, TOKEN_LPAREN);
    
    tok = lexer_next(lex);
    ASSERT_EQ(tok.type, TOKEN_RPAREN);
    
    tok = lexer_next(lex);
    ASSERT_EQ(tok.type, TOKEN_COLON);
    
    tok = lexer_next(lex);
    ASSERT_EQ(tok.type, TOKEN_NEWLINE);
    
    tok = lexer_next(lex);
    ASSERT_EQ(tok.type, TOKEN_INDENT);
    
    tok = lexer_next(lex);
    ASSERT_EQ(tok.type, TOKEN_IDENT);  // x
    
    tok = lexer_next(lex);
    ASSERT_EQ(tok.type, TOKEN_NEWLINE);
    
    tok = lexer_next(lex);
    ASSERT_EQ(tok.type, TOKEN_DEDENT);
    
    tok = lexer_next(lex);
    ASSERT_EQ(tok.type, TOKEN_IDENT);  // y
    
    tok = lexer_next(lex);
    ASSERT_EQ(tok.type, TOKEN_EOF);
    
    arena_destroy(arena);
}

/* Test: Multiple indent levels */
void test_lex_indent_multiple_levels(void) {
    Arena* arena = arena_create(4096);
    Lexer* lex = lexer_new(arena, "if a:\n    if b:\n        x\n    y\nz");
    
    Token tok;
    
    // if a:
    tok = lexer_next(lex);
    ASSERT_EQ(tok.type, TOKEN_IF);
    tok = lexer_next(lex);
    ASSERT_EQ(tok.type, TOKEN_IDENT);  // a
    tok = lexer_next(lex);
    ASSERT_EQ(tok.type, TOKEN_COLON);
    tok = lexer_next(lex);
    ASSERT_EQ(tok.type, TOKEN_NEWLINE);
    
    // INDENT (level 1)
    tok = lexer_next(lex);
    ASSERT_EQ(tok.type, TOKEN_INDENT);
    
    // if b:
    tok = lexer_next(lex);
    ASSERT_EQ(tok.type, TOKEN_IF);
    tok = lexer_next(lex);
    ASSERT_EQ(tok.type, TOKEN_IDENT);  // b
    tok = lexer_next(lex);
    ASSERT_EQ(tok.type, TOKEN_COLON);
    tok = lexer_next(lex);
    ASSERT_EQ(tok.type, TOKEN_NEWLINE);
    
    // INDENT (level 2)
    tok = lexer_next(lex);
    ASSERT_EQ(tok.type, TOKEN_INDENT);
    
    // x
    tok = lexer_next(lex);
    ASSERT_EQ(tok.type, TOKEN_IDENT);  // x
    tok = lexer_next(lex);
    ASSERT_EQ(tok.type, TOKEN_NEWLINE);
    
    // DEDENT (back to level 1)
    tok = lexer_next(lex);
    ASSERT_EQ(tok.type, TOKEN_DEDENT);
    
    // y
    tok = lexer_next(lex);
    ASSERT_EQ(tok.type, TOKEN_IDENT);  // y
    tok = lexer_next(lex);
    ASSERT_EQ(tok.type, TOKEN_NEWLINE);
    
    // DEDENT (back to level 0)
    tok = lexer_next(lex);
    ASSERT_EQ(tok.type, TOKEN_DEDENT);
    
    // z
    tok = lexer_next(lex);
    ASSERT_EQ(tok.type, TOKEN_IDENT);  // z
    
    tok = lexer_next(lex);
    ASSERT_EQ(tok.type, TOKEN_EOF);
    
    arena_destroy(arena);
}

/* Test: Blank lines don't affect indentation */
void test_lex_indent_blank_lines(void) {
    Arena* arena = arena_create(4096);
    Lexer* lex = lexer_new(arena, "if a:\n    x\n\n    y");
    
    Token tok;
    
    // if a:
    tok = lexer_next(lex);
    ASSERT_EQ(tok.type, TOKEN_IF);
    tok = lexer_next(lex);
    ASSERT_EQ(tok.type, TOKEN_IDENT);
    tok = lexer_next(lex);
    ASSERT_EQ(tok.type, TOKEN_COLON);
    tok = lexer_next(lex);
    ASSERT_EQ(tok.type, TOKEN_NEWLINE);
    
    tok = lexer_next(lex);
    ASSERT_EQ(tok.type, TOKEN_INDENT);
    
    // x
    tok = lexer_next(lex);
    ASSERT_EQ(tok.type, TOKEN_IDENT);
    tok = lexer_next(lex);
    ASSERT_EQ(tok.type, TOKEN_NEWLINE);
    
    // blank line - should be skipped, no dedent
    // y (still at same indent level)
    tok = lexer_next(lex);
    ASSERT_EQ(tok.type, TOKEN_IDENT);  // y, not DEDENT
    
    tok = lexer_next(lex);
    ASSERT_EQ(tok.type, TOKEN_DEDENT);
    
    tok = lexer_next(lex);
    ASSERT_EQ(tok.type, TOKEN_EOF);
    
    arena_destroy(arena);
}

/* Compare every token to a fresh lexer after speculative lookahead and rollback. */
static void check_lexer_replay(const char* source, bool peek_only) {
    Arena* arena = arena_create(4096);
    Lexer* actual = lexer_new(arena, source);
    Lexer* reference = lexer_new(arena, source);
    for (int index = 0; index < 256; index++) {
        Token expected = lexer_next(reference);
        if (peek_only && (expected.type == TOKEN_LPAREN || expected.type == TOKEN_LBRACKET)) {
            for (int repeat = 0; repeat < 3; repeat++) {
                Token peeked = lexer_peek(actual);
                ASSERT_EQ(peeked.type, expected.type);
            }
        } else if (!peek_only) {
            LexerState state = lexer_save(actual);
            for (int advance = 0; advance < 7; advance++) {
                if (lexer_next(actual).type == TOKEN_EOF) break;
            }
            lexer_restore(actual, state);
        }
        Token got = lexer_next(actual);
        ASSERT_EQ(got.type, expected.type);
        ASSERT_EQ(got.loc.line, expected.loc.line);
        ASSERT_EQ(got.loc.column, expected.loc.column);
        if (got.text && expected.text) {
            ASSERT_STR_EQ(string_cstr(got.text), string_cstr(expected.text));
        }
        if (expected.type == TOKEN_EOF) {
            arena_destroy(arena);
            return;
        }
    }
    ASSERT_TRUE(false);
    arena_destroy(arena);
}

void test_lex_peek_preserves_bracket_layout(void) {
    check_lexer_replay("fn main():\n    let x = f(g(1), [2, 3])\n    if x:\n        x\n    x\n", true);
}

void test_lex_restore_preserves_brackets_and_pending_dedents(void) {
    check_lexer_replay("fn main():\n    if true:\n        if false:\n            f((1), [2])\n    3\nfn end():4\n", false);
}

void test_lex_restore_preserves_interpolation_and_blank_lines(void) {
    check_lexer_replay("fn main():\n    let x = \"a{f(1)}b{2}c\"\n\n    # comment\n    x\n", false);
}

void run_lexer_tests(void) {
    printf("\n=== Lexer Tests ===\n");
    TEST_RUN(test_lex_peek_preserves_bracket_layout);
    TEST_RUN(test_lex_restore_preserves_brackets_and_pending_dedents);
    TEST_RUN(test_lex_restore_preserves_interpolation_and_blank_lines);
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
    TEST_RUN(test_lex_string_escape);
    TEST_RUN(test_lex_string_escaped_quote);
    TEST_RUN(test_lex_string_escaped_brace);
    TEST_RUN(test_lex_string_escaped_brace_only);
    TEST_RUN(test_lex_string_escaped_closing_brace);
    TEST_RUN(test_lex_string_mixed_escapes);
    TEST_RUN(test_lex_string_trailing_brace);
    TEST_RUN(test_lex_string_trailing_brace_interp);
    TEST_RUN(test_lex_hex_literal);
    TEST_RUN(test_lex_binary_literal);
    TEST_RUN(test_lex_octal_literal);
    TEST_RUN(test_lex_question_mark);
    TEST_RUN(test_lex_block_comment);
    TEST_RUN(test_lex_block_comment_multiline);
    TEST_RUN(test_lex_block_comment_end);
    TEST_RUN(test_lex_unicode_greek);
    TEST_RUN(test_lex_unicode_japanese);
    TEST_RUN(test_lex_unicode_emoji);
    TEST_RUN(test_lex_unicode_mixed);
    TEST_RUN(test_lex_unicode_let);
    TEST_RUN(test_lex_indent_simple);
    TEST_RUN(test_lex_indent_dedent);
    TEST_RUN(test_lex_indent_multiple_levels);
    TEST_RUN(test_lex_indent_blank_lines);
}
