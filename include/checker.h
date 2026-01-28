/* Fern Type Checker
 *
 * Performs type inference and type checking for Fern programs.
 * Reports type errors and enforces Result/Option handling.
 */

#ifndef FERN_CHECKER_H
#define FERN_CHECKER_H

#include "arena.h"
#include "ast.h"
#include "type.h"
#include "type_env.h"
#include <stdbool.h>

/* Forward declaration */
typedef struct Checker Checker;

/* ========== Checker Creation ========== */

/* Create a new type checker */
Checker* checker_new(Arena* arena);

/* ========== Type Inference ========== */

/* Infer the type of an expression */
Type* checker_infer_expr(Checker* checker, Expr* expr);

/* Check a statement (returns success) */
bool checker_check_stmt(Checker* checker, Stmt* stmt);

/* Check a list of statements (e.g., a program) */
bool checker_check_stmts(Checker* checker, StmtVec* stmts);

/* ========== Error Handling ========== */

/* Check if the checker has recorded any errors */
bool checker_has_errors(Checker* checker);

/* Get the first error message (or NULL if none) */
const char* checker_first_error(Checker* checker);

/* Get all error messages */
/* StringVec* checker_errors(Checker* checker); */

/* ========== Environment Access ========== */

/* Get the type environment (for testing/debugging) */
TypeEnv* checker_env(Checker* checker);

/* Define a variable in the current scope */
void checker_define(Checker* checker, String* name, Type* type);

/* Push a new scope */
void checker_push_scope(Checker* checker);

/* Pop the current scope */
void checker_pop_scope(Checker* checker);

#endif /* FERN_CHECKER_H */
