/**
 * @file checker.h
 * @brief Fern Type Checker - Type inference and error detection.
 *
 * The type checker is the third stage of the compiler pipeline. It performs
 * Hindley-Milner style type inference and enforces Fern's type rules:
 * - All Result types must be handled (no ignored errors)
 * - All Option types must be unwrapped safely
 * - Function arguments must match parameter types
 *
 * @example
 *     Arena* arena = arena_create(4096);
 *     Parser* parser = parser_new(arena, "let x = 1 + 2");
 *     Expr* expr = parse_expr(parser);
 *
 *     Checker* checker = checker_new(arena);
 *     Type* type = checker_infer_expr(checker, expr);
 *
 *     if (checker_has_errors(checker)) {
 *         printf("Error: %s\n", checker_first_error(checker));
 *     } else {
 *         printf("Type: Int\n");  // Inferred type
 *     }
 *     arena_destroy(arena);
 *
 * @see parser.h, type.h
 */

#ifndef FERN_CHECKER_H
#define FERN_CHECKER_H

#include "arena.h"
#include "ast.h"
#include "type.h"
#include "type_env.h"
#include <stdbool.h>

/** @brief Opaque type checker state. */
typedef struct Checker Checker;

/* ========== Checker Creation ========== */

/**
 * @brief Create a new type checker.
 *
 * Initializes a checker with built-in types (Int, String, Bool, etc.)
 * and an empty type environment.
 *
 * @param arena Memory arena for allocations (must not be NULL)
 * @return New Checker instance
 */
Checker* checker_new(Arena* arena);

/* ========== Type Inference ========== */

/**
 * @brief Infer the type of an expression.
 *
 * Performs type inference on the expression, unifying type variables
 * and checking constraints. Returns TYPE_ERROR on type errors.
 *
 * @param checker Checker instance (must not be NULL)
 * @param expr Expression to type check (must not be NULL)
 * @return Inferred type, or TYPE_ERROR if type checking fails
 *
 * @note Errors are accumulated; call checker_has_errors() to check.
 */
Type* checker_infer_expr(Checker* checker, Expr* expr);

/**
 * @brief Type check a statement.
 *
 * Checks the statement for type errors. For declarations (let, fn),
 * adds bindings to the type environment.
 *
 * @param checker Checker instance (must not be NULL)
 * @param stmt Statement to check (must not be NULL)
 * @return true if no errors, false if type errors occurred
 */
bool checker_check_stmt(Checker* checker, Stmt* stmt);

/**
 * @brief Type check a list of statements (a program).
 *
 * Checks all statements in order, accumulating type bindings.
 *
 * @param checker Checker instance (must not be NULL)
 * @param stmts Statements to check (must not be NULL)
 * @return true if no errors, false if any type errors occurred
 */
bool checker_check_stmts(Checker* checker, StmtVec* stmts);

/* ========== Error Handling ========== */

/**
 * @brief Check if the checker has recorded any errors.
 *
 * @param checker Checker instance (must not be NULL)
 * @return true if errors occurred, false otherwise
 */
bool checker_has_errors(Checker* checker);

/**
 * @brief Get the first error message.
 *
 * @param checker Checker instance (must not be NULL)
 * @return First error message, or NULL if no errors
 */
const char* checker_first_error(Checker* checker);

/**
 * @brief Clear all accumulated errors.
 *
 * Resets the error list so the checker can be reused for a new
 * evaluation (e.g., in a REPL).
 *
 * @param checker Checker instance (must not be NULL)
 */
void checker_clear_errors(Checker* checker);

/* ========== Environment Access ========== */

/**
 * @brief Get the type environment.
 *
 * Useful for testing and debugging. The environment maps variable
 * names to their inferred types.
 *
 * @param checker Checker instance (must not be NULL)
 * @return Type environment
 */
TypeEnv* checker_env(Checker* checker);

/**
 * @brief Define a variable in the current scope.
 *
 * Adds a binding from name to type in the innermost scope.
 *
 * @param checker Checker instance (must not be NULL)
 * @param name Variable name (must not be NULL)
 * @param type Variable type (must not be NULL)
 */
void checker_define(Checker* checker, String* name, Type* type);

/**
 * @brief Push a new scope.
 *
 * Creates a new scope for local bindings. Bindings in inner scopes
 * shadow bindings in outer scopes.
 *
 * @param checker Checker instance (must not be NULL)
 *
 * @see checker_pop_scope
 */
void checker_push_scope(Checker* checker);

/**
 * @brief Pop the current scope.
 *
 * Removes the innermost scope and all its bindings.
 *
 * @param checker Checker instance (must not be NULL)
 *
 * @see checker_push_scope
 */
void checker_pop_scope(Checker* checker);

#endif /* FERN_CHECKER_H */
