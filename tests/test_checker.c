/* Type Checker Tests */

#include "test.h"
#include "checker.h"
#include "parser.h"
#include "arena.h"
#include "fern_string.h"

/* Helper to type check an expression string */
static Type* check_expr(Arena* arena, const char* src) {
    Parser* parser = parser_new(arena, src);
    Expr* expr = parse_expr(parser);
    if (!expr || parser->had_error) return NULL;
    
    Checker* checker = checker_new(arena);
    return checker_infer_expr(checker, expr);
}

/* Helper to get error from checker */
static const char* check_expr_error(Arena* arena, const char* src) {
    Parser* parser = parser_new(arena, src);
    Expr* expr = parse_expr(parser);
    if (!expr || parser->had_error) return "parse error";
    
    Checker* checker = checker_new(arena);
    Type* t = checker_infer_expr(checker, expr);
    if (t && t->kind == TYPE_ERROR) {
        return string_cstr(t->data.error_msg);
    }
    if (checker_has_errors(checker)) {
        return checker_first_error(checker);
    }
    return NULL;
}

/* ========== Literal Type Tests ========== */

void test_check_int_literal(void) {
    Arena* arena = arena_create(4096);
    
    Type* t = check_expr(arena, "42");
    
    ASSERT_NOT_NULL(t);
    ASSERT_EQ(t->kind, TYPE_INT);
    
    arena_destroy(arena);
}

void test_check_float_literal(void) {
    Arena* arena = arena_create(4096);
    
    Type* t = check_expr(arena, "3.14");
    
    ASSERT_NOT_NULL(t);
    ASSERT_EQ(t->kind, TYPE_FLOAT);
    
    arena_destroy(arena);
}

void test_check_string_literal(void) {
    Arena* arena = arena_create(4096);
    
    Type* t = check_expr(arena, "\"hello\"");
    
    ASSERT_NOT_NULL(t);
    ASSERT_EQ(t->kind, TYPE_STRING);
    
    arena_destroy(arena);
}

void test_check_bool_true(void) {
    Arena* arena = arena_create(4096);
    
    Type* t = check_expr(arena, "true");
    
    ASSERT_NOT_NULL(t);
    ASSERT_EQ(t->kind, TYPE_BOOL);
    
    arena_destroy(arena);
}

void test_check_bool_false(void) {
    Arena* arena = arena_create(4096);
    
    Type* t = check_expr(arena, "false");
    
    ASSERT_NOT_NULL(t);
    ASSERT_EQ(t->kind, TYPE_BOOL);
    
    arena_destroy(arena);
}

/* ========== Binary Operator Tests ========== */

void test_check_add_int(void) {
    Arena* arena = arena_create(4096);
    
    Type* t = check_expr(arena, "1 + 2");
    
    ASSERT_NOT_NULL(t);
    ASSERT_EQ(t->kind, TYPE_INT);
    
    arena_destroy(arena);
}

void test_check_add_float(void) {
    Arena* arena = arena_create(4096);
    
    Type* t = check_expr(arena, "1.5 + 2.5");
    
    ASSERT_NOT_NULL(t);
    ASSERT_EQ(t->kind, TYPE_FLOAT);
    
    arena_destroy(arena);
}

void test_check_add_mixed_error(void) {
    Arena* arena = arena_create(4096);
    
    const char* err = check_expr_error(arena, "1 + 2.5");
    
    ASSERT_NOT_NULL(err);
    // Should contain info about type mismatch
    
    arena_destroy(arena);
}

void test_check_sub_int(void) {
    Arena* arena = arena_create(4096);
    
    Type* t = check_expr(arena, "10 - 3");
    
    ASSERT_NOT_NULL(t);
    ASSERT_EQ(t->kind, TYPE_INT);
    
    arena_destroy(arena);
}

void test_check_mul_int(void) {
    Arena* arena = arena_create(4096);
    
    Type* t = check_expr(arena, "4 * 5");
    
    ASSERT_NOT_NULL(t);
    ASSERT_EQ(t->kind, TYPE_INT);
    
    arena_destroy(arena);
}

void test_check_div_int(void) {
    Arena* arena = arena_create(4096);
    
    Type* t = check_expr(arena, "10 / 2");
    
    ASSERT_NOT_NULL(t);
    ASSERT_EQ(t->kind, TYPE_INT);
    
    arena_destroy(arena);
}

void test_check_mod_int(void) {
    Arena* arena = arena_create(4096);
    
    Type* t = check_expr(arena, "10 % 3");
    
    ASSERT_NOT_NULL(t);
    ASSERT_EQ(t->kind, TYPE_INT);
    
    arena_destroy(arena);
}

void test_check_pow_int(void) {
    Arena* arena = arena_create(4096);
    
    Type* t = check_expr(arena, "2 ** 3");
    
    ASSERT_NOT_NULL(t);
    ASSERT_EQ(t->kind, TYPE_INT);
    
    arena_destroy(arena);
}

void test_check_comparison_int(void) {
    Arena* arena = arena_create(4096);
    
    Type* t = check_expr(arena, "1 < 2");
    
    ASSERT_NOT_NULL(t);
    ASSERT_EQ(t->kind, TYPE_BOOL);
    
    arena_destroy(arena);
}

void test_check_equality_int(void) {
    Arena* arena = arena_create(4096);
    
    Type* t = check_expr(arena, "1 == 2");
    
    ASSERT_NOT_NULL(t);
    ASSERT_EQ(t->kind, TYPE_BOOL);
    
    arena_destroy(arena);
}

void test_check_logical_and(void) {
    Arena* arena = arena_create(4096);
    
    Type* t = check_expr(arena, "true and false");
    
    ASSERT_NOT_NULL(t);
    ASSERT_EQ(t->kind, TYPE_BOOL);
    
    arena_destroy(arena);
}

void test_check_logical_or(void) {
    Arena* arena = arena_create(4096);
    
    Type* t = check_expr(arena, "true or false");
    
    ASSERT_NOT_NULL(t);
    ASSERT_EQ(t->kind, TYPE_BOOL);
    
    arena_destroy(arena);
}

void test_check_logical_and_error(void) {
    Arena* arena = arena_create(4096);
    
    const char* err = check_expr_error(arena, "1 and 2");
    
    ASSERT_NOT_NULL(err);
    // Should report that 'and' requires Bool operands
    
    arena_destroy(arena);
}

void test_check_string_concat(void) {
    Arena* arena = arena_create(4096);
    
    Type* t = check_expr(arena, "\"hello\" + \" world\"");
    
    ASSERT_NOT_NULL(t);
    ASSERT_EQ(t->kind, TYPE_STRING);
    
    arena_destroy(arena);
}

/* ========== Unary Operator Tests ========== */

void test_check_negate_int(void) {
    Arena* arena = arena_create(4096);
    
    Type* t = check_expr(arena, "-42");
    
    ASSERT_NOT_NULL(t);
    ASSERT_EQ(t->kind, TYPE_INT);
    
    arena_destroy(arena);
}

void test_check_negate_float(void) {
    Arena* arena = arena_create(4096);
    
    Type* t = check_expr(arena, "-3.14");
    
    ASSERT_NOT_NULL(t);
    ASSERT_EQ(t->kind, TYPE_FLOAT);
    
    arena_destroy(arena);
}

void test_check_not_bool(void) {
    Arena* arena = arena_create(4096);
    
    Type* t = check_expr(arena, "not true");
    
    ASSERT_NOT_NULL(t);
    ASSERT_EQ(t->kind, TYPE_BOOL);
    
    arena_destroy(arena);
}

void test_check_not_error(void) {
    Arena* arena = arena_create(4096);
    
    const char* err = check_expr_error(arena, "not 42");
    
    ASSERT_NOT_NULL(err);
    // Should report that 'not' requires Bool operand
    
    arena_destroy(arena);
}

/* ========== List Tests ========== */

void test_check_list_int(void) {
    Arena* arena = arena_create(4096);
    
    Type* t = check_expr(arena, "[1, 2, 3]");
    
    ASSERT_NOT_NULL(t);
    ASSERT_EQ(t->kind, TYPE_CON);
    ASSERT_STR_EQ(string_cstr(t->data.con.name), "List");
    ASSERT_EQ(t->data.con.args->len, 1);
    ASSERT_EQ(t->data.con.args->data[0]->kind, TYPE_INT);
    
    arena_destroy(arena);
}

void test_check_list_empty(void) {
    Arena* arena = arena_create(4096);
    
    Type* t = check_expr(arena, "[]");
    
    ASSERT_NOT_NULL(t);
    ASSERT_EQ(t->kind, TYPE_CON);
    ASSERT_STR_EQ(string_cstr(t->data.con.name), "List");
    // Empty list has type variable for element type
    ASSERT_EQ(t->data.con.args->len, 1);
    
    arena_destroy(arena);
}

void test_check_list_mixed_error(void) {
    Arena* arena = arena_create(4096);
    
    const char* err = check_expr_error(arena, "[1, \"hello\"]");
    
    ASSERT_NOT_NULL(err);
    // Should report that list elements must have same type
    
    arena_destroy(arena);
}

/* ========== Tuple Tests ========== */

void test_check_tuple(void) {
    Arena* arena = arena_create(4096);
    
    Type* t = check_expr(arena, "(1, \"hello\", true)");
    
    ASSERT_NOT_NULL(t);
    ASSERT_EQ(t->kind, TYPE_TUPLE);
    ASSERT_EQ(t->data.tuple.elements->len, 3);
    ASSERT_EQ(t->data.tuple.elements->data[0]->kind, TYPE_INT);
    ASSERT_EQ(t->data.tuple.elements->data[1]->kind, TYPE_STRING);
    ASSERT_EQ(t->data.tuple.elements->data[2]->kind, TYPE_BOOL);
    
    arena_destroy(arena);
}

/* ========== Variable Tests ========== */

void test_check_undefined_variable(void) {
    Arena* arena = arena_create(4096);
    
    const char* err = check_expr_error(arena, "undefined_var");
    
    ASSERT_NOT_NULL(err);
    // Should report undefined variable
    
    arena_destroy(arena);
}

/* ========== Function Call Tests ========== */

/* Helper to type check with a pre-defined function in the environment */
static Type* check_expr_with_fn(Arena* arena, const char* src,
                                 const char* fn_name, Type* fn_type) {
    Parser* parser = parser_new(arena, src);
    Expr* expr = parse_expr(parser);
    if (!expr || parser->had_error) return NULL;
    
    Checker* checker = checker_new(arena);
    checker_define(checker, string_new(arena, fn_name), fn_type);
    return checker_infer_expr(checker, expr);
}

void test_check_call_no_args(void) {
    Arena* arena = arena_create(4096);
    
    // Define: fn get_value() -> Int
    TypeVec* params = TypeVec_new(arena);
    Type* fn_type = type_fn(arena, params, type_int(arena));
    
    Type* t = check_expr_with_fn(arena, "get_value()", "get_value", fn_type);
    
    ASSERT_NOT_NULL(t);
    ASSERT_EQ(t->kind, TYPE_INT);
    
    arena_destroy(arena);
}

void test_check_call_with_args(void) {
    Arena* arena = arena_create(4096);
    
    // Define: fn add(Int, Int) -> Int
    TypeVec* params = TypeVec_new(arena);
    TypeVec_push(arena, params, type_int(arena));
    TypeVec_push(arena, params, type_int(arena));
    Type* fn_type = type_fn(arena, params, type_int(arena));
    
    Type* t = check_expr_with_fn(arena, "add(1, 2)", "add", fn_type);
    
    ASSERT_NOT_NULL(t);
    ASSERT_EQ(t->kind, TYPE_INT);
    
    arena_destroy(arena);
}

void test_check_call_wrong_arg_count(void) {
    Arena* arena = arena_create(4096);
    
    // Define: fn add(Int, Int) -> Int
    TypeVec* params = TypeVec_new(arena);
    TypeVec_push(arena, params, type_int(arena));
    TypeVec_push(arena, params, type_int(arena));
    Type* fn_type = type_fn(arena, params, type_int(arena));
    
    Parser* parser = parser_new(arena, "add(1)");
    Expr* expr = parse_expr(parser);
    Checker* checker = checker_new(arena);
    checker_define(checker, string_new(arena, "add"), fn_type);
    Type* t = checker_infer_expr(checker, expr);
    
    ASSERT_NOT_NULL(t);
    ASSERT_EQ(t->kind, TYPE_ERROR);
    
    arena_destroy(arena);
}

void test_check_call_wrong_arg_type(void) {
    Arena* arena = arena_create(4096);
    
    // Define: fn greet(String) -> String
    TypeVec* params = TypeVec_new(arena);
    TypeVec_push(arena, params, type_string(arena));
    Type* fn_type = type_fn(arena, params, type_string(arena));
    
    Parser* parser = parser_new(arena, "greet(42)");
    Expr* expr = parse_expr(parser);
    Checker* checker = checker_new(arena);
    checker_define(checker, string_new(arena, "greet"), fn_type);
    Type* t = checker_infer_expr(checker, expr);
    
    ASSERT_NOT_NULL(t);
    ASSERT_EQ(t->kind, TYPE_ERROR);
    
    arena_destroy(arena);
}

void test_check_call_not_a_function(void) {
    Arena* arena = arena_create(4096);
    
    Parser* parser = parser_new(arena, "x(1)");
    Expr* expr = parse_expr(parser);
    Checker* checker = checker_new(arena);
    checker_define(checker, string_new(arena, "x"), type_int(arena));
    Type* t = checker_infer_expr(checker, expr);
    
    ASSERT_NOT_NULL(t);
    ASSERT_EQ(t->kind, TYPE_ERROR);
    
    arena_destroy(arena);
}

/* ========== If Expression Tests ========== */

void test_check_if_simple(void) {
    Arena* arena = arena_create(4096);
    
    Type* t = check_expr(arena, "if true: 1 else: 2");
    
    ASSERT_NOT_NULL(t);
    ASSERT_EQ(t->kind, TYPE_INT);
    
    arena_destroy(arena);
}

void test_check_if_branch_mismatch(void) {
    Arena* arena = arena_create(4096);
    
    const char* err = check_expr_error(arena, "if true: 1 else: \"hello\"");
    
    ASSERT_NOT_NULL(err);
    // Should report that branches have different types
    
    arena_destroy(arena);
}

void test_check_if_non_bool_condition(void) {
    Arena* arena = arena_create(4096);
    
    const char* err = check_expr_error(arena, "if 42: 1 else: 2");
    
    ASSERT_NOT_NULL(err);
    // Should report that condition must be Bool
    
    arena_destroy(arena);
}

void test_check_if_no_else(void) {
    Arena* arena = arena_create(4096);
    
    // if without else returns Unit
    Type* t = check_expr(arena, "if true: 42");
    
    ASSERT_NOT_NULL(t);
    ASSERT_EQ(t->kind, TYPE_UNIT);
    
    arena_destroy(arena);
}

/* ========== Block Expression Tests ========== */

void test_check_block_returns_final(void) {
    Arena* arena = arena_create(4096);
    
    Type* t = check_expr(arena, "{ 42 }");
    
    ASSERT_NOT_NULL(t);
    ASSERT_EQ(t->kind, TYPE_INT);
    
    arena_destroy(arena);
}

/* ========== Let Statement Tests ========== */

void test_check_let_infers_type(void) {
    Arena* arena = arena_create(4096);
    
    // let x = 42, then use x
    Type* t = check_expr(arena, "{ let x = 42, x }");
    
    ASSERT_NOT_NULL(t);
    ASSERT_EQ(t->kind, TYPE_INT);
    
    arena_destroy(arena);
}

void test_check_let_with_type_annotation(void) {
    Arena* arena = arena_create(4096);
    
    // let x: Int = 42
    Type* t = check_expr(arena, "{ let x: Int = 42, x }");
    
    ASSERT_NOT_NULL(t);
    ASSERT_EQ(t->kind, TYPE_INT);
    
    arena_destroy(arena);
}

void test_check_let_type_mismatch(void) {
    Arena* arena = arena_create(4096);
    
    // let x: String = 42 should fail
    const char* err = check_expr_error(arena, "{ let x: String = 42, x }");
    
    ASSERT_NOT_NULL(err);
    // Should report type mismatch
    
    arena_destroy(arena);
}

void test_check_let_multiple(void) {
    Arena* arena = arena_create(4096);
    
    // Multiple let bindings
    Type* t = check_expr(arena, "{ let a = 1, let b = 2, a + b }");
    
    ASSERT_NOT_NULL(t);
    ASSERT_EQ(t->kind, TYPE_INT);
    
    arena_destroy(arena);
}

void test_check_let_shadowing(void) {
    Arena* arena = arena_create(4096);
    
    // let x = 1, let x = "hello", x should be String
    Type* t = check_expr(arena, "{ let x = 1, let x = \"hello\", x }");
    
    ASSERT_NOT_NULL(t);
    ASSERT_EQ(t->kind, TYPE_STRING);
    
    arena_destroy(arena);
}

/* ========== Match Expression Tests ========== */

void test_check_match_simple(void) {
    Arena* arena = arena_create(4096);
    
    // match 1: 1 -> "one", _ -> "other"
    Type* t = check_expr(arena, "match 1: 1 -> \"one\", _ -> \"other\"");
    
    ASSERT_NOT_NULL(t);
    ASSERT_EQ(t->kind, TYPE_STRING);
    
    arena_destroy(arena);
}

void test_check_match_branch_types_must_match(void) {
    Arena* arena = arena_create(4096);
    
    // Different branch types should error
    const char* err = check_expr_error(arena, "match 1: 1 -> \"one\", _ -> 42");
    
    ASSERT_NOT_NULL(err);
    // Should report that branches have different types
    
    arena_destroy(arena);
}

void test_check_match_binds_pattern_var(void) {
    Arena* arena = arena_create(4096);
    
    // Pattern variable should be bound in body
    // match 42: x -> x + 1
    Type* t = check_expr(arena, "match 42: x -> x + 1");
    
    ASSERT_NOT_NULL(t);
    ASSERT_EQ(t->kind, TYPE_INT);
    
    arena_destroy(arena);
}

/* ========== Test Runner ========== */

void run_checker_tests(void) {
    printf("\n--- Type Checker Tests ---\n");
    
    // Literals
    TEST_RUN(test_check_int_literal);
    TEST_RUN(test_check_float_literal);
    TEST_RUN(test_check_string_literal);
    TEST_RUN(test_check_bool_true);
    TEST_RUN(test_check_bool_false);
    
    // Binary operators
    TEST_RUN(test_check_add_int);
    TEST_RUN(test_check_add_float);
    TEST_RUN(test_check_add_mixed_error);
    TEST_RUN(test_check_sub_int);
    TEST_RUN(test_check_mul_int);
    TEST_RUN(test_check_div_int);
    TEST_RUN(test_check_mod_int);
    TEST_RUN(test_check_pow_int);
    TEST_RUN(test_check_comparison_int);
    TEST_RUN(test_check_equality_int);
    TEST_RUN(test_check_logical_and);
    TEST_RUN(test_check_logical_or);
    TEST_RUN(test_check_logical_and_error);
    TEST_RUN(test_check_string_concat);
    
    // Unary operators
    TEST_RUN(test_check_negate_int);
    TEST_RUN(test_check_negate_float);
    TEST_RUN(test_check_not_bool);
    TEST_RUN(test_check_not_error);
    
    // Lists
    TEST_RUN(test_check_list_int);
    TEST_RUN(test_check_list_empty);
    TEST_RUN(test_check_list_mixed_error);
    
    // Tuples
    TEST_RUN(test_check_tuple);
    
    // Variables
    TEST_RUN(test_check_undefined_variable);
    
    // Function calls
    TEST_RUN(test_check_call_no_args);
    TEST_RUN(test_check_call_with_args);
    TEST_RUN(test_check_call_wrong_arg_count);
    TEST_RUN(test_check_call_wrong_arg_type);
    TEST_RUN(test_check_call_not_a_function);
    
    // If expressions
    TEST_RUN(test_check_if_simple);
    TEST_RUN(test_check_if_branch_mismatch);
    TEST_RUN(test_check_if_non_bool_condition);
    TEST_RUN(test_check_if_no_else);
    
    // Block expressions
    TEST_RUN(test_check_block_returns_final);
    
    // Let statements
    TEST_RUN(test_check_let_infers_type);
    TEST_RUN(test_check_let_with_type_annotation);
    TEST_RUN(test_check_let_type_mismatch);
    TEST_RUN(test_check_let_multiple);
    TEST_RUN(test_check_let_shadowing);
    
    // Match expressions
    TEST_RUN(test_check_match_simple);
    TEST_RUN(test_check_match_branch_types_must_match);
    TEST_RUN(test_check_match_binds_pattern_var);
}
