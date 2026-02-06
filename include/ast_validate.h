/* AST Validation
 *
 * Functions for validating AST nodes for basic structural invariants.
 * Useful for debugging the parser and catching malformed trees early.
 */

#ifndef FERN_AST_VALIDATE_H
#define FERN_AST_VALIDATE_H

#include "ast.h"

/* Validation error details */
typedef struct {
    const char* message;
    SourceLoc loc;
} AstValidationError;

/* Validate a full program (statement list). */
bool ast_validate_program(const StmtVec* stmts, AstValidationError* err);

/* Validate a single statement. */
bool ast_validate_stmt(const Stmt* stmt, AstValidationError* err);

/* Validate a single expression. */
bool ast_validate_expr(const Expr* expr, AstValidationError* err);

/* Validate a type expression. */
bool ast_validate_type(const TypeExpr* type, AstValidationError* err);

/* Validate a pattern. */
bool ast_validate_pattern(const Pattern* pattern, AstValidationError* err);

#endif /* FERN_AST_VALIDATE_H */
