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
static Pattern* parse_pattern(Parser* parser);

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
        Expr* end = parse_logical_or(parser);
        expr = expr_range(parser->arena, expr, end, false, loc);
    } else if (match(parser, TOKEN_DOTDOTEQ)) {
        SourceLoc loc = parser->previous.loc;
        Expr* end = parse_logical_or(parser);
        expr = expr_range(parser->arena, expr, end, true, loc);
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
static Expr* parse_power(Parser* parser);

static Expr* parse_factor(Parser* parser) {
    Expr* expr = parse_power(parser);
    
    while (true) {
        if (match(parser, TOKEN_STAR)) {
            SourceLoc loc = parser->previous.loc;
            Expr* right = parse_power(parser);
            expr = expr_binary(parser->arena, BINOP_MUL, expr, right, loc);
        } else if (match(parser, TOKEN_SLASH)) {
            SourceLoc loc = parser->previous.loc;
            Expr* right = parse_power(parser);
            expr = expr_binary(parser->arena, BINOP_DIV, expr, right, loc);
        } else if (match(parser, TOKEN_PERCENT)) {
            SourceLoc loc = parser->previous.loc;
            Expr* right = parse_power(parser);
            expr = expr_binary(parser->arena, BINOP_MOD, expr, right, loc);
        } else {
            break;
        }
    }
    
    return expr;
}

// Precedence: ** (right-associative)
static Expr* parse_power(Parser* parser) {
    Expr* expr = parse_unary(parser);
    
    if (match(parser, TOKEN_POWER)) {
        SourceLoc loc = parser->previous.loc;
        Expr* right = parse_power(parser); // Right-recursive for right-associativity
        expr = expr_binary(parser->arena, BINOP_POW, expr, right, loc);
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
    
    while (check(parser, TOKEN_LPAREN) || check(parser, TOKEN_DOT) || check(parser, TOKEN_LBRACKET)) {
    if (match(parser, TOKEN_LBRACKET)) {
        SourceLoc loc = parser->previous.loc;
        Expr* index = parse_expression(parser);
        consume(parser, TOKEN_RBRACKET, "Expected ']' after index expression");
        expr = expr_index(parser->arena, expr, index, loc);
        continue;
    }
    if (match(parser, TOKEN_DOT)) {
        SourceLoc loc = parser->previous.loc;
        if (match(parser, TOKEN_INT)) {
            // Tuple field access: expr.0, expr.1
            expr = expr_dot(parser->arena, expr, parser->previous.text, loc);
        } else if (match(parser, TOKEN_FLOAT)) {
            // Handle expr.0.1 which lexes as dot + float "0.1"
            // Split float text "0.1" into two dot accesses
            const char* text = string_cstr(parser->previous.text);
            const char* dot_pos = strchr(text, '.');
            if (dot_pos) {
                size_t first_len = (size_t)(dot_pos - text);
                String* first_field = string_new_len(parser->arena, text, first_len);
                String* second_field = string_new(parser->arena, dot_pos + 1);
                expr = expr_dot(parser->arena, expr, first_field, loc);
                expr = expr_dot(parser->arena, expr, second_field, loc);
            }
        } else {
            Token field_tok = consume(parser, TOKEN_IDENT, "Expected field name after '.'");
            expr = expr_dot(parser->arena, expr, field_tok.text, loc);
        }
        continue;
    }
    match(parser, TOKEN_LPAREN); // consume '('
    {
        SourceLoc loc = parser->previous.loc;
        
        CallArgVec* arg_vec = CallArgVec_new(parser->arena);
        
        if (!check(parser, TOKEN_RPAREN)) {
            do {
                CallArg arg;
                arg.label = NULL;
                
                // Check for labeled argument: ident: expr
                // Detect by checking IDENT followed by COLON via lexer_peek
                if (check(parser, TOKEN_IDENT)) {
                    Token peek_tok = lexer_peek(parser->lexer);
                    if (peek_tok.type == TOKEN_COLON) {
                        // Labeled argument
                        arg.label = parser->current.text;
                        advance(parser); // consume ident
                        advance(parser); // consume colon
                    }
                }
                
                arg.value = parse_expression(parser);
                CallArgVec_push(parser->arena, arg_vec, arg);
            } while (match(parser, TOKEN_COMMA));
        }
        
        consume(parser, TOKEN_RPAREN, "Expected ')' after arguments");
        
        Expr* call = arena_alloc(parser->arena, sizeof(Expr));
        call->type = EXPR_CALL;
        call->loc = loc;
        call->data.call.func = expr;
        call->data.call.args = arg_vec;
        expr = call;
    } // end call block
    } // end while
    
    return expr;
}

// Primary expressions
Expr* parse_primary(Parser* parser) {
    return parse_primary_internal(parser);
}

// Parse a pattern: _, identifier, literal, or constructor (Name(sub_patterns))
static Pattern* parse_pattern(Parser* parser) {
    if (match(parser, TOKEN_UNDERSCORE)) {
        return pattern_wildcard(parser->arena, parser->previous.loc);
    }

    // Rest pattern: ..rest or .._
    if (match(parser, TOKEN_DOTDOT)) {
        SourceLoc loc = parser->previous.loc;
        if (match(parser, TOKEN_UNDERSCORE)) {
            return pattern_rest(parser->arena, NULL, loc);
        }
        Token name_tok = consume(parser, TOKEN_IDENT, "Expected identifier or '_' after '..'");
        return pattern_rest(parser->arena, name_tok.text, loc);
    }

    // List pattern: [x, y, ..rest]
    if (match(parser, TOKEN_LBRACKET)) {
        SourceLoc loc = parser->previous.loc;
        PatternVec* elements = PatternVec_new(parser->arena);
        if (!check(parser, TOKEN_RBRACKET)) {
            do {
                Pattern* sub = parse_pattern(parser);
                PatternVec_push(parser->arena, elements, sub);
            } while (match(parser, TOKEN_COMMA));
        }
        consume(parser, TOKEN_RBRACKET, "Expected ']' after list pattern");
        return pattern_tuple(parser->arena, elements, loc);
    }

    // Tuple pattern: (x, y, z)
    if (match(parser, TOKEN_LPAREN)) {
        SourceLoc loc = parser->previous.loc;
        PatternVec* elements = PatternVec_new(parser->arena);
        if (!check(parser, TOKEN_RPAREN)) {
            do {
                Pattern* sub = parse_pattern(parser);
                PatternVec_push(parser->arena, elements, sub);
            } while (match(parser, TOKEN_COMMA));
        }
        consume(parser, TOKEN_RPAREN, "Expected ')' after tuple pattern");
        return pattern_tuple(parser->arena, elements, loc);
    }

    if (check(parser, TOKEN_IDENT)) {
        Token ident_tok = parser->current;
        advance(parser);

        // Constructor pattern: Name(sub_pat, sub_pat, ...)
        if (check(parser, TOKEN_LPAREN)) {
            advance(parser); // consume '('
            PatternVec* args = PatternVec_new(parser->arena);
            if (!check(parser, TOKEN_RPAREN)) {
                do {
                    Pattern* sub = parse_pattern(parser);
                    PatternVec_push(parser->arena, args, sub);
                } while (match(parser, TOKEN_COMMA));
            }
            consume(parser, TOKEN_RPAREN, "Expected ')' after constructor pattern arguments");
            return pattern_constructor(parser->arena, ident_tok.text, args, ident_tok.loc);
        }

        return pattern_ident(parser->arena, ident_tok.text, ident_tok.loc);
    }

    // Literal patterns: integers, strings, booleans
    Expr* pattern_expr = parse_primary_internal(parser);
    Pattern* pat = arena_alloc(parser->arena, sizeof(Pattern));
    pat->type = PATTERN_LIT;
    pat->loc = pattern_expr->loc;
    pat->data.literal = pattern_expr;
    return pat;
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

    // Interpolated string: "Hello, {name}!"
    if (match(parser, TOKEN_STRING_BEGIN)) {
        SourceLoc loc = parser->previous.loc;
        ExprVec* parts = ExprVec_new(parser->arena);

        // Add the initial string segment (may be empty)
        ExprVec_push(parser->arena, parts,
            expr_string_lit(parser->arena, parser->previous.text, parser->previous.loc));

        // Parse: expression (STRING_MID expression)* STRING_END
        while (true) {
            // Parse the interpolated expression
            Expr* interp_expr = parse_expression(parser);
            ExprVec_push(parser->arena, parts, interp_expr);

            if (match(parser, TOKEN_STRING_MID)) {
                // More interpolation segments: add mid string, continue
                ExprVec_push(parser->arena, parts,
                    expr_string_lit(parser->arena, parser->previous.text, parser->previous.loc));
            } else if (match(parser, TOKEN_STRING_END)) {
                // Final segment
                ExprVec_push(parser->arena, parts,
                    expr_string_lit(parser->arena, parser->previous.text, parser->previous.loc));
                break;
            } else {
                error_at_current(parser, "Expected string continuation or end after interpolation");
                break;
            }
        }

        return expr_interp_string(parser->arena, parts, loc);
    }
    
    // Boolean literals
    if (match(parser, TOKEN_TRUE)) {
        return expr_bool_lit(parser->arena, true, parser->previous.loc);
    }
    
    if (match(parser, TOKEN_FALSE)) {
        return expr_bool_lit(parser->arena, false, parser->previous.loc);
    }

    // Map literal: %{ key: value, ... }
    if (check(parser, TOKEN_PERCENT) && lexer_peek(parser->lexer).type == TOKEN_LBRACE) {
        advance(parser); // consume %
        advance(parser); // consume {
        SourceLoc loc = parser->previous.loc;
        MapEntryVec* entries = MapEntryVec_new(parser->arena);

        if (!check(parser, TOKEN_RBRACE)) {
            do {
                Expr* key = parse_expression(parser);
                consume(parser, TOKEN_COLON, "Expected ':' after map key");
                Expr* value = parse_expression(parser);
                MapEntry entry = { .key = key, .value = value };
                MapEntryVec_push(parser->arena, entries, entry);
            } while (match(parser, TOKEN_COMMA));
        }

        consume(parser, TOKEN_RBRACE, "Expected '}' after map entries");
        return expr_map(parser->arena, entries, loc);
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
    
    // Spawn expression: spawn(expr)
    if (match(parser, TOKEN_SPAWN)) {
        SourceLoc loc = parser->previous.loc;
        consume(parser, TOKEN_LPAREN, "Expected '(' after 'spawn'");
        Expr* func = parse_expression(parser);
        consume(parser, TOKEN_RPAREN, "Expected ')' after spawn argument");
        return expr_spawn(parser->arena, func, loc);
    }

    // Send expression: send(pid, msg)
    if (match(parser, TOKEN_SEND)) {
        SourceLoc loc = parser->previous.loc;
        consume(parser, TOKEN_LPAREN, "Expected '(' after 'send'");
        Expr* pid = parse_expression(parser);
        consume(parser, TOKEN_COMMA, "Expected ',' after pid in send");
        Expr* message = parse_expression(parser);
        consume(parser, TOKEN_RPAREN, "Expected ')' after send arguments");
        return expr_send(parser->arena, pid, message, loc);
    }

    // Receive expression: receive: pattern -> body, ...
    if (match(parser, TOKEN_RECEIVE)) {
        SourceLoc loc = parser->previous.loc;
        consume(parser, TOKEN_COLON, "Expected ':' after 'receive'");

        MatchArmVec* arms = MatchArmVec_new(parser->arena);
        Expr* after_timeout = NULL;
        Expr* after_body = NULL;

        do {
            // Check for _ after timeout -> body
            if (match(parser, TOKEN_UNDERSCORE)) {
                if (match(parser, TOKEN_AFTER)) {
                    after_timeout = parse_expression(parser);
                    consume(parser, TOKEN_ARROW, "Expected '->' after timeout value");
                    after_body = parse_expression(parser);
                    break;
                }
                // Plain wildcard pattern arm
                consume(parser, TOKEN_ARROW, "Expected '->' after '_'");
                Expr* body = parse_expression(parser);
                MatchArm arm;
                arm.pattern = pattern_wildcard(parser->arena, parser->previous.loc);
                arm.guard = NULL;
                arm.body = body;
                MatchArmVec_push(parser->arena, arms, arm);
            } else {
                Pattern* pattern = parse_pattern(parser);

                Expr* guard = NULL;
                if (match(parser, TOKEN_IF)) {
                    guard = parse_expression(parser);
                }

                // Check for after on this arm
                if (match(parser, TOKEN_AFTER)) {
                    after_timeout = parse_expression(parser);
                    consume(parser, TOKEN_ARROW, "Expected '->' after timeout value");
                    after_body = parse_expression(parser);
                    // Store the pattern arm too
                    MatchArm arm;
                    arm.pattern = pattern;
                    arm.guard = guard;
                    arm.body = after_body;
                    MatchArmVec_push(parser->arena, arms, arm);
                    break;
                }

                consume(parser, TOKEN_ARROW, "Expected '->' after receive pattern");
                Expr* body = parse_expression(parser);
                MatchArm arm;
                arm.pattern = pattern;
                arm.guard = guard;
                arm.body = body;
                MatchArmVec_push(parser->arena, arms, arm);
            }
        } while (match(parser, TOKEN_COMMA));

        return expr_receive(parser->arena, arms, after_timeout, after_body, loc);
    }

    // Match expression
    if (match(parser, TOKEN_MATCH)) {
        SourceLoc loc = parser->previous.loc;
        
        // Condition-only match: match: cond -> body, ...
        // Value-based match: match value: pattern -> body, ...
        Expr* value = NULL;
        bool condition_only = false;
        
        if (match(parser, TOKEN_COLON)) {
            // match: — no value, conditions as guards
            condition_only = true;
        } else {
            value = parse_expression(parser);
            consume(parser, TOKEN_COLON, "Expected ':' after match value");
        }
        
        // Parse match arms
        MatchArmVec* arms = MatchArmVec_new(parser->arena);
        
        do {
            if (condition_only) {
                // Condition-only: each arm is condition -> body
                // _ -> body is the default (wildcard, no guard)
                Pattern* pattern = pattern_wildcard(parser->arena, parser->current.loc);
                Expr* guard = NULL;
                
                if (!match(parser, TOKEN_UNDERSCORE)) {
                    // Parse the condition as the guard
                    guard = parse_expression(parser);
                }
                
                consume(parser, TOKEN_ARROW, "Expected '->' after condition");
                Expr* body = parse_expression(parser);
                
                MatchArm arm;
                arm.pattern = pattern;
                arm.guard = guard;
                arm.body = body;
                MatchArmVec_push(parser->arena, arms, arm);
            } else {
                Pattern* pattern = parse_pattern(parser);
                
                // Optional guard: pattern if condition -> body
                Expr* guard = NULL;
                if (match(parser, TOKEN_IF)) {
                    guard = parse_expression(parser);
                }

                consume(parser, TOKEN_ARROW, "Expected '->' after match pattern");
                Expr* body = parse_expression(parser);
                
                MatchArm arm;
                arm.pattern = pattern;
                arm.guard = guard;
                arm.body = body;
                MatchArmVec_push(parser->arena, arms, arm);
            }
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
    
    // Block expression or record update
    if (match(parser, TOKEN_LBRACE)) {
        SourceLoc loc = parser->previous.loc;
        
        // Try record update: { expr | field: value, ... }
        // Save state and try parsing expr followed by |
        LexerState saved_lex = lexer_save(parser->lexer);
        Token saved_current = parser->current;
        Token saved_previous = parser->previous;
        
        // Only attempt if not obviously a statement or empty block
        if (!check(parser, TOKEN_LET) && !check(parser, TOKEN_RETURN) &&
            !check(parser, TOKEN_DEFER) && !check(parser, TOKEN_BREAK) &&
            !check(parser, TOKEN_CONTINUE) && !check(parser, TOKEN_RBRACE)) {
            
            Expr* maybe_base = parse_expression(parser);
            
            if (check(parser, TOKEN_BAR)) {
                advance(parser); // consume |
                
                // Parse field updates: name: value, name: value, ...
                RecordFieldVec* fields = RecordFieldVec_new(parser->arena);
                do {
                    Token name_tok = parser->current;
                    consume(parser, TOKEN_IDENT, "Expected field name in record update");
                    consume(parser, TOKEN_COLON, "Expected ':' after field name");
                    Expr* value = parse_expression(parser);
                    
                    RecordField field;
                    field.name = name_tok.text;
                    field.value = value;
                    RecordFieldVec_push(parser->arena, fields, field);
                } while (match(parser, TOKEN_COMMA));
                
                consume(parser, TOKEN_RBRACE, "Expected '}' after record update");
                return expr_record_update(parser->arena, maybe_base, fields, loc);
            }
            
            // Not a record update — restore and parse as block
            lexer_restore(parser->lexer, saved_lex);
            parser->current = saved_current;
            parser->previous = saved_previous;
        }
        
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
    
    // List literal or list comprehension
    if (match(parser, TOKEN_LBRACKET)) {
        SourceLoc loc = parser->previous.loc;
        
        ExprVec* elements = ExprVec_new(parser->arena);
        
        // Parse first element, then check for comprehension or list
        if (!check(parser, TOKEN_RBRACKET)) {
            Expr* first = parse_expression(parser);
            
            // List comprehension: [expr for var in iterable [if cond]]
            if (match(parser, TOKEN_FOR)) {
                Token var_tok = consume(parser, TOKEN_IDENT, "Expected variable name after 'for'");
                consume(parser, TOKEN_IN, "Expected 'in' after comprehension variable");
                Expr* iterable = parse_expression(parser);
                
                Expr* condition = NULL;
                if (match(parser, TOKEN_IF)) {
                    condition = parse_expression(parser);
                }
                
                consume(parser, TOKEN_RBRACKET, "Expected ']' after list comprehension");
                return expr_list_comp(parser->arena, first, var_tok.text, iterable, condition, loc);
            }
            
            // Regular list: first element already parsed
            ExprVec_push(parser->arena, elements, first);
            while (match(parser, TOKEN_COMMA)) {
                Expr* elem = parse_expression(parser);
                ExprVec_push(parser->arena, elements, elem);
            }
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
                Pattern* pattern = parse_pattern(parser);

                consume(parser, TOKEN_ARROW, "Expected '->' after else pattern");
                Expr* arm_body = parse_expression(parser);

                MatchArm arm;
                arm.pattern = pattern;
                arm.guard = NULL;
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

    // Lambda or grouped expression
    if (match(parser, TOKEN_LPAREN)) {
        SourceLoc loc = parser->previous.loc;

        // Save state for speculative parsing
        Token saved_current = parser->current;
        Token saved_previous = parser->previous;
        bool saved_error = parser->had_error;
        bool saved_panic = parser->panic_mode;
        LexerState saved_lexer = lexer_save(parser->lexer);

        // Try to parse as lambda: (ident, ident, ...) -> expr
        bool is_lambda = false;
        StringVec* params = StringVec_new(parser->arena);

        if (check(parser, TOKEN_RPAREN)) {
            // () -> expr  (zero-param lambda)
            advance(parser);
            if (check(parser, TOKEN_ARROW)) {
                is_lambda = true;
            }
        } else if (check(parser, TOKEN_IDENT)) {
            // Try collecting ident, ident, ... )
            bool valid = true;
            do {
                if (!check(parser, TOKEN_IDENT)) {
                    valid = false;
                    break;
                }
                StringVec_push(parser->arena, params, parser->current.text);
                advance(parser);
            } while (match(parser, TOKEN_COMMA));

            if (valid && match(parser, TOKEN_RPAREN) && check(parser, TOKEN_ARROW)) {
                is_lambda = true;
            }
        }

        if (is_lambda) {
            // Consume -> and parse body
            advance(parser); // consume TOKEN_ARROW
            Expr* body = parse_expression(parser);
            return expr_lambda(parser->arena, params, body, loc);
        }

        // Not a lambda — restore state and parse as grouped expression or tuple
        parser->current = saved_current;
        parser->previous = saved_previous;
        parser->had_error = saved_error;
        parser->panic_mode = saved_panic;
        lexer_restore(parser->lexer, saved_lexer);

        Expr* first = parse_expression(parser);

        if (match(parser, TOKEN_COMMA)) {
            // Tuple: (a, b, ...)
            ExprVec* elements = ExprVec_new(parser->arena);
            ExprVec_push(parser->arena, elements, first);
            do {
                ExprVec_push(parser->arena, elements, parse_expression(parser));
            } while (match(parser, TOKEN_COMMA));
            consume(parser, TOKEN_RPAREN, "Expected ')' after tuple elements");
            return expr_tuple(parser->arena, elements, loc);
        }

        // Grouped expression: (expr)
        consume(parser, TOKEN_RPAREN, "Expected ')' after expression");
        return first;
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

// Statement parsing
Stmt* parse_stmt(Parser* parser) {
    // Module declaration: module math.geometry
    if (match(parser, TOKEN_MODULE)) {
        SourceLoc loc = parser->previous.loc;
        StringVec* path = StringVec_new(parser->arena);
        Token first = consume(parser, TOKEN_IDENT, "Expected module name after 'module'");
        StringVec_push(parser->arena, path, first.text);
        while (match(parser, TOKEN_DOT)) {
            Token part = consume(parser, TOKEN_IDENT, "Expected identifier after '.' in module name");
            StringVec_push(parser->arena, path, part.text);
        }
        return stmt_module(parser->arena, path, loc);
    }

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

        // Optional super-trait constraints: with Eq(a), Show(a)
        TypeExprVec* constraints = NULL;
        if (match(parser, TOKEN_WITH)) {
            constraints = TypeExprVec_new(parser->arena);
            do {
                TypeExpr* constraint = parse_type(parser);
                TypeExprVec_push(parser->arena, constraints, constraint);
            } while (match(parser, TOKEN_COMMA));
        }

        consume(parser, TOKEN_COLON, "Expected ':' after trait declaration");

        // Parse methods (fn definitions)
        StmtVec* methods = StmtVec_new(parser->arena);
        while (check(parser, TOKEN_FN)) {
            Stmt* method = parse_stmt(parser);
            StmtVec_push(parser->arena, methods, method);
        }

        return stmt_trait(parser->arena, name_tok.text, type_params, constraints, methods, loc);
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
        if (!check(parser, TOKEN_FN) && !check(parser, TOKEN_TYPE) && !check(parser, TOKEN_NEWTYPE)) {
            error_at_current(parser, "Expected 'fn', 'type', or 'newtype' after 'pub'");
        }
    }

    // Newtype definition: newtype Name = Constructor(InnerType)
    if (match(parser, TOKEN_NEWTYPE)) {
        SourceLoc loc = parser->previous.loc;
        Token name_tok = consume(parser, TOKEN_IDENT, "Expected type name after 'newtype'");
        consume(parser, TOKEN_ASSIGN, "Expected '=' after newtype name");
        Token ctor_tok = consume(parser, TOKEN_IDENT, "Expected constructor name");
        consume(parser, TOKEN_LPAREN, "Expected '(' after constructor name");
        TypeExpr* inner_type = parse_type(parser);
        consume(parser, TOKEN_RPAREN, "Expected ')' after inner type");

        return stmt_newtype(parser->arena, name_tok.text, is_public, ctor_tok.text, inner_type, loc);
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

        // Optional derive clause: type Name derive(Show, Eq):
        StringVec* derives = NULL;
        if (match(parser, TOKEN_DERIVE)) {
            consume(parser, TOKEN_LPAREN, "Expected '(' after derive");
            derives = StringVec_new(parser->arena);
            if (!check(parser, TOKEN_RPAREN)) {
                do {
                    Token trait_tok = consume(parser, TOKEN_IDENT, "Expected trait name in derive");
                    StringVec_push(parser->arena, derives, trait_tok.text);
                } while (match(parser, TOKEN_COMMA));
            }
            consume(parser, TOKEN_RPAREN, "Expected ')' after derive traits");
        }

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
            return stmt_type_def(parser->arena, name_tok.text, is_public, type_params, derives, NULL, fields, loc);
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
            return stmt_type_def(parser->arena, name_tok.text, is_public, type_params, derives, variants, NULL, loc);
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

            // Optional where clause: where Ord(a), Show(a)
            TypeExprVec* where_clauses = NULL;
            if (match(parser, TOKEN_WHERE)) {
                where_clauses = TypeExprVec_new(parser->arena);
                do {
                    TypeExpr* constraint = parse_type(parser);
                    TypeExprVec_push(parser->arena, where_clauses, constraint);
                } while (match(parser, TOKEN_COMMA));
            }

            // Parse body after colon
            consume(parser, TOKEN_COLON, "Expected ':' after function signature");
            Expr* body = parse_expression(parser);

            Stmt* fn_stmt = stmt_fn(parser->arena, name_tok.text, is_public, params, return_type, body, loc);
            fn_stmt->data.fn.where_clauses = where_clauses;
            return fn_stmt;
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
            stmt->data.fn.where_clauses = NULL;
            stmt->data.fn.body = NULL;
            stmt->data.fn.clauses = clauses;

            return stmt;
        }
    }

    // Let statement
    if (match(parser, TOKEN_LET)) {
        SourceLoc loc = parser->previous.loc;
        
        // Parse pattern: identifier, (x, y), Some(v), _, etc.
        Pattern* pattern = parse_pattern(parser);
        
        // Optional type annotation (only for identifier patterns): let x: Type = expr
        TypeExpr* type_ann = NULL;
        if (pattern->type == PATTERN_IDENT && match(parser, TOKEN_COLON)) {
            type_ann = parse_type(parser);
        }

        // Expect =
        consume(parser, TOKEN_ASSIGN, "Expected '=' after pattern");
        
        // Parse value
        Expr* value = parse_expression(parser);
        
        // Optional else clause: let Some(x) = val else: fallback
        Stmt* let_stmt = stmt_let(parser->arena, pattern, type_ann, value, loc);
        if (match(parser, TOKEN_ELSE)) {
            consume(parser, TOKEN_COLON, "Expected ':' after else");
            let_stmt->data.let.else_expr = parse_expression(parser);
        }
        return let_stmt;
    }
    
    // Return statement
    if (match(parser, TOKEN_RETURN)) {
        SourceLoc loc = parser->previous.loc;
        Expr* value = NULL;
        
        if (!check(parser, TOKEN_EOF) && !check(parser, TOKEN_RBRACE) && !check(parser, TOKEN_COMMA)) {
            value = parse_expression(parser);
        }

        // Postfix guard: return expr if condition
        Stmt* ret = stmt_return(parser->arena, value, loc);
        if (match(parser, TOKEN_IF)) {
            ret->data.return_stmt.condition = parse_expression(parser);
        } else if (match(parser, TOKEN_UNLESS)) {
            Expr* cond = parse_expression(parser);
            ret->data.return_stmt.condition = expr_unary(parser->arena, UNOP_NOT, cond, cond->loc);
        }
        return ret;
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
