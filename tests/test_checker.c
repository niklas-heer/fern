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

/* ========== Try Operator Tests ========== */

void test_check_try_unwraps_result(void) {
    Arena* arena = arena_create(4096);
    
    // Define: fn get_value() -> Result(Int, String)
    TypeVec* params = TypeVec_new(arena);
    Type* result_type = type_result(arena, type_int(arena), type_string(arena));
    Type* fn_type = type_fn(arena, params, result_type);
    
    // get_value()? should have type Int (unwrapped from Result)
    Parser* parser = parser_new(arena, "get_value()?");
    Expr* expr = parse_expr(parser);
    Checker* checker = checker_new(arena);
    checker_define(checker, string_new(arena, "get_value"), fn_type);
    Type* t = checker_infer_expr(checker, expr);
    
    ASSERT_NOT_NULL(t);
    ASSERT_EQ(t->kind, TYPE_INT);
    
    arena_destroy(arena);
}

void test_check_try_requires_result(void) {
    Arena* arena = arena_create(4096);
    
    // 42? should error - can't use ? on non-Result
    const char* err = check_expr_error(arena, "42?");
    
    ASSERT_NOT_NULL(err);
    // Should report that ? requires Result type
    
    arena_destroy(arena);
}

/* ========== Generic Type Instantiation Tests ========== */

void test_check_generic_identity(void) {
    Arena* arena = arena_create(4096);
    
    // Define: fn identity(a) -> a  (generic function)
    // When called with Int, should return Int
    int var_id = type_fresh_var_id();
    Type* type_a = type_var(arena, string_new(arena, "a"), var_id);
    
    TypeVec* params = TypeVec_new(arena);
    TypeVec_push(arena, params, type_a);
    Type* fn_type = type_fn(arena, params, type_a);
    
    // identity(42) should have type Int
    Parser* parser = parser_new(arena, "identity(42)");
    Expr* expr = parse_expr(parser);
    Checker* checker = checker_new(arena);
    checker_define(checker, string_new(arena, "identity"), fn_type);
    Type* t = checker_infer_expr(checker, expr);
    
    ASSERT_NOT_NULL(t);
    ASSERT_EQ(t->kind, TYPE_INT);
    
    arena_destroy(arena);
}

void test_check_generic_list_head(void) {
    Arena* arena = arena_create(4096);
    
    // Define: fn head(List(a)) -> Option(a)
    int var_id = type_fresh_var_id();
    Type* type_a = type_var(arena, string_new(arena, "a"), var_id);
    
    TypeVec* params = TypeVec_new(arena);
    TypeVec_push(arena, params, type_list(arena, type_a));
    Type* fn_type = type_fn(arena, params, type_option(arena, type_a));
    
    // head([1, 2, 3]) should have type Option(Int)
    Parser* parser = parser_new(arena, "head([1, 2, 3])");
    Expr* expr = parse_expr(parser);
    Checker* checker = checker_new(arena);
    checker_define(checker, string_new(arena, "head"), fn_type);
    Type* t = checker_infer_expr(checker, expr);
    
    ASSERT_NOT_NULL(t);
    ASSERT_EQ(t->kind, TYPE_CON);
    ASSERT_STR_EQ(string_cstr(t->data.con.name), "Option");
    ASSERT_EQ(t->data.con.args->data[0]->kind, TYPE_INT);
    
    arena_destroy(arena);
}

/* ========== Bind Expression Tests ========== */

void test_check_bind_unwraps_result(void) {
    Arena* arena = arena_create(4096);
    
    // Define: fn get_value() -> Result(Int, String)
    TypeVec* params = TypeVec_new(arena);
    Type* result_type = type_result(arena, type_int(arena), type_string(arena));
    Type* fn_type = type_fn(arena, params, result_type);
    
    // x <- get_value() should bind x to Int (the Ok type)
    // We test this by using the bind inside a block where we use x
    Parser* parser = parser_new(arena, "{ x <- get_value(), x + 1 }");
    Expr* expr = parse_expr(parser);
    Checker* checker = checker_new(arena);
    checker_define(checker, string_new(arena, "get_value"), fn_type);
    Type* t = checker_infer_expr(checker, expr);
    
    ASSERT_NOT_NULL(t);
    ASSERT_EQ(t->kind, TYPE_INT);
    
    arena_destroy(arena);
}

void test_check_bind_requires_result(void) {
    Arena* arena = arena_create(4096);
    
    // x <- 42 should error - can't bind from non-Result
    const char* err = check_expr_error(arena, "{ x <- 42, x }");
    
    ASSERT_NOT_NULL(err);
    // Should report that <- requires Result type
    
    arena_destroy(arena);
}

void test_check_bind_propagates_error_type(void) {
    Arena* arena = arena_create(4096);
    
    // Define: fn read_file() -> Result(String, Error)
    TypeVec* params = TypeVec_new(arena);
    Type* error_type_t = type_con(arena, string_new(arena, "Error"), NULL);
    Type* result_type = type_result(arena, type_string(arena), error_type_t);
    Type* fn_type = type_fn(arena, params, result_type);
    
    // content <- read_file() should bind content to String
    Parser* parser = parser_new(arena, "{ content <- read_file(), content }");
    Expr* expr = parse_expr(parser);
    Checker* checker = checker_new(arena);
    checker_define(checker, string_new(arena, "read_file"), fn_type);
    Type* t = checker_infer_expr(checker, expr);
    
    ASSERT_NOT_NULL(t);
    ASSERT_EQ(t->kind, TYPE_STRING);
    
    arena_destroy(arena);
}

/* ========== With Expression Tests ========== */

void test_check_with_simple(void) {
    Arena* arena = arena_create(4096);
    
    // Define: fn get_value() -> Result(Int, String)
    TypeVec* params = TypeVec_new(arena);
    Type* result_type = type_result(arena, type_int(arena), type_string(arena));
    Type* fn_type = type_fn(arena, params, result_type);
    
    // with x <- get_value() do x + 1
    Parser* parser = parser_new(arena, "with x <- get_value() do x + 1");
    Expr* expr = parse_expr(parser);
    Checker* checker = checker_new(arena);
    checker_define(checker, string_new(arena, "get_value"), fn_type);
    Type* t = checker_infer_expr(checker, expr);
    
    ASSERT_NOT_NULL(t);
    ASSERT_EQ(t->kind, TYPE_INT);
    
    arena_destroy(arena);
}

void test_check_with_multiple_bindings(void) {
    Arena* arena = arena_create(4096);
    
    // Define: fn f() -> Result(Int, String)
    TypeVec* params_f = TypeVec_new(arena);
    Type* result_int = type_result(arena, type_int(arena), type_string(arena));
    Type* fn_f = type_fn(arena, params_f, result_int);
    
    // Define: fn g(Int) -> Result(Int, String)
    TypeVec* params_g = TypeVec_new(arena);
    TypeVec_push(arena, params_g, type_int(arena));
    Type* fn_g = type_fn(arena, params_g, result_int);
    
    // with x <- f(), y <- g(x) do x + y
    Parser* parser = parser_new(arena, "with x <- f(), y <- g(x) do x + y");
    Expr* expr = parse_expr(parser);
    Checker* checker = checker_new(arena);
    checker_define(checker, string_new(arena, "f"), fn_f);
    checker_define(checker, string_new(arena, "g"), fn_g);
    Type* t = checker_infer_expr(checker, expr);
    
    ASSERT_NOT_NULL(t);
    ASSERT_EQ(t->kind, TYPE_INT);
    
    arena_destroy(arena);
}

void test_check_with_requires_result(void) {
    Arena* arena = arena_create(4096);
    
    // with x <- 42 do x should error - can't bind from non-Result
    const char* err = check_expr_error(arena, "with x <- 42 do x");
    
    ASSERT_NOT_NULL(err);
    // Should report that with binding requires Result type
    
    arena_destroy(arena);
}

/* ========== Lambda Expression Tests ========== */

void test_check_lambda_simple(void) {
    Arena* arena = arena_create(4096);
    
    // (x) -> x + 1 applied to 5 should return Int
    // We need to call it since we can't know param types without application
    // For now, test that lambda creates a function type
    
    Parser* parser = parser_new(arena, "((x) -> x)");
    Expr* expr = parse_expr(parser);
    Checker* checker = checker_new(arena);
    Type* t = checker_infer_expr(checker, expr);
    
    ASSERT_NOT_NULL(t);
    // Lambda should return a function type
    ASSERT_EQ(t->kind, TYPE_FN);
    
    arena_destroy(arena);
}

void test_check_lambda_applied(void) {
    Arena* arena = arena_create(4096);
    
    // Define a map function: fn map(List(a), (a) -> b) -> List(b)
    int var_a = type_fresh_var_id();
    int var_b = type_fresh_var_id();
    Type* type_a = type_var(arena, string_new(arena, "a"), var_a);
    Type* type_b = type_var(arena, string_new(arena, "b"), var_b);
    
    TypeVec* lambda_params = TypeVec_new(arena);
    TypeVec_push(arena, lambda_params, type_a);
    Type* lambda_type = type_fn(arena, lambda_params, type_b);
    
    TypeVec* map_params = TypeVec_new(arena);
    TypeVec_push(arena, map_params, type_list(arena, type_a));
    TypeVec_push(arena, map_params, lambda_type);
    Type* map_fn = type_fn(arena, map_params, type_list(arena, type_b));
    
    // map([1, 2, 3], (x) -> x + 1) should return List(Int)
    Parser* parser = parser_new(arena, "map([1, 2, 3], (x) -> x + 1)");
    Expr* expr = parse_expr(parser);
    Checker* checker = checker_new(arena);
    checker_define(checker, string_new(arena, "map"), map_fn);
    Type* t = checker_infer_expr(checker, expr);
    
    ASSERT_NOT_NULL(t);
    ASSERT_EQ(t->kind, TYPE_CON);
    ASSERT_STR_EQ(string_cstr(t->data.con.name), "List");
    
    arena_destroy(arena);
}

/* ========== For Loop Tests ========== */

void test_check_for_loop_basic(void) {
    Arena* arena = arena_create(4096);
    
    // for x in [1, 2, 3]: x + 1
    // For loop returns Unit (it's a statement-like expression)
    Type* t = check_expr(arena, "for x in [1, 2, 3]: x + 1");
    
    ASSERT_NOT_NULL(t);
    // For loop has type Unit (side-effect only)
    ASSERT_EQ(t->kind, TYPE_UNIT);
    
    arena_destroy(arena);
}

void test_check_for_binds_loop_var(void) {
    Arena* arena = arena_create(4096);
    
    // Define a print function to use the loop variable
    TypeVec* params = TypeVec_new(arena);
    TypeVec_push(arena, params, type_int(arena));
    Type* fn_type = type_fn(arena, params, type_unit(arena));
    
    // for x in [1, 2]: print(x)
    Parser* parser = parser_new(arena, "for x in [1, 2]: print(x)");
    Expr* expr = parse_expr(parser);
    Checker* checker = checker_new(arena);
    checker_define(checker, string_new(arena, "print"), fn_type);
    Type* t = checker_infer_expr(checker, expr);
    
    ASSERT_NOT_NULL(t);
    ASSERT_EQ(t->kind, TYPE_UNIT);
    
    arena_destroy(arena);
}

void test_check_for_requires_iterable(void) {
    Arena* arena = arena_create(4096);
    
    // for x in 42: x  -- can't iterate over Int
    const char* err = check_expr_error(arena, "for x in 42: x");
    
    ASSERT_NOT_NULL(err);
    // Should report that Int is not iterable
    
    arena_destroy(arena);
}

/* ========== Index Expression Tests ========== */

void test_check_index_list(void) {
    Arena* arena = arena_create(4096);
    
    // Define a list variable
    Parser* parser = parser_new(arena, "{ let items = [1, 2, 3], items[0] }");
    Expr* expr = parse_expr(parser);
    Checker* checker = checker_new(arena);
    Type* t = checker_infer_expr(checker, expr);
    
    ASSERT_NOT_NULL(t);
    ASSERT_EQ(t->kind, TYPE_INT);
    
    arena_destroy(arena);
}

void test_check_index_requires_int(void) {
    Arena* arena = arena_create(4096);
    
    // items["key"] should error for a list (needs Int index)
    Parser* parser = parser_new(arena, "{ let items = [1, 2, 3], items[\"key\"] }");
    Expr* expr = parse_expr(parser);
    Checker* checker = checker_new(arena);
    Type* t = checker_infer_expr(checker, expr);
    
    ASSERT_NOT_NULL(t);
    ASSERT_EQ(t->kind, TYPE_ERROR);
    
    arena_destroy(arena);
}

void test_check_index_non_indexable(void) {
    Arena* arena = arena_create(4096);
    
    // 42[0] should error - Int is not indexable
    const char* err = check_expr_error(arena, "42[0]");
    
    ASSERT_NOT_NULL(err);
    // Should report that Int is not indexable
    
    arena_destroy(arena);
}

/* ========== Pipe Operator Tests ========== */

void test_check_pipe_basic(void) {
    Arena* arena = arena_create(4096);
    
    // Define: fn double(Int) -> Int
    TypeVec* params = TypeVec_new(arena);
    TypeVec_push(arena, params, type_int(arena));
    Type* fn_type = type_fn(arena, params, type_int(arena));
    
    // 5 |> double() should have type Int
    Parser* parser = parser_new(arena, "5 |> double()");
    Expr* expr = parse_expr(parser);
    Checker* checker = checker_new(arena);
    checker_define(checker, string_new(arena, "double"), fn_type);
    Type* t = checker_infer_expr(checker, expr);
    
    ASSERT_NOT_NULL(t);
    ASSERT_EQ(t->kind, TYPE_INT);
    
    arena_destroy(arena);
}

void test_check_pipe_chain(void) {
    Arena* arena = arena_create(4096);
    
    // Define: fn double(Int) -> Int
    TypeVec* params = TypeVec_new(arena);
    TypeVec_push(arena, params, type_int(arena));
    Type* fn_type = type_fn(arena, params, type_int(arena));
    
    // 5 |> double() |> double() should have type Int
    Parser* parser = parser_new(arena, "5 |> double() |> double()");
    Expr* expr = parse_expr(parser);
    Checker* checker = checker_new(arena);
    checker_define(checker, string_new(arena, "double"), fn_type);
    Type* t = checker_infer_expr(checker, expr);
    
    ASSERT_NOT_NULL(t);
    ASSERT_EQ(t->kind, TYPE_INT);
    
    arena_destroy(arena);
}

void test_check_pipe_type_mismatch(void) {
    Arena* arena = arena_create(4096);
    
    // Define: fn greet(String) -> String
    TypeVec* params = TypeVec_new(arena);
    TypeVec_push(arena, params, type_string(arena));
    Type* fn_type = type_fn(arena, params, type_string(arena));
    
    // 42 |> greet() should error - Int is not String
    Parser* parser = parser_new(arena, "42 |> greet()");
    Expr* expr = parse_expr(parser);
    Checker* checker = checker_new(arena);
    checker_define(checker, string_new(arena, "greet"), fn_type);
    Type* t = checker_infer_expr(checker, expr);
    
    ASSERT_NOT_NULL(t);
    ASSERT_EQ(t->kind, TYPE_ERROR);
    
    arena_destroy(arena);
}

/* ========== Constructor Pattern Binding Tests ========== */

void test_check_match_option_some(void) {
    Arena* arena = arena_create(4096);
    
    // Define: fn get_value() -> Option(Int)
    TypeVec* params = TypeVec_new(arena);
    Type* option_int = type_option(arena, type_int(arena));
    Type* fn_type = type_fn(arena, params, option_int);
    
    // match get_value(): Some(x) -> x + 1, None -> 0
    // The x in Some(x) should be bound to Int
    Parser* parser = parser_new(arena, "match get_value(): Some(x) -> x + 1, None -> 0");
    Expr* expr = parse_expr(parser);
    Checker* checker = checker_new(arena);
    checker_define(checker, string_new(arena, "get_value"), fn_type);
    Type* t = checker_infer_expr(checker, expr);
    
    ASSERT_NOT_NULL(t);
    ASSERT_EQ(t->kind, TYPE_INT);
    
    arena_destroy(arena);
}

void test_check_match_option_none(void) {
    Arena* arena = arena_create(4096);
    
    // Define: fn get_value() -> Option(String)
    TypeVec* params = TypeVec_new(arena);
    Type* option_str = type_option(arena, type_string(arena));
    Type* fn_type = type_fn(arena, params, option_str);
    
    // match get_value(): Some(s) -> s, None -> "default"
    // Both branches return String
    Parser* parser = parser_new(arena, "match get_value(): Some(s) -> s, None -> \"default\"");
    Expr* expr = parse_expr(parser);
    Checker* checker = checker_new(arena);
    checker_define(checker, string_new(arena, "get_value"), fn_type);
    Type* t = checker_infer_expr(checker, expr);
    
    ASSERT_NOT_NULL(t);
    ASSERT_EQ(t->kind, TYPE_STRING);
    
    arena_destroy(arena);
}

void test_check_match_result_ok_err(void) {
    Arena* arena = arena_create(4096);
    
    // Define: fn get_data() -> Result(Int, String)
    TypeVec* params = TypeVec_new(arena);
    Type* result_type = type_result(arena, type_int(arena), type_string(arena));
    Type* fn_type = type_fn(arena, params, result_type);
    
    // match get_data(): Ok(n) -> n * 2, Err(msg) -> 0
    // n should be Int, msg should be String, result is Int
    Parser* parser = parser_new(arena, "match get_data(): Ok(n) -> n * 2, Err(msg) -> 0");
    Expr* expr = parse_expr(parser);
    Checker* checker = checker_new(arena);
    checker_define(checker, string_new(arena, "get_data"), fn_type);
    Type* t = checker_infer_expr(checker, expr);
    
    ASSERT_NOT_NULL(t);
    ASSERT_EQ(t->kind, TYPE_INT);
    
    arena_destroy(arena);
}

/* ========== Range Expression Tests ========== */

void test_check_range_int(void) {
    Arena* arena = arena_create(4096);
    
    // 0..10 should have type Range(Int)
    Type* t = check_expr(arena, "0..10");
    
    ASSERT_NOT_NULL(t);
    ASSERT_EQ(t->kind, TYPE_CON);
    ASSERT_STR_EQ(string_cstr(t->data.con.name), "Range");
    ASSERT_EQ(t->data.con.args->data[0]->kind, TYPE_INT);
    
    arena_destroy(arena);
}

void test_check_range_inclusive(void) {
    Arena* arena = arena_create(4096);
    
    // 1..=100 should have type Range(Int)
    Type* t = check_expr(arena, "1..=100");
    
    ASSERT_NOT_NULL(t);
    ASSERT_EQ(t->kind, TYPE_CON);
    ASSERT_STR_EQ(string_cstr(t->data.con.name), "Range");
    
    arena_destroy(arena);
}

void test_check_range_requires_same_type(void) {
    Arena* arena = arena_create(4096);
    
    // 0.."ten" should error - bounds must be same type
    const char* err = check_expr_error(arena, "0..\"ten\"");
    
    ASSERT_NOT_NULL(err);
    // Should report type mismatch
    
    arena_destroy(arena);
}

/* ========== Map Literal Tests ========== */

void test_check_map_string_int(void) {
    Arena* arena = arena_create(4096);
    
    // %{ "a": 1, "b": 2 } should have type Map(String, Int)
    Type* t = check_expr(arena, "%{ \"a\": 1, \"b\": 2 }");
    
    ASSERT_NOT_NULL(t);
    ASSERT_EQ(t->kind, TYPE_CON);
    ASSERT_STR_EQ(string_cstr(t->data.con.name), "Map");
    ASSERT_EQ(t->data.con.args->len, 2);
    ASSERT_EQ(t->data.con.args->data[0]->kind, TYPE_STRING);
    ASSERT_EQ(t->data.con.args->data[1]->kind, TYPE_INT);
    
    arena_destroy(arena);
}

void test_check_map_empty(void) {
    Arena* arena = arena_create(4096);
    
    // %{} should have type Map(a, b) with type variables
    Type* t = check_expr(arena, "%{}");
    
    ASSERT_NOT_NULL(t);
    ASSERT_EQ(t->kind, TYPE_CON);
    ASSERT_STR_EQ(string_cstr(t->data.con.name), "Map");
    
    arena_destroy(arena);
}

void test_check_map_mixed_keys_error(void) {
    Arena* arena = arena_create(4096);
    
    // %{ "a": 1, 2: 3 } should error - keys must have same type
    const char* err = check_expr_error(arena, "%{ \"a\": 1, 2: 3 }");
    
    ASSERT_NOT_NULL(err);
    // Should report key type mismatch
    
    arena_destroy(arena);
}

/* ========== Tuple Field Access Tests ========== */

void test_check_tuple_field_access(void) {
    Arena* arena = arena_create(4096);
    
    // (1, "hello", true).0 should be Int
    Type* t = check_expr(arena, "(1, \"hello\", true).0");
    
    ASSERT_NOT_NULL(t);
    ASSERT_EQ(t->kind, TYPE_INT);
    
    arena_destroy(arena);
}

void test_check_tuple_field_access_second(void) {
    Arena* arena = arena_create(4096);
    
    // (1, "hello", true).1 should be String
    Type* t = check_expr(arena, "(1, \"hello\", true).1");
    
    ASSERT_NOT_NULL(t);
    ASSERT_EQ(t->kind, TYPE_STRING);
    
    arena_destroy(arena);
}

void test_check_tuple_field_out_of_bounds(void) {
    Arena* arena = arena_create(4096);
    
    // (1, 2).5 should error - index out of bounds
    const char* err = check_expr_error(arena, "(1, 2).5");
    
    ASSERT_NOT_NULL(err);
    // Should report index out of bounds
    
    arena_destroy(arena);
}

/* ========== Function Definition Tests ========== */

/* Helper to parse and type check a statement */
static bool check_stmt_ok(Arena* arena, const char* src) {
    Parser* parser = parser_new(arena, src);
    StmtVec* stmts = parse_stmts(parser);
    if (!stmts || parser->had_error) return false;
    
    Checker* checker = checker_new(arena);
    return checker_check_stmts(checker, stmts);
}

static const char* check_stmt_error(Arena* arena, const char* src) {
    Parser* parser = parser_new(arena, src);
    StmtVec* stmts = parse_stmts(parser);
    if (!stmts || parser->had_error) return "parse error";
    
    Checker* checker = checker_new(arena);
    checker_check_stmts(checker, stmts);
    if (checker_has_errors(checker)) {
        return checker_first_error(checker);
    }
    return NULL;
}

void test_check_fn_simple(void) {
    Arena* arena = arena_create(4096);
    
    // fn add(x: Int, y: Int) -> Int: x + y
    bool ok = check_stmt_ok(arena, "fn add(x: Int, y: Int) -> Int: x + y");
    
    ASSERT_TRUE(ok);
    
    arena_destroy(arena);
}

void test_check_fn_wrong_return_type(void) {
    Arena* arena = arena_create(4096);
    
    // fn get_int() -> Int: "hello" should error - String is not Int
    const char* err = check_stmt_error(arena, "fn get_int() -> Int: \"hello\"");
    
    ASSERT_NOT_NULL(err);
    // Should report type mismatch
    
    arena_destroy(arena);
}

void test_check_fn_uses_params(void) {
    Arena* arena = arena_create(4096);
    
    // Parameters should be in scope for body
    bool ok = check_stmt_ok(arena, "fn greet(name: String) -> String: name");
    
    ASSERT_TRUE(ok);
    
    arena_destroy(arena);
}

void test_check_fn_no_return_type(void) {
    Arena* arena = arena_create(4096);
    
    // Function without return type annotation
    bool ok = check_stmt_ok(arena, "fn say_hi(): 42");
    
    ASSERT_TRUE(ok);
    
    arena_destroy(arena);
}

void test_check_fn_param_type_mismatch(void) {
    Arena* arena = arena_create(4096);
    
    // fn add(x: Int, y: Int) -> Int: x + "hello" should error
    const char* err = check_stmt_error(arena, "fn bad(x: Int) -> Int: x + \"hello\"");
    
    ASSERT_NOT_NULL(err);
    // Should report cannot add Int and String
    
    arena_destroy(arena);
}

/* ========== Type Definition Tests ========== */

void test_check_type_def_simple(void) {
    Arena* arena = arena_create(4096);
    
    // type Status:\n    Active\n    Inactive
    bool ok = check_stmt_ok(arena, "type Status:\n    Active\n    Inactive");
    
    ASSERT_TRUE(ok);
    
    arena_destroy(arena);
}

void test_check_type_def_with_fields(void) {
    Arena* arena = arena_create(4096);
    
    // type Shape:\n    Circle(radius: Float)\n    Rect(w: Int, h: Int)
    bool ok = check_stmt_ok(arena, "type Shape:\n    Circle(radius: Float)\n    Rect(w: Int, h: Int)");
    
    ASSERT_TRUE(ok);
    
    arena_destroy(arena);
}

void test_check_type_def_unknown_field_type(void) {
    Arena* arena = arena_create(4096);
    
    // type Bad:\n    Variant(x: Unknown) should error - Unknown is not a type
    const char* err = check_stmt_error(arena, "type Bad:\n    Variant(x: Unknown)");
    
    ASSERT_NOT_NULL(err);
    // Should report Unknown is not a known type
    
    arena_destroy(arena);
}

void test_check_type_def_record(void) {
    Arena* arena = arena_create(4096);
    
    // type User:\n    name: String\n    age: Int
    bool ok = check_stmt_ok(arena, "type User:\n    name: String\n    age: Int");
    
    ASSERT_TRUE(ok);
    
    arena_destroy(arena);
}

void test_check_type_def_record_unknown_field_type(void) {
    Arena* arena = arena_create(4096);
    
    // type BadRecord:\n    field: Unknown should error
    const char* err = check_stmt_error(arena, "type BadRecord:\n    field: Unknown");
    
    ASSERT_NOT_NULL(err);
    // Should report Unknown is not a known type
    
    arena_destroy(arena);
}

/* ========== List Comprehension Tests ========== */

void test_check_list_comp_basic(void) {
    Arena* arena = arena_create(4096);
    
    // [x * 2 for x in nums] where nums: List(Int) -> List(Int)
    Checker* checker = checker_new(arena);
    checker_define(checker, string_new(arena, "nums"), 
        type_list(arena, type_int(arena)));
    
    Parser* parser = parser_new(arena, "[x * 2 for x in nums]");
    Expr* expr = parse_expr(parser);
    ASSERT_NOT_NULL(expr);
    
    Type* t = checker_infer_expr(checker, expr);
    ASSERT_NOT_NULL(t);
    ASSERT_EQ(t->kind, TYPE_CON);
    ASSERT_STR_EQ(string_cstr(t->data.con.name), "List");
    ASSERT_EQ(t->data.con.args->data[0]->kind, TYPE_INT);
    
    arena_destroy(arena);
}

void test_check_list_comp_with_filter(void) {
    Arena* arena = arena_create(4096);
    
    // [x for x in nums if x > 0] where nums: List(Int) -> List(Int)
    Checker* checker = checker_new(arena);
    checker_define(checker, string_new(arena, "nums"), 
        type_list(arena, type_int(arena)));
    
    Parser* parser = parser_new(arena, "[x for x in nums if x > 0]");
    Expr* expr = parse_expr(parser);
    ASSERT_NOT_NULL(expr);
    
    Type* t = checker_infer_expr(checker, expr);
    ASSERT_NOT_NULL(t);
    ASSERT_EQ(t->kind, TYPE_CON);
    ASSERT_STR_EQ(string_cstr(t->data.con.name), "List");
    
    arena_destroy(arena);
}

void test_check_list_comp_non_bool_filter(void) {
    Arena* arena = arena_create(4096);
    
    // [x for x in nums if x] where filter expr is Int, not Bool
    Checker* checker = checker_new(arena);
    checker_define(checker, string_new(arena, "nums"), 
        type_list(arena, type_int(arena)));
    
    Parser* parser = parser_new(arena, "[x for x in nums if x]");
    Expr* expr = parse_expr(parser);
    ASSERT_NOT_NULL(expr);
    
    Type* t = checker_infer_expr(checker, expr);
    // Should be an error because the filter must be Bool
    ASSERT_NOT_NULL(t);
    ASSERT_EQ(t->kind, TYPE_ERROR);
    
    arena_destroy(arena);
}

void test_check_list_comp_requires_iterable(void) {
    Arena* arena = arena_create(4096);
    
    // [x for x in num] where num is Int, not a list
    Checker* checker = checker_new(arena);
    checker_define(checker, string_new(arena, "num"), type_int(arena));
    
    Parser* parser = parser_new(arena, "[x for x in num]");
    Expr* expr = parse_expr(parser);
    ASSERT_NOT_NULL(expr);
    
    Type* t = checker_infer_expr(checker, expr);
    // Should be an error because Int is not iterable
    ASSERT_NOT_NULL(t);
    ASSERT_EQ(t->kind, TYPE_ERROR);
    
    arena_destroy(arena);
}

/* ========== String Interpolation Tests ========== */

void test_check_interp_string_basic(void) {
    Arena* arena = arena_create(4096);
    
    // "Hello, {name}!" where name: String -> String
    Checker* checker = checker_new(arena);
    checker_define(checker, string_new(arena, "name"), type_string(arena));
    
    Parser* parser = parser_new(arena, "\"Hello, {name}!\"");
    Expr* expr = parse_expr(parser);
    ASSERT_NOT_NULL(expr);
    
    Type* t = checker_infer_expr(checker, expr);
    ASSERT_NOT_NULL(t);
    ASSERT_EQ(t->kind, TYPE_STRING);
    
    arena_destroy(arena);
}

void test_check_interp_string_int(void) {
    Arena* arena = arena_create(4096);
    
    // "Count: {n}" where n: Int -> String
    Checker* checker = checker_new(arena);
    checker_define(checker, string_new(arena, "n"), type_int(arena));
    
    Parser* parser = parser_new(arena, "\"Count: {n}\"");
    Expr* expr = parse_expr(parser);
    ASSERT_NOT_NULL(expr);
    
    Type* t = checker_infer_expr(checker, expr);
    ASSERT_NOT_NULL(t);
    ASSERT_EQ(t->kind, TYPE_STRING);
    
    arena_destroy(arena);
}

void test_check_interp_string_expr(void) {
    Arena* arena = arena_create(4096);
    
    // "Result: {1 + 2}" -> String
    Parser* parser = parser_new(arena, "\"Result: {1 + 2}\"");
    Expr* expr = parse_expr(parser);
    ASSERT_NOT_NULL(expr);
    
    Checker* checker = checker_new(arena);
    Type* t = checker_infer_expr(checker, expr);
    ASSERT_NOT_NULL(t);
    ASSERT_EQ(t->kind, TYPE_STRING);
    
    arena_destroy(arena);
}

void test_check_interp_string_undefined_var(void) {
    Arena* arena = arena_create(4096);
    
    // "Hello, {unknown}!" where unknown is not defined -> error
    Parser* parser = parser_new(arena, "\"Hello, {unknown}!\"");
    Expr* expr = parse_expr(parser);
    ASSERT_NOT_NULL(expr);
    
    Checker* checker = checker_new(arena);
    Type* t = checker_infer_expr(checker, expr);
    ASSERT_NOT_NULL(t);
    ASSERT_EQ(t->kind, TYPE_ERROR);
    
    arena_destroy(arena);
}

/* ========== Tui Module Tests ========== */

void test_check_tui_style_returns_string(void) {
    Arena* arena = arena_create(4096);
    
    // Tui.Style.red("text") -> String
    Type* t = check_expr(arena, "Tui.Style.red(\"hello\")");
    
    ASSERT_NOT_NULL(t);
    ASSERT_EQ(t->kind, TYPE_STRING);
    
    arena_destroy(arena);
}

void test_check_tui_style_bold_returns_string(void) {
    Arena* arena = arena_create(4096);
    
    // Tui.Style.bold("text") -> String
    Type* t = check_expr(arena, "Tui.Style.bold(\"important\")");
    
    ASSERT_NOT_NULL(t);
    ASSERT_EQ(t->kind, TYPE_STRING);
    
    arena_destroy(arena);
}

void test_check_tui_panel_new_returns_panel(void) {
    Arena* arena = arena_create(4096);
    
    // Tui.Panel.new("content") -> Panel
    Type* t = check_expr(arena, "Tui.Panel.new(\"Hello\")");
    
    ASSERT_NOT_NULL(t);
    ASSERT_EQ(t->kind, TYPE_CON);
    ASSERT_STR_EQ(string_cstr(t->data.con.name), "Panel");
    
    arena_destroy(arena);
}

void test_check_tui_panel_render_returns_string(void) {
    Arena* arena = arena_create(4096);
    
    // Tui.Panel.render(panel) -> String
    Type* t = check_expr(arena, "Tui.Panel.render(Tui.Panel.new(\"test\"))");
    
    ASSERT_NOT_NULL(t);
    ASSERT_EQ(t->kind, TYPE_STRING);
    
    arena_destroy(arena);
}

void test_check_tui_table_new_returns_table(void) {
    Arena* arena = arena_create(4096);
    
    // Tui.Table.new() -> Table
    Type* t = check_expr(arena, "Tui.Table.new()");
    
    ASSERT_NOT_NULL(t);
    ASSERT_EQ(t->kind, TYPE_CON);
    ASSERT_STR_EQ(string_cstr(t->data.con.name), "Table");
    
    arena_destroy(arena);
}

void test_check_tui_status_ok_returns_string(void) {
    Arena* arena = arena_create(4096);
    
    // Tui.Status.ok("message") -> String
    Type* t = check_expr(arena, "Tui.Status.ok(\"All good\")");
    
    ASSERT_NOT_NULL(t);
    ASSERT_EQ(t->kind, TYPE_STRING);
    
    arena_destroy(arena);
}

void test_check_tui_term_is_tty_returns_bool(void) {
    Arena* arena = arena_create(4096);
    
    // Tui.Term.is_tty() -> Bool
    Type* t = check_expr(arena, "Tui.Term.is_tty()");
    
    ASSERT_NOT_NULL(t);
    ASSERT_EQ(t->kind, TYPE_BOOL);
    
    arena_destroy(arena);
}

void test_check_tui_term_color_support_returns_int(void) {
    Arena* arena = arena_create(4096);
    
    // Tui.Term.color_support() -> Int
    Type* t = check_expr(arena, "Tui.Term.color_support()");
    
    ASSERT_NOT_NULL(t);
    ASSERT_EQ(t->kind, TYPE_INT);
    
    arena_destroy(arena);
}

void test_check_tui_live_sleep_returns_unit(void) {
    Arena* arena = arena_create(4096);
    
    // Tui.Live.sleep(100) -> Unit
    Type* t = check_expr(arena, "Tui.Live.sleep(100)");
    
    ASSERT_NOT_NULL(t);
    ASSERT_EQ(t->kind, TYPE_UNIT);
    
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
    
    // Try operator (?)
    TEST_RUN(test_check_try_unwraps_result);
    TEST_RUN(test_check_try_requires_result);
    
    // Generic type instantiation
    TEST_RUN(test_check_generic_identity);
    TEST_RUN(test_check_generic_list_head);
    
    // Bind expressions (<-)
    TEST_RUN(test_check_bind_unwraps_result);
    TEST_RUN(test_check_bind_requires_result);
    TEST_RUN(test_check_bind_propagates_error_type);
    
    // With expressions
    TEST_RUN(test_check_with_simple);
    TEST_RUN(test_check_with_multiple_bindings);
    TEST_RUN(test_check_with_requires_result);
    
    // Lambda expressions
    TEST_RUN(test_check_lambda_simple);
    TEST_RUN(test_check_lambda_applied);
    
    // For loops
    TEST_RUN(test_check_for_loop_basic);
    TEST_RUN(test_check_for_binds_loop_var);
    TEST_RUN(test_check_for_requires_iterable);
    
    // Index expressions
    TEST_RUN(test_check_index_list);
    TEST_RUN(test_check_index_requires_int);
    TEST_RUN(test_check_index_non_indexable);
    
    // Pipe operator
    TEST_RUN(test_check_pipe_basic);
    TEST_RUN(test_check_pipe_chain);
    TEST_RUN(test_check_pipe_type_mismatch);
    
    // Constructor pattern binding
    TEST_RUN(test_check_match_option_some);
    TEST_RUN(test_check_match_option_none);
    TEST_RUN(test_check_match_result_ok_err);
    
    // Range expressions
    TEST_RUN(test_check_range_int);
    TEST_RUN(test_check_range_inclusive);
    TEST_RUN(test_check_range_requires_same_type);
    
    // Map literals
    TEST_RUN(test_check_map_string_int);
    TEST_RUN(test_check_map_empty);
    TEST_RUN(test_check_map_mixed_keys_error);
    
    // Tuple field access (dot notation)
    TEST_RUN(test_check_tuple_field_access);
    TEST_RUN(test_check_tuple_field_access_second);
    TEST_RUN(test_check_tuple_field_out_of_bounds);
    
    // Function definitions
    TEST_RUN(test_check_fn_simple);
    TEST_RUN(test_check_fn_wrong_return_type);
    TEST_RUN(test_check_fn_uses_params);
    TEST_RUN(test_check_fn_no_return_type);
    TEST_RUN(test_check_fn_param_type_mismatch);
    
    // Type definitions
    TEST_RUN(test_check_type_def_simple);
    TEST_RUN(test_check_type_def_with_fields);
    TEST_RUN(test_check_type_def_unknown_field_type);
    TEST_RUN(test_check_type_def_record);
    TEST_RUN(test_check_type_def_record_unknown_field_type);
    
    // List comprehensions
    TEST_RUN(test_check_list_comp_basic);
    TEST_RUN(test_check_list_comp_with_filter);
    TEST_RUN(test_check_list_comp_non_bool_filter);
    TEST_RUN(test_check_list_comp_requires_iterable);
    
    // String interpolation
    TEST_RUN(test_check_interp_string_basic);
    TEST_RUN(test_check_interp_string_int);
    TEST_RUN(test_check_interp_string_expr);
    TEST_RUN(test_check_interp_string_undefined_var);
    
    // Tui module type checking
    TEST_RUN(test_check_tui_style_returns_string);
    TEST_RUN(test_check_tui_style_bold_returns_string);
    TEST_RUN(test_check_tui_panel_new_returns_panel);
    TEST_RUN(test_check_tui_panel_render_returns_string);
    TEST_RUN(test_check_tui_table_new_returns_table);
    TEST_RUN(test_check_tui_status_ok_returns_string);
    TEST_RUN(test_check_tui_term_is_tty_returns_bool);
    TEST_RUN(test_check_tui_term_color_support_returns_int);
    TEST_RUN(test_check_tui_live_sleep_returns_unit);
}
