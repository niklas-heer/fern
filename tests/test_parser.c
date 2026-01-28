/* Parser Tests
 * 
 * Test-driven development: Write tests FIRST, then implement parser.
 * Based on DESIGN.md specification.
 */

#include "test.h"
#include "arena.h"
#include "parser.h"
#include "ast.h"

/* Test: Parse integer literal */
void test_parse_int_literal(void) {
    Arena* arena = arena_create(4096);
    Parser* parser = parser_new(arena, "42");
    
    Expr* expr = parse_expr(parser);
    ASSERT_NOT_NULL(expr);
    ASSERT_EQ(expr->type, EXPR_INT_LIT);
    ASSERT_EQ(expr->data.int_lit.value, 42);
    
    arena_destroy(arena);
}

/* Test: Parse string literal */
void test_parse_string_literal(void) {
    Arena* arena = arena_create(4096);
    Parser* parser = parser_new(arena, "\"hello\"");
    
    Expr* expr = parse_expr(parser);
    ASSERT_NOT_NULL(expr);
    ASSERT_EQ(expr->type, EXPR_STRING_LIT);
    ASSERT_STR_EQ(string_cstr(expr->data.string_lit.value), "hello");
    
    arena_destroy(arena);
}

/* Test: Parse boolean literals */
void test_parse_bool_literal(void) {
    Arena* arena = arena_create(4096);
    
    Parser* p1 = parser_new(arena, "true");
    Expr* expr1 = parse_expr(p1);
    ASSERT_EQ(expr1->type, EXPR_BOOL_LIT);
    ASSERT_TRUE(expr1->data.bool_lit.value);
    
    Parser* p2 = parser_new(arena, "false");
    Expr* expr2 = parse_expr(p2);
    ASSERT_EQ(expr2->type, EXPR_BOOL_LIT);
    ASSERT_FALSE(expr2->data.bool_lit.value);
    
    arena_destroy(arena);
}

/* Test: Parse identifier */
void test_parse_identifier(void) {
    Arena* arena = arena_create(4096);
    Parser* parser = parser_new(arena, "my_variable");
    
    Expr* expr = parse_expr(parser);
    ASSERT_NOT_NULL(expr);
    ASSERT_EQ(expr->type, EXPR_IDENT);
    ASSERT_STR_EQ(string_cstr(expr->data.ident.name), "my_variable");
    
    arena_destroy(arena);
}

/* Test: Parse binary expression (addition) */
void test_parse_binary_add(void) {
    Arena* arena = arena_create(4096);
    Parser* parser = parser_new(arena, "1 + 2");
    
    Expr* expr = parse_expr(parser);
    ASSERT_NOT_NULL(expr);
    ASSERT_EQ(expr->type, EXPR_BINARY);
    ASSERT_EQ(expr->data.binary.op, BINOP_ADD);
    
    ASSERT_EQ(expr->data.binary.left->type, EXPR_INT_LIT);
    ASSERT_EQ(expr->data.binary.left->data.int_lit.value, 1);
    
    ASSERT_EQ(expr->data.binary.right->type, EXPR_INT_LIT);
    ASSERT_EQ(expr->data.binary.right->data.int_lit.value, 2);
    
    arena_destroy(arena);
}

/* Test: Parse binary expression with precedence (1 + 2 * 3) */
void test_parse_binary_precedence(void) {
    Arena* arena = arena_create(4096);
    Parser* parser = parser_new(arena, "1 + 2 * 3");
    
    Expr* expr = parse_expr(parser);
    ASSERT_NOT_NULL(expr);
    
    // Should parse as: 1 + (2 * 3)
    ASSERT_EQ(expr->type, EXPR_BINARY);
    ASSERT_EQ(expr->data.binary.op, BINOP_ADD);
    
    // Left side: 1
    ASSERT_EQ(expr->data.binary.left->type, EXPR_INT_LIT);
    ASSERT_EQ(expr->data.binary.left->data.int_lit.value, 1);
    
    // Right side: 2 * 3
    Expr* right = expr->data.binary.right;
    ASSERT_EQ(right->type, EXPR_BINARY);
    ASSERT_EQ(right->data.binary.op, BINOP_MUL);
    ASSERT_EQ(right->data.binary.left->data.int_lit.value, 2);
    ASSERT_EQ(right->data.binary.right->data.int_lit.value, 3);
    
    arena_destroy(arena);
}

/* Test: Parse comparison expression */
void test_parse_comparison(void) {
    Arena* arena = arena_create(4096);
    Parser* parser = parser_new(arena, "x == 42");
    
    Expr* expr = parse_expr(parser);
    ASSERT_NOT_NULL(expr);
    ASSERT_EQ(expr->type, EXPR_BINARY);
    ASSERT_EQ(expr->data.binary.op, BINOP_EQ);
    
    arena_destroy(arena);
}

/* Test: Parse function call (no arguments) */
void test_parse_call_no_args(void) {
    Arena* arena = arena_create(4096);
    Parser* parser = parser_new(arena, "foo()");
    
    Expr* expr = parse_expr(parser);
    ASSERT_NOT_NULL(expr);
    ASSERT_EQ(expr->type, EXPR_CALL);
    ASSERT_EQ(expr->data.call.func->type, EXPR_IDENT);
    ASSERT_EQ(expr->data.call.args->len, 0);
    
    arena_destroy(arena);
}

/* Test: Parse function call (with arguments) */
void test_parse_call_with_args(void) {
    Arena* arena = arena_create(4096);
    Parser* parser = parser_new(arena, "add(1, 2)");
    
    Expr* expr = parse_expr(parser);
    ASSERT_NOT_NULL(expr);
    ASSERT_EQ(expr->type, EXPR_CALL);
    ASSERT_EQ(expr->data.call.args->len, 2);
    
    // First argument
    CallArg arg1 = CallArgVec_get(expr->data.call.args, 0);
    ASSERT_NULL(arg1.label);  // Positional argument
    ASSERT_EQ(arg1.value->type, EXPR_INT_LIT);
    ASSERT_EQ(arg1.value->data.int_lit.value, 1);
    
    // Second argument
    CallArg arg2 = CallArgVec_get(expr->data.call.args, 1);
    ASSERT_NULL(arg2.label);
    ASSERT_EQ(arg2.value->type, EXPR_INT_LIT);
    ASSERT_EQ(arg2.value->data.int_lit.value, 2);
    
    arena_destroy(arena);
}

/* Test: Parse let statement */
void test_parse_let_statement(void) {
    Arena* arena = arena_create(4096);
    Parser* parser = parser_new(arena, "let x = 42");
    
    Stmt* stmt = parse_stmt(parser);
    ASSERT_NOT_NULL(stmt);
    ASSERT_EQ(stmt->type, STMT_LET);
    ASSERT_EQ(stmt->data.let.pattern->type, PATTERN_IDENT);
    ASSERT_STR_EQ(string_cstr(stmt->data.let.pattern->data.ident), "x");
    ASSERT_EQ(stmt->data.let.value->type, EXPR_INT_LIT);
    ASSERT_EQ(stmt->data.let.value->data.int_lit.value, 42);
    
    arena_destroy(arena);
}

/* Test: Parse return statement */
void test_parse_return_statement(void) {
    Arena* arena = arena_create(4096);
    Parser* parser = parser_new(arena, "return 42");
    
    Stmt* stmt = parse_stmt(parser);
    ASSERT_NOT_NULL(stmt);
    ASSERT_EQ(stmt->type, STMT_RETURN);
    ASSERT_NOT_NULL(stmt->data.return_stmt.value);
    ASSERT_EQ(stmt->data.return_stmt.value->type, EXPR_INT_LIT);
    ASSERT_EQ(stmt->data.return_stmt.value->data.int_lit.value, 42);
    
    arena_destroy(arena);
}

/* Test: Parse unary expression (negation) */
void test_parse_unary_neg(void) {
    Arena* arena = arena_create(4096);
    Parser* parser = parser_new(arena, "-42");
    
    Expr* expr = parse_expr(parser);
    ASSERT_NOT_NULL(expr);
    ASSERT_EQ(expr->type, EXPR_UNARY);
    ASSERT_EQ(expr->data.unary.op, UNOP_NEG);
    ASSERT_EQ(expr->data.unary.operand->type, EXPR_INT_LIT);
    ASSERT_EQ(expr->data.unary.operand->data.int_lit.value, 42);
    
    arena_destroy(arena);
}

/* Test: Parse unary expression (not) */
void test_parse_unary_not(void) {
    Arena* arena = arena_create(4096);
    Parser* parser = parser_new(arena, "not true");
    
    Expr* expr = parse_expr(parser);
    ASSERT_NOT_NULL(expr);
    ASSERT_EQ(expr->type, EXPR_UNARY);
    ASSERT_EQ(expr->data.unary.op, UNOP_NOT);
    ASSERT_EQ(expr->data.unary.operand->type, EXPR_BOOL_LIT);
    
    arena_destroy(arena);
}

/* Test: Parse simple if expression */
void test_parse_if_simple(void) {
    Arena* arena = arena_create(4096);
    Parser* parser = parser_new(arena, "if true: 42");
    
    Expr* expr = parse_expr(parser);
    ASSERT_NOT_NULL(expr);
    ASSERT_EQ(expr->type, EXPR_IF);
    ASSERT_NOT_NULL(expr->data.if_expr.condition);
    ASSERT_EQ(expr->data.if_expr.condition->type, EXPR_BOOL_LIT);
    ASSERT_NOT_NULL(expr->data.if_expr.then_branch);
    ASSERT_EQ(expr->data.if_expr.then_branch->type, EXPR_INT_LIT);
    ASSERT_NULL(expr->data.if_expr.else_branch);
    
    arena_destroy(arena);
}

/* Test: Parse if-else expression */
void test_parse_if_else(void) {
    Arena* arena = arena_create(4096);
    Parser* parser = parser_new(arena, "if x > 0: 1 else: 0");
    
    Expr* expr = parse_expr(parser);
    ASSERT_NOT_NULL(expr);
    ASSERT_EQ(expr->type, EXPR_IF);
    ASSERT_NOT_NULL(expr->data.if_expr.condition);
    ASSERT_EQ(expr->data.if_expr.condition->type, EXPR_BINARY);
    ASSERT_NOT_NULL(expr->data.if_expr.then_branch);
    ASSERT_EQ(expr->data.if_expr.then_branch->type, EXPR_INT_LIT);
    ASSERT_NOT_NULL(expr->data.if_expr.else_branch);
    ASSERT_EQ(expr->data.if_expr.else_branch->type, EXPR_INT_LIT);
    
    arena_destroy(arena);
}

/* Test: Parse simple match expression */
void test_parse_match_simple(void) {
    Arena* arena = arena_create(4096);
    Parser* parser = parser_new(arena, "match x: 1 -> true, 2 -> false");
    
    Expr* expr = parse_expr(parser);
    ASSERT_NOT_NULL(expr);
    ASSERT_EQ(expr->type, EXPR_MATCH);
    ASSERT_NOT_NULL(expr->data.match_expr.value);
    ASSERT_EQ(expr->data.match_expr.value->type, EXPR_IDENT);
    ASSERT_NOT_NULL(expr->data.match_expr.arms);
    ASSERT_EQ(expr->data.match_expr.arms->len, 2);
    
    arena_destroy(arena);
}

/* Test: Parse match with wildcard default */
void test_parse_match_with_default(void) {
    Arena* arena = arena_create(4096);
    Parser* parser = parser_new(arena, "match x: 1 -> true, _ -> false");
    
    Expr* expr = parse_expr(parser);
    ASSERT_NOT_NULL(expr);
    ASSERT_EQ(expr->type, EXPR_MATCH);
    ASSERT_NOT_NULL(expr->data.match_expr.arms);
    ASSERT_EQ(expr->data.match_expr.arms->len, 2);
    
    // Second arm should have wildcard pattern
    MatchArm second_arm = MatchArmVec_get(expr->data.match_expr.arms, 1);
    ASSERT_NOT_NULL(second_arm.pattern);
    ASSERT_EQ(second_arm.pattern->type, PATTERN_WILDCARD);
    
    arena_destroy(arena);
}

/* Test: Parse simple block expression */
void test_parse_block_simple(void) {
    Arena* arena = arena_create(4096);
    Parser* parser = parser_new(arena, "{ let x = 5, x + 1 }");
    
    Expr* expr = parse_expr(parser);
    ASSERT_NOT_NULL(expr);
    ASSERT_EQ(expr->type, EXPR_BLOCK);
    ASSERT_NOT_NULL(expr->data.block.stmts);
    ASSERT_EQ(expr->data.block.stmts->len, 1);
    
    // Check final expression
    ASSERT_NOT_NULL(expr->data.block.final_expr);
    ASSERT_EQ(expr->data.block.final_expr->type, EXPR_BINARY);
    
    arena_destroy(arena);
}

/* Test: Parse block with multiple statements */
void test_parse_block_multiple_statements(void) {
    Arena* arena = arena_create(4096);
    Parser* parser = parser_new(arena, "{ let a = 1, let b = 2, a + b }");
    
    Expr* expr = parse_expr(parser);
    ASSERT_NOT_NULL(expr);
    ASSERT_EQ(expr->type, EXPR_BLOCK);
    ASSERT_NOT_NULL(expr->data.block.stmts);
    ASSERT_EQ(expr->data.block.stmts->len, 2);
    
    // Check final expression
    ASSERT_NOT_NULL(expr->data.block.final_expr);
    ASSERT_EQ(expr->data.block.final_expr->type, EXPR_BINARY);
    
    arena_destroy(arena);
}

/* Test: Parse empty list */
void test_parse_list_empty(void) {
    Arena* arena = arena_create(4096);
    Parser* parser = parser_new(arena, "[]");
    
    Expr* expr = parse_expr(parser);
    ASSERT_NOT_NULL(expr);
    ASSERT_EQ(expr->type, EXPR_LIST);
    ASSERT_NOT_NULL(expr->data.list.elements);
    ASSERT_EQ(expr->data.list.elements->len, 0);
    
    arena_destroy(arena);
}

/* Test: Parse simple list */
void test_parse_list_simple(void) {
    Arena* arena = arena_create(4096);
    Parser* parser = parser_new(arena, "[1, 2, 3]");
    
    Expr* expr = parse_expr(parser);
    ASSERT_NOT_NULL(expr);
    ASSERT_EQ(expr->type, EXPR_LIST);
    ASSERT_NOT_NULL(expr->data.list.elements);
    ASSERT_EQ(expr->data.list.elements->len, 3);
    
    // Check first element
    Expr* first = ExprVec_get(expr->data.list.elements, 0);
    ASSERT_EQ(first->type, EXPR_INT_LIT);
    ASSERT_EQ(first->data.int_lit.value, 1);
    
    arena_destroy(arena);
}

/* Test: Parse list with expressions */
void test_parse_list_expressions(void) {
    Arena* arena = arena_create(4096);
    Parser* parser = parser_new(arena, "[x + 1, y * 2, f()]");
    
    Expr* expr = parse_expr(parser);
    ASSERT_NOT_NULL(expr);
    ASSERT_EQ(expr->type, EXPR_LIST);
    ASSERT_NOT_NULL(expr->data.list.elements);
    ASSERT_EQ(expr->data.list.elements->len, 3);
    
    // Check first element is binary expression
    Expr* first = ExprVec_get(expr->data.list.elements, 0);
    ASSERT_EQ(first->type, EXPR_BINARY);
    
    // Check third element is function call
    Expr* third = ExprVec_get(expr->data.list.elements, 2);
    ASSERT_EQ(third->type, EXPR_CALL);
    
    arena_destroy(arena);
}

/* Test: Parse nested lists */
void test_parse_nested_lists(void) {
    Arena* arena = arena_create(4096);
    Parser* parser = parser_new(arena, "[[1, 2], [3, 4]]");

    Expr* expr = parse_expr(parser);
    ASSERT_NOT_NULL(expr);
    ASSERT_EQ(expr->type, EXPR_LIST);
    ASSERT_EQ(expr->data.list.elements->len, 2);

    // First element: [1, 2]
    Expr* first = ExprVec_get(expr->data.list.elements, 0);
    ASSERT_EQ(first->type, EXPR_LIST);
    ASSERT_EQ(first->data.list.elements->len, 2);

    Expr* one = ExprVec_get(first->data.list.elements, 0);
    ASSERT_EQ(one->type, EXPR_INT_LIT);
    ASSERT_EQ(one->data.int_lit.value, 1);

    Expr* two = ExprVec_get(first->data.list.elements, 1);
    ASSERT_EQ(two->type, EXPR_INT_LIT);
    ASSERT_EQ(two->data.int_lit.value, 2);

    // Second element: [3, 4]
    Expr* second = ExprVec_get(expr->data.list.elements, 1);
    ASSERT_EQ(second->type, EXPR_LIST);
    ASSERT_EQ(second->data.list.elements->len, 2);

    Expr* three = ExprVec_get(second->data.list.elements, 0);
    ASSERT_EQ(three->type, EXPR_INT_LIT);
    ASSERT_EQ(three->data.int_lit.value, 3);

    arena_destroy(arena);
}

/* Test: Parse list inside block expression */
void test_parse_list_in_block(void) {
    Arena* arena = arena_create(4096);
    Parser* parser = parser_new(arena, "{ let x = [1, 2], x }");

    Expr* expr = parse_expr(parser);
    ASSERT_NOT_NULL(expr);
    ASSERT_EQ(expr->type, EXPR_BLOCK);
    ASSERT_EQ(expr->data.block.stmts->len, 1);

    // The let statement should bind a list
    Stmt* let_stmt = StmtVec_get(expr->data.block.stmts, 0);
    ASSERT_EQ(let_stmt->type, STMT_LET);
    ASSERT_EQ(let_stmt->data.let.value->type, EXPR_LIST);
    ASSERT_EQ(let_stmt->data.let.value->data.list.elements->len, 2);

    // Final expression should be the identifier
    ASSERT_NOT_NULL(expr->data.block.final_expr);
    ASSERT_EQ(expr->data.block.final_expr->type, EXPR_IDENT);

    arena_destroy(arena);
}

/* Test: Parse block expressions inside list */
void test_parse_block_in_list(void) {
    Arena* arena = arena_create(4096);
    Parser* parser = parser_new(arena, "[{ let a = 1, a }, { let b = 2, b }]");

    Expr* expr = parse_expr(parser);
    ASSERT_NOT_NULL(expr);
    ASSERT_EQ(expr->type, EXPR_LIST);
    ASSERT_EQ(expr->data.list.elements->len, 2);

    // First element: { let a = 1, a }
    Expr* first = ExprVec_get(expr->data.list.elements, 0);
    ASSERT_EQ(first->type, EXPR_BLOCK);
    ASSERT_EQ(first->data.block.stmts->len, 1);
    ASSERT_NOT_NULL(first->data.block.final_expr);
    ASSERT_EQ(first->data.block.final_expr->type, EXPR_IDENT);

    // Second element: { let b = 2, b }
    Expr* second = ExprVec_get(expr->data.list.elements, 1);
    ASSERT_EQ(second->type, EXPR_BLOCK);
    ASSERT_EQ(second->data.block.stmts->len, 1);
    ASSERT_NOT_NULL(second->data.block.final_expr);
    ASSERT_EQ(second->data.block.final_expr->type, EXPR_IDENT);

    arena_destroy(arena);
}

/* Test: Parse simple pipe expression (x |> f()) */
void test_parse_pipe_simple(void) {
    Arena* arena = arena_create(4096);
    Parser* parser = parser_new(arena, "x |> double()");

    Expr* expr = parse_expr(parser);
    ASSERT_NOT_NULL(expr);
    ASSERT_EQ(expr->type, EXPR_BINARY);
    ASSERT_EQ(expr->data.binary.op, BINOP_PIPE);

    // Left side: identifier x
    ASSERT_EQ(expr->data.binary.left->type, EXPR_IDENT);
    ASSERT_STR_EQ(string_cstr(expr->data.binary.left->data.ident.name), "x");

    // Right side: function call double()
    ASSERT_EQ(expr->data.binary.right->type, EXPR_CALL);

    arena_destroy(arena);
}

/* Test: Parse chained pipe expression (x |> f() |> g()) */
void test_parse_pipe_chain(void) {
    Arena* arena = arena_create(4096);
    Parser* parser = parser_new(arena, "data |> parse() |> validate()");

    Expr* expr = parse_expr(parser);
    ASSERT_NOT_NULL(expr);

    // Should parse as left-associative: (data |> parse()) |> validate()
    ASSERT_EQ(expr->type, EXPR_BINARY);
    ASSERT_EQ(expr->data.binary.op, BINOP_PIPE);

    // Right side: validate() call
    ASSERT_EQ(expr->data.binary.right->type, EXPR_CALL);

    // Left side: data |> parse()
    Expr* left = expr->data.binary.left;
    ASSERT_EQ(left->type, EXPR_BINARY);
    ASSERT_EQ(left->data.binary.op, BINOP_PIPE);
    ASSERT_EQ(left->data.binary.left->type, EXPR_IDENT);
    ASSERT_STR_EQ(string_cstr(left->data.binary.left->data.ident.name), "data");
    ASSERT_EQ(left->data.binary.right->type, EXPR_CALL);

    arena_destroy(arena);
}

/* Test: Parse pipe expression inside block */
void test_parse_pipe_in_block(void) {
    Arena* arena = arena_create(4096);
    Parser* parser = parser_new(arena, "{ let result = x |> double(), result }");

    Expr* expr = parse_expr(parser);
    ASSERT_NOT_NULL(expr);
    ASSERT_EQ(expr->type, EXPR_BLOCK);
    ASSERT_EQ(expr->data.block.stmts->len, 1);

    // The let statement value should be a pipe expression
    Stmt* let_stmt = StmtVec_get(expr->data.block.stmts, 0);
    ASSERT_EQ(let_stmt->type, STMT_LET);
    ASSERT_EQ(let_stmt->data.let.value->type, EXPR_BINARY);
    ASSERT_EQ(let_stmt->data.let.value->data.binary.op, BINOP_PIPE);

    // Final expression should be the identifier
    ASSERT_NOT_NULL(expr->data.block.final_expr);
    ASSERT_EQ(expr->data.block.final_expr->type, EXPR_IDENT);

    arena_destroy(arena);
}

/* Test: Parse simple bind expression (x <- f()) */
void test_parse_bind_simple(void) {
    Arena* arena = arena_create(4096);
    Parser* parser = parser_new(arena, "x <- f()");

    Expr* expr = parse_expr(parser);
    ASSERT_NOT_NULL(expr);
    ASSERT_EQ(expr->type, EXPR_BIND);
    ASSERT_STR_EQ(string_cstr(expr->data.bind.name), "x");
    ASSERT_NOT_NULL(expr->data.bind.value);
    ASSERT_EQ(expr->data.bind.value->type, EXPR_CALL);

    arena_destroy(arena);
}

/* Test: Parse bind with function call argument */
void test_parse_bind_with_call(void) {
    Arena* arena = arena_create(4096);
    Parser* parser = parser_new(arena, "result <- read_file(\"test.txt\")");

    Expr* expr = parse_expr(parser);
    ASSERT_NOT_NULL(expr);
    ASSERT_EQ(expr->type, EXPR_BIND);
    ASSERT_STR_EQ(string_cstr(expr->data.bind.name), "result");
    ASSERT_NOT_NULL(expr->data.bind.value);
    ASSERT_EQ(expr->data.bind.value->type, EXPR_CALL);

    // Check the call has one argument
    ASSERT_EQ(expr->data.bind.value->data.call.args->len, 1);

    arena_destroy(arena);
}

/* Test: Parse bind inside block expression */
void test_parse_bind_in_block(void) {
    Arena* arena = arena_create(4096);
    Parser* parser = parser_new(arena, "{ content <- load(), process(content) }");

    Expr* expr = parse_expr(parser);
    ASSERT_NOT_NULL(expr);
    ASSERT_EQ(expr->type, EXPR_BLOCK);
    ASSERT_NOT_NULL(expr->data.block.stmts);
    ASSERT_EQ(expr->data.block.stmts->len, 1);

    // First statement should be a bind expression statement
    Stmt* bind_stmt = StmtVec_get(expr->data.block.stmts, 0);
    ASSERT_EQ(bind_stmt->type, STMT_EXPR);
    ASSERT_EQ(bind_stmt->data.expr.expr->type, EXPR_BIND);
    ASSERT_STR_EQ(string_cstr(bind_stmt->data.expr.expr->data.bind.name), "content");

    // Final expression should be a function call
    ASSERT_NOT_NULL(expr->data.block.final_expr);
    ASSERT_EQ(expr->data.block.final_expr->type, EXPR_CALL);

    arena_destroy(arena);
}

void run_parser_tests(void) {
    printf("\n=== Parser Tests ===\n");
    TEST_RUN(test_parse_int_literal);
    TEST_RUN(test_parse_string_literal);
    TEST_RUN(test_parse_bool_literal);
    TEST_RUN(test_parse_identifier);
    TEST_RUN(test_parse_binary_add);
    TEST_RUN(test_parse_binary_precedence);
    TEST_RUN(test_parse_comparison);
    TEST_RUN(test_parse_call_no_args);
    TEST_RUN(test_parse_call_with_args);
    TEST_RUN(test_parse_let_statement);
    TEST_RUN(test_parse_return_statement);
    TEST_RUN(test_parse_unary_neg);
    TEST_RUN(test_parse_unary_not);
    TEST_RUN(test_parse_if_simple);
    TEST_RUN(test_parse_if_else);
    TEST_RUN(test_parse_match_simple);
    TEST_RUN(test_parse_match_with_default);
    TEST_RUN(test_parse_block_simple);
    TEST_RUN(test_parse_block_multiple_statements);
    TEST_RUN(test_parse_list_empty);
    TEST_RUN(test_parse_list_simple);
    TEST_RUN(test_parse_list_expressions);
    TEST_RUN(test_parse_nested_lists);
    TEST_RUN(test_parse_list_in_block);
    TEST_RUN(test_parse_block_in_list);
    TEST_RUN(test_parse_pipe_simple);
    TEST_RUN(test_parse_pipe_chain);
    TEST_RUN(test_parse_pipe_in_block);
    TEST_RUN(test_parse_bind_simple);
    TEST_RUN(test_parse_bind_with_call);
    TEST_RUN(test_parse_bind_in_block);
}
