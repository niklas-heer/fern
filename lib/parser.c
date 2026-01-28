#include "parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Forward declarations for expression parsing with precedence
static Expr* parse_expression(Parser* parser);
static Expr* parse_pipe(Parser* parser);
static Expr* parse_range(Parser* parser);
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
    Expr* expr = parse_range(parser);
    
    while (match(parser, TOKEN_PIPE)) {
        SourceLoc loc = parser->previous.loc;
        Expr* right = parse_range(parser);
        expr = expr_binary(parser->arena, BINOP_PIPE, expr, right, loc);
    }
    
    return expr;
}

// Precedence: range (above logical, below pipe)
static Expr* parse_range(Parser* parser) {
    Expr* expr = parse_logical_or(parser);

    if (match(parser, TOKEN_DOTDOT)) {
        SourceLoc loc = parser->previous.loc;
        // TODO: check for ..= (inclusive) when TOKEN_DOTDOT_EQ exists
        Expr* end = parse_logical_or(parser);
        expr = expr_range(parser->arena, expr, end, false, loc);
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

    // Float literal
    if (match(parser, TOKEN_FLOAT)) {
        Token tok = parser->previous;
        const char* text = string_cstr(tok.text);
        double value = strtod(text, NULL);
        return expr_float_lit(parser->arena, value, tok.loc);
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
    
    // Identifier (or bind expression: name <- expr)
    if (match(parser, TOKEN_IDENT)) {
        Token tok = parser->previous;

        // Check for bind operator: name <- expression
        if (match(parser, TOKEN_BIND)) {
            Expr* value = parse_expression(parser);
            return expr_bind(parser->arena, tok.text, value, tok.loc);
        }

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
            } else if (check(parser, TOKEN_IDENT)) {
                // Identifier pattern: binding pattern that captures the matched value
                Token ident_tok = parser->current;
                advance(parser);
                pattern = pattern_ident(parser->arena, ident_tok.text, ident_tok.loc);
            } else {
                // Literal patterns: integers, strings, booleans
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
    
    // Block expression
    if (match(parser, TOKEN_LBRACE)) {
        SourceLoc loc = parser->previous.loc;
        
        StmtVec* stmts = StmtVec_new(parser->arena);
        Expr* final_expr = NULL;
        
        // Parse comma-separated statements/expressions: { stmt, stmt, expr }
        while (!check(parser, TOKEN_RBRACE)) {
            // Check if this is a statement (let/return) or expression
            if (check(parser, TOKEN_LET) || check(parser, TOKEN_RETURN) || check(parser, TOKEN_DEFER) ||
                check(parser, TOKEN_BREAK) || check(parser, TOKEN_CONTINUE)) {
                Stmt* stmt = parse_stmt(parser);
                StmtVec_push(parser->arena, stmts, stmt);
                
                // After statement, expect comma or closing brace
                if (!check(parser, TOKEN_RBRACE)) {
                    consume(parser, TOKEN_COMMA, "Expected ',' after statement in block");
                }
            } else {
                // Parse expression
                Expr* expr = parse_expression(parser);
                
                // If followed by comma, it's an expression statement
                if (match(parser, TOKEN_COMMA)) {
                    Stmt* stmt = stmt_expr(parser->arena, expr, expr->loc);
                    StmtVec_push(parser->arena, stmts, stmt);
                } else {
                    // No comma means this is the final expression
                    final_expr = expr;
                    break;
                }
            }
        }
        
        consume(parser, TOKEN_RBRACE, "Expected '}' after block");
        return expr_block(parser->arena, stmts, final_expr, loc);
    }
    
    // List literal
    if (match(parser, TOKEN_LBRACKET)) {
        SourceLoc loc = parser->previous.loc;
        
        ExprVec* elements = ExprVec_new(parser->arena);
        
        // Parse comma-separated expressions: [expr, expr, ...]
        if (!check(parser, TOKEN_RBRACKET)) {
            do {
                Expr* elem = parse_expression(parser);
                ExprVec_push(parser->arena, elements, elem);
            } while (match(parser, TOKEN_COMMA));
        }
        
        consume(parser, TOKEN_RBRACKET, "Expected ']' after list elements");
        return expr_list(parser->arena, elements, loc);
    }
    
    // With expression: with x <- f(), y <- g(x) do body [else pattern -> expr, ...]
    if (match(parser, TOKEN_WITH)) {
        SourceLoc loc = parser->previous.loc;

        // Parse bindings: name <- expr, name <- expr, ...
        WithBindingVec* bindings = WithBindingVec_new(parser->arena);
        do {
            Token name_tok = consume(parser, TOKEN_IDENT, "Expected binding name in with expression");
            consume(parser, TOKEN_BIND, "Expected '<-' after binding name");
            Expr* value = parse_expression(parser);

            WithBinding binding;
            binding.name = name_tok.text;
            binding.value = value;
            WithBindingVec_push(parser->arena, bindings, binding);
        } while (match(parser, TOKEN_COMMA));

        // Parse do body
        consume(parser, TOKEN_DO, "Expected 'do' after with bindings");
        Expr* body = parse_expression(parser);

        // Parse optional else clause with match arms
        MatchArmVec* else_arms = NULL;
        if (match(parser, TOKEN_ELSE)) {
            else_arms = MatchArmVec_new(parser->arena);
            do {
                // Parse pattern
                Pattern* pattern;
                if (match(parser, TOKEN_UNDERSCORE)) {
                    pattern = pattern_wildcard(parser->arena, parser->previous.loc);
                } else if (check(parser, TOKEN_IDENT)) {
                    // Check if this is a constructor call pattern like Err(e)
                    // For now, parse as identifier pattern
                    Token ident_tok = parser->current;
                    advance(parser);
                    // If followed by '(', parse as a literal pattern (constructor call)
                    if (check(parser, TOKEN_LPAREN)) {
                        // Put the identifier back by creating a call expression
                        Expr* func = expr_ident(parser->arena, ident_tok.text, ident_tok.loc);
                        advance(parser); // consume '('
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
                        consume(parser, TOKEN_RPAREN, "Expected ')' after pattern arguments");
                        Expr* call = expr_call(parser->arena, func, args, arg_count, ident_tok.loc);
                        pattern = arena_alloc(parser->arena, sizeof(Pattern));
                        pattern->type = PATTERN_LIT;
                        pattern->loc = ident_tok.loc;
                        pattern->data.literal = call;
                    } else {
                        pattern = pattern_ident(parser->arena, ident_tok.text, ident_tok.loc);
                    }
                } else {
                    Expr* pattern_expr = parse_primary_internal(parser);
                    pattern = arena_alloc(parser->arena, sizeof(Pattern));
                    pattern->type = PATTERN_LIT;
                    pattern->loc = pattern_expr->loc;
                    pattern->data.literal = pattern_expr;
                }

                consume(parser, TOKEN_ARROW, "Expected '->' after else pattern");
                Expr* arm_body = parse_expression(parser);

                MatchArm arm;
                arm.pattern = pattern;
                arm.body = arm_body;
                MatchArmVec_push(parser->arena, else_arms, arm);
            } while (match(parser, TOKEN_COMMA));
        }

        return expr_with(parser->arena, bindings, body, else_arms, loc);
    }

    // For loop: for var in iterable: body
    if (match(parser, TOKEN_FOR)) {
        SourceLoc loc = parser->previous.loc;
        Token var_tok = consume(parser, TOKEN_IDENT, "Expected variable name after 'for'");
        consume(parser, TOKEN_IN, "Expected 'in' after for variable");
        Expr* iterable = parse_expression(parser);
        consume(parser, TOKEN_COLON, "Expected ':' after for iterable");
        Expr* body = parse_expression(parser);
        return expr_for(parser->arena, var_tok.text, iterable, body, loc);
    }

    // While loop: while condition: body
    if (match(parser, TOKEN_WHILE)) {
        SourceLoc loc = parser->previous.loc;
        Expr* condition = parse_expression(parser);
        consume(parser, TOKEN_COLON, "Expected ':' after while condition");
        Expr* body = parse_expression(parser);
        return expr_while(parser->arena, condition, body, loc);
    }

    // Infinite loop: loop: body
    if (match(parser, TOKEN_LOOP)) {
        SourceLoc loc = parser->previous.loc;
        consume(parser, TOKEN_COLON, "Expected ':' after 'loop'");
        Expr* body = parse_expression(parser);
        return expr_loop(parser->arena, body, loc);
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

// Type parsing
// Parses type annotations: Int, String, Result(String, Error), (Int, String) -> Bool
TypeExpr* parse_type(Parser* parser) {
    // Parenthesized type: either () unit type or (params) -> return function type
    if (match(parser, TOKEN_LPAREN)) {
        SourceLoc loc = parser->previous.loc;
        TypeExprVec* params = TypeExprVec_new(parser->arena);

        if (!check(parser, TOKEN_RPAREN)) {
            do {
                TypeExpr* param = parse_type(parser);
                TypeExprVec_push(parser->arena, params, param);
            } while (match(parser, TOKEN_COMMA));
        }

        consume(parser, TOKEN_RPAREN, "Expected ')' after type parameters");

        // If followed by ->, it's a function type; otherwise () is the unit type
        if (match(parser, TOKEN_ARROW)) {
            TypeExpr* return_type = parse_type(parser);
            return type_function(parser->arena, params, return_type, loc);
        }

        // Unit type: () — represent as named type "()"
        String* unit_name = string_new(parser->arena, "()");
        return type_named(parser->arena, unit_name, NULL, loc);
    }

    // Named type: Ident or Ident(args)
    Token name_tok = consume(parser, TOKEN_IDENT, "Expected type name");
    SourceLoc loc = name_tok.loc;

    // Check for type arguments: Name(arg1, arg2)
    TypeExprVec* args = NULL;
    if (match(parser, TOKEN_LPAREN)) {
        args = TypeExprVec_new(parser->arena);

        if (!check(parser, TOKEN_RPAREN)) {
            do {
                TypeExpr* arg = parse_type(parser);
                TypeExprVec_push(parser->arena, args, arg);
            } while (match(parser, TOKEN_COMMA));
        }

        consume(parser, TOKEN_RPAREN, "Expected ')' after type arguments");
    }

    return type_named(parser->arena, name_tok.text, args, loc);
}

// Forward declaration
static Pattern* parse_pattern(Parser* parser);

// Detect whether the current fn parameter list uses typed params (name: Type)
// or pattern params (pattern-based dispatch). Call after consuming '('.
// Returns true if params are typed (single-clause style).
static bool is_typed_params(Parser* parser) {
    // Empty params: () — treat as single-clause
    if (check(parser, TOKEN_RPAREN)) return true;
    // If first token is IDENT, peek at the lexer's next token to check for ':'
    // IDENT followed by COLON means typed param (name: Type)
    // IDENT followed by anything else means pattern param (identifier binding)
    if (check(parser, TOKEN_IDENT)) {
        Token next = lexer_peek(parser->lexer);
        return next.type == TOKEN_COLON;
    }
    // Non-identifier first param (literal, underscore) → pattern params
    return false;
}

// Parse a pattern (for match arms and function clause parameters)
static Pattern* parse_pattern(Parser* parser) {
    if (match(parser, TOKEN_UNDERSCORE)) {
        return pattern_wildcard(parser->arena, parser->previous.loc);
    }
    if (check(parser, TOKEN_IDENT)) {
        Token ident_tok = parser->current;
        advance(parser);
        return pattern_ident(parser->arena, ident_tok.text, ident_tok.loc);
    }
    // Literal patterns: integers, strings, booleans
    Expr* pattern_expr = parse_primary_internal(parser);
    Pattern* pattern = arena_alloc(parser->arena, sizeof(Pattern));
    pattern->type = PATTERN_LIT;
    pattern->loc = pattern_expr->loc;
    pattern->data.literal = pattern_expr;
    return pattern;
}

// Statement parsing
Stmt* parse_stmt(Parser* parser) {
    // Import declaration: import path.to.module [.{items}] [as alias]
    if (match(parser, TOKEN_IMPORT)) {
        SourceLoc loc = parser->previous.loc;

        // Parse module path: ident.ident.ident
        StringVec* path = StringVec_new(parser->arena);
        Token first = consume(parser, TOKEN_IDENT, "Expected module name after 'import'");
        StringVec_push(parser->arena, path, first.text);

        while (match(parser, TOKEN_DOT)) {
            // Check for selective import: path.{item, item}
            if (check(parser, TOKEN_LBRACE)) {
                break;
            }
            Token segment = consume(parser, TOKEN_IDENT, "Expected module name after '.'");
            StringVec_push(parser->arena, path, segment.text);
        }

        // Check for selective import: .{item, item}
        StringVec* items = NULL;
        if (match(parser, TOKEN_LBRACE)) {
            items = StringVec_new(parser->arena);
            if (!check(parser, TOKEN_RBRACE)) {
                do {
                    Token item = consume(parser, TOKEN_IDENT, "Expected import item name");
                    StringVec_push(parser->arena, items, item.text);
                } while (match(parser, TOKEN_COMMA));
            }
            consume(parser, TOKEN_RBRACE, "Expected '}' after import items");
        }

        // Check for alias: as name
        String* alias = NULL;
        if (match(parser, TOKEN_AS)) {
            Token alias_tok = consume(parser, TOKEN_IDENT, "Expected alias name after 'as'");
            alias = alias_tok.text;
        }

        return stmt_import(parser->arena, path, items, alias, loc);
    }

    // Break statement: break [value]
    if (match(parser, TOKEN_BREAK)) {
        SourceLoc loc = parser->previous.loc;
        Expr* value = NULL;
        // break can optionally have a value expression
        if (!check(parser, TOKEN_EOF) && !check(parser, TOKEN_RBRACE) &&
            !check(parser, TOKEN_COMMA)) {
            value = parse_expression(parser);
        }
        return stmt_break(parser->arena, value, loc);
    }

    // Continue statement
    if (match(parser, TOKEN_CONTINUE)) {
        SourceLoc loc = parser->previous.loc;
        return stmt_continue(parser->arena, loc);
    }

    // Defer statement: defer <expression>
    if (match(parser, TOKEN_DEFER)) {
        SourceLoc loc = parser->previous.loc;
        Expr* expr = parse_expression(parser);
        return stmt_defer(parser->arena, expr, loc);
    }

    // Trait definition: trait Name(params): methods
    if (match(parser, TOKEN_TRAIT)) {
        SourceLoc loc = parser->previous.loc;
        Token name_tok = consume(parser, TOKEN_IDENT, "Expected trait name");

        // Type parameters: (a, b)
        StringVec* type_params = NULL;
        if (match(parser, TOKEN_LPAREN)) {
            type_params = StringVec_new(parser->arena);
            if (!check(parser, TOKEN_RPAREN)) {
                do {
                    Token param = consume(parser, TOKEN_IDENT, "Expected type parameter");
                    StringVec_push(parser->arena, type_params, param.text);
                } while (match(parser, TOKEN_COMMA));
            }
            consume(parser, TOKEN_RPAREN, "Expected ')' after trait type parameters");
        }

        // TODO: parse optional "with Constraint(a)" super-trait clause

        consume(parser, TOKEN_COLON, "Expected ':' after trait declaration");

        // Parse methods (fn definitions)
        StmtVec* methods = StmtVec_new(parser->arena);
        while (check(parser, TOKEN_FN)) {
            Stmt* method = parse_stmt(parser);
            StmtVec_push(parser->arena, methods, method);
        }

        return stmt_trait(parser->arena, name_tok.text, type_params, methods, loc);
    }

    // Impl block: impl Trait(Type): methods
    if (match(parser, TOKEN_IMPL)) {
        SourceLoc loc = parser->previous.loc;
        Token trait_tok = consume(parser, TOKEN_IDENT, "Expected trait name after 'impl'");

        // Type arguments: (Point, String)
        TypeExprVec* type_args = NULL;
        if (match(parser, TOKEN_LPAREN)) {
            type_args = TypeExprVec_new(parser->arena);
            if (!check(parser, TOKEN_RPAREN)) {
                do {
                    TypeExpr* arg = parse_type(parser);
                    TypeExprVec_push(parser->arena, type_args, arg);
                } while (match(parser, TOKEN_COMMA));
            }
            consume(parser, TOKEN_RPAREN, "Expected ')' after impl type arguments");
        }

        consume(parser, TOKEN_COLON, "Expected ':' after impl declaration");

        // Parse methods (fn definitions)
        StmtVec* methods = StmtVec_new(parser->arena);
        while (check(parser, TOKEN_FN)) {
            Stmt* method = parse_stmt(parser);
            StmtVec_push(parser->arena, methods, method);
        }

        return stmt_impl(parser->arena, trait_tok.text, type_args, methods, loc);
    }

    // Public declaration: pub fn ... or pub type ...
    bool is_public = false;
    if (match(parser, TOKEN_PUB)) {
        is_public = true;
        if (!check(parser, TOKEN_FN) && !check(parser, TOKEN_TYPE)) {
            error_at_current(parser, "Expected 'fn' or 'type' after 'pub'");
        }
    }

    // Type definition: type Name[(params)]: variants/fields
    if (match(parser, TOKEN_TYPE)) {
        SourceLoc loc = parser->previous.loc;
        Token name_tok = consume(parser, TOKEN_IDENT, "Expected type name after 'type'");

        // Optional type parameters: (a, b)
        StringVec* type_params = NULL;
        if (match(parser, TOKEN_LPAREN)) {
            type_params = StringVec_new(parser->arena);
            if (!check(parser, TOKEN_RPAREN)) {
                do {
                    Token param = consume(parser, TOKEN_IDENT, "Expected type parameter name");
                    StringVec_push(parser->arena, type_params, param.text);
                } while (match(parser, TOKEN_COMMA));
            }
            consume(parser, TOKEN_RPAREN, "Expected ')' after type parameters");
        }

        // TODO: parse optional derive(...) clause

        consume(parser, TOKEN_COLON, "Expected ':' after type name");

        // Determine if this is a sum type or record type.
        // Record type: first member is lowercase ident followed by ':'
        // Sum type: first member is uppercase ident (variant constructor)
        // We use a heuristic: peek at the first identifier and the token after it.
        bool is_record = false;
        if (check(parser, TOKEN_IDENT)) {
            const char* first_char = string_cstr(parser->current.text);
            if (first_char[0] >= 'a' && first_char[0] <= 'z') {
                // Lowercase identifier — check if followed by ':'
                Token after = lexer_peek(parser->lexer);
                if (after.type == TOKEN_COLON) {
                    is_record = true;
                }
            }
        }

        if (is_record) {
            // Record type: name: Type, name: Type, ...
            TypeFieldVec* fields = TypeFieldVec_new(parser->arena);
            // Parse fields until we hit something that's not a lowercase ident followed by ':'
            while (check(parser, TOKEN_IDENT)) {
                const char* fc = string_cstr(parser->current.text);
                if (fc[0] >= 'A' && fc[0] <= 'Z') break; // uppercase = not a field
                Token peek_tok = lexer_peek(parser->lexer);
                if (peek_tok.type != TOKEN_COLON) break;

                Token field_name = parser->current;
                advance(parser);
                consume(parser, TOKEN_COLON, "Expected ':' after field name");
                TypeExpr* field_type = parse_type(parser);

                TypeField field;
                field.name = field_name.text;
                field.type_ann = field_type;
                TypeFieldVec_push(parser->arena, fields, field);
            }
            return stmt_type_def(parser->arena, name_tok.text, is_public, type_params, NULL, fields, loc);
        } else {
            // Sum type: Variant, Variant(fields), ...
            TypeVariantVec* variants = TypeVariantVec_new(parser->arena);
            while (check(parser, TOKEN_IDENT)) {
                Token variant_name = parser->current;
                advance(parser);

                TypeFieldVec* fields = NULL;
                if (match(parser, TOKEN_LPAREN)) {
                    fields = TypeFieldVec_new(parser->arena);
                    if (!check(parser, TOKEN_RPAREN)) {
                        do {
                            // Variant fields can be either:
                            //   name: Type (named field)
                            //   Type       (positional/type-only, e.g., Some(a))
                            Token field_tok = consume(parser, TOKEN_IDENT, "Expected field name or type");
                            if (match(parser, TOKEN_COLON)) {
                                // Named field: name: Type
                                TypeExpr* field_type = parse_type(parser);
                                TypeField f;
                                f.name = field_tok.text;
                                f.type_ann = field_type;
                                TypeFieldVec_push(parser->arena, fields, f);
                            } else {
                                // Positional/type-only: just a type name (e.g., Some(a))
                                TypeField f;
                                f.name = field_tok.text;
                                f.type_ann = NULL;
                                TypeFieldVec_push(parser->arena, fields, f);
                            }
                        } while (match(parser, TOKEN_COMMA));
                    }
                    consume(parser, TOKEN_RPAREN, "Expected ')' after variant fields");
                }

                TypeVariant variant;
                variant.name = variant_name.text;
                variant.fields = fields;
                TypeVariantVec_push(parser->arena, variants, variant);
            }
            return stmt_type_def(parser->arena, name_tok.text, is_public, type_params, variants, NULL, loc);
        }
    }

    // Function definition: fn name(params) -> type: body
    //   OR multi-clause:   fn name(patterns) -> body
    if (match(parser, TOKEN_FN)) {
        SourceLoc loc = parser->previous.loc;

        Token name_tok = consume(parser, TOKEN_IDENT, "Expected function name");
        consume(parser, TOKEN_LPAREN, "Expected '(' after function name");

        // Detect if this is a typed-param (single-clause) or pattern-param (multi-clause)
        if (is_typed_params(parser)) {
            // Single-clause: fn name(param: Type, ...) -> RetType: body
            ParameterVec* params = ParameterVec_new(parser->arena);
            if (!check(parser, TOKEN_RPAREN)) {
                do {
                    Token param_name = consume(parser, TOKEN_IDENT, "Expected parameter name");
                    consume(parser, TOKEN_COLON, "Expected ':' after parameter name");
                    TypeExpr* param_type = parse_type(parser);

                    Parameter param;
                    param.name = param_name.text;
                    param.type_ann = param_type;
                    ParameterVec_push(parser->arena, params, param);
                } while (match(parser, TOKEN_COMMA));
            }
            consume(parser, TOKEN_RPAREN, "Expected ')' after parameters");

            // Parse return type: -> Type
            TypeExpr* return_type = NULL;
            if (match(parser, TOKEN_ARROW)) {
                return_type = parse_type(parser);
            }

            // Parse body after colon
            consume(parser, TOKEN_COLON, "Expected ':' after function signature");
            Expr* body = parse_expression(parser);

            return stmt_fn(parser->arena, name_tok.text, is_public, params, return_type, body, loc);
        } else {
            // Multi-clause: fn name(pattern, ...) -> body
            // We already consumed '(' — parse pattern params
            PatternVec* params = PatternVec_new(parser->arena);
            if (!check(parser, TOKEN_RPAREN)) {
                do {
                    Pattern* pat = parse_pattern(parser);
                    PatternVec_push(parser->arena, params, pat);
                } while (match(parser, TOKEN_COMMA));
            }
            consume(parser, TOKEN_RPAREN, "Expected ')' after pattern parameters");

            // Parse body after ->
            consume(parser, TOKEN_ARROW, "Expected '->' after pattern parameters");
            Expr* body = parse_expression(parser);

            // Create a function def with a single clause
            FunctionClause clause;
            clause.params = params;
            clause.return_type = NULL;
            clause.body = body;

            FunctionClauseVec* clauses = FunctionClauseVec_new(parser->arena);
            FunctionClauseVec_push(parser->arena, clauses, clause);

            Stmt* stmt = arena_alloc(parser->arena, sizeof(Stmt));
            stmt->type = STMT_FN;
            stmt->loc = loc;
            stmt->data.fn.name = name_tok.text;
            stmt->data.fn.is_public = is_public;
            stmt->data.fn.params = NULL;
            stmt->data.fn.return_type = NULL;
            stmt->data.fn.body = NULL;
            stmt->data.fn.clauses = clauses;

            return stmt;
        }
    }

    // Let statement
    if (match(parser, TOKEN_LET)) {
        SourceLoc loc = parser->previous.loc;
        
        // Parse pattern (for now, just identifier)
        Token name_tok = consume(parser, TOKEN_IDENT, "Expected variable name");
        Pattern* pattern = pattern_ident(parser->arena, name_tok.text, name_tok.loc);
        
        // Optional type annotation: let x: Type = expr
        TypeExpr* type_ann = NULL;
        if (match(parser, TOKEN_COLON)) {
            type_ann = parse_type(parser);
        }

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

// Parse multiple statements until EOF.
// Groups adjacent fn definitions with the same name into multi-clause functions.
// Emits an error if non-adjacent clauses share the same name.
StmtVec* parse_stmts(Parser* parser) {
    StmtVec* stmts = StmtVec_new(parser->arena);

    while (!check(parser, TOKEN_EOF)) {
        Stmt* stmt = parse_stmt(parser);

        // If this is a multi-clause fn (has clauses), try to merge with adjacent same-name fn
        if (stmt->type == STMT_FN && stmt->data.fn.clauses != NULL) {
            // Check if the previous statement is the same multi-clause fn name
            if (stmts->len > 0) {
                Stmt* prev = StmtVec_get(stmts, stmts->len - 1);
                if (prev->type == STMT_FN && prev->data.fn.clauses != NULL &&
                    strcmp(string_cstr(prev->data.fn.name), string_cstr(stmt->data.fn.name)) == 0) {
                    // Merge: append this clause to the previous fn's clause list
                    FunctionClause clause = FunctionClauseVec_get(stmt->data.fn.clauses, 0);
                    FunctionClauseVec_push(parser->arena, prev->data.fn.clauses, clause);
                    continue; // Don't add as separate statement
                }

                // Check for non-adjacent duplicate: same name but not immediately before
                for (size_t i = 0; i < stmts->len - 1; i++) {
                    Stmt* earlier = StmtVec_get(stmts, i);
                    if (earlier->type == STMT_FN &&
                        strcmp(string_cstr(earlier->data.fn.name), string_cstr(stmt->data.fn.name)) == 0) {
                        // Non-adjacent clauses — emit error
                        fprintf(stderr, "Error: Function clauses for '%s' must be adjacent\n",
                                string_cstr(stmt->data.fn.name));
                        parser->had_error = true;
                        break;
                    }
                }
            }
        }

        StmtVec_push(parser->arena, stmts, stmt);
    }

    return stmts;
}
