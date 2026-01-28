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

/* Test: Parse type annotation - simple named type (Int) */
void test_parse_type_int(void) {
    Arena* arena = arena_create(4096);
    Parser* parser = parser_new(arena, "Int");

    TypeExpr* type = parse_type(parser);
    ASSERT_NOT_NULL(type);
    ASSERT_EQ(type->kind, TYPE_NAMED);
    ASSERT_STR_EQ(string_cstr(type->data.named.name), "Int");
    ASSERT_NULL(type->data.named.args);

    arena_destroy(arena);
}

/* Test: Parse type annotation - String */
void test_parse_type_string(void) {
    Arena* arena = arena_create(4096);
    Parser* parser = parser_new(arena, "String");

    TypeExpr* type = parse_type(parser);
    ASSERT_NOT_NULL(type);
    ASSERT_EQ(type->kind, TYPE_NAMED);
    ASSERT_STR_EQ(string_cstr(type->data.named.name), "String");

    arena_destroy(arena);
}

/* Test: Parse type annotation - Bool */
void test_parse_type_bool(void) {
    Arena* arena = arena_create(4096);
    Parser* parser = parser_new(arena, "Bool");

    TypeExpr* type = parse_type(parser);
    ASSERT_NOT_NULL(type);
    ASSERT_EQ(type->kind, TYPE_NAMED);
    ASSERT_STR_EQ(string_cstr(type->data.named.name), "Bool");

    arena_destroy(arena);
}

/* Test: Parse type annotation - custom type (User) */
void test_parse_type_custom(void) {
    Arena* arena = arena_create(4096);
    Parser* parser = parser_new(arena, "User");

    TypeExpr* type = parse_type(parser);
    ASSERT_NOT_NULL(type);
    ASSERT_EQ(type->kind, TYPE_NAMED);
    ASSERT_STR_EQ(string_cstr(type->data.named.name), "User");

    arena_destroy(arena);
}

/* Test: Parse type annotation - Result(String, Error) */
void test_parse_type_result(void) {
    Arena* arena = arena_create(4096);
    Parser* parser = parser_new(arena, "Result(String, Error)");

    TypeExpr* type = parse_type(parser);
    ASSERT_NOT_NULL(type);
    ASSERT_EQ(type->kind, TYPE_NAMED);
    ASSERT_STR_EQ(string_cstr(type->data.named.name), "Result");
    ASSERT_NOT_NULL(type->data.named.args);
    ASSERT_EQ(type->data.named.args->len, 2);

    // First type arg: String
    TypeExpr* arg1 = TypeExprVec_get(type->data.named.args, 0);
    ASSERT_EQ(arg1->kind, TYPE_NAMED);
    ASSERT_STR_EQ(string_cstr(arg1->data.named.name), "String");

    // Second type arg: Error
    TypeExpr* arg2 = TypeExprVec_get(type->data.named.args, 1);
    ASSERT_EQ(arg2->kind, TYPE_NAMED);
    ASSERT_STR_EQ(string_cstr(arg2->data.named.name), "Error");

    arena_destroy(arena);
}

/* Test: Parse type annotation - List(Int) */
void test_parse_type_list(void) {
    Arena* arena = arena_create(4096);
    Parser* parser = parser_new(arena, "List(Int)");

    TypeExpr* type = parse_type(parser);
    ASSERT_NOT_NULL(type);
    ASSERT_EQ(type->kind, TYPE_NAMED);
    ASSERT_STR_EQ(string_cstr(type->data.named.name), "List");
    ASSERT_NOT_NULL(type->data.named.args);
    ASSERT_EQ(type->data.named.args->len, 1);

    // Type arg: Int
    TypeExpr* arg1 = TypeExprVec_get(type->data.named.args, 0);
    ASSERT_EQ(arg1->kind, TYPE_NAMED);
    ASSERT_STR_EQ(string_cstr(arg1->data.named.name), "Int");

    arena_destroy(arena);
}

/* Test: Parse type annotation - function type (Int, String) -> Bool */
void test_parse_type_function(void) {
    Arena* arena = arena_create(4096);
    Parser* parser = parser_new(arena, "(Int, String) -> Bool");

    TypeExpr* type = parse_type(parser);
    ASSERT_NOT_NULL(type);
    ASSERT_EQ(type->kind, TYPE_FUNCTION);
    ASSERT_NOT_NULL(type->data.func.params);
    ASSERT_EQ(type->data.func.params->len, 2);
    ASSERT_NOT_NULL(type->data.func.return_type);

    // First param: Int
    TypeExpr* param1 = TypeExprVec_get(type->data.func.params, 0);
    ASSERT_EQ(param1->kind, TYPE_NAMED);
    ASSERT_STR_EQ(string_cstr(param1->data.named.name), "Int");

    // Second param: String
    TypeExpr* param2 = TypeExprVec_get(type->data.func.params, 1);
    ASSERT_EQ(param2->kind, TYPE_NAMED);
    ASSERT_STR_EQ(string_cstr(param2->data.named.name), "String");

    // Return type: Bool
    ASSERT_EQ(type->data.func.return_type->kind, TYPE_NAMED);
    ASSERT_STR_EQ(string_cstr(type->data.func.return_type->data.named.name), "Bool");

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

/* Test: Parse function definition with no parameters */
void test_parse_function_no_params(void) {
    Arena* arena = arena_create(4096);
    Parser* parser = parser_new(arena, "fn main() -> (): Ok(())");

    Stmt* stmt = parse_stmt(parser);
    ASSERT_NOT_NULL(stmt);
    ASSERT_EQ(stmt->type, STMT_FN);
    ASSERT_STR_EQ(string_cstr(stmt->data.fn.name), "main");
    ASSERT_NOT_NULL(stmt->data.fn.params);
    ASSERT_EQ(stmt->data.fn.params->len, 0);

    // Return type should be () - unit type
    ASSERT_NOT_NULL(stmt->data.fn.return_type);

    // Body should be a call expression: Ok(())
    ASSERT_NOT_NULL(stmt->data.fn.body);
    ASSERT_EQ(stmt->data.fn.body->type, EXPR_CALL);

    arena_destroy(arena);
}

/* Test: Parse function definition with parameters */
void test_parse_function_with_params(void) {
    Arena* arena = arena_create(4096);
    Parser* parser = parser_new(arena, "fn add(x: Int, y: Int) -> Int: x + y");

    Stmt* stmt = parse_stmt(parser);
    ASSERT_NOT_NULL(stmt);
    ASSERT_EQ(stmt->type, STMT_FN);
    ASSERT_STR_EQ(string_cstr(stmt->data.fn.name), "add");
    ASSERT_NOT_NULL(stmt->data.fn.params);
    ASSERT_EQ(stmt->data.fn.params->len, 2);

    // First parameter: x: Int
    Parameter p1 = ParameterVec_get(stmt->data.fn.params, 0);
    ASSERT_STR_EQ(string_cstr(p1.name), "x");
    ASSERT_NOT_NULL(p1.type_ann);
    ASSERT_EQ(p1.type_ann->kind, TYPE_NAMED);
    ASSERT_STR_EQ(string_cstr(p1.type_ann->data.named.name), "Int");

    // Second parameter: y: Int
    Parameter p2 = ParameterVec_get(stmt->data.fn.params, 1);
    ASSERT_STR_EQ(string_cstr(p2.name), "y");
    ASSERT_NOT_NULL(p2.type_ann);
    ASSERT_EQ(p2.type_ann->kind, TYPE_NAMED);
    ASSERT_STR_EQ(string_cstr(p2.type_ann->data.named.name), "Int");

    // Return type: Int
    ASSERT_NOT_NULL(stmt->data.fn.return_type);
    ASSERT_EQ(stmt->data.fn.return_type->kind, TYPE_NAMED);
    ASSERT_STR_EQ(string_cstr(stmt->data.fn.return_type->data.named.name), "Int");

    // Body: x + y (binary expression)
    ASSERT_NOT_NULL(stmt->data.fn.body);
    ASSERT_EQ(stmt->data.fn.body->type, EXPR_BINARY);
    ASSERT_EQ(stmt->data.fn.body->data.binary.op, BINOP_ADD);

    arena_destroy(arena);
}

/* Test: Parse function definition with block body */
void test_parse_function_with_body(void) {
    Arena* arena = arena_create(4096);
    Parser* parser = parser_new(arena, "fn double(x: Int) -> Int: { let result = x * 2, result }");

    Stmt* stmt = parse_stmt(parser);
    ASSERT_NOT_NULL(stmt);
    ASSERT_EQ(stmt->type, STMT_FN);
    ASSERT_STR_EQ(string_cstr(stmt->data.fn.name), "double");
    ASSERT_NOT_NULL(stmt->data.fn.params);
    ASSERT_EQ(stmt->data.fn.params->len, 1);

    // Parameter: x: Int
    Parameter p1 = ParameterVec_get(stmt->data.fn.params, 0);
    ASSERT_STR_EQ(string_cstr(p1.name), "x");
    ASSERT_NOT_NULL(p1.type_ann);
    ASSERT_EQ(p1.type_ann->kind, TYPE_NAMED);
    ASSERT_STR_EQ(string_cstr(p1.type_ann->data.named.name), "Int");

    // Return type: Int
    ASSERT_NOT_NULL(stmt->data.fn.return_type);
    ASSERT_EQ(stmt->data.fn.return_type->kind, TYPE_NAMED);

    // Body should be a block expression
    ASSERT_NOT_NULL(stmt->data.fn.body);
    ASSERT_EQ(stmt->data.fn.body->type, EXPR_BLOCK);
    ASSERT_EQ(stmt->data.fn.body->data.block.stmts->len, 1);
    ASSERT_NOT_NULL(stmt->data.fn.body->data.block.final_expr);

    arena_destroy(arena);
}

/* Test: Parse match with integer literal pattern */
void test_parse_pattern_int_literal(void) {
    Arena* arena = arena_create(4096);
    Parser* parser = parser_new(arena, "match x: 42 -> \"found\"");

    Expr* expr = parse_expr(parser);
    ASSERT_NOT_NULL(expr);
    ASSERT_EQ(expr->type, EXPR_MATCH);
    ASSERT_EQ(expr->data.match_expr.arms->len, 1);

    // The pattern should be a literal pattern wrapping an int literal
    MatchArm arm = MatchArmVec_get(expr->data.match_expr.arms, 0);
    ASSERT_NOT_NULL(arm.pattern);
    ASSERT_EQ(arm.pattern->type, PATTERN_LIT);
    ASSERT_NOT_NULL(arm.pattern->data.literal);
    ASSERT_EQ(arm.pattern->data.literal->type, EXPR_INT_LIT);
    ASSERT_EQ(arm.pattern->data.literal->data.int_lit.value, 42);

    // Body should be a string literal
    ASSERT_EQ(arm.body->type, EXPR_STRING_LIT);

    arena_destroy(arena);
}

/* Test: Parse match with string literal pattern */
void test_parse_pattern_string_literal(void) {
    Arena* arena = arena_create(4096);
    Parser* parser = parser_new(arena, "match x: \"test\" -> \"found\"");

    Expr* expr = parse_expr(parser);
    ASSERT_NOT_NULL(expr);
    ASSERT_EQ(expr->type, EXPR_MATCH);
    ASSERT_EQ(expr->data.match_expr.arms->len, 1);

    // The pattern should be a literal pattern wrapping a string literal
    MatchArm arm = MatchArmVec_get(expr->data.match_expr.arms, 0);
    ASSERT_NOT_NULL(arm.pattern);
    ASSERT_EQ(arm.pattern->type, PATTERN_LIT);
    ASSERT_NOT_NULL(arm.pattern->data.literal);
    ASSERT_EQ(arm.pattern->data.literal->type, EXPR_STRING_LIT);
    ASSERT_STR_EQ(string_cstr(arm.pattern->data.literal->data.string_lit.value), "test");

    arena_destroy(arena);
}

/* Test: Parse match with boolean literal patterns */
void test_parse_pattern_bool_literal(void) {
    Arena* arena = arena_create(4096);
    Parser* parser = parser_new(arena, "match x: true -> \"yes\", false -> \"no\"");

    Expr* expr = parse_expr(parser);
    ASSERT_NOT_NULL(expr);
    ASSERT_EQ(expr->type, EXPR_MATCH);
    ASSERT_EQ(expr->data.match_expr.arms->len, 2);

    // First arm: true -> "yes"
    MatchArm arm1 = MatchArmVec_get(expr->data.match_expr.arms, 0);
    ASSERT_NOT_NULL(arm1.pattern);
    ASSERT_EQ(arm1.pattern->type, PATTERN_LIT);
    ASSERT_NOT_NULL(arm1.pattern->data.literal);
    ASSERT_EQ(arm1.pattern->data.literal->type, EXPR_BOOL_LIT);
    ASSERT_TRUE(arm1.pattern->data.literal->data.bool_lit.value);

    // Second arm: false -> "no"
    MatchArm arm2 = MatchArmVec_get(expr->data.match_expr.arms, 1);
    ASSERT_NOT_NULL(arm2.pattern);
    ASSERT_EQ(arm2.pattern->type, PATTERN_LIT);
    ASSERT_NOT_NULL(arm2.pattern->data.literal);
    ASSERT_EQ(arm2.pattern->data.literal->type, EXPR_BOOL_LIT);
    ASSERT_FALSE(arm2.pattern->data.literal->data.bool_lit.value);

    arena_destroy(arena);
}

/* Test: Parse match with wildcard pattern (verify existing support) */
void test_parse_pattern_wildcard(void) {
    Arena* arena = arena_create(4096);
    Parser* parser = parser_new(arena, "match x: _ -> \"anything\"");

    Expr* expr = parse_expr(parser);
    ASSERT_NOT_NULL(expr);
    ASSERT_EQ(expr->type, EXPR_MATCH);
    ASSERT_EQ(expr->data.match_expr.arms->len, 1);

    // The pattern should be a wildcard
    MatchArm arm = MatchArmVec_get(expr->data.match_expr.arms, 0);
    ASSERT_NOT_NULL(arm.pattern);
    ASSERT_EQ(arm.pattern->type, PATTERN_WILDCARD);

    // Body should be a string literal
    ASSERT_EQ(arm.body->type, EXPR_STRING_LIT);

    arena_destroy(arena);
}

/* Test: Parse match with identifier pattern (binding pattern) */
void test_parse_pattern_identifier(void) {
    Arena* arena = arena_create(4096);
    Parser* parser = parser_new(arena, "match x: value -> value");

    Expr* expr = parse_expr(parser);
    ASSERT_NOT_NULL(expr);
    ASSERT_EQ(expr->type, EXPR_MATCH);
    ASSERT_EQ(expr->data.match_expr.arms->len, 1);

    // The pattern should be an identifier pattern (binding), NOT a literal pattern
    MatchArm arm = MatchArmVec_get(expr->data.match_expr.arms, 0);
    ASSERT_NOT_NULL(arm.pattern);
    ASSERT_EQ(arm.pattern->type, PATTERN_IDENT);
    ASSERT_STR_EQ(string_cstr(arm.pattern->data.ident), "value");

    // Body should be an identifier expression
    ASSERT_EQ(arm.body->type, EXPR_IDENT);
    ASSERT_STR_EQ(string_cstr(arm.body->data.ident.name), "value");

    arena_destroy(arena);
}

/* Test: Parse let with Int type annotation */
void test_parse_let_with_type_int(void) {
    Arena* arena = arena_create(4096);
    Parser* parser = parser_new(arena, "let x: Int = 42");

    Stmt* stmt = parse_stmt(parser);
    ASSERT_NOT_NULL(stmt);
    ASSERT_EQ(stmt->type, STMT_LET);
    ASSERT_EQ(stmt->data.let.pattern->type, PATTERN_IDENT);
    ASSERT_STR_EQ(string_cstr(stmt->data.let.pattern->data.ident), "x");

    // Type annotation should be Int
    ASSERT_NOT_NULL(stmt->data.let.type_ann);
    ASSERT_EQ(stmt->data.let.type_ann->kind, TYPE_NAMED);
    ASSERT_STR_EQ(string_cstr(stmt->data.let.type_ann->data.named.name), "Int");

    // Value should be 42
    ASSERT_EQ(stmt->data.let.value->type, EXPR_INT_LIT);
    ASSERT_EQ(stmt->data.let.value->data.int_lit.value, 42);

    arena_destroy(arena);
}

/* Test: Parse let with String type annotation */
void test_parse_let_with_type_string(void) {
    Arena* arena = arena_create(4096);
    Parser* parser = parser_new(arena, "let name: String = \"test\"");

    Stmt* stmt = parse_stmt(parser);
    ASSERT_NOT_NULL(stmt);
    ASSERT_EQ(stmt->type, STMT_LET);
    ASSERT_STR_EQ(string_cstr(stmt->data.let.pattern->data.ident), "name");

    // Type annotation should be String
    ASSERT_NOT_NULL(stmt->data.let.type_ann);
    ASSERT_EQ(stmt->data.let.type_ann->kind, TYPE_NAMED);
    ASSERT_STR_EQ(string_cstr(stmt->data.let.type_ann->data.named.name), "String");

    // Value should be "test"
    ASSERT_EQ(stmt->data.let.value->type, EXPR_STRING_LIT);
    ASSERT_STR_EQ(string_cstr(stmt->data.let.value->data.string_lit.value), "test");

    arena_destroy(arena);
}

/* Test: Parse let with parameterized type annotation */
void test_parse_let_with_type_parameterized(void) {
    Arena* arena = arena_create(4096);
    Parser* parser = parser_new(arena, "let items: List(Int) = [1, 2]");

    Stmt* stmt = parse_stmt(parser);
    ASSERT_NOT_NULL(stmt);
    ASSERT_EQ(stmt->type, STMT_LET);
    ASSERT_STR_EQ(string_cstr(stmt->data.let.pattern->data.ident), "items");

    // Type annotation should be List(Int)
    ASSERT_NOT_NULL(stmt->data.let.type_ann);
    ASSERT_EQ(stmt->data.let.type_ann->kind, TYPE_NAMED);
    ASSERT_STR_EQ(string_cstr(stmt->data.let.type_ann->data.named.name), "List");
    ASSERT_NOT_NULL(stmt->data.let.type_ann->data.named.args);
    ASSERT_EQ(stmt->data.let.type_ann->data.named.args->len, 1);

    TypeExpr* arg = TypeExprVec_get(stmt->data.let.type_ann->data.named.args, 0);
    ASSERT_EQ(arg->kind, TYPE_NAMED);
    ASSERT_STR_EQ(string_cstr(arg->data.named.name), "Int");

    // Value should be a list
    ASSERT_EQ(stmt->data.let.value->type, EXPR_LIST);
    ASSERT_EQ(stmt->data.let.value->data.list.elements->len, 2);

    arena_destroy(arena);
}

/* Test: Parse let with function type annotation */
void test_parse_let_with_type_function(void) {
    Arena* arena = arena_create(4096);
    Parser* parser = parser_new(arena, "let f: (Int) -> Int = double");

    Stmt* stmt = parse_stmt(parser);
    ASSERT_NOT_NULL(stmt);
    ASSERT_EQ(stmt->type, STMT_LET);
    ASSERT_STR_EQ(string_cstr(stmt->data.let.pattern->data.ident), "f");

    // Type annotation should be (Int) -> Int
    ASSERT_NOT_NULL(stmt->data.let.type_ann);
    ASSERT_EQ(stmt->data.let.type_ann->kind, TYPE_FUNCTION);
    ASSERT_NOT_NULL(stmt->data.let.type_ann->data.func.params);
    ASSERT_EQ(stmt->data.let.type_ann->data.func.params->len, 1);
    ASSERT_NOT_NULL(stmt->data.let.type_ann->data.func.return_type);
    ASSERT_EQ(stmt->data.let.type_ann->data.func.return_type->kind, TYPE_NAMED);
    ASSERT_STR_EQ(string_cstr(stmt->data.let.type_ann->data.func.return_type->data.named.name), "Int");

    // Value should be identifier "double"
    ASSERT_EQ(stmt->data.let.value->type, EXPR_IDENT);
    ASSERT_STR_EQ(string_cstr(stmt->data.let.value->data.ident.name), "double");

    arena_destroy(arena);
}

/* Test: Parse let without type annotation (verify existing behavior) */
void test_parse_let_without_type(void) {
    Arena* arena = arena_create(4096);
    Parser* parser = parser_new(arena, "let x = 42");

    Stmt* stmt = parse_stmt(parser);
    ASSERT_NOT_NULL(stmt);
    ASSERT_EQ(stmt->type, STMT_LET);
    ASSERT_STR_EQ(string_cstr(stmt->data.let.pattern->data.ident), "x");

    // Type annotation should be NULL (no annotation)
    ASSERT_NULL(stmt->data.let.type_ann);

    // Value should be 42
    ASSERT_EQ(stmt->data.let.value->type, EXPR_INT_LIT);
    ASSERT_EQ(stmt->data.let.value->data.int_lit.value, 42);

    arena_destroy(arena);
}

/* Test: Parse multi-clause function (simple factorial) */
void test_parse_function_multi_clause_simple(void) {
    Arena* arena = arena_create(4096);
    /* Parse two adjacent clauses for the same function name:
     *   fn fact(0) -> 1
     *   fn fact(n) -> n * fact(n - 1)
     */
    Parser* parser = parser_new(arena, "fn fact(0) -> 1\nfn fact(n) -> n * fact(n - 1)");

    StmtVec* stmts = parse_stmts(parser);
    ASSERT_NOT_NULL(stmts);
    /* Adjacent fn clauses with same name should be grouped into a single STMT_FN */
    ASSERT_EQ(stmts->len, 1);

    Stmt* fn_stmt = StmtVec_get(stmts, 0);
    ASSERT_EQ(fn_stmt->type, STMT_FN);
    ASSERT_STR_EQ(string_cstr(fn_stmt->data.fn.name), "fact");

    /* Should have 2 clauses */
    ASSERT_NOT_NULL(fn_stmt->data.fn.clauses);
    ASSERT_EQ(fn_stmt->data.fn.clauses->len, 2);

    /* First clause: fact(0) -> 1 */
    FunctionClause c1 = FunctionClauseVec_get(fn_stmt->data.fn.clauses, 0);
    ASSERT_EQ(c1.params->len, 1);
    ASSERT_EQ(PatternVec_get(c1.params, 0)->type, PATTERN_LIT);
    ASSERT_EQ(c1.body->type, EXPR_INT_LIT);
    ASSERT_EQ(c1.body->data.int_lit.value, 1);

    /* Second clause: fact(n) -> n * fact(n - 1) */
    FunctionClause c2 = FunctionClauseVec_get(fn_stmt->data.fn.clauses, 1);
    ASSERT_EQ(c2.params->len, 1);
    ASSERT_EQ(PatternVec_get(c2.params, 0)->type, PATTERN_IDENT);
    ASSERT_EQ(c2.body->type, EXPR_BINARY);
    ASSERT_EQ(c2.body->data.binary.op, BINOP_MUL);

    arena_destroy(arena);
}

/* Test: Parse multi-clause function (fibonacci with 3 clauses) */
void test_parse_function_multi_clause_fibonacci(void) {
    Arena* arena = arena_create(4096);
    Parser* parser = parser_new(arena,
        "fn fib(0) -> 0\n"
        "fn fib(1) -> 1\n"
        "fn fib(n) -> fib(n - 1) + fib(n - 2)");

    StmtVec* stmts = parse_stmts(parser);
    ASSERT_NOT_NULL(stmts);
    ASSERT_EQ(stmts->len, 1);

    Stmt* fn_stmt = StmtVec_get(stmts, 0);
    ASSERT_EQ(fn_stmt->type, STMT_FN);
    ASSERT_STR_EQ(string_cstr(fn_stmt->data.fn.name), "fib");
    ASSERT_EQ(fn_stmt->data.fn.clauses->len, 3);

    /* Third clause body: fib(n - 1) + fib(n - 2) */
    FunctionClause c3 = FunctionClauseVec_get(fn_stmt->data.fn.clauses, 2);
    ASSERT_EQ(c3.body->type, EXPR_BINARY);
    ASSERT_EQ(c3.body->data.binary.op, BINOP_ADD);

    arena_destroy(arena);
}

/* Test: Parse function clauses must be adjacent (error on separation) */
void test_parse_function_clauses_must_be_adjacent(void) {
    Arena* arena = arena_create(4096);
    /* Clauses of factorial are separated by another function definition */
    Parser* parser = parser_new(arena,
        "fn fact(0) -> 1\n"
        "fn other() -> Int: 42\n"
        "fn fact(n) -> n * fact(n - 1)");

    StmtVec* stmts = parse_stmts(parser);
    ASSERT_NOT_NULL(stmts);

    /* The parser should report an error when it encounters a second set of
     * clauses for 'fact' after a different function was defined in between. */
    ASSERT_TRUE(parser_had_error(parser));

    arena_destroy(arena);
}

/* Test: Parse function with string pattern parameters */
void test_parse_function_pattern_params(void) {
    Arena* arena = arena_create(4096);
    Parser* parser = parser_new(arena,
        "fn greet(\"Alice\") -> \"Hi Alice\"\n"
        "fn greet(name) -> \"Hello\"");

    StmtVec* stmts = parse_stmts(parser);
    ASSERT_NOT_NULL(stmts);
    ASSERT_EQ(stmts->len, 1);

    Stmt* fn_stmt = StmtVec_get(stmts, 0);
    ASSERT_EQ(fn_stmt->type, STMT_FN);
    ASSERT_STR_EQ(string_cstr(fn_stmt->data.fn.name), "greet");
    ASSERT_EQ(fn_stmt->data.fn.clauses->len, 2);

    /* First clause: greet("Alice") -> "Hi Alice" */
    FunctionClause c1 = FunctionClauseVec_get(fn_stmt->data.fn.clauses, 0);
    ASSERT_EQ(c1.params->len, 1);
    ASSERT_EQ(PatternVec_get(c1.params, 0)->type, PATTERN_LIT);
    ASSERT_EQ(PatternVec_get(c1.params, 0)->data.literal->type, EXPR_STRING_LIT);

    /* Second clause: greet(name) -> "Hello" */
    FunctionClause c2 = FunctionClauseVec_get(fn_stmt->data.fn.clauses, 1);
    ASSERT_EQ(c2.params->len, 1);
    ASSERT_EQ(PatternVec_get(c2.params, 0)->type, PATTERN_IDENT);

    arena_destroy(arena);
}

/* Test: Parse simple with expression (single binding, no else) */
void test_parse_with_simple(void) {
    Arena* arena = arena_create(4096);
    Parser* parser = parser_new(arena, "with x <- f() do Ok(x)");

    Expr* expr = parse_expr(parser);
    ASSERT_NOT_NULL(expr);
    ASSERT_EQ(expr->type, EXPR_WITH);
    ASSERT_NOT_NULL(expr->data.with_expr.bindings);
    ASSERT_EQ(expr->data.with_expr.bindings->len, 1);

    // Check binding: x <- f()
    WithBinding b1 = WithBindingVec_get(expr->data.with_expr.bindings, 0);
    ASSERT_STR_EQ(string_cstr(b1.name), "x");
    ASSERT_NOT_NULL(b1.value);
    ASSERT_EQ(b1.value->type, EXPR_CALL);

    // Check do body: Ok(x)
    ASSERT_NOT_NULL(expr->data.with_expr.body);
    ASSERT_EQ(expr->data.with_expr.body->type, EXPR_CALL);

    // No else clause
    ASSERT_NULL(expr->data.with_expr.else_arms);

    arena_destroy(arena);
}

/* Test: Parse with expression with multiple bindings */
void test_parse_with_multiple_bindings(void) {
    Arena* arena = arena_create(4096);
    Parser* parser = parser_new(arena, "with x <- f(), y <- g(x) do Ok(y)");

    Expr* expr = parse_expr(parser);
    ASSERT_NOT_NULL(expr);
    ASSERT_EQ(expr->type, EXPR_WITH);
    ASSERT_NOT_NULL(expr->data.with_expr.bindings);
    ASSERT_EQ(expr->data.with_expr.bindings->len, 2);

    // First binding: x <- f()
    WithBinding b1 = WithBindingVec_get(expr->data.with_expr.bindings, 0);
    ASSERT_STR_EQ(string_cstr(b1.name), "x");
    ASSERT_EQ(b1.value->type, EXPR_CALL);

    // Second binding: y <- g(x)
    WithBinding b2 = WithBindingVec_get(expr->data.with_expr.bindings, 1);
    ASSERT_STR_EQ(string_cstr(b2.name), "y");
    ASSERT_EQ(b2.value->type, EXPR_CALL);

    // Check do body: Ok(y)
    ASSERT_NOT_NULL(expr->data.with_expr.body);
    ASSERT_EQ(expr->data.with_expr.body->type, EXPR_CALL);

    arena_destroy(arena);
}

/* Test: Parse with expression with else clause */
void test_parse_with_else_clause(void) {
    Arena* arena = arena_create(4096);
    Parser* parser = parser_new(arena, "with x <- f() do Ok(x) else Err(e) -> e");

    Expr* expr = parse_expr(parser);
    ASSERT_NOT_NULL(expr);
    ASSERT_EQ(expr->type, EXPR_WITH);
    ASSERT_EQ(expr->data.with_expr.bindings->len, 1);

    // Check do body
    ASSERT_NOT_NULL(expr->data.with_expr.body);
    ASSERT_EQ(expr->data.with_expr.body->type, EXPR_CALL);

    // Check else arms
    ASSERT_NOT_NULL(expr->data.with_expr.else_arms);
    ASSERT_EQ(expr->data.with_expr.else_arms->len, 1);

    // First else arm: Err(e) -> e
    MatchArm arm = MatchArmVec_get(expr->data.with_expr.else_arms, 0);
    ASSERT_NOT_NULL(arm.pattern);
    ASSERT_NOT_NULL(arm.body);
    ASSERT_EQ(arm.body->type, EXPR_IDENT);

    arena_destroy(arena);
}

/* Test: Parse with expression inside a block */
void test_parse_with_in_block(void) {
    Arena* arena = arena_create(4096);
    Parser* parser = parser_new(arena, "{ let z = with x <- f() do Ok(x), z }");

    Expr* expr = parse_expr(parser);
    ASSERT_NOT_NULL(expr);
    ASSERT_EQ(expr->type, EXPR_BLOCK);
    ASSERT_EQ(expr->data.block.stmts->len, 1);

    // The let statement should bind a with expression
    Stmt* let_stmt = StmtVec_get(expr->data.block.stmts, 0);
    ASSERT_EQ(let_stmt->type, STMT_LET);
    ASSERT_EQ(let_stmt->data.let.value->type, EXPR_WITH);

    // Final expression should be the identifier z
    ASSERT_NOT_NULL(expr->data.block.final_expr);
    ASSERT_EQ(expr->data.block.final_expr->type, EXPR_IDENT);

    arena_destroy(arena);
}

/* Test: Parse public function definition */
void test_parse_pub_function(void) {
    Arena* arena = arena_create(4096);
    Parser* parser = parser_new(arena, "pub fn add(x: Int, y: Int) -> Int: x + y");

    Stmt* stmt = parse_stmt(parser);
    ASSERT_NOT_NULL(stmt);
    ASSERT_EQ(stmt->type, STMT_FN);
    ASSERT_STR_EQ(string_cstr(stmt->data.fn.name), "add");
    ASSERT_TRUE(stmt->data.fn.is_public);

    // Should still parse params and body correctly
    ASSERT_NOT_NULL(stmt->data.fn.params);
    ASSERT_EQ(stmt->data.fn.params->len, 2);
    ASSERT_NOT_NULL(stmt->data.fn.body);
    ASSERT_EQ(stmt->data.fn.body->type, EXPR_BINARY);

    arena_destroy(arena);
}

/* Test: Parse private function (default, no pub keyword) */
void test_parse_private_function(void) {
    Arena* arena = arena_create(4096);
    Parser* parser = parser_new(arena, "fn helper() -> Int: 42");

    Stmt* stmt = parse_stmt(parser);
    ASSERT_NOT_NULL(stmt);
    ASSERT_EQ(stmt->type, STMT_FN);
    ASSERT_STR_EQ(string_cstr(stmt->data.fn.name), "helper");
    ASSERT_FALSE(stmt->data.fn.is_public);

    arena_destroy(arena);
}

/* Test: Parse import of entire module */
void test_parse_import_module(void) {
    Arena* arena = arena_create(4096);
    Parser* parser = parser_new(arena, "import math.geometry");

    Stmt* stmt = parse_stmt(parser);
    ASSERT_NOT_NULL(stmt);
    ASSERT_EQ(stmt->type, STMT_IMPORT);

    // Module path should be "math.geometry"
    ASSERT_NOT_NULL(stmt->data.import.path);
    ASSERT_EQ(stmt->data.import.path->len, 2);
    ASSERT_STR_EQ(string_cstr(StringVec_get(stmt->data.import.path, 0)), "math");
    ASSERT_STR_EQ(string_cstr(StringVec_get(stmt->data.import.path, 1)), "geometry");

    // No specific items, no alias
    ASSERT_NULL(stmt->data.import.items);
    ASSERT_NULL(stmt->data.import.alias);

    arena_destroy(arena);
}

/* Test: Parse import with specific items */
void test_parse_import_items(void) {
    Arena* arena = arena_create(4096);
    Parser* parser = parser_new(arena, "import http.server.{cors, auth}");

    Stmt* stmt = parse_stmt(parser);
    ASSERT_NOT_NULL(stmt);
    ASSERT_EQ(stmt->type, STMT_IMPORT);

    // Module path should be "http.server"
    ASSERT_NOT_NULL(stmt->data.import.path);
    ASSERT_EQ(stmt->data.import.path->len, 2);
    ASSERT_STR_EQ(string_cstr(StringVec_get(stmt->data.import.path, 0)), "http");
    ASSERT_STR_EQ(string_cstr(StringVec_get(stmt->data.import.path, 1)), "server");

    // Should have 2 specific items
    ASSERT_NOT_NULL(stmt->data.import.items);
    ASSERT_EQ(stmt->data.import.items->len, 2);
    ASSERT_STR_EQ(string_cstr(StringVec_get(stmt->data.import.items, 0)), "cors");
    ASSERT_STR_EQ(string_cstr(StringVec_get(stmt->data.import.items, 1)), "auth");

    // No alias
    ASSERT_NULL(stmt->data.import.alias);

    arena_destroy(arena);
}

/* Test: Parse import with alias */
void test_parse_import_alias(void) {
    Arena* arena = arena_create(4096);
    Parser* parser = parser_new(arena, "import math.geometry as geo");

    Stmt* stmt = parse_stmt(parser);
    ASSERT_NOT_NULL(stmt);
    ASSERT_EQ(stmt->type, STMT_IMPORT);

    // Module path should be "math.geometry"
    ASSERT_NOT_NULL(stmt->data.import.path);
    ASSERT_EQ(stmt->data.import.path->len, 2);
    ASSERT_STR_EQ(string_cstr(StringVec_get(stmt->data.import.path, 0)), "math");
    ASSERT_STR_EQ(string_cstr(StringVec_get(stmt->data.import.path, 1)), "geometry");

    // No specific items
    ASSERT_NULL(stmt->data.import.items);

    // Should have alias "geo"
    ASSERT_NOT_NULL(stmt->data.import.alias);
    ASSERT_STR_EQ(string_cstr(stmt->data.import.alias), "geo");

    arena_destroy(arena);
}

/* Test: Parse simple defer statement */
void test_parse_defer_simple(void) {
    Arena* arena = arena_create(4096);
    Parser* parser = parser_new(arena, "defer close(file)");

    Stmt* stmt = parse_stmt(parser);
    ASSERT_NOT_NULL(stmt);
    ASSERT_EQ(stmt->type, STMT_DEFER);
    ASSERT_NOT_NULL(stmt->data.defer_stmt.expr);
    ASSERT_EQ(stmt->data.defer_stmt.expr->type, EXPR_CALL);

    // The call should be close(file)
    ASSERT_EQ(stmt->data.defer_stmt.expr->data.call.func->type, EXPR_IDENT);
    ASSERT_STR_EQ(string_cstr(stmt->data.defer_stmt.expr->data.call.func->data.ident.name), "close");
    ASSERT_EQ(stmt->data.defer_stmt.expr->data.call.args->len, 1);

    arena_destroy(arena);
}

/* Test: Parse defer with function call argument */
void test_parse_defer_with_call(void) {
    Arena* arena = arena_create(4096);
    Parser* parser = parser_new(arena, "defer cleanup_resource(handle)");

    Stmt* stmt = parse_stmt(parser);
    ASSERT_NOT_NULL(stmt);
    ASSERT_EQ(stmt->type, STMT_DEFER);
    ASSERT_NOT_NULL(stmt->data.defer_stmt.expr);
    ASSERT_EQ(stmt->data.defer_stmt.expr->type, EXPR_CALL);

    // The call should be cleanup_resource(handle)
    ASSERT_EQ(stmt->data.defer_stmt.expr->data.call.func->type, EXPR_IDENT);
    ASSERT_STR_EQ(string_cstr(stmt->data.defer_stmt.expr->data.call.func->data.ident.name), "cleanup_resource");
    ASSERT_EQ(stmt->data.defer_stmt.expr->data.call.args->len, 1);

    arena_destroy(arena);
}

/* Test: Parse defer inside block expression */
void test_parse_defer_in_block(void) {
    Arena* arena = arena_create(4096);
    Parser* parser = parser_new(arena, "{ file <- open(), defer close(file), read(file) }");

    Expr* expr = parse_expr(parser);
    ASSERT_NOT_NULL(expr);
    ASSERT_EQ(expr->type, EXPR_BLOCK);
    ASSERT_NOT_NULL(expr->data.block.stmts);
    ASSERT_EQ(expr->data.block.stmts->len, 2);

    // First statement: file <- open() (bind expression statement)
    Stmt* bind_stmt = StmtVec_get(expr->data.block.stmts, 0);
    ASSERT_EQ(bind_stmt->type, STMT_EXPR);
    ASSERT_EQ(bind_stmt->data.expr.expr->type, EXPR_BIND);

    // Second statement: defer close(file)
    Stmt* defer_stmt = StmtVec_get(expr->data.block.stmts, 1);
    ASSERT_EQ(defer_stmt->type, STMT_DEFER);
    ASSERT_NOT_NULL(defer_stmt->data.defer_stmt.expr);
    ASSERT_EQ(defer_stmt->data.defer_stmt.expr->type, EXPR_CALL);

    // Final expression: read(file)
    ASSERT_NOT_NULL(expr->data.block.final_expr);
    ASSERT_EQ(expr->data.block.final_expr->type, EXPR_CALL);

    arena_destroy(arena);
}

/* Test: Parse multiple defer statements in sequence */
void test_parse_defer_multiple(void) {
    Arena* arena = arena_create(4096);
    Parser* parser = parser_new(arena,
        "{ defer release1(r1), defer release2(r2), compute() }");

    Expr* expr = parse_expr(parser);
    ASSERT_NOT_NULL(expr);
    ASSERT_EQ(expr->type, EXPR_BLOCK);
    ASSERT_NOT_NULL(expr->data.block.stmts);
    ASSERT_EQ(expr->data.block.stmts->len, 2);

    // First statement: defer release1(r1)
    Stmt* defer1 = StmtVec_get(expr->data.block.stmts, 0);
    ASSERT_EQ(defer1->type, STMT_DEFER);
    ASSERT_EQ(defer1->data.defer_stmt.expr->type, EXPR_CALL);
    ASSERT_STR_EQ(string_cstr(defer1->data.defer_stmt.expr->data.call.func->data.ident.name), "release1");

    // Second statement: defer release2(r2)
    Stmt* defer2 = StmtVec_get(expr->data.block.stmts, 1);
    ASSERT_EQ(defer2->type, STMT_DEFER);
    ASSERT_EQ(defer2->data.defer_stmt.expr->type, EXPR_CALL);
    ASSERT_STR_EQ(string_cstr(defer2->data.defer_stmt.expr->data.call.func->data.ident.name), "release2");

    // Final expression: compute()
    ASSERT_NOT_NULL(expr->data.block.final_expr);
    ASSERT_EQ(expr->data.block.final_expr->type, EXPR_CALL);

    arena_destroy(arena);
}

/* Test: Parse float literal */
void test_parse_float_literal(void) {
    Arena* arena = arena_create(4096);
    Parser* parser = parser_new(arena, "3.14");

    Expr* expr = parse_primary(parser);
    ASSERT_NOT_NULL(expr);
    ASSERT_EQ(expr->type, EXPR_FLOAT_LIT);
    ASSERT_TRUE(expr->data.float_lit.value == 3.14);

    arena_destroy(arena);
}

/* Test: Parse float in binary expression */
void test_parse_float_in_expr(void) {
    Arena* arena = arena_create(4096);
    Parser* parser = parser_new(arena, "x + 3.14");

    Expr* expr = parse_expr(parser);
    ASSERT_NOT_NULL(expr);
    ASSERT_EQ(expr->type, EXPR_BINARY);
    ASSERT_EQ(expr->data.binary.op, BINOP_ADD);
    ASSERT_EQ(expr->data.binary.left->type, EXPR_IDENT);
    ASSERT_EQ(expr->data.binary.right->type, EXPR_FLOAT_LIT);
    ASSERT_TRUE(expr->data.binary.right->data.float_lit.value == 3.14);

    arena_destroy(arena);
}

/* Test: Parse simple sum type definition */
void test_parse_type_def_simple(void) {
    Arena* arena = arena_create(4096);
    Parser* parser = parser_new(arena, "type Status:\n    Active\n    Inactive");

    StmtVec* stmts = parse_stmts(parser);
    ASSERT_NOT_NULL(stmts);
    ASSERT_EQ(stmts->len, 1);

    Stmt* stmt = StmtVec_get(stmts, 0);
    ASSERT_EQ(stmt->type, STMT_TYPE_DEF);
    ASSERT_STR_EQ(string_cstr(stmt->data.type_def.name), "Status");
    ASSERT_NULL(stmt->data.type_def.type_params); // No type params
    ASSERT_EQ(stmt->data.type_def.variants->len, 2);

    // First variant: Active (no fields)
    TypeVariant v1 = TypeVariantVec_get(stmt->data.type_def.variants, 0);
    ASSERT_STR_EQ(string_cstr(v1.name), "Active");
    ASSERT_NULL(v1.fields);

    // Second variant: Inactive (no fields)
    TypeVariant v2 = TypeVariantVec_get(stmt->data.type_def.variants, 1);
    ASSERT_STR_EQ(string_cstr(v2.name), "Inactive");
    ASSERT_NULL(v2.fields);

    arena_destroy(arena);
}

/* Test: Parse sum type with variant fields */
void test_parse_type_def_with_fields(void) {
    Arena* arena = arena_create(4096);
    Parser* parser = parser_new(arena, "type Shape:\n    Circle(radius: Float)\n    Rect(w: Int, h: Int)");

    StmtVec* stmts = parse_stmts(parser);
    ASSERT_NOT_NULL(stmts);
    ASSERT_EQ(stmts->len, 1);

    Stmt* stmt = StmtVec_get(stmts, 0);
    ASSERT_EQ(stmt->type, STMT_TYPE_DEF);
    ASSERT_STR_EQ(string_cstr(stmt->data.type_def.name), "Shape");
    ASSERT_EQ(stmt->data.type_def.variants->len, 2);

    // Circle(radius: Float) — 1 field
    TypeVariant v1 = TypeVariantVec_get(stmt->data.type_def.variants, 0);
    ASSERT_STR_EQ(string_cstr(v1.name), "Circle");
    ASSERT_NOT_NULL(v1.fields);
    ASSERT_EQ(v1.fields->len, 1);

    // Rect(w: Int, h: Int) — 2 fields
    TypeVariant v2 = TypeVariantVec_get(stmt->data.type_def.variants, 1);
    ASSERT_STR_EQ(string_cstr(v2.name), "Rect");
    ASSERT_NOT_NULL(v2.fields);
    ASSERT_EQ(v2.fields->len, 2);

    arena_destroy(arena);
}

/* Test: Parse parameterized type definition */
void test_parse_type_def_parameterized(void) {
    Arena* arena = arena_create(4096);
    Parser* parser = parser_new(arena, "type Option(a):\n    Some(a)\n    None");

    StmtVec* stmts = parse_stmts(parser);
    ASSERT_NOT_NULL(stmts);
    ASSERT_EQ(stmts->len, 1);

    Stmt* stmt = StmtVec_get(stmts, 0);
    ASSERT_EQ(stmt->type, STMT_TYPE_DEF);
    ASSERT_STR_EQ(string_cstr(stmt->data.type_def.name), "Option");

    // Type parameters: (a)
    ASSERT_NOT_NULL(stmt->data.type_def.type_params);
    ASSERT_EQ(stmt->data.type_def.type_params->len, 1);

    // Variants: Some(a), None
    ASSERT_EQ(stmt->data.type_def.variants->len, 2);

    TypeVariant v1 = TypeVariantVec_get(stmt->data.type_def.variants, 0);
    ASSERT_STR_EQ(string_cstr(v1.name), "Some");

    TypeVariant v2 = TypeVariantVec_get(stmt->data.type_def.variants, 1);
    ASSERT_STR_EQ(string_cstr(v2.name), "None");
    ASSERT_NULL(v2.fields);

    arena_destroy(arena);
}

/* Test: Parse record type definition */
void test_parse_type_def_record(void) {
    Arena* arena = arena_create(4096);
    Parser* parser = parser_new(arena, "type User:\n    name: String\n    age: Int");

    StmtVec* stmts = parse_stmts(parser);
    ASSERT_NOT_NULL(stmts);
    ASSERT_EQ(stmts->len, 1);

    Stmt* stmt = StmtVec_get(stmts, 0);
    ASSERT_EQ(stmt->type, STMT_TYPE_DEF);
    ASSERT_STR_EQ(string_cstr(stmt->data.type_def.name), "User");

    // Record types have fields (stored as variants with a single variant matching the type name,
    // or as a special record_fields list)
    // For now, we represent record fields as a single variant with the type name
    ASSERT_NOT_NULL(stmt->data.type_def.record_fields);
    ASSERT_EQ(stmt->data.type_def.record_fields->len, 2);

    arena_destroy(arena);
}

/* Test: Parse pub type definition */
void test_parse_type_def_pub(void) {
    Arena* arena = arena_create(4096);
    Parser* parser = parser_new(arena, "pub type Color:\n    Red\n    Green\n    Blue");

    StmtVec* stmts = parse_stmts(parser);
    ASSERT_NOT_NULL(stmts);
    ASSERT_EQ(stmts->len, 1);

    Stmt* stmt = StmtVec_get(stmts, 0);
    ASSERT_EQ(stmt->type, STMT_TYPE_DEF);
    ASSERT_TRUE(stmt->data.type_def.is_public);
    ASSERT_STR_EQ(string_cstr(stmt->data.type_def.name), "Color");
    ASSERT_EQ(stmt->data.type_def.variants->len, 3);

    arena_destroy(arena);
}

/* Test: Parse for loop */
void test_parse_for_loop(void) {
    Arena* arena = arena_create(4096);
    Parser* parser = parser_new(arena, "for item in items: process(item)");

    Expr* expr = parse_expr(parser);
    ASSERT_NOT_NULL(expr);
    ASSERT_EQ(expr->type, EXPR_FOR);
    ASSERT_STR_EQ(string_cstr(expr->data.for_loop.var_name), "item");
    ASSERT_EQ(expr->data.for_loop.iterable->type, EXPR_IDENT);
    ASSERT_EQ(expr->data.for_loop.body->type, EXPR_CALL);

    arena_destroy(arena);
}

/* Test: Parse while loop */
void test_parse_while_loop(void) {
    Arena* arena = arena_create(4096);
    Parser* parser = parser_new(arena, "while x < 10: process(x)");

    Expr* expr = parse_expr(parser);
    ASSERT_NOT_NULL(expr);
    ASSERT_EQ(expr->type, EXPR_WHILE);
    ASSERT_EQ(expr->data.while_loop.condition->type, EXPR_BINARY);
    ASSERT_EQ(expr->data.while_loop.body->type, EXPR_CALL);

    arena_destroy(arena);
}

/* Test: Parse infinite loop */
void test_parse_loop(void) {
    Arena* arena = arena_create(4096);
    Parser* parser = parser_new(arena, "loop: process()");

    Expr* expr = parse_expr(parser);
    ASSERT_NOT_NULL(expr);
    ASSERT_EQ(expr->type, EXPR_LOOP);
    ASSERT_EQ(expr->data.loop.body->type, EXPR_CALL);

    arena_destroy(arena);
}

/* Test: Parse break statement */
void test_parse_break(void) {
    Arena* arena = arena_create(4096);
    Parser* parser = parser_new(arena, "break");

    Stmt* stmt = parse_stmt(parser);
    ASSERT_NOT_NULL(stmt);
    ASSERT_EQ(stmt->type, STMT_BREAK);
    ASSERT_NULL(stmt->data.break_stmt.value);

    arena_destroy(arena);
}

/* Test: Parse break with value */
void test_parse_break_with_value(void) {
    Arena* arena = arena_create(4096);
    Parser* parser = parser_new(arena, "break 42");

    Stmt* stmt = parse_stmt(parser);
    ASSERT_NOT_NULL(stmt);
    ASSERT_EQ(stmt->type, STMT_BREAK);
    ASSERT_NOT_NULL(stmt->data.break_stmt.value);
    ASSERT_EQ(stmt->data.break_stmt.value->type, EXPR_INT_LIT);

    arena_destroy(arena);
}

/* Test: Parse continue statement */
void test_parse_continue(void) {
    Arena* arena = arena_create(4096);
    Parser* parser = parser_new(arena, "continue");

    Stmt* stmt = parse_stmt(parser);
    ASSERT_NOT_NULL(stmt);
    ASSERT_EQ(stmt->type, STMT_CONTINUE);

    arena_destroy(arena);
}

/* Test: Parse range expression (exclusive) */
/* Test: Parse inclusive range ..= */
void test_parse_range_inclusive(void) {
    Arena* arena = arena_create(4096);
    Parser* parser = parser_new(arena, "0..=10");

    Expr* expr = parse_expr(parser);
    ASSERT_NOT_NULL(expr);
    ASSERT_EQ(expr->type, EXPR_RANGE);
    ASSERT_EQ(expr->data.range.start->data.int_lit.value, 0);
    ASSERT_EQ(expr->data.range.end->data.int_lit.value, 10);
    ASSERT_TRUE(expr->data.range.inclusive);

    arena_destroy(arena);
}

void test_parse_range_exclusive(void) {
    Arena* arena = arena_create(4096);
    Parser* parser = parser_new(arena, "0..10");

    Expr* expr = parse_expr(parser);
    ASSERT_NOT_NULL(expr);
    ASSERT_EQ(expr->type, EXPR_RANGE);
    ASSERT_EQ(expr->data.range.start->type, EXPR_INT_LIT);
    ASSERT_EQ(expr->data.range.start->data.int_lit.value, 0);
    ASSERT_EQ(expr->data.range.end->type, EXPR_INT_LIT);
    ASSERT_EQ(expr->data.range.end->data.int_lit.value, 10);
    ASSERT_FALSE(expr->data.range.inclusive);

    arena_destroy(arena);
}

/* Test: Parse range in for loop */
void test_parse_for_range(void) {
    Arena* arena = arena_create(4096);
    Parser* parser = parser_new(arena, "for i in 0..5: process(i)");

    Expr* expr = parse_expr(parser);
    ASSERT_NOT_NULL(expr);
    ASSERT_EQ(expr->type, EXPR_FOR);
    ASSERT_EQ(expr->data.for_loop.iterable->type, EXPR_RANGE);
    ASSERT_FALSE(expr->data.for_loop.iterable->data.range.inclusive);

    arena_destroy(arena);
}

/* Test: Parse trait definition */
void test_parse_trait_def(void) {
    Arena* arena = arena_create(4096);
    Parser* parser = parser_new(arena, "trait Show(a):\n    fn show(a: a) -> String: \"\"");

    StmtVec* stmts = parse_stmts(parser);
    ASSERT_NOT_NULL(stmts);
    ASSERT_EQ(stmts->len, 1);

    Stmt* stmt = StmtVec_get(stmts, 0);
    ASSERT_EQ(stmt->type, STMT_TRAIT);
    ASSERT_STR_EQ(string_cstr(stmt->data.trait_def.name), "Show");
    ASSERT_NOT_NULL(stmt->data.trait_def.type_params);
    ASSERT_EQ(stmt->data.trait_def.type_params->len, 1);
    ASSERT_EQ(stmt->data.trait_def.methods->len, 1);

    arena_destroy(arena);
}

/* Test: Parse impl block */
void test_parse_impl_block(void) {
    Arena* arena = arena_create(4096);
    Parser* parser = parser_new(arena, "impl Show(Point):\n    fn show(p: Point) -> String: \"point\"");

    StmtVec* stmts = parse_stmts(parser);
    ASSERT_NOT_NULL(stmts);
    ASSERT_EQ(stmts->len, 1);

    Stmt* stmt = StmtVec_get(stmts, 0);
    ASSERT_EQ(stmt->type, STMT_IMPL);
    ASSERT_STR_EQ(string_cstr(stmt->data.impl_def.trait_name), "Show");
    ASSERT_NOT_NULL(stmt->data.impl_def.type_args);
    ASSERT_EQ(stmt->data.impl_def.type_args->len, 1);
    ASSERT_EQ(stmt->data.impl_def.methods->len, 1);

    arena_destroy(arena);
}

/* Test: Parse trait with multiple methods */
void test_parse_trait_multiple_methods(void) {
    Arena* arena = arena_create(4096);
    Parser* parser = parser_new(arena,
        "trait Eq(a):\n"
        "    fn eq(x: a, y: a) -> Bool: false\n"
        "    fn neq(x: a, y: a) -> Bool: true");

    StmtVec* stmts = parse_stmts(parser);
    ASSERT_NOT_NULL(stmts);
    ASSERT_EQ(stmts->len, 1);

    Stmt* stmt = StmtVec_get(stmts, 0);
    ASSERT_EQ(stmt->type, STMT_TRAIT);
    ASSERT_EQ(stmt->data.trait_def.methods->len, 2);

    arena_destroy(arena);
}

/* Test: Parse labeled function arguments */
void test_parse_call_labeled_args(void) {
    Arena* arena = arena_create(4096);
    Parser* parser = parser_new(arena, "connect(host: \"localhost\", port: 8080)");

    Expr* expr = parse_expr(parser);
    ASSERT_NOT_NULL(expr);
    ASSERT_EQ(expr->type, EXPR_CALL);
    ASSERT_EQ(expr->data.call.args->len, 2);

    CallArg arg1 = CallArgVec_get(expr->data.call.args, 0);
    ASSERT_NOT_NULL(arg1.label);
    ASSERT_STR_EQ(string_cstr(arg1.label), "host");
    ASSERT_EQ(arg1.value->type, EXPR_STRING_LIT);

    CallArg arg2 = CallArgVec_get(expr->data.call.args, 1);
    ASSERT_NOT_NULL(arg2.label);
    ASSERT_STR_EQ(string_cstr(arg2.label), "port");
    ASSERT_EQ(arg2.value->type, EXPR_INT_LIT);

    arena_destroy(arena);
}

/* Test: Parse mixed positional and labeled args */
void test_parse_call_mixed_args(void) {
    Arena* arena = arena_create(4096);
    Parser* parser = parser_new(arena, "f(1, 2, name: \"test\")");

    Expr* expr = parse_expr(parser);
    ASSERT_NOT_NULL(expr);
    ASSERT_EQ(expr->type, EXPR_CALL);
    ASSERT_EQ(expr->data.call.args->len, 3);

    // First two are positional (no label)
    CallArg a1 = CallArgVec_get(expr->data.call.args, 0);
    ASSERT_NULL(a1.label);

    CallArg a2 = CallArgVec_get(expr->data.call.args, 1);
    ASSERT_NULL(a2.label);

    // Third is labeled
    CallArg a3 = CallArgVec_get(expr->data.call.args, 2);
    ASSERT_NOT_NULL(a3.label);
    ASSERT_STR_EQ(string_cstr(a3.label), "name");

    arena_destroy(arena);
}

/* Test: Parse dot access */
void test_parse_dot_access(void) {
    Arena* arena = arena_create(4096);
    Parser* parser = parser_new(arena, "user.name");

    Expr* expr = parse_expr(parser);
    ASSERT_NOT_NULL(expr);
    ASSERT_EQ(expr->type, EXPR_DOT);
    ASSERT_EQ(expr->data.dot.object->type, EXPR_IDENT);
    ASSERT_STR_EQ(string_cstr(expr->data.dot.field), "name");

    arena_destroy(arena);
}

/* Test: Parse chained dot access */
void test_parse_dot_chain(void) {
    Arena* arena = arena_create(4096);
    Parser* parser = parser_new(arena, "a.b.c");

    Expr* expr = parse_expr(parser);
    ASSERT_NOT_NULL(expr);
    ASSERT_EQ(expr->type, EXPR_DOT);
    ASSERT_STR_EQ(string_cstr(expr->data.dot.field), "c");
    ASSERT_EQ(expr->data.dot.object->type, EXPR_DOT);
    ASSERT_STR_EQ(string_cstr(expr->data.dot.object->data.dot.field), "b");

    arena_destroy(arena);
}

/* Test: Parse method call (dot + call) */
void test_parse_method_call(void) {
    Arena* arena = arena_create(4096);
    Parser* parser = parser_new(arena, "list.len()");

    Expr* expr = parse_expr(parser);
    ASSERT_NOT_NULL(expr);
    ASSERT_EQ(expr->type, EXPR_CALL);
    ASSERT_EQ(expr->data.call.func->type, EXPR_DOT);
    ASSERT_STR_EQ(string_cstr(expr->data.call.func->data.dot.field), "len");

    arena_destroy(arena);
}

/* Test: Parse return with postfix guard */
void test_parse_return_postfix_if(void) {
    Arena* arena = arena_create(4096);
    Parser* parser = parser_new(arena, "return 0 if x == 0");

    Stmt* stmt = parse_stmt(parser);
    ASSERT_NOT_NULL(stmt);
    ASSERT_EQ(stmt->type, STMT_RETURN);
    ASSERT_NOT_NULL(stmt->data.return_stmt.value);
    ASSERT_EQ(stmt->data.return_stmt.value->type, EXPR_INT_LIT);
    ASSERT_NOT_NULL(stmt->data.return_stmt.condition);
    ASSERT_EQ(stmt->data.return_stmt.condition->type, EXPR_BINARY);

    arena_destroy(arena);
}

/* Test: Return without postfix guard has NULL condition */
void test_parse_return_no_guard(void) {
    Arena* arena = arena_create(4096);
    Parser* parser = parser_new(arena, "return 42");

    Stmt* stmt = parse_stmt(parser);
    ASSERT_NOT_NULL(stmt);
    ASSERT_EQ(stmt->type, STMT_RETURN);
    ASSERT_NOT_NULL(stmt->data.return_stmt.value);
    ASSERT_NULL(stmt->data.return_stmt.condition);

    arena_destroy(arena);
}

/* Test: Parse constructor pattern in match */
void test_parse_match_constructor(void) {
    Arena* arena = arena_create(4096);
    Parser* parser = parser_new(arena, "match x: Some(v) -> v, None -> 0");

    Expr* expr = parse_expr(parser);
    ASSERT_NOT_NULL(expr);
    ASSERT_EQ(expr->type, EXPR_MATCH);
    ASSERT_EQ(expr->data.match_expr.arms->len, 2);
    // First arm: Some(v) — constructor pattern
    ASSERT_EQ(expr->data.match_expr.arms->data[0].pattern->type, PATTERN_CONSTRUCTOR);
    ASSERT_STR_EQ(string_cstr(expr->data.match_expr.arms->data[0].pattern->data.constructor.name), "Some");
    ASSERT_EQ(expr->data.match_expr.arms->data[0].pattern->data.constructor.args->len, 1);
    // Sub-pattern is an identifier
    ASSERT_EQ(expr->data.match_expr.arms->data[0].pattern->data.constructor.args->data[0]->type, PATTERN_IDENT);
    // Second arm: None — plain identifier pattern
    ASSERT_EQ(expr->data.match_expr.arms->data[1].pattern->type, PATTERN_IDENT);

    arena_destroy(arena);
}

/* Test: Parse nested constructor pattern */
void test_parse_match_nested_constructor(void) {
    Arena* arena = arena_create(4096);
    Parser* parser = parser_new(arena, "match x: Ok(Some(v)) -> v, _ -> 0");

    Expr* expr = parse_expr(parser);
    ASSERT_NOT_NULL(expr);
    ASSERT_EQ(expr->type, EXPR_MATCH);
    // Ok(Some(v)) — nested constructor
    Pattern* outer = expr->data.match_expr.arms->data[0].pattern;
    ASSERT_EQ(outer->type, PATTERN_CONSTRUCTOR);
    ASSERT_STR_EQ(string_cstr(outer->data.constructor.name), "Ok");
    Pattern* inner = outer->data.constructor.args->data[0];
    ASSERT_EQ(inner->type, PATTERN_CONSTRUCTOR);
    ASSERT_STR_EQ(string_cstr(inner->data.constructor.name), "Some");

    arena_destroy(arena);
}

/* Test: Parse match with guard */
void test_parse_match_guard(void) {
    Arena* arena = arena_create(4096);
    Parser* parser = parser_new(arena, "match x: n if n > 0 -> n, _ -> 0");

    Expr* expr = parse_expr(parser);
    ASSERT_NOT_NULL(expr);
    ASSERT_EQ(expr->type, EXPR_MATCH);
    ASSERT_EQ(expr->data.match_expr.arms->len, 2);
    // First arm has a guard
    ASSERT_NOT_NULL(expr->data.match_expr.arms->data[0].guard);
    ASSERT_EQ(expr->data.match_expr.arms->data[0].guard->type, EXPR_BINARY);
    // Second arm has no guard
    ASSERT_NULL(expr->data.match_expr.arms->data[1].guard);

    arena_destroy(arena);
}

/* Test: Parse tuple literal */
void test_parse_tuple(void) {
    Arena* arena = arena_create(4096);
    Parser* parser = parser_new(arena, "(1, 2, 3)");

    Expr* expr = parse_expr(parser);
    ASSERT_NOT_NULL(expr);
    ASSERT_EQ(expr->type, EXPR_TUPLE);
    ASSERT_EQ(expr->data.tuple.elements->len, 3);
    ASSERT_EQ(expr->data.tuple.elements->data[0]->type, EXPR_INT_LIT);
    ASSERT_EQ(expr->data.tuple.elements->data[1]->type, EXPR_INT_LIT);
    ASSERT_EQ(expr->data.tuple.elements->data[2]->type, EXPR_INT_LIT);

    arena_destroy(arena);
}

/* Test: Parse two-element tuple */
void test_parse_tuple_pair(void) {
    Arena* arena = arena_create(4096);
    Parser* parser = parser_new(arena, "(10, 20)");

    Expr* expr = parse_expr(parser);
    ASSERT_NOT_NULL(expr);
    ASSERT_EQ(expr->type, EXPR_TUPLE);
    ASSERT_EQ(expr->data.tuple.elements->len, 2);

    arena_destroy(arena);
}

/* Test: Grouped expression (not tuple) stays as grouped */
void test_parse_grouped_not_tuple(void) {
    Arena* arena = arena_create(4096);
    Parser* parser = parser_new(arena, "(42)");

    Expr* expr = parse_expr(parser);
    ASSERT_NOT_NULL(expr);
    // (42) is a grouped expression, not a tuple
    ASSERT_EQ(expr->type, EXPR_INT_LIT);
    ASSERT_EQ(expr->data.int_lit.value, 42);

    arena_destroy(arena);
}

/* Test: Parse empty map literal */
void test_parse_map_empty(void) {
    Arena* arena = arena_create(4096);
    Parser* parser = parser_new(arena, "%{}");

    Expr* expr = parse_expr(parser);
    ASSERT_NOT_NULL(expr);
    ASSERT_EQ(expr->type, EXPR_MAP);
    ASSERT_EQ(expr->data.map.entries->len, 0);

    arena_destroy(arena);
}

/* Test: Parse map literal with entries */
void test_parse_map_literal(void) {
    Arena* arena = arena_create(4096);
    Parser* parser = parser_new(arena, "%{\"name\": \"Alice\", \"age\": 30}");

    Expr* expr = parse_expr(parser);
    ASSERT_NOT_NULL(expr);
    ASSERT_EQ(expr->type, EXPR_MAP);
    ASSERT_EQ(expr->data.map.entries->len, 2);
    ASSERT_EQ(expr->data.map.entries->data[0].key->type, EXPR_STRING_LIT);
    ASSERT_EQ(expr->data.map.entries->data[0].value->type, EXPR_STRING_LIT);
    ASSERT_EQ(expr->data.map.entries->data[1].key->type, EXPR_STRING_LIT);
    ASSERT_EQ(expr->data.map.entries->data[1].value->type, EXPR_INT_LIT);

    arena_destroy(arena);
}

/* Test: Parse interpolated string */
void test_parse_interp_string(void) {
    Arena* arena = arena_create(4096);
    Parser* parser = parser_new(arena, "\"Hello, {name}!\"");

    Expr* expr = parse_expr(parser);
    ASSERT_NOT_NULL(expr);
    ASSERT_EQ(expr->type, EXPR_INTERP_STRING);
    // Parts: "Hello, ", name, "!"
    ASSERT_EQ(expr->data.interp_string.parts->len, 3);
    ASSERT_EQ(expr->data.interp_string.parts->data[0]->type, EXPR_STRING_LIT);
    ASSERT_EQ(expr->data.interp_string.parts->data[1]->type, EXPR_IDENT);
    ASSERT_EQ(expr->data.interp_string.parts->data[2]->type, EXPR_STRING_LIT);

    arena_destroy(arena);
}

/* Test: Parse interpolated string with expression */
void test_parse_interp_string_expr(void) {
    Arena* arena = arena_create(4096);
    Parser* parser = parser_new(arena, "\"result: {1 + 2}\"");

    Expr* expr = parse_expr(parser);
    ASSERT_NOT_NULL(expr);
    ASSERT_EQ(expr->type, EXPR_INTERP_STRING);
    // Parts: "result: ", (1 + 2), ""
    ASSERT_EQ(expr->data.interp_string.parts->len, 3);
    ASSERT_EQ(expr->data.interp_string.parts->data[1]->type, EXPR_BINARY);

    arena_destroy(arena);
}

/* Test: Parse interpolated string with multiple interpolations */
void test_parse_interp_string_multi(void) {
    Arena* arena = arena_create(4096);
    Parser* parser = parser_new(arena, "\"{a} and {b}\"");

    Expr* expr = parse_expr(parser);
    ASSERT_NOT_NULL(expr);
    ASSERT_EQ(expr->type, EXPR_INTERP_STRING);
    // Parts: "", a, " and ", b, ""
    ASSERT_EQ(expr->data.interp_string.parts->len, 5);
    ASSERT_EQ(expr->data.interp_string.parts->data[0]->type, EXPR_STRING_LIT);
    ASSERT_EQ(expr->data.interp_string.parts->data[1]->type, EXPR_IDENT);
    ASSERT_EQ(expr->data.interp_string.parts->data[2]->type, EXPR_STRING_LIT);
    ASSERT_EQ(expr->data.interp_string.parts->data[3]->type, EXPR_IDENT);
    ASSERT_EQ(expr->data.interp_string.parts->data[4]->type, EXPR_STRING_LIT);

    arena_destroy(arena);
}

/* Test: Parse modulo operator */
void test_parse_modulo(void) {
    Arena* arena = arena_create(4096);
    Parser* parser = parser_new(arena, "10 % 3");

    Expr* expr = parse_expr(parser);
    ASSERT_NOT_NULL(expr);
    ASSERT_EQ(expr->type, EXPR_BINARY);
    ASSERT_EQ(expr->data.binary.op, BINOP_MOD);
    ASSERT_EQ(expr->data.binary.left->type, EXPR_INT_LIT);
    ASSERT_EQ(expr->data.binary.right->type, EXPR_INT_LIT);

    arena_destroy(arena);
}

/* Test: Parse power operator */
void test_parse_power(void) {
    Arena* arena = arena_create(4096);
    Parser* parser = parser_new(arena, "2 ** 3");

    Expr* expr = parse_expr(parser);
    ASSERT_NOT_NULL(expr);
    ASSERT_EQ(expr->type, EXPR_BINARY);
    ASSERT_EQ(expr->data.binary.op, BINOP_POW);
    ASSERT_EQ(expr->data.binary.left->type, EXPR_INT_LIT);
    ASSERT_EQ(expr->data.binary.right->type, EXPR_INT_LIT);

    arena_destroy(arena);
}

/* Test: Power is right-associative: 2 ** 3 ** 2 = 2 ** (3 ** 2) */
void test_parse_power_right_assoc(void) {
    Arena* arena = arena_create(4096);
    Parser* parser = parser_new(arena, "2 ** 3 ** 2");

    Expr* expr = parse_expr(parser);
    ASSERT_NOT_NULL(expr);
    ASSERT_EQ(expr->type, EXPR_BINARY);
    ASSERT_EQ(expr->data.binary.op, BINOP_POW);
    // Right side should be (3 ** 2), not flat
    ASSERT_EQ(expr->data.binary.right->type, EXPR_BINARY);
    ASSERT_EQ(expr->data.binary.right->data.binary.op, BINOP_POW);

    arena_destroy(arena);
}

/* Test: Power binds tighter than multiply: 2 * 3 ** 2 = 2 * (3 ** 2) */
void test_parse_power_precedence(void) {
    Arena* arena = arena_create(4096);
    Parser* parser = parser_new(arena, "2 * 3 ** 2");

    Expr* expr = parse_expr(parser);
    ASSERT_NOT_NULL(expr);
    ASSERT_EQ(expr->type, EXPR_BINARY);
    ASSERT_EQ(expr->data.binary.op, BINOP_MUL);
    // Right side should be (3 ** 2)
    ASSERT_EQ(expr->data.binary.right->type, EXPR_BINARY);
    ASSERT_EQ(expr->data.binary.right->data.binary.op, BINOP_POW);

    arena_destroy(arena);
}

/* Test: Parse lambda expression */
void test_parse_lambda(void) {
    Arena* arena = arena_create(4096);
    Parser* parser = parser_new(arena, "(x) -> x * 2");

    Expr* expr = parse_expr(parser);
    ASSERT_NOT_NULL(expr);
    ASSERT_EQ(expr->type, EXPR_LAMBDA);
    ASSERT_EQ(expr->data.lambda.params->len, 1);
    ASSERT_STR_EQ(string_cstr(expr->data.lambda.params->data[0]), "x");
    ASSERT_EQ(expr->data.lambda.body->type, EXPR_BINARY);

    arena_destroy(arena);
}

/* Test: Parse lambda with multiple params */
void test_parse_lambda_multi_params(void) {
    Arena* arena = arena_create(4096);
    Parser* parser = parser_new(arena, "(a, b) -> a + b");

    Expr* expr = parse_expr(parser);
    ASSERT_NOT_NULL(expr);
    ASSERT_EQ(expr->type, EXPR_LAMBDA);
    ASSERT_EQ(expr->data.lambda.params->len, 2);

    arena_destroy(arena);
}

/* Test: Parse lambda in pipe */
void test_parse_lambda_in_pipe(void) {
    Arena* arena = arena_create(4096);
    Parser* parser = parser_new(arena, "items |> map((x) -> x * 2)");

    Expr* expr = parse_expr(parser);
    ASSERT_NOT_NULL(expr);
    ASSERT_EQ(expr->type, EXPR_BINARY);
    ASSERT_EQ(expr->data.binary.op, BINOP_PIPE);
    // RHS is map call with lambda arg
    ASSERT_EQ(expr->data.binary.right->type, EXPR_CALL);

    arena_destroy(arena);
}

/* Test: let (x, y) = point — tuple destructuring */
void test_parse_let_destructure_tuple(void) {
    Arena* arena = arena_create(4096);
    Parser* parser = parser_new(arena, "let (x, y) = point");

    Stmt* stmt = parse_stmt(parser);
    ASSERT_NOT_NULL(stmt);
    ASSERT_EQ(stmt->type, STMT_LET);
    ASSERT_NOT_NULL(stmt->data.let.pattern);
    ASSERT_EQ(stmt->data.let.pattern->type, PATTERN_TUPLE);

    PatternVec* elems = stmt->data.let.pattern->data.tuple;
    ASSERT_EQ(elems->len, 2);
    ASSERT_EQ(PatternVec_get(elems, 0)->type, PATTERN_IDENT);
    ASSERT_STR_EQ(string_cstr(PatternVec_get(elems, 0)->data.ident), "x");
    ASSERT_EQ(PatternVec_get(elems, 1)->type, PATTERN_IDENT);
    ASSERT_STR_EQ(string_cstr(PatternVec_get(elems, 1)->data.ident), "y");

    ASSERT_NOT_NULL(stmt->data.let.value);
    ASSERT_EQ(stmt->data.let.value->type, EXPR_IDENT);

    arena_destroy(arena);
}

/* Test: let Some(v) = option — constructor destructuring */
void test_parse_let_destructure_constructor(void) {
    Arena* arena = arena_create(4096);
    Parser* parser = parser_new(arena, "let Some(v) = option");

    Stmt* stmt = parse_stmt(parser);
    ASSERT_NOT_NULL(stmt);
    ASSERT_EQ(stmt->type, STMT_LET);
    ASSERT_NOT_NULL(stmt->data.let.pattern);
    ASSERT_EQ(stmt->data.let.pattern->type, PATTERN_CONSTRUCTOR);
    ASSERT_STR_EQ(string_cstr(stmt->data.let.pattern->data.constructor.name), "Some");

    PatternVec* args = stmt->data.let.pattern->data.constructor.args;
    ASSERT_EQ(args->len, 1);
    ASSERT_EQ(PatternVec_get(args, 0)->type, PATTERN_IDENT);
    ASSERT_STR_EQ(string_cstr(PatternVec_get(args, 0)->data.ident), "v");

    arena_destroy(arena);
}

/* Test: let (a, b, c) = triple — triple destructuring */
void test_parse_let_destructure_triple(void) {
    Arena* arena = arena_create(4096);
    Parser* parser = parser_new(arena, "let (a, b, c) = triple");

    Stmt* stmt = parse_stmt(parser);
    ASSERT_NOT_NULL(stmt);
    ASSERT_EQ(stmt->type, STMT_LET);
    ASSERT_EQ(stmt->data.let.pattern->type, PATTERN_TUPLE);

    PatternVec* elems = stmt->data.let.pattern->data.tuple;
    ASSERT_EQ(elems->len, 3);
    ASSERT_STR_EQ(string_cstr(PatternVec_get(elems, 0)->data.ident), "a");
    ASSERT_STR_EQ(string_cstr(PatternVec_get(elems, 1)->data.ident), "b");
    ASSERT_STR_EQ(string_cstr(PatternVec_get(elems, 2)->data.ident), "c");

    arena_destroy(arena);
}

/* Test: match: (condition-only, no value) */
void test_parse_match_condition_only(void) {
    Arena* arena = arena_create(4096);
    Parser* parser = parser_new(arena, "match: x > 0 -> \"positive\", x < 0 -> \"negative\", _ -> \"zero\"");

    Expr* expr = parse_expr(parser);
    ASSERT_NOT_NULL(expr);
    ASSERT_EQ(expr->type, EXPR_MATCH);
    // value is NULL for condition-only match
    ASSERT_NULL(expr->data.match_expr.value);

    MatchArmVec* arms = expr->data.match_expr.arms;
    ASSERT_EQ(arms->len, 3);

    // First arm: x > 0 -> "positive" (wildcard pattern + guard)
    MatchArm arm0 = MatchArmVec_get(arms, 0);
    ASSERT_EQ(arm0.pattern->type, PATTERN_WILDCARD);
    ASSERT_NOT_NULL(arm0.guard);
    ASSERT_EQ(arm0.guard->type, EXPR_BINARY);
    ASSERT_EQ(arm0.body->type, EXPR_STRING_LIT);

    // Third arm: _ -> "zero" (wildcard, no guard)
    MatchArm arm2 = MatchArmVec_get(arms, 2);
    ASSERT_EQ(arm2.pattern->type, PATTERN_WILDCARD);
    ASSERT_NULL(arm2.guard);

    arena_destroy(arena);
}

/* Test: match: with complex conditions */
void test_parse_match_condition_complex(void) {
    Arena* arena = arena_create(4096);
    Parser* parser = parser_new(arena, "match: x > 10 and y > 10 -> \"both\", _ -> \"other\"");

    Expr* expr = parse_expr(parser);
    ASSERT_NOT_NULL(expr);
    ASSERT_EQ(expr->type, EXPR_MATCH);
    ASSERT_NULL(expr->data.match_expr.value);

    MatchArmVec* arms = expr->data.match_expr.arms;
    ASSERT_EQ(arms->len, 2);

    // First arm has a complex guard expression
    MatchArm arm0 = MatchArmVec_get(arms, 0);
    ASSERT_EQ(arm0.pattern->type, PATTERN_WILDCARD);
    ASSERT_NOT_NULL(arm0.guard);
    ASSERT_EQ(arm0.body->type, EXPR_STRING_LIT);

    arena_destroy(arena);
}

/* Test: { user | age: 31 } — record update single field */
void test_parse_record_update(void) {
    Arena* arena = arena_create(4096);
    Parser* parser = parser_new(arena, "{ user | age: 31 }");

    Expr* expr = parse_expr(parser);
    ASSERT_NOT_NULL(expr);
    ASSERT_EQ(expr->type, EXPR_RECORD_UPDATE);
    ASSERT_EQ(expr->data.record_update.base->type, EXPR_IDENT);
    ASSERT_STR_EQ(string_cstr(expr->data.record_update.base->data.ident.name), "user");

    RecordFieldVec* fields = expr->data.record_update.fields;
    ASSERT_EQ(fields->len, 1);
    ASSERT_STR_EQ(string_cstr(RecordFieldVec_get(fields, 0).name), "age");
    ASSERT_EQ(RecordFieldVec_get(fields, 0).value->type, EXPR_INT_LIT);

    arena_destroy(arena);
}

/* Test: { user | age: 31, name: "Nik" } — record update multiple fields */
void test_parse_record_update_multi(void) {
    Arena* arena = arena_create(4096);
    Parser* parser = parser_new(arena, "{ user | age: 31, name: \"Nik\" }");

    Expr* expr = parse_expr(parser);
    ASSERT_NOT_NULL(expr);
    ASSERT_EQ(expr->type, EXPR_RECORD_UPDATE);

    RecordFieldVec* fields = expr->data.record_update.fields;
    ASSERT_EQ(fields->len, 2);
    ASSERT_STR_EQ(string_cstr(RecordFieldVec_get(fields, 0).name), "age");
    ASSERT_STR_EQ(string_cstr(RecordFieldVec_get(fields, 1).name), "name");

    arena_destroy(arena);
}

/* Test: return x unless cond — postfix unless on return */
void test_parse_return_postfix_unless(void) {
    Arena* arena = arena_create(4096);
    Parser* parser = parser_new(arena, "return None unless valid");

    Stmt* stmt = parse_stmt(parser);
    ASSERT_NOT_NULL(stmt);
    ASSERT_EQ(stmt->type, STMT_RETURN);
    ASSERT_NOT_NULL(stmt->data.return_stmt.value);

    // unless wraps the condition in a NOT
    ASSERT_NOT_NULL(stmt->data.return_stmt.condition);
    ASSERT_EQ(stmt->data.return_stmt.condition->type, EXPR_UNARY);
    ASSERT_EQ(stmt->data.return_stmt.condition->data.unary.op, UNOP_NOT);
    ASSERT_EQ(stmt->data.return_stmt.condition->data.unary.operand->type, EXPR_IDENT);

    arena_destroy(arena);
}

/* Test: tuple.0 — numeric tuple field access */
void test_parse_tuple_field_access(void) {
    Arena* arena = arena_create(4096);
    Parser* parser = parser_new(arena, "point.0");

    Expr* expr = parse_expr(parser);
    ASSERT_NOT_NULL(expr);
    ASSERT_EQ(expr->type, EXPR_DOT);
    ASSERT_EQ(expr->data.dot.object->type, EXPR_IDENT);
    ASSERT_STR_EQ(string_cstr(expr->data.dot.field), "0");

    arena_destroy(arena);
}

/* Test: tuple.1 chained — pair.0.1 */
void test_parse_tuple_field_chain(void) {
    Arena* arena = arena_create(4096);
    Parser* parser = parser_new(arena, "matrix.0.1");

    Expr* expr = parse_expr(parser);
    ASSERT_NOT_NULL(expr);
    ASSERT_EQ(expr->type, EXPR_DOT);
    ASSERT_STR_EQ(string_cstr(expr->data.dot.field), "1");
    ASSERT_EQ(expr->data.dot.object->type, EXPR_DOT);
    ASSERT_STR_EQ(string_cstr(expr->data.dot.object->data.dot.field), "0");

    arena_destroy(arena);
}

/* Test: [x * x for x in numbers] — list comprehension */
void test_parse_list_comp(void) {
    Arena* arena = arena_create(4096);
    Parser* parser = parser_new(arena, "[x * x for x in numbers]");

    Expr* expr = parse_expr(parser);
    ASSERT_NOT_NULL(expr);
    ASSERT_EQ(expr->type, EXPR_LIST_COMP);
    ASSERT_NOT_NULL(expr->data.list_comp.body);
    ASSERT_EQ(expr->data.list_comp.body->type, EXPR_BINARY);
    ASSERT_STR_EQ(string_cstr(expr->data.list_comp.var_name), "x");
    ASSERT_EQ(expr->data.list_comp.iterable->type, EXPR_IDENT);
    ASSERT_NULL(expr->data.list_comp.condition);

    arena_destroy(arena);
}

/* Test: [x for x in numbers if x % 2 == 0] — list comprehension with filter */
void test_parse_list_comp_filter(void) {
    Arena* arena = arena_create(4096);
    Parser* parser = parser_new(arena, "[x for x in numbers if x % 2 == 0]");

    Expr* expr = parse_expr(parser);
    ASSERT_NOT_NULL(expr);
    ASSERT_EQ(expr->type, EXPR_LIST_COMP);
    ASSERT_STR_EQ(string_cstr(expr->data.list_comp.var_name), "x");
    ASSERT_NOT_NULL(expr->data.list_comp.condition);
    ASSERT_EQ(expr->data.list_comp.condition->type, EXPR_BINARY);

    arena_destroy(arena);
}

/* Test: let Some(x) = val else: fallback — let-else pattern */
void test_parse_let_else(void) {
    Arena* arena = arena_create(4096);
    Parser* parser = parser_new(arena, "let Some(x) = input else: default_val");

    Stmt* stmt = parse_stmt(parser);
    ASSERT_NOT_NULL(stmt);
    ASSERT_EQ(stmt->type, STMT_LET);
    ASSERT_EQ(stmt->data.let.pattern->type, PATTERN_CONSTRUCTOR);
    ASSERT_STR_EQ(string_cstr(stmt->data.let.pattern->data.constructor.name), "Some");
    ASSERT_NOT_NULL(stmt->data.let.value);
    ASSERT_NOT_NULL(stmt->data.let.else_expr);
    ASSERT_EQ(stmt->data.let.else_expr->type, EXPR_IDENT);

    arena_destroy(arena);
}

/* Test: let (a, b) = pair else: fallback — let-else with tuple pattern */
void test_parse_let_else_tuple(void) {
    Arena* arena = arena_create(4096);
    Parser* parser = parser_new(arena, "let (a, b) = val else: default_val");

    Stmt* stmt = parse_stmt(parser);
    ASSERT_NOT_NULL(stmt);
    ASSERT_EQ(stmt->type, STMT_LET);
    ASSERT_EQ(stmt->data.let.pattern->type, PATTERN_TUPLE);
    ASSERT_NOT_NULL(stmt->data.let.else_expr);

    arena_destroy(arena);
}

/* Test: type User derive(Show, Eq): — derive clause */
void test_parse_type_derive(void) {
    Arena* arena = arena_create(4096);
    Parser* parser = parser_new(arena, "type User derive(Show, Eq): name: String, age: Int");

    Stmt* stmt = parse_stmt(parser);
    ASSERT_NOT_NULL(stmt);
    ASSERT_EQ(stmt->type, STMT_TYPE_DEF);
    ASSERT_STR_EQ(string_cstr(stmt->data.type_def.name), "User");
    ASSERT_NOT_NULL(stmt->data.type_def.derives);
    ASSERT_EQ(stmt->data.type_def.derives->len, 2);
    ASSERT_STR_EQ(string_cstr(StringVec_get(stmt->data.type_def.derives, 0)), "Show");
    ASSERT_STR_EQ(string_cstr(StringVec_get(stmt->data.type_def.derives, 1)), "Eq");
    ASSERT_NOT_NULL(stmt->data.type_def.record_fields);

    arena_destroy(arena);
}

/* Test: type without derive — derives is NULL */
void test_parse_type_no_derive(void) {
    Arena* arena = arena_create(4096);
    Parser* parser = parser_new(arena, "type Color: Red, Green, Blue");

    Stmt* stmt = parse_stmt(parser);
    ASSERT_NOT_NULL(stmt);
    ASSERT_EQ(stmt->type, STMT_TYPE_DEF);
    ASSERT_NULL(stmt->data.type_def.derives);

    arena_destroy(arena);
}

/* Test: fn sort(items: List(a)) -> List(a) where Ord(a): body */
void test_parse_fn_where_clause(void) {
    Arena* arena = arena_create(4096);
    Parser* parser = parser_new(arena, "fn sort(items: List(a)) -> List(a) where Ord(a): items");

    Stmt* stmt = parse_stmt(parser);
    ASSERT_NOT_NULL(stmt);
    ASSERT_EQ(stmt->type, STMT_FN);
    ASSERT_STR_EQ(string_cstr(stmt->data.fn.name), "sort");
    ASSERT_NOT_NULL(stmt->data.fn.return_type);
    ASSERT_NOT_NULL(stmt->data.fn.where_clauses);
    ASSERT_EQ(stmt->data.fn.where_clauses->len, 1);

    arena_destroy(arena);
}

/* Test: fn with multiple where constraints */
void test_parse_fn_where_multi(void) {
    Arena* arena = arena_create(4096);
    Parser* parser = parser_new(arena, "fn display(items: List(a)) -> () where Ord(a), Show(a): items");

    Stmt* stmt = parse_stmt(parser);
    ASSERT_NOT_NULL(stmt);
    ASSERT_EQ(stmt->type, STMT_FN);
    ASSERT_NOT_NULL(stmt->data.fn.where_clauses);
    ASSERT_EQ(stmt->data.fn.where_clauses->len, 2);

    arena_destroy(arena);
}

/* Test: fn without where clause — where_clauses is NULL */
void test_parse_fn_no_where(void) {
    Arena* arena = arena_create(4096);
    Parser* parser = parser_new(arena, "fn add(x: Int, y: Int) -> Int: x");

    Stmt* stmt = parse_stmt(parser);
    ASSERT_NOT_NULL(stmt);
    ASSERT_EQ(stmt->type, STMT_FN);
    ASSERT_NULL(stmt->data.fn.where_clauses);

    arena_destroy(arena);
}

void test_parse_trait_with_constraint(void) {
    Arena* arena = arena_create(4096);
    Parser* parser = parser_new(arena, "trait Ord(a) with Eq(a): fn compare(x: a, y: a) -> Int: 0");

    Stmt* stmt = parse_stmt(parser);
    ASSERT_NOT_NULL(stmt);
    ASSERT_EQ(stmt->type, STMT_TRAIT);
    ASSERT_STR_EQ(string_cstr(stmt->data.trait_def.name), "Ord");
    ASSERT_NOT_NULL(stmt->data.trait_def.constraints);
    ASSERT_EQ(stmt->data.trait_def.constraints->len, (size_t)1);

    arena_destroy(arena);
}

void test_parse_trait_multi_constraints(void) {
    Arena* arena = arena_create(4096);
    Parser* parser = parser_new(arena, "trait Sortable(a) with Eq(a), Ord(a): fn sort(x: a) -> a: x");

    Stmt* stmt = parse_stmt(parser);
    ASSERT_NOT_NULL(stmt);
    ASSERT_EQ(stmt->type, STMT_TRAIT);
    ASSERT_STR_EQ(string_cstr(stmt->data.trait_def.name), "Sortable");
    ASSERT_NOT_NULL(stmt->data.trait_def.constraints);
    ASSERT_EQ(stmt->data.trait_def.constraints->len, (size_t)2);

    arena_destroy(arena);
}

void test_parse_trait_no_constraint(void) {
    Arena* arena = arena_create(4096);
    Parser* parser = parser_new(arena, "trait Show(a): fn show(x: a) -> String: x");

    Stmt* stmt = parse_stmt(parser);
    ASSERT_NOT_NULL(stmt);
    ASSERT_EQ(stmt->type, STMT_TRAIT);
    ASSERT_STR_EQ(string_cstr(stmt->data.trait_def.name), "Show");
    ASSERT_NULL(stmt->data.trait_def.constraints);

    arena_destroy(arena);
}

void test_parse_rest_pattern(void) {
    Arena* arena = arena_create(4096);
    Parser* parser = parser_new(arena, "let [first, ..rest] = items");

    Stmt* stmt = parse_stmt(parser);
    ASSERT_NOT_NULL(stmt);
    ASSERT_EQ(stmt->type, STMT_LET);
    // Pattern should be a tuple with 2 elements: first, ..rest
    ASSERT_EQ(stmt->data.let.pattern->type, PATTERN_TUPLE);
    ASSERT_EQ(stmt->data.let.pattern->data.tuple->len, (size_t)2);
    Pattern* second = PatternVec_get(stmt->data.let.pattern->data.tuple, 1);
    ASSERT_EQ(second->type, PATTERN_REST);
    ASSERT_NOT_NULL(second->data.rest_name);
    ASSERT_STR_EQ(string_cstr(second->data.rest_name), "rest");

    arena_destroy(arena);
}

void test_parse_rest_pattern_ignore(void) {
    Arena* arena = arena_create(4096);
    Parser* parser = parser_new(arena, "let [first, .._] = items");

    Stmt* stmt = parse_stmt(parser);
    ASSERT_NOT_NULL(stmt);
    ASSERT_EQ(stmt->type, STMT_LET);
    ASSERT_EQ(stmt->data.let.pattern->type, PATTERN_TUPLE);
    ASSERT_EQ(stmt->data.let.pattern->data.tuple->len, (size_t)2);
    Pattern* second = PatternVec_get(stmt->data.let.pattern->data.tuple, 1);
    ASSERT_EQ(second->type, PATTERN_REST);
    ASSERT_NULL(second->data.rest_name);

    arena_destroy(arena);
}

void test_parse_spawn(void) {
    Arena* arena = arena_create(4096);
    Parser* parser = parser_new(arena, "spawn(worker_loop)");

    Expr* expr = parse_expr(parser);
    ASSERT_NOT_NULL(expr);
    ASSERT_EQ(expr->type, EXPR_SPAWN);
    ASSERT_NOT_NULL(expr->data.spawn_expr.func);
    ASSERT_EQ(expr->data.spawn_expr.func->type, EXPR_IDENT);

    arena_destroy(arena);
}

void test_parse_send(void) {
    Arena* arena = arena_create(4096);
    Parser* parser = parser_new(arena, "send(pid, Request(\"get\", \"/users\"))");

    Expr* expr = parse_expr(parser);
    ASSERT_NOT_NULL(expr);
    ASSERT_EQ(expr->type, EXPR_SEND);
    ASSERT_NOT_NULL(expr->data.send_expr.pid);
    ASSERT_EQ(expr->data.send_expr.pid->type, EXPR_IDENT);
    ASSERT_NOT_NULL(expr->data.send_expr.message);
    ASSERT_EQ(expr->data.send_expr.message->type, EXPR_CALL);

    arena_destroy(arena);
}

void test_parse_receive(void) {
    Arena* arena = arena_create(4096);
    Parser* parser = parser_new(arena, "receive: Ping -> pong(), Shutdown -> cleanup()");

    Expr* expr = parse_expr(parser);
    ASSERT_NOT_NULL(expr);
    ASSERT_EQ(expr->type, EXPR_RECEIVE);
    ASSERT_EQ(expr->data.receive_expr.arms->len, (size_t)2);
    ASSERT_NULL(expr->data.receive_expr.after_timeout);
    ASSERT_NULL(expr->data.receive_expr.after_body);

    arena_destroy(arena);
}

void test_parse_receive_after(void) {
    Arena* arena = arena_create(4096);
    Parser* parser = parser_new(arena, "receive: Msg(x) -> handle(x), _ after 5000 -> timeout()");

    Expr* expr = parse_expr(parser);
    ASSERT_NOT_NULL(expr);
    ASSERT_EQ(expr->type, EXPR_RECEIVE);
    ASSERT_EQ(expr->data.receive_expr.arms->len, (size_t)1);
    ASSERT_NOT_NULL(expr->data.receive_expr.after_timeout);
    ASSERT_NOT_NULL(expr->data.receive_expr.after_body);

    arena_destroy(arena);
}

void test_parse_newtype(void) {
    Arena* arena = arena_create(4096);
    Parser* parser = parser_new(arena, "newtype UserId = UserId(Int)");

    Stmt* stmt = parse_stmt(parser);
    ASSERT_NOT_NULL(stmt);
    ASSERT_EQ(stmt->type, STMT_NEWTYPE);
    ASSERT_STR_EQ(string_cstr(stmt->data.newtype_def.name), "UserId");
    ASSERT_EQ(stmt->data.newtype_def.is_public, false);
    ASSERT_STR_EQ(string_cstr(stmt->data.newtype_def.constructor), "UserId");
    ASSERT_NOT_NULL(stmt->data.newtype_def.inner_type);

    arena_destroy(arena);
}

void test_parse_pub_newtype(void) {
    Arena* arena = arena_create(4096);
    Parser* parser = parser_new(arena, "pub newtype Email = Email(String)");

    Stmt* stmt = parse_stmt(parser);
    ASSERT_NOT_NULL(stmt);
    ASSERT_EQ(stmt->type, STMT_NEWTYPE);
    ASSERT_STR_EQ(string_cstr(stmt->data.newtype_def.name), "Email");
    ASSERT_EQ(stmt->data.newtype_def.is_public, true);
    ASSERT_STR_EQ(string_cstr(stmt->data.newtype_def.constructor), "Email");
    ASSERT_NOT_NULL(stmt->data.newtype_def.inner_type);

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
    TEST_RUN(test_parse_type_int);
    TEST_RUN(test_parse_type_string);
    TEST_RUN(test_parse_type_bool);
    TEST_RUN(test_parse_type_custom);
    TEST_RUN(test_parse_type_result);
    TEST_RUN(test_parse_type_list);
    TEST_RUN(test_parse_type_function);
    TEST_RUN(test_parse_function_no_params);
    TEST_RUN(test_parse_function_with_params);
    TEST_RUN(test_parse_function_with_body);
    TEST_RUN(test_parse_pattern_int_literal);
    TEST_RUN(test_parse_pattern_string_literal);
    TEST_RUN(test_parse_pattern_bool_literal);
    TEST_RUN(test_parse_pattern_wildcard);
    TEST_RUN(test_parse_pattern_identifier);
    TEST_RUN(test_parse_let_with_type_int);
    TEST_RUN(test_parse_let_with_type_string);
    TEST_RUN(test_parse_let_with_type_parameterized);
    TEST_RUN(test_parse_let_with_type_function);
    TEST_RUN(test_parse_let_without_type);
    TEST_RUN(test_parse_function_multi_clause_simple);
    TEST_RUN(test_parse_function_multi_clause_fibonacci);
    TEST_RUN(test_parse_function_clauses_must_be_adjacent);
    TEST_RUN(test_parse_function_pattern_params);
    TEST_RUN(test_parse_with_simple);
    TEST_RUN(test_parse_with_multiple_bindings);
    TEST_RUN(test_parse_with_else_clause);
    TEST_RUN(test_parse_with_in_block);
    TEST_RUN(test_parse_pub_function);
    TEST_RUN(test_parse_private_function);
    TEST_RUN(test_parse_import_module);
    TEST_RUN(test_parse_import_items);
    TEST_RUN(test_parse_import_alias);
    TEST_RUN(test_parse_defer_simple);
    TEST_RUN(test_parse_defer_with_call);
    TEST_RUN(test_parse_defer_in_block);
    TEST_RUN(test_parse_defer_multiple);
    TEST_RUN(test_parse_float_literal);
    TEST_RUN(test_parse_float_in_expr);
    TEST_RUN(test_parse_type_def_simple);
    TEST_RUN(test_parse_type_def_with_fields);
    TEST_RUN(test_parse_type_def_parameterized);
    TEST_RUN(test_parse_type_def_record);
    TEST_RUN(test_parse_type_def_pub);
    TEST_RUN(test_parse_for_loop);
    TEST_RUN(test_parse_while_loop);
    TEST_RUN(test_parse_loop);
    TEST_RUN(test_parse_break);
    TEST_RUN(test_parse_break_with_value);
    TEST_RUN(test_parse_continue);
    TEST_RUN(test_parse_range_inclusive);
    TEST_RUN(test_parse_range_exclusive);
    TEST_RUN(test_parse_for_range);
    TEST_RUN(test_parse_trait_def);
    TEST_RUN(test_parse_impl_block);
    TEST_RUN(test_parse_trait_multiple_methods);
    TEST_RUN(test_parse_call_labeled_args);
    TEST_RUN(test_parse_call_mixed_args);
    TEST_RUN(test_parse_dot_access);
    TEST_RUN(test_parse_dot_chain);
    TEST_RUN(test_parse_method_call);
    TEST_RUN(test_parse_return_postfix_if);
    TEST_RUN(test_parse_return_no_guard);
    TEST_RUN(test_parse_match_constructor);
    TEST_RUN(test_parse_match_nested_constructor);
    TEST_RUN(test_parse_match_guard);
    TEST_RUN(test_parse_tuple);
    TEST_RUN(test_parse_tuple_pair);
    TEST_RUN(test_parse_grouped_not_tuple);
    TEST_RUN(test_parse_map_empty);
    TEST_RUN(test_parse_map_literal);
    TEST_RUN(test_parse_interp_string);
    TEST_RUN(test_parse_interp_string_expr);
    TEST_RUN(test_parse_interp_string_multi);
    TEST_RUN(test_parse_modulo);
    TEST_RUN(test_parse_power);
    TEST_RUN(test_parse_power_right_assoc);
    TEST_RUN(test_parse_power_precedence);
    TEST_RUN(test_parse_lambda);
    TEST_RUN(test_parse_lambda_multi_params);
    TEST_RUN(test_parse_lambda_in_pipe);
    TEST_RUN(test_parse_let_destructure_tuple);
    TEST_RUN(test_parse_let_destructure_constructor);
    TEST_RUN(test_parse_let_destructure_triple);
    TEST_RUN(test_parse_match_condition_only);
    TEST_RUN(test_parse_match_condition_complex);
    TEST_RUN(test_parse_record_update);
    TEST_RUN(test_parse_record_update_multi);
    TEST_RUN(test_parse_return_postfix_unless);
    TEST_RUN(test_parse_tuple_field_access);
    TEST_RUN(test_parse_tuple_field_chain);
    TEST_RUN(test_parse_list_comp);
    TEST_RUN(test_parse_list_comp_filter);
    TEST_RUN(test_parse_let_else);
    TEST_RUN(test_parse_let_else_tuple);
    TEST_RUN(test_parse_type_derive);
    TEST_RUN(test_parse_type_no_derive);
    TEST_RUN(test_parse_fn_where_clause);
    TEST_RUN(test_parse_fn_where_multi);
    TEST_RUN(test_parse_fn_no_where);
    TEST_RUN(test_parse_newtype);
    TEST_RUN(test_parse_pub_newtype);
    TEST_RUN(test_parse_trait_with_constraint);
    TEST_RUN(test_parse_trait_multi_constraints);
    TEST_RUN(test_parse_trait_no_constraint);
    TEST_RUN(test_parse_rest_pattern);
    TEST_RUN(test_parse_rest_pattern_ignore);
    TEST_RUN(test_parse_spawn);
    TEST_RUN(test_parse_send);
    TEST_RUN(test_parse_receive);
    TEST_RUN(test_parse_receive_after);
}
