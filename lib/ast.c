/* AST Helper Functions */

#include "ast.h"
#include <assert.h>
#include <string.h>

/* Create integer literal expression */
Expr* expr_int_lit(Arena* arena, int64_t value, SourceLoc loc) {
    assert(arena != NULL);
    
    Expr* expr = arena_alloc(arena, sizeof(Expr));
    expr->type = EXPR_INT_LIT;
    expr->loc = loc;
    expr->data.int_lit.value = value;
    
    return expr;
}

/* Create string literal expression */
Expr* expr_string_lit(Arena* arena, String* value, SourceLoc loc) {
    assert(arena != NULL);
    assert(value != NULL);
    
    Expr* expr = arena_alloc(arena, sizeof(Expr));
    expr->type = EXPR_STRING_LIT;
    expr->loc = loc;
    expr->data.string_lit.value = value;
    
    return expr;
}

/* Create boolean literal expression */
Expr* expr_bool_lit(Arena* arena, bool value, SourceLoc loc) {
    assert(arena != NULL);
    
    Expr* expr = arena_alloc(arena, sizeof(Expr));
    expr->type = EXPR_BOOL_LIT;
    expr->loc = loc;
    expr->data.bool_lit.value = value;
    
    return expr;
}

/* Create identifier expression */
Expr* expr_ident(Arena* arena, String* name, SourceLoc loc) {
    assert(arena != NULL);
    assert(name != NULL);
    
    Expr* expr = arena_alloc(arena, sizeof(Expr));
    expr->type = EXPR_IDENT;
    expr->loc = loc;
    expr->data.ident.name = name;
    
    return expr;
}

/* Create binary expression */
Expr* expr_binary(Arena* arena, BinaryOp op, Expr* left, Expr* right, SourceLoc loc) {
    assert(arena != NULL);
    assert(left != NULL);
    assert(right != NULL);
    
    Expr* expr = arena_alloc(arena, sizeof(Expr));
    expr->type = EXPR_BINARY;
    expr->loc = loc;
    expr->data.binary.op = op;
    expr->data.binary.left = left;
    expr->data.binary.right = right;
    
    return expr;
}

/* Create unary expression */
Expr* expr_unary(Arena* arena, UnaryOp op, Expr* operand, SourceLoc loc) {
    assert(arena != NULL);
    assert(operand != NULL);
    
    Expr* expr = arena_alloc(arena, sizeof(Expr));
    expr->type = EXPR_UNARY;
    expr->loc = loc;
    expr->data.unary.op = op;
    expr->data.unary.operand = operand;
    
    return expr;
}

/* Create call expression */
Expr* expr_call(Arena* arena, Expr* func, Expr** args, size_t num_args, SourceLoc loc) {
    assert(arena != NULL);
    assert(func != NULL);
    
    // Create CallArgVec and populate with positional arguments
    CallArgVec* arg_vec = CallArgVec_new(arena);
    for (size_t i = 0; i < num_args; i++) {
        CallArg arg;
        arg.label = NULL;  // Positional argument
        arg.value = args[i];
        CallArgVec_push(arena, arg_vec, arg);
    }
    
    Expr* expr = arena_alloc(arena, sizeof(Expr));
    expr->type = EXPR_CALL;
    expr->loc = loc;
    expr->data.call.func = func;
    expr->data.call.args = arg_vec;
    
    return expr;
}

/* Create if expression */
Expr* expr_if(Arena* arena, Expr* condition, Expr* then_branch, Expr* else_branch, SourceLoc loc) {
    assert(arena != NULL);
    assert(condition != NULL);
    assert(then_branch != NULL);
    // else_branch can be NULL
    
    Expr* expr = arena_alloc(arena, sizeof(Expr));
    expr->type = EXPR_IF;
    expr->loc = loc;
    expr->data.if_expr.condition = condition;
    expr->data.if_expr.then_branch = then_branch;
    expr->data.if_expr.else_branch = else_branch;
    
    return expr;
}

/* Create let statement */
Stmt* stmt_let(Arena* arena, Pattern* pattern, TypeExpr* type_ann, Expr* value, SourceLoc loc) {
    assert(arena != NULL);
    assert(pattern != NULL);
    assert(value != NULL);
    
    Stmt* stmt = arena_alloc(arena, sizeof(Stmt));
    stmt->type = STMT_LET;
    stmt->loc = loc;
    stmt->data.let.pattern = pattern;
    stmt->data.let.type_ann = type_ann;
    stmt->data.let.value = value;
    
    return stmt;
}

/* Create return statement */
Stmt* stmt_return(Arena* arena, Expr* value, SourceLoc loc) {
    assert(arena != NULL);
    
    Stmt* stmt = arena_alloc(arena, sizeof(Stmt));
    stmt->type = STMT_RETURN;
    stmt->loc = loc;
    stmt->data.return_stmt.value = value;
    
    return stmt;
}

/* Create expression statement */
Stmt* stmt_expr(Arena* arena, Expr* expr, SourceLoc loc) {
    assert(arena != NULL);
    assert(expr != NULL);
    
    Stmt* stmt = arena_alloc(arena, sizeof(Stmt));
    stmt->type = STMT_EXPR;
    stmt->loc = loc;
    stmt->data.expr.expr = expr;
    
    return stmt;
}

/* Create identifier pattern */
Pattern* pattern_ident(Arena* arena, String* name, SourceLoc loc) {
    assert(arena != NULL);
    assert(name != NULL);
    
    Pattern* pat = arena_alloc(arena, sizeof(Pattern));
    pat->type = PATTERN_IDENT;
    pat->loc = loc;
    pat->data.ident = name;
    
    return pat;
}

/* Create wildcard pattern */
Pattern* pattern_wildcard(Arena* arena, SourceLoc loc) {
    assert(arena != NULL);
    
    Pattern* pat = arena_alloc(arena, sizeof(Pattern));
    pat->type = PATTERN_WILDCARD;
    pat->loc = loc;
    
    return pat;
}
