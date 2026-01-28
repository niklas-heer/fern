#ifndef PARSER_H
#define PARSER_H

#include "arena.h"
#include "ast.h"
#include "lexer.h"
#include "result.h"

typedef struct Parser {
    Arena* arena;
    Lexer* lexer;
    Token current;
    Token previous;
    bool had_error;
    bool panic_mode;
} Parser;

// Parser lifecycle
Parser* parser_new(Arena* arena, const char* source);

// Expression parsing
Expr* parse_expr(Parser* parser);
Expr* parse_primary(Parser* parser);

// Type parsing
TypeExpr* parse_type(Parser* parser);

// Statement parsing
Stmt* parse_stmt(Parser* parser);

// Helper functions
bool parser_had_error(Parser* parser);

#endif // PARSER_H
