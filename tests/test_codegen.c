/* Code Generator Tests */

#include "test.h"
#include "codegen.h"
#include "parser.h"
#include "arena.h"
#include "fern_string.h"
#include <string.h>

/* Helper to generate QBE from source code */
static const char* generate_qbe(Arena* arena, const char* src) {
    Parser* parser = parser_new(arena, src);
    StmtVec* stmts = parse_stmts(parser);
    if (!stmts || parser->had_error) return NULL;
    
    Codegen* cg = codegen_new(arena);
    codegen_program(cg, stmts);
    return string_cstr(codegen_output(cg));
}

/* Helper to generate QBE for a single expression */
static const char* generate_expr_qbe(Arena* arena, const char* src) {
    Parser* parser = parser_new(arena, src);
    Expr* expr = parse_expr(parser);
    if (!expr || parser->had_error) return NULL;
    
    Codegen* cg = codegen_new(arena);
    codegen_expr(cg, expr);
    return string_cstr(codegen_output(cg));
}

/* ========== Integer Literal Tests ========== */

void test_codegen_int_literal(void) {
    Arena* arena = arena_create(4096);
    
    const char* qbe = generate_expr_qbe(arena, "42");
    
    ASSERT_NOT_NULL(qbe);
    ASSERT_TRUE(strstr(qbe, "copy 42") != NULL);
    
    arena_destroy(arena);
}

void test_codegen_negative_int(void) {
    Arena* arena = arena_create(4096);
    
    const char* qbe = generate_expr_qbe(arena, "-5");
    
    ASSERT_NOT_NULL(qbe);
    /* -5 is parsed as unary negation of 5 */
    ASSERT_TRUE(strstr(qbe, "copy 5") != NULL);
    ASSERT_TRUE(strstr(qbe, "sub 0") != NULL);
    
    arena_destroy(arena);
}

/* ========== Binary Operation Tests ========== */

void test_codegen_add(void) {
    Arena* arena = arena_create(4096);
    
    const char* qbe = generate_expr_qbe(arena, "1 + 2");
    
    ASSERT_NOT_NULL(qbe);
    ASSERT_TRUE(strstr(qbe, "copy 1") != NULL);
    ASSERT_TRUE(strstr(qbe, "copy 2") != NULL);
    ASSERT_TRUE(strstr(qbe, "add") != NULL);
    
    arena_destroy(arena);
}

void test_codegen_sub(void) {
    Arena* arena = arena_create(4096);
    
    const char* qbe = generate_expr_qbe(arena, "10 - 3");
    
    ASSERT_NOT_NULL(qbe);
    ASSERT_TRUE(strstr(qbe, "sub") != NULL);
    
    arena_destroy(arena);
}

void test_codegen_mul(void) {
    Arena* arena = arena_create(4096);
    
    const char* qbe = generate_expr_qbe(arena, "4 * 5");
    
    ASSERT_NOT_NULL(qbe);
    ASSERT_TRUE(strstr(qbe, "mul") != NULL);
    
    arena_destroy(arena);
}

void test_codegen_div(void) {
    Arena* arena = arena_create(4096);
    
    const char* qbe = generate_expr_qbe(arena, "20 / 4");
    
    ASSERT_NOT_NULL(qbe);
    ASSERT_TRUE(strstr(qbe, "div") != NULL);
    
    arena_destroy(arena);
}

void test_codegen_complex_expr(void) {
    Arena* arena = arena_create(4096);
    
    /* (1 + 2) * 3 */
    const char* qbe = generate_expr_qbe(arena, "(1 + 2) * 3");
    
    ASSERT_NOT_NULL(qbe);
    ASSERT_TRUE(strstr(qbe, "add") != NULL);
    ASSERT_TRUE(strstr(qbe, "mul") != NULL);
    
    arena_destroy(arena);
}

/* ========== Comparison Tests ========== */

void test_codegen_eq(void) {
    Arena* arena = arena_create(4096);
    
    const char* qbe = generate_expr_qbe(arena, "1 == 2");
    
    ASSERT_NOT_NULL(qbe);
    ASSERT_TRUE(strstr(qbe, "ceqw") != NULL);
    
    arena_destroy(arena);
}

void test_codegen_lt(void) {
    Arena* arena = arena_create(4096);
    
    const char* qbe = generate_expr_qbe(arena, "1 < 2");
    
    ASSERT_NOT_NULL(qbe);
    ASSERT_TRUE(strstr(qbe, "csltw") != NULL);
    
    arena_destroy(arena);
}

/* ========== Function Definition Tests ========== */

void test_codegen_fn_simple(void) {
    Arena* arena = arena_create(4096);
    
    const char* qbe = generate_qbe(arena, "fn answer() -> Int: 42");
    
    ASSERT_NOT_NULL(qbe);
    ASSERT_TRUE(strstr(qbe, "function") != NULL);
    ASSERT_TRUE(strstr(qbe, "$answer") != NULL);
    ASSERT_TRUE(strstr(qbe, "ret") != NULL);
    
    arena_destroy(arena);
}

void test_codegen_fn_with_params(void) {
    Arena* arena = arena_create(4096);
    
    const char* qbe = generate_qbe(arena, "fn add(a: Int, b: Int) -> Int: a + b");
    
    ASSERT_NOT_NULL(qbe);
    ASSERT_TRUE(strstr(qbe, "$add") != NULL);
    ASSERT_TRUE(strstr(qbe, "%a") != NULL);
    ASSERT_TRUE(strstr(qbe, "%b") != NULL);
    ASSERT_TRUE(strstr(qbe, "add") != NULL);
    
    arena_destroy(arena);
}

void test_codegen_fn_call(void) {
    Arena* arena = arena_create(4096);
    
    const char* qbe = generate_expr_qbe(arena, "add(1, 2)");
    
    ASSERT_NOT_NULL(qbe);
    ASSERT_TRUE(strstr(qbe, "call $add") != NULL);
    
    arena_destroy(arena);
}

/* ========== If Expression Tests ========== */

void test_codegen_if_expr(void) {
    Arena* arena = arena_create(4096);
    
    const char* qbe = generate_expr_qbe(arena, "if true: 1 else: 0");
    
    ASSERT_NOT_NULL(qbe);
    ASSERT_TRUE(strstr(qbe, "jnz") != NULL);  /* Conditional jump */
    ASSERT_TRUE(strstr(qbe, "@L") != NULL);   /* Labels */
    
    arena_destroy(arena);
}

/* ========== Let Statement Tests ========== */

void test_codegen_let(void) {
    Arena* arena = arena_create(4096);
    
    const char* qbe = generate_qbe(arena, "let x = 42");
    
    ASSERT_NOT_NULL(qbe);
    ASSERT_TRUE(strstr(qbe, "%x") != NULL);
    ASSERT_TRUE(strstr(qbe, "copy") != NULL);
    
    arena_destroy(arena);
}

/* ========== String Tests ========== */

void test_codegen_string_literal(void) {
    Arena* arena = arena_create(4096);
    
    const char* qbe = generate_expr_qbe(arena, "\"hello\"");
    
    ASSERT_NOT_NULL(qbe);
    /* String literals should create a data section */
    ASSERT_TRUE(strstr(qbe, "data") != NULL);
    ASSERT_TRUE(strstr(qbe, "hello") != NULL);
    
    arena_destroy(arena);
}

void test_codegen_string_in_fn(void) {
    Arena* arena = arena_create(4096);
    
    const char* qbe = generate_qbe(arena, "fn greet() -> String: \"hello\"");
    
    ASSERT_NOT_NULL(qbe);
    ASSERT_TRUE(strstr(qbe, "$greet") != NULL);
    ASSERT_TRUE(strstr(qbe, "data") != NULL);
    
    arena_destroy(arena);
}

/* ========== Boolean Tests ========== */

void test_codegen_bool_true(void) {
    Arena* arena = arena_create(4096);
    
    const char* qbe = generate_expr_qbe(arena, "true");
    
    ASSERT_NOT_NULL(qbe);
    ASSERT_TRUE(strstr(qbe, "copy 1") != NULL);
    
    arena_destroy(arena);
}

void test_codegen_bool_false(void) {
    Arena* arena = arena_create(4096);
    
    const char* qbe = generate_expr_qbe(arena, "false");
    
    ASSERT_NOT_NULL(qbe);
    ASSERT_TRUE(strstr(qbe, "copy 0") != NULL);
    
    arena_destroy(arena);
}

/* ========== Float Tests ========== */

void test_codegen_float_literal(void) {
    Arena* arena = arena_create(4096);
    
    const char* qbe = generate_expr_qbe(arena, "3.14");
    
    ASSERT_NOT_NULL(qbe);
    /* Float literals use 'd' (double) type in QBE */
    ASSERT_TRUE(strstr(qbe, "3.14") != NULL);
    
    arena_destroy(arena);
}

/* ========== Match Expression Tests ========== */

void test_codegen_match_int(void) {
    Arena* arena = arena_create(4096);
    
    /* match x: 1 -> 10, 2 -> 20, _ -> 0 */
    const char* qbe = generate_qbe(arena, 
        "fn test(x: Int) -> Int: match x: 1 -> 10, 2 -> 20, _ -> 0");
    
    ASSERT_NOT_NULL(qbe);
    ASSERT_TRUE(strstr(qbe, "$test") != NULL);
    /* Should have comparison and jumps */
    ASSERT_TRUE(strstr(qbe, "ceqw") != NULL);  /* compare equal */
    ASSERT_TRUE(strstr(qbe, "jnz") != NULL);   /* conditional jump */
    
    arena_destroy(arena);
}

void test_codegen_match_wildcard(void) {
    Arena* arena = arena_create(4096);
    
    /* Simple match with wildcard */
    const char* qbe = generate_qbe(arena,
        "fn always_zero(x: Int) -> Int: match x: _ -> 0");
    
    ASSERT_NOT_NULL(qbe);
    ASSERT_TRUE(strstr(qbe, "copy 0") != NULL);
    
    arena_destroy(arena);
}

/* ========== Tuple Tests ========== */

void test_codegen_tuple(void) {
    Arena* arena = arena_create(4096);
    
    /* Simple tuple */
    const char* qbe = generate_expr_qbe(arena, "(1, 2, 3)");
    
    ASSERT_NOT_NULL(qbe);
    /* Tuple elements should be generated */
    ASSERT_TRUE(strstr(qbe, "copy 1") != NULL);
    ASSERT_TRUE(strstr(qbe, "copy 2") != NULL);
    ASSERT_TRUE(strstr(qbe, "copy 3") != NULL);
    
    arena_destroy(arena);
}

/* ========== List Tests ========== */

void test_codegen_list(void) {
    Arena* arena = arena_create(4096);
    
    /* Simple list */
    const char* qbe = generate_expr_qbe(arena, "[1, 2, 3]");
    
    ASSERT_NOT_NULL(qbe);
    /* List elements should be generated */
    ASSERT_TRUE(strstr(qbe, "copy 1") != NULL);
    ASSERT_TRUE(strstr(qbe, "copy 2") != NULL);
    ASSERT_TRUE(strstr(qbe, "copy 3") != NULL);
    
    arena_destroy(arena);
}

/* ========== Lambda Tests ========== */

void test_codegen_lambda(void) {
    Arena* arena = arena_create(4096);
    
    /* Lambda that adds 1 to its argument */
    const char* qbe = generate_expr_qbe(arena, "(x) -> x + 1");
    
    ASSERT_NOT_NULL(qbe);
    /* Lambda should generate a function */
    ASSERT_TRUE(strstr(qbe, "function") != NULL);
    ASSERT_TRUE(strstr(qbe, "add") != NULL);
    
    arena_destroy(arena);
}

/* ========== Try Expression (?) Tests ========== */

void test_codegen_try_operator(void) {
    Arena* arena = arena_create(4096);
    
    /* fn get_value() -> Result(Int, String): Ok(42)? */
    const char* qbe = generate_qbe(arena, 
        "fn get_value() -> Result(Int, String): Ok(42)?");
    
    ASSERT_NOT_NULL(qbe);
    /* ? operator should generate:
     * 1. Call to check if Result is Ok
     * 2. Conditional branch on error
     * 3. Return early if Err
     * 4. Unwrap value if Ok
     */
    ASSERT_TRUE(strstr(qbe, "call") != NULL);  /* Runtime call */
    ASSERT_TRUE(strstr(qbe, "jnz") != NULL);   /* Conditional branch */
    
    arena_destroy(arena);
}

void test_codegen_try_in_chain(void) {
    Arena* arena = arena_create(8192);  /* Larger arena for complex code */
    
    /* Multiple ? in sequence - each needs its own check */
    /* Simpler version: just test that ? generates expected pattern */
    const char* qbe = generate_qbe(arena,
        "fn chain(x: Int) -> Result(Int, String): Ok(x)?");
    
    ASSERT_NOT_NULL(qbe);
    /* Should have conditional branch for ? operator */
    ASSERT_TRUE(strstr(qbe, "jnz") != NULL);
    ASSERT_TRUE(strstr(qbe, "fern_result") != NULL);
    
    arena_destroy(arena);
}

/* ========== String Runtime Integration Tests ========== */

void test_codegen_string_concat(void) {
    Arena* arena = arena_create(8192);
    
    /* String concatenation - for now just verify the function compiles */
    /* Full string concat requires runtime integration */
    const char* qbe = generate_qbe(arena,
        "fn greet(name: String) -> String: name");
    
    ASSERT_NOT_NULL(qbe);
    ASSERT_TRUE(strstr(qbe, "$greet") != NULL);
    
    arena_destroy(arena);
}

void test_codegen_string_new(void) {
    Arena* arena = arena_create(4096);
    
    /* String literal should create a FernString via runtime */
    const char* qbe = generate_qbe(arena,
        "fn get_str() -> String: \"hello\"");
    
    ASSERT_NOT_NULL(qbe);
    /* Should have string data and potentially runtime call */
    ASSERT_TRUE(strstr(qbe, "data") != NULL);
    ASSERT_TRUE(strstr(qbe, "hello") != NULL);
    
    arena_destroy(arena);
}

/* ========== List Runtime Integration Tests ========== */

void test_codegen_list_new(void) {
    Arena* arena = arena_create(8192);
    
    /* List creation should call fern_list_new and fern_list_push */
    const char* qbe = generate_qbe(arena,
        "fn make_list() -> List(Int): [1, 2, 3]");
    
    ASSERT_NOT_NULL(qbe);
    /* Should call runtime list functions */
    ASSERT_TRUE(strstr(qbe, "$fern_list_new") != NULL);
    ASSERT_TRUE(strstr(qbe, "$fern_list_push") != NULL);
    
    arena_destroy(arena);
}

void test_codegen_list_index(void) {
    Arena* arena = arena_create(8192);
    
    /* List indexing should call fern_list_get */
    const char* qbe = generate_qbe(arena,
        "fn first(items: List(Int)) -> Int: items[0]");
    
    ASSERT_NOT_NULL(qbe);
    ASSERT_TRUE(strstr(qbe, "$first") != NULL);
    ASSERT_TRUE(strstr(qbe, "$fern_list_get") != NULL);
    
    arena_destroy(arena);
}

/* ========== Result Type Tests ========== */

void test_codegen_ok_constructor(void) {
    Arena* arena = arena_create(8192);
    
    /* Ok() constructor should create a Result with tag=0 */
    const char* qbe = generate_qbe(arena,
        "fn success() -> Result(Int, String): Ok(42)");
    
    ASSERT_NOT_NULL(qbe);
    ASSERT_TRUE(strstr(qbe, "$success") != NULL);
    /* Should call fern_result_ok */
    ASSERT_TRUE(strstr(qbe, "$fern_result_ok") != NULL);
    
    arena_destroy(arena);
}

void test_codegen_err_constructor(void) {
    Arena* arena = arena_create(8192);
    
    /* Err() constructor should create a Result with tag=1 */
    const char* qbe = generate_qbe(arena,
        "fn failure() -> Result(Int, String): Err(\"error\")");
    
    ASSERT_NOT_NULL(qbe);
    ASSERT_TRUE(strstr(qbe, "$failure") != NULL);
    /* Should call fern_result_err */
    ASSERT_TRUE(strstr(qbe, "$fern_result_err") != NULL);
    
    arena_destroy(arena);
}

/* ========== For Loop Tests ========== */

void test_codegen_for_loop(void) {
    Arena* arena = arena_create(8192);
    
    /* For loop iterating over a list */
    const char* qbe = generate_expr_qbe(arena, "for x in [1, 2, 3]: x");
    
    ASSERT_NOT_NULL(qbe);
    /* Should have loop structure: labels for loop start, body, end */
    ASSERT_TRUE(strstr(qbe, "@L") != NULL);  /* Should have labels */
    ASSERT_TRUE(strstr(qbe, "jnz") != NULL); /* Conditional jump for loop condition */
    ASSERT_TRUE(strstr(qbe, "jmp") != NULL); /* Unconditional jump back to loop start */
    /* Should call fern_list_len to get length */
    ASSERT_TRUE(strstr(qbe, "$fern_list_len") != NULL);
    /* Should call fern_list_get to get elements */
    ASSERT_TRUE(strstr(qbe, "$fern_list_get") != NULL);
    
    arena_destroy(arena);
}

void test_codegen_for_in_function(void) {
    Arena* arena = arena_create(8192);
    
    /* For loop inside a function */
    const char* qbe = generate_qbe(arena,
        "fn sum_list(items: List(Int)) -> Int: for x in items: x");
    
    ASSERT_NOT_NULL(qbe);
    ASSERT_TRUE(strstr(qbe, "$sum_list") != NULL);
    /* Should have loop labels */
    ASSERT_TRUE(strstr(qbe, "@L") != NULL);
    
    arena_destroy(arena);
}

/* ========== Defer Statement Tests ========== */

void test_codegen_defer_simple(void) {
    Arena* arena = arena_create(8192);
    
    /* Simple defer statement - cleanup should be called before return */
    const char* qbe = generate_qbe(arena,
        "fn process() -> Int: { defer cleanup(), 42 }");
    
    ASSERT_NOT_NULL(qbe);
    ASSERT_TRUE(strstr(qbe, "$process") != NULL);
    /* Should call cleanup before returning */
    ASSERT_TRUE(strstr(qbe, "$cleanup") != NULL);
    
    arena_destroy(arena);
}

void test_codegen_defer_multiple(void) {
    Arena* arena = arena_create(8192);
    
    /* Multiple defers - should run in reverse order (LIFO) */
    const char* qbe = generate_qbe(arena,
        "fn process() -> Int: { defer cleanup1(), defer cleanup2(), 42 }");
    
    ASSERT_NOT_NULL(qbe);
    /* Both cleanups should be called */
    ASSERT_TRUE(strstr(qbe, "$cleanup1") != NULL);
    ASSERT_TRUE(strstr(qbe, "$cleanup2") != NULL);
    /* cleanup2 should appear before cleanup1 in the output (LIFO) */
    const char* pos1 = strstr(qbe, "$cleanup1");
    const char* pos2 = strstr(qbe, "$cleanup2");
    ASSERT_TRUE(pos2 < pos1); /* cleanup2 runs first, so appears first */
    
    arena_destroy(arena);
}

/* ========== Test Runner ========== */

void run_codegen_tests(void) {
    printf("\n--- Code Generator Tests ---\n");
    
    /* Integer literals */
    TEST_RUN(test_codegen_int_literal);
    TEST_RUN(test_codegen_negative_int);
    
    /* Binary operations */
    TEST_RUN(test_codegen_add);
    TEST_RUN(test_codegen_sub);
    TEST_RUN(test_codegen_mul);
    TEST_RUN(test_codegen_div);
    TEST_RUN(test_codegen_complex_expr);
    
    /* Comparisons */
    TEST_RUN(test_codegen_eq);
    TEST_RUN(test_codegen_lt);
    
    /* Functions */
    TEST_RUN(test_codegen_fn_simple);
    TEST_RUN(test_codegen_fn_with_params);
    TEST_RUN(test_codegen_fn_call);
    
    /* Control flow */
    TEST_RUN(test_codegen_if_expr);
    
    /* Statements */
    TEST_RUN(test_codegen_let);
    
    /* Strings */
    TEST_RUN(test_codegen_string_literal);
    TEST_RUN(test_codegen_string_in_fn);
    
    /* Booleans */
    TEST_RUN(test_codegen_bool_true);
    TEST_RUN(test_codegen_bool_false);
    
    /* Float literals */
    TEST_RUN(test_codegen_float_literal);
    
    /* Match expressions */
    TEST_RUN(test_codegen_match_int);
    TEST_RUN(test_codegen_match_wildcard);
    
    /* Tuple expressions */
    TEST_RUN(test_codegen_tuple);
    
    /* List expressions */
    TEST_RUN(test_codegen_list);
    
    /* Lambda expressions */
    TEST_RUN(test_codegen_lambda);
    
    /* Try expression (? operator) */
    TEST_RUN(test_codegen_try_operator);
    TEST_RUN(test_codegen_try_in_chain);
    
    /* String runtime integration */
    TEST_RUN(test_codegen_string_concat);
    TEST_RUN(test_codegen_string_new);
    
    /* List runtime integration */
    TEST_RUN(test_codegen_list_new);
    TEST_RUN(test_codegen_list_index);
    
    /* Result type constructors */
    TEST_RUN(test_codegen_ok_constructor);
    TEST_RUN(test_codegen_err_constructor);
    
    /* For loops */
    TEST_RUN(test_codegen_for_loop);
    TEST_RUN(test_codegen_for_in_function);
    
    /* Defer statements */
    TEST_RUN(test_codegen_defer_simple);
    TEST_RUN(test_codegen_defer_multiple);
}
