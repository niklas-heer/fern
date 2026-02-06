/* AST Validation Tests */

#include "test.h"
#include "arena.h"
#include "parser.h"
#include "ast.h"
#include "ast_validate.h"
#include "fern_string.h"

void test_validate_program_ok(void) {
    Arena* arena = arena_create(4096);
    Parser* parser = parser_new(arena, "let x = 1");
    StmtVec* stmts = parse_stmts(parser);

    AstValidationError err = {0};
    ASSERT_TRUE(ast_validate_program(stmts, &err));
    ASSERT_NULL(err.message);

    arena_destroy(arena);
}

void test_validate_expr_binary_missing_left(void) {
    Arena* arena = arena_create(4096);

    Expr* right = expr_int_lit(arena, 1, (SourceLoc){0});
    Expr* expr = arena_alloc(arena, sizeof(Expr));
    expr->type = EXPR_BINARY;
    expr->loc = (SourceLoc){
        .filename = string_new(arena, "test.fn"),
        .line = 3,
        .column = 5,
    };
    expr->data.binary.op = BINOP_ADD;
    expr->data.binary.left = NULL;
    expr->data.binary.right = right;

    AstValidationError err = {0};
    ASSERT_FALSE(ast_validate_expr(expr, &err));
    ASSERT_STR_EQ(err.message, "binary left is NULL");
    ASSERT_EQ(err.loc.line, 3);

    arena_destroy(arena);
}

void test_validate_stmt_fn_missing_params(void) {
    Arena* arena = arena_create(4096);

    Stmt* stmt = arena_alloc(arena, sizeof(Stmt));
    stmt->type = STMT_FN;
    stmt->loc = (SourceLoc){
        .filename = string_new(arena, "test.fn"),
        .line = 1,
        .column = 1,
    };
    stmt->data.fn.name = string_new(arena, "foo");
    stmt->data.fn.is_public = false;
    stmt->data.fn.params = NULL;
    stmt->data.fn.clauses = NULL;
    stmt->data.fn.return_type = NULL;
    stmt->data.fn.where_clauses = NULL;
    stmt->data.fn.body = NULL;

    AstValidationError err = {0};
    ASSERT_FALSE(ast_validate_stmt(stmt, &err));
    ASSERT_STR_EQ(err.message, "function missing params and clauses");
    ASSERT_EQ(err.loc.line, 1);

    arena_destroy(arena);
}

void test_validate_type_fn_missing_return(void) {
    Arena* arena = arena_create(4096);

    TypeExpr* type = arena_alloc(arena, sizeof(TypeExpr));
    type->kind = TYPEEXPR_FUNCTION;
    type->loc = (SourceLoc){
        .filename = string_new(arena, "test.fn"),
        .line = 2,
        .column = 4,
    };
    type->data.func.params = TypeExprVec_new(arena);
    type->data.func.return_type = NULL;

    AstValidationError err = {0};
    ASSERT_FALSE(ast_validate_type(type, &err));
    ASSERT_STR_EQ(err.message, "function type return is NULL");
    ASSERT_EQ(err.loc.line, 2);

    arena_destroy(arena);
}

void run_ast_validate_tests(void) {
    printf("\n=== AST Validation Tests ===\n");
    TEST_RUN(test_validate_program_ok);
    TEST_RUN(test_validate_expr_binary_missing_left);
    TEST_RUN(test_validate_stmt_fn_missing_params);
    TEST_RUN(test_validate_type_fn_missing_return);
}
