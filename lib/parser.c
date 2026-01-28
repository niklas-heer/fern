#include "parser.h"
#include <stdio.h>
#include <stdlib.h>

// Forward declarations for expression parsing with precedence
static Expr* parse_expression(Parser* parser);
static Expr* parse_pipe(Parser* parser);
static Expr* parse_logical_or(Parser* parser);
static Expr* parse_logical_and(Parser* parser);
static Expr* parse_equality(Parser* parser);
static Expr* parse_comparison(Parser* parser);
static Expr* parse_term(Parser* parser);
static Expr* parse_factor(Parser* parser);
static Expr* parse_unary(Parser* parser);
static Expr* parse_call(Parser* parser);
static Expr* parse_primary_internal(Parser* parser);

// Helper functions
static void advance(Parser* parser);
static bool check(Parser* parser, TokenType type);
static bool match(Parser* parser, TokenType type);
static Token consume(Parser* parser, TokenType type, const char* message);
static void error_at_current(Parser* parser, const char* message);

// Parser lifecycle
Parser* parser_new(Arena* arena, const char* source) {
    Parser* parser = arena_alloc(arena, sizeof(Parser));
    parser->arena = arena;
    parser->lexer = lexer_new(arena, source);
    parser->had_error = false;
    parser->panic_mode = false;
    
    // Initialize with first token
    advance(parser);
    
    return parser;
}

bool parser_had_error(Parser* parser) {
    return parser->had_error;
}

// Helper functions
static void advance(Parser* parser) {
    parser->previous = parser->current;
    parser->current = lexer_next(parser->lexer);
}

static bool check(Parser* parser, TokenType type) {
    return parser->current.type == type;
}

static bool match(Parser* parser, TokenType type) {
    if (!check(parser, type)) return false;
    advance(parser);
    return true;
}

static Token consume(Parser* parser, TokenType type, const char* message) {
    if (parser->current.type == type) {
        Token tok = parser->current;
        advance(parser);
        return tok;
    }
    
    error_at_current(parser, message);
    return parser->current;
}

static void error_at_current(Parser* parser, const char* message) {
    if (parser->panic_mode) return;
    parser->panic_mode = true;
    parser->had_error = true;
    
    fprintf(stderr, "[line %zu] Error at '%s': %s\n",
            parser->current.loc.line,
            parser->current.text ? string_cstr(parser->current.text) : "EOF",
            message);
}

// Expression parsing (operator precedence climbing)

Expr* parse_expr(Parser* parser) {
    return parse_expression(parser);
}

static Expr* parse_expression(Parser* parser) {
    return parse_pipe(parser);
}

// Precedence: pipe (lowest)
static Expr* parse_pipe(Parser* parser) {
    Expr* expr = parse_logical_or(parser);
    
    while (match(parser, TOKEN_PIPE)) {
        SourceLoc loc = parser->previous.loc;
        Expr* right = parse_logical_or(parser);
        expr = expr_binary(parser->arena, BINOP_PIPE, expr, right, loc);
    }
    
    return expr;
}

// Precedence: or
static Expr* parse_logical_or(Parser* parser) {
    Expr* expr = parse_logical_and(parser);
    
    while (match(parser, TOKEN_OR)) {
        SourceLoc loc = parser->previous.loc;
        Expr* right = parse_logical_and(parser);
        expr = expr_binary(parser->arena, BINOP_OR, expr, right, loc);
    }
    
    return expr;
}

// Precedence: and
static Expr* parse_logical_and(Parser* parser) {
    Expr* expr = parse_equality(parser);
    
    while (match(parser, TOKEN_AND)) {
        SourceLoc loc = parser->previous.loc;
        Expr* right = parse_equality(parser);
        expr = expr_binary(parser->arena, BINOP_AND, expr, right, loc);
    }
    
    return expr;
}

// Precedence: == !=
static Expr* parse_equality(Parser* parser) {
    Expr* expr = parse_comparison(parser);
    
    while (true) {
        if (match(parser, TOKEN_EQ)) {
            SourceLoc loc = parser->previous.loc;
            Expr* right = parse_comparison(parser);
            expr = expr_binary(parser->arena, BINOP_EQ, expr, right, loc);
        } else if (match(parser, TOKEN_NE)) {
            SourceLoc loc = parser->previous.loc;
            Expr* right = parse_comparison(parser);
            expr = expr_binary(parser->arena, BINOP_NE, expr, right, loc);
        } else {
            break;
        }
    }
    
    return expr;
}

// Precedence: < <= > >=
static Expr* parse_comparison(Parser* parser) {
    Expr* expr = parse_term(parser);
    
    while (true) {
        if (match(parser, TOKEN_LT)) {
            SourceLoc loc = parser->previous.loc;
            Expr* right = parse_term(parser);
            expr = expr_binary(parser->arena, BINOP_LT, expr, right, loc);
        } else if (match(parser, TOKEN_LE)) {
            SourceLoc loc = parser->previous.loc;
            Expr* right = parse_term(parser);
            expr = expr_binary(parser->arena, BINOP_LE, expr, right, loc);
        } else if (match(parser, TOKEN_GT)) {
            SourceLoc loc = parser->previous.loc;
            Expr* right = parse_term(parser);
            expr = expr_binary(parser->arena, BINOP_GT, expr, right, loc);
        } else if (match(parser, TOKEN_GE)) {
            SourceLoc loc = parser->previous.loc;
            Expr* right = parse_term(parser);
            expr = expr_binary(parser->arena, BINOP_GE, expr, right, loc);
        } else {
            break;
        }
    }
    
    return expr;
}

// Precedence: + -
static Expr* parse_term(Parser* parser) {
    Expr* expr = parse_factor(parser);
    
    while (true) {
        if (match(parser, TOKEN_PLUS)) {
            SourceLoc loc = parser->previous.loc;
            Expr* right = parse_factor(parser);
            expr = expr_binary(parser->arena, BINOP_ADD, expr, right, loc);
        } else if (match(parser, TOKEN_MINUS)) {
            SourceLoc loc = parser->previous.loc;
            Expr* right = parse_factor(parser);
            expr = expr_binary(parser->arena, BINOP_SUB, expr, right, loc);
        } else {
            break;
        }
    }
    
    return expr;
}

// Precedence: * /
static Expr* parse_factor(Parser* parser) {
    Expr* expr = parse_unary(parser);
    
    while (true) {
        if (match(parser, TOKEN_STAR)) {
            SourceLoc loc = parser->previous.loc;
            Expr* right = parse_unary(parser);
            expr = expr_binary(parser->arena, BINOP_MUL, expr, right, loc);
        } else if (match(parser, TOKEN_SLASH)) {
            SourceLoc loc = parser->previous.loc;
            Expr* right = parse_unary(parser);
            expr = expr_binary(parser->arena, BINOP_DIV, expr, right, loc);
        } else {
            break;
        }
    }
    
    return expr;
}

// Precedence: unary - !
static Expr* parse_unary(Parser* parser) {
    if (match(parser, TOKEN_MINUS)) {
        SourceLoc loc = parser->previous.loc;
        Expr* operand = parse_unary(parser);
        return expr_unary(parser->arena, UNOP_NEG, operand, loc);
    }
    
    if (match(parser, TOKEN_NOT)) {
        SourceLoc loc = parser->previous.loc;
        Expr* operand = parse_unary(parser);
        return expr_unary(parser->arena, UNOP_NOT, operand, loc);
    }
    
    return parse_call(parser);
}

// Precedence: function calls
static Expr* parse_call(Parser* parser) {
    Expr* expr = parse_primary_internal(parser);
    
    while (match(parser, TOKEN_LPAREN)) {
        SourceLoc loc = parser->previous.loc;
        
        // Parse arguments into temporary array
        Expr** args = NULL;
        size_t arg_count = 0;
        size_t arg_cap = 0;
        
        if (!check(parser, TOKEN_RPAREN)) {
            arg_cap = 4;
            args = arena_alloc(parser->arena, sizeof(Expr*) * arg_cap);
            
            do {
                if (arg_count >= arg_cap) {
                    size_t new_cap = arg_cap * 2;
                    Expr** new_data = arena_alloc(parser->arena, sizeof(Expr*) * new_cap);
                    for (size_t i = 0; i < arg_count; i++) {
                        new_data[i] = args[i];
                    }
                    args = new_data;
                    arg_cap = new_cap;
                }
                args[arg_count++] = parse_expression(parser);
            } while (match(parser, TOKEN_COMMA));
        }
        
        consume(parser, TOKEN_RPAREN, "Expected ')' after arguments");
        
        expr = expr_call(parser->arena, expr, args, arg_count, loc);
    }
    
    return expr;
}

// Primary expressions
Expr* parse_primary(Parser* parser) {
    return parse_primary_internal(parser);
}

static Expr* parse_primary_internal(Parser* parser) {
    // Integer literal
    if (match(parser, TOKEN_INT)) {
        Token tok = parser->previous;
        const char* text = string_cstr(tok.text);
        int64_t value = 0;
        for (const char* p = text; *p; p++) {
            value = value * 10 + (*p - '0');
        }
        return expr_int_lit(parser->arena, value, tok.loc);
    }
    
    // String literal
    if (match(parser, TOKEN_STRING)) {
        Token tok = parser->previous;
        return expr_string_lit(parser->arena, tok.text, tok.loc);
    }
    
    // Boolean literals
    if (match(parser, TOKEN_TRUE)) {
        return expr_bool_lit(parser->arena, true, parser->previous.loc);
    }
    
    if (match(parser, TOKEN_FALSE)) {
        return expr_bool_lit(parser->arena, false, parser->previous.loc);
    }
    
    // Identifier
    if (match(parser, TOKEN_IDENT)) {
        Token tok = parser->previous;
        return expr_ident(parser->arena, tok.text, tok.loc);
    }
    
    // Match expression
    if (match(parser, TOKEN_MATCH)) {
        SourceLoc loc = parser->previous.loc;
        
        Expr* value = parse_expression(parser);
        consume(parser, TOKEN_COLON, "Expected ':' after match value");
        
        // Parse match arms: pattern -> expr, pattern -> expr, ...
        MatchArmVec* arms = MatchArmVec_new(parser->arena);
        
        do {
            // Parse pattern
            Pattern* pattern;
            if (match(parser, TOKEN_UNDERSCORE)) {
                pattern = pattern_wildcard(parser->arena, parser->previous.loc);
            } else {
                // For now, just parse literal patterns
                Expr* pattern_expr = parse_primary_internal(parser);
                pattern = arena_alloc(parser->arena, sizeof(Pattern));
                pattern->type = PATTERN_LIT;
                pattern->loc = pattern_expr->loc;
                pattern->data.literal = pattern_expr;
            }
            
            consume(parser, TOKEN_ARROW, "Expected '->' after match pattern");
            Expr* body = parse_expression(parser);
            
            MatchArm arm;
            arm.pattern = pattern;
            arm.body = body;
            MatchArmVec_push(parser->arena, arms, arm);
            
        } while (match(parser, TOKEN_COMMA));
        
        return expr_match(parser->arena, value, arms, loc);
    }
    
    // If expression
    if (match(parser, TOKEN_IF)) {
        SourceLoc loc = parser->previous.loc;
        
        Expr* condition = parse_expression(parser);
        consume(parser, TOKEN_COLON, "Expected ':' after if condition");
        Expr* then_branch = parse_expression(parser);
        
        Expr* else_branch = NULL;
        if (match(parser, TOKEN_ELSE)) {
            consume(parser, TOKEN_COLON, "Expected ':' after else");
            else_branch = parse_expression(parser);
        }
        
        return expr_if(parser->arena, condition, then_branch, else_branch, loc);
    }
    
    // Grouped expression
    if (match(parser, TOKEN_LPAREN)) {
        Expr* expr = parse_expression(parser);
        consume(parser, TOKEN_RPAREN, "Expected ')' after expression");
        return expr;
    }
    
    error_at_current(parser, "Expected expression");
    // Return dummy expression to avoid crash
    return expr_int_lit(parser->arena, 0, parser->current.loc);
}

// Statement parsing
Stmt* parse_stmt(Parser* parser) {
    // Let statement
    if (match(parser, TOKEN_LET)) {
        SourceLoc loc = parser->previous.loc;
        
        // Parse pattern (for now, just identifier)
        Token name_tok = consume(parser, TOKEN_IDENT, "Expected variable name");
        Pattern* pattern = pattern_ident(parser->arena, name_tok.text, name_tok.loc);
        
        // Optional type annotation (skip for now)
        TypeExpr* type_ann = NULL;
        
        // Expect =
        consume(parser, TOKEN_ASSIGN, "Expected '=' after variable name");
        
        // Parse value
        Expr* value = parse_expression(parser);
        
        return stmt_let(parser->arena, pattern, type_ann, value, loc);
    }
    
    // Return statement
    if (match(parser, TOKEN_RETURN)) {
        SourceLoc loc = parser->previous.loc;
        Expr* value = NULL;
        
        if (!check(parser, TOKEN_EOF) && !check(parser, TOKEN_RBRACE)) {
            value = parse_expression(parser);
        }
        
        return stmt_return(parser->arena, value, loc);
    }
    
    // Expression statement
    Expr* expr = parse_expression(parser);
    return stmt_expr(parser->arena, expr, expr->loc);
}
