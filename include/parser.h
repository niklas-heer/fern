/**
 * @file parser.h
 * @brief Fern Parser - Transforms tokens into an Abstract Syntax Tree.
 *
 * The parser is the second stage of the compiler pipeline. It reads tokens
 * from the lexer and builds an AST representing the program structure.
 *
 * Uses recursive descent with Pratt parsing for expressions to handle
 * operator precedence correctly.
 *
 * @example
 *     Arena* arena = arena_create(4096);
 *     Parser* parser = parser_new(arena, "let x = 1 + 2");
 *     Expr* expr = parse_expr(parser);
 *     if (parser_had_error(parser)) {
 *         // Handle parse error
 *     }
 *     arena_destroy(arena);
 *
 * @see lexer.h, ast.h
 */

#ifndef PARSER_H
#define PARSER_H

#include "arena.h"
#include "ast.h"
#include "lexer.h"
#include "result.h"

/**
 * @brief Parser state.
 *
 * Tracks the current position in the token stream and error state.
 * Use parser_new() to create, then call parse_* functions.
 */
typedef struct Parser {
    Arena* arena;       /**< Memory arena for AST allocations */
    Lexer* lexer;       /**< Token source */
    const char* source; /**< Source code for error context */
    Token current;      /**< Current token being examined */
    Token previous;     /**< Previously consumed token */
    bool had_error;     /**< True if any parse error occurred */
    bool panic_mode;    /**< True if currently in error recovery */
} Parser;

/**
 * @brief Create a new parser for the given source code.
 *
 * Creates a parser with its own lexer. The parser is ready to parse
 * immediately after creation.
 *
 * @param arena Memory arena for allocations (must not be NULL)
 * @param source Source code to parse (must not be NULL)
 * @return New Parser instance
 *
 * @see parse_expr, parse_stmt
 */
Parser* parser_new(Arena* arena, const char* source);

/**
 * @brief Parse an expression.
 *
 * Parses a single expression using Pratt parsing for correct precedence.
 * Examples: `1 + 2`, `foo(x, y)`, `if cond: a else: b`
 *
 * @param parser Parser instance (must not be NULL)
 * @return Parsed expression AST node
 *
 * @see parse_primary
 */
Expr* parse_expr(Parser* parser);

/**
 * @brief Parse a primary expression.
 *
 * Parses literals, identifiers, and parenthesized expressions.
 * Called by parse_expr() as the base case.
 *
 * @param parser Parser instance (must not be NULL)
 * @return Parsed primary expression
 */
Expr* parse_primary(Parser* parser);

/**
 * @brief Parse a type expression.
 *
 * Parses type annotations like `Int`, `String`, `List(Int)`,
 * `Result(String, Error)`, `(Int, String) -> Bool`.
 *
 * @param parser Parser instance (must not be NULL)
 * @return Parsed type expression
 */
TypeExpr* parse_type(Parser* parser);

/**
 * @brief Parse a single statement.
 *
 * Parses one statement: let, fn, if, match, return, etc.
 *
 * @param parser Parser instance (must not be NULL)
 * @return Parsed statement AST node
 *
 * @see parse_stmts
 */
Stmt* parse_stmt(Parser* parser);

/**
 * @brief Parse multiple statements.
 *
 * Parses statements until EOF. Groups adjacent function clauses
 * with the same name into a single function definition.
 *
 * @param parser Parser instance (must not be NULL)
 * @return Vector of parsed statements
 *
 * @see parse_stmt
 */
StmtVec* parse_stmts(Parser* parser);

/**
 * @brief Check if the parser encountered any errors.
 *
 * @param parser Parser instance (must not be NULL)
 * @return true if any parse error occurred, false otherwise
 */
bool parser_had_error(Parser* parser);

#endif /* PARSER_H */
