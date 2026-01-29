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
static void advance_raw(Parser* parser);
static void skip_newlines(Parser* parser);
static bool check(Parser* parser, TokenType type);
static bool match(Parser* parser, TokenType type);
static Token consume(Parser* parser, TokenType type, const char* message);
static void error_at_current(Parser* parser, const char* message);
static Pattern* parse_pattern(Parser* parser);
static Expr* parse_indented_block(Parser* parser);

/**
 * Create a new parser for the given source code.
 * @param arena The arena to allocate from.
 * @param source The source code to parse.
 * @return The new parser.
 */
Parser* parser_new(Arena* arena, const char* source) {
    // FERN_STYLE: allow(assertion-density) simple constructor with obvious invariants
    Parser* parser = arena_alloc(arena, sizeof(Parser));
    parser->arena = arena;
    parser->lexer = lexer_new(arena, source);
    parser->source = source;
    parser->had_error = false;
    parser->panic_mode = false;
    
    // Initialize with first token
    advance(parser);
    
    return parser;
}

/**
 * Check if the parser encountered any errors.
 * @param parser The parser to check.
 * @return True if errors occurred, false otherwise.
 */
bool parser_had_error(Parser* parser) {
    // FERN_STYLE: allow(assertion-density) trivial accessor
    return parser->had_error;
}

/**
 * Check if token is a layout token (NEWLINE/INDENT/DEDENT).
 * @param type The token type to check.
 * @return True if it is a layout token, false otherwise.
 */
static bool is_layout_token(TokenType type) {
    // FERN_STYLE: allow(assertion-density) pure predicate with no state
    return type == TOKEN_NEWLINE || type == TOKEN_INDENT || type == TOKEN_DEDENT;
}

/**
 * Advance to the next token (raw, keeps layout tokens).
 * @param parser The parser to advance.
 */
static void advance_raw(Parser* parser) {
    // FERN_STYLE: allow(assertion-density) trivial state update
    parser->previous = parser->current;
    parser->current = lexer_next(parser->lexer);
}

/**
 * Skip newline tokens only (not INDENT/DEDENT).
 * @param parser The parser.
 */
static void __attribute__((unused)) skip_newlines(Parser* parser) {
    // FERN_STYLE: allow(assertion-density) simple skip loop
    while (parser->current.type == TOKEN_NEWLINE) {
        advance_raw(parser);
    }
}

/** Global flag set when advance() skips a DEDENT token */
static int g_dedent_seen = 0;

/** Global flag set when advance() skips a NEWLINE token (for postfix operator handling) */
static bool g_newline_seen = false;

/**
 * Advance to the next token, skipping layout tokens.
 * Use this for most parsing; use advance_raw when layout matters.
 * Sets g_dedent_seen if a DEDENT was skipped.
 * @param parser The parser to advance.
 */
static void advance(Parser* parser) {
    // FERN_STYLE: allow(assertion-density) trivial state update with skip
    advance_raw(parser);
    
    // Reset newline flag - we track newlines between significant tokens
    g_newline_seen = false;
    
    // Skip layout tokens for backward compatibility in most contexts
    // Don't use advance_raw here - we don't want to overwrite previous
    // Track DEDENT and NEWLINE tokens that we skip
    while (is_layout_token(parser->current.type)) {
        if (parser->current.type == TOKEN_DEDENT) {
            g_dedent_seen++;
        }
        if (parser->current.type == TOKEN_NEWLINE) {
            g_newline_seen = true;
        }
        parser->current = lexer_next(parser->lexer);
    }
}

/**
 * Check if current token is of the given type.
 * @param parser The parser to check.
 * @param type The expected token type.
 * @return True if current token matches, false otherwise.
 */
static bool check(Parser* parser, TokenType type) {
    // FERN_STYLE: allow(assertion-density) trivial token check
    return parser->current.type == type;
}

/**
 * Match and consume a token if it's of the given type.
 * @param parser The parser to use.
 * @param type The expected token type.
 * @return True if matched and consumed, false otherwise.
 */
static bool match(Parser* parser, TokenType type) {
    // FERN_STYLE: allow(assertion-density) trivial token match
    if (!check(parser, type)) return false;
    advance(parser);
    return true;
}

/**
 * Consume a token of the given type, or report an error.
 * @param parser The parser to use.
 * @param type The expected token type.
 * @param message The error message if token doesn't match.
 * @return The consumed token.
 */
static Token consume(Parser* parser, TokenType type, const char* message) {
    // FERN_STYLE: allow(assertion-density) simple consume-or-error pattern
    if (parser->current.type == type) {
        Token tok = parser->current;
        advance(parser);
        return tok;
    }
    
    error_at_current(parser, message);
    
    // Advance past bad token for error recovery (unless at EOF)
    if (parser->current.type != TOKEN_EOF) {
        advance(parser);
    }
    
    return parser->current;
}

/**
 * Get the start of line N (1-indexed) in source.
 * @param source The source code string.
 * @param target_line The line number to find (1-indexed).
 * @return Pointer to the start of the target line.
 */
static const char* get_line_start(const char* source, size_t target_line) {
    // FERN_STYLE: allow(assertion-density) simple string scan helper
    if (target_line <= 1) return source;
    
    const char* p = source;
    size_t line = 1;
    while (*p && line < target_line) {
        if (*p == '\n') line++;
        p++;
    }
    return p;
}

/**
 * Get the length of the current line (up to newline or EOF).
 * @param line_start Pointer to the start of the line.
 * @return The length of the line.
 */
static size_t get_line_length(const char* line_start) {
    // FERN_STYLE: allow(assertion-density) simple string length helper
    size_t len = 0;
    while (line_start[len] && line_start[len] != '\n') {
        len++;
    }
    return len;
}

/**
 * Report an error at the current token.
 * @param parser The parser reporting the error.
 * @param message The error message to display.
 */
static void error_at_current(Parser* parser, const char* message) {
    // FERN_STYLE: allow(assertion-density) error reporting function
    if (parser->panic_mode) return;
    parser->panic_mode = true;
    parser->had_error = true;
    
    SourceLoc loc = parser->current.loc;
    const char* token_text = parser->current.text ? string_cstr(parser->current.text) : "EOF";
    
    // Print error header
    fprintf(stderr, "\nerror: %s\n", message);
    fprintf(stderr, "  --> %s:%zu:%zu\n",
            loc.filename ? string_cstr(loc.filename) : "<input>",
            loc.line, loc.column);
    
    // Print source line with context
    if (parser->source && loc.line > 0) {
        const char* line_start = get_line_start(parser->source, loc.line);
        size_t line_len = get_line_length(line_start);
        
        // Print line number and source
        fprintf(stderr, "   |\n");
        fprintf(stderr, "%3zu| %.*s\n", loc.line, (int)line_len, line_start);
        
        // Print caret pointing to error location
        fprintf(stderr, "   | ");
        for (size_t i = 1; i < loc.column && i <= line_len; i++) {
            fprintf(stderr, " ");
        }
        
        // Underline the token
        size_t token_len = strlen(token_text);
        if (token_len == 0) token_len = 1;
        for (size_t i = 0; i < token_len; i++) {
            fprintf(stderr, "^");
        }
        fprintf(stderr, " %s\n\n", message);
    }
}

// Indentation-based block parsing

/**
 * Check if current token can start an expression within a block.
 * Excludes top-level declarations like 'fn', 'type', 'trait', etc.
 * @param parser The parser to check.
 * @return True if this token can start an expression in a block.
 */
static bool can_start_block_expr(Parser* parser) {
    // FERN_STYLE: allow(assertion-density) simple predicate
    TokenType t = parser->current.type;
    /* Tokens that can start expressions within a function body */
    return t == TOKEN_IDENT || t == TOKEN_INT || t == TOKEN_FLOAT ||
           t == TOKEN_STRING || t == TOKEN_TRUE || t == TOKEN_FALSE ||
           t == TOKEN_LPAREN || t == TOKEN_LBRACKET || t == TOKEN_LBRACE ||
           t == TOKEN_IF || t == TOKEN_MATCH || t == TOKEN_NOT ||
           t == TOKEN_MINUS || t == TOKEN_LET || t == TOKEN_RETURN ||
           t == TOKEN_DEFER || t == TOKEN_BREAK || t == TOKEN_CONTINUE ||
           t == TOKEN_FOR || t == TOKEN_WITH;
    /* Notably EXCLUDES: TOKEN_FN, TOKEN_TYPE, TOKEN_TRAIT, TOKEN_IMPL, TOKEN_PUB */
}

/**
 * Check if current token can start a pattern (for match arms).
 * @param parser The parser to check.
 * @return True if this token can start a pattern.
 */
static bool can_start_pattern(Parser* parser) {
    // FERN_STYLE: allow(assertion-density) simple predicate
    TokenType t = parser->current.type;
    /* Patterns can start with: identifiers, literals, _, (, [ */
    return t == TOKEN_IDENT || t == TOKEN_INT || t == TOKEN_FLOAT ||
           t == TOKEN_STRING || t == TOKEN_TRUE || t == TOKEN_FALSE ||
           t == TOKEN_UNDERSCORE || t == TOKEN_LPAREN || t == TOKEN_LBRACKET;
}

/**
 * Check if inside an indented block (saw INDENT but not matching DEDENT).
 * Uses raw token access to peek at layout tokens that advance() skips.
 * @param parser The parser to check.
 * @return True if a DEDENT or block-terminating token is next.
 */
static bool at_block_end(Parser* parser) {
    /* FERN_STYLE: allow(assertion-density) - simple predicate */
    /* Check for tokens that terminate a block: else, DEDENT, EOF, fn, type, etc. */
    TokenType t = parser->current.type;
    return t == TOKEN_EOF || t == TOKEN_ELSE || t == TOKEN_FN || 
           t == TOKEN_TYPE || t == TOKEN_TRAIT || t == TOKEN_IMPL ||
           t == TOKEN_PUB;
}

/**
 * Parse an indented block after a colon, tracking indent depth.
 * @param parser The parser to use.
 * @param track_dedent If true, stop when DEDENT is encountered via g_dedent_seen.
 * @return A block expression containing the statements and final expression.
 */
static Expr* parse_indented_block_with_indent(Parser* parser, bool track_dedent) {
    // FERN_STYLE: allow(function-length) block parsing with multiple cases
    assert(parser != NULL);
    assert(parser->arena != NULL);
    
    SourceLoc loc = parser->previous.loc;
    
    StmtVec* stmts = StmtVec_new(parser->arena);
    Expr* final_expr = NULL;
    
    /* Reset dedent counter at block start if tracking */
    int starting_dedent_count = g_dedent_seen;
    
    /* Parse statements: let, return, defer, or expression statements */
    /* Stop when we see something that can't be part of this block */
    while (!check(parser, TOKEN_EOF) && can_start_block_expr(parser) && !at_block_end(parser)) {
        /* Check if a DEDENT was seen since we started this block */
        if (track_dedent && g_dedent_seen > starting_dedent_count) {
            /* Consume exactly ONE dedent for this block's termination.
             * Decrement by 1 so outer blocks can see their dedents too. */
            g_dedent_seen--;
            break;
        }
        
        /* Check for statement keywords */
        if (check(parser, TOKEN_LET) || check(parser, TOKEN_RETURN) || 
            check(parser, TOKEN_DEFER) || check(parser, TOKEN_BREAK) ||
            check(parser, TOKEN_CONTINUE)) {
            Stmt* stmt = parse_stmt(parser);
            StmtVec_push(parser->arena, stmts, stmt);
        } else {
            /* Parse as expression */
            Expr* expr = parse_expression(parser);
            
            /* Check if this is followed by another expression in the block */
            /* Also check for dedent to properly terminate */
            bool saw_dedent = track_dedent && (g_dedent_seen > starting_dedent_count);
            bool more_in_block = can_start_block_expr(parser) && 
                                 !at_block_end(parser) &&
                                 !saw_dedent;
            if (more_in_block) {
                /* More stuff follows - this expression is a statement */
                Stmt* stmt = stmt_expr(parser->arena, expr, expr->loc);
                StmtVec_push(parser->arena, stmts, stmt);
            } else {
                /* Nothing follows at this indent level - this is the final expr */
                final_expr = expr;
                /* Consume the dedent if we're exiting due to it */
                if (saw_dedent) {
                    g_dedent_seen--;
                }
                break;
            }
        }
    }
    
    /* If we have statements but no final expression, use unit (0) */
    if (final_expr == NULL) {
        final_expr = expr_int_lit(parser->arena, 0, loc);
    }
    
    /* If there are no statements, just return the expression directly
     * (no need to wrap single expressions in a block) */
    if (stmts->len == 0) {
        return final_expr;
    }
    
    return expr_block(parser->arena, stmts, final_expr, loc);
}

/**
 * Parse an indented block after a colon.
 * Expects: NEWLINE INDENT stmt* expr DEDENT (or single expression on same line)
 * @param parser The parser to use.
 * @return A block expression containing the statements and final expression.
 */
static Expr* parse_indented_block(Parser* parser) {
    /* FERN_STYLE: allow(assertion-density) - simple delegation wrapper */
    /* Default: no dedent tracking (for function bodies) */
    return parse_indented_block_with_indent(parser, false);
}

// Expression parsing (operator precedence climbing)

/** Binary operator mapping for generic operator parsing. */
typedef struct {
    TokenType token;
    BinaryOp op;
} BinOpMapping;

/**
 * Generic left-associative binary operator parser.
 * @param parser The parser to use.
 * @param next_prec Function to parse the next precedence level.
 * @param ops Array of operator mappings.
 * @param op_count Number of operators in the array.
 * @return The parsed expression.
 */
static Expr* parse_binary_left(
    Parser* parser,
    Expr* (*next_prec)(Parser*),
    const BinOpMapping* ops,
    size_t op_count
) {
    Expr* expr = next_prec(parser);
    
    while (true) {
        bool matched = false;
        for (size_t i = 0; i < op_count; i++) {
            if (match(parser, ops[i].token)) {
                SourceLoc loc = parser->previous.loc;
                Expr* right = next_prec(parser);
                expr = expr_binary(parser->arena, ops[i].op, expr, right, loc);
                matched = true;
                break;
            }
        }
        if (!matched) break;
    }
    
    return expr;
}

/**
 * Parse an expression (top-level entry point).
 * @param parser The parser to use.
 * @return The parsed expression.
 */
Expr* parse_expr(Parser* parser) {
    // FERN_STYLE: allow(assertion-density) trivial delegation
    return parse_expression(parser);
}

/**
 * Parse an expression with full precedence.
 * @param parser The parser to use.
 * @return The parsed expression.
 */
static Expr* parse_expression(Parser* parser) {
    // FERN_STYLE: allow(assertion-density) trivial delegation
    return parse_pipe(parser);
}

/**
 * Parse pipe expressions (lowest precedence).
 * @param parser The parser to use.
 * @return The parsed expression.
 */
static Expr* parse_pipe(Parser* parser) {
    // FERN_STYLE: allow(assertion-density) simple precedence level
    static const BinOpMapping ops[] = {{TOKEN_PIPE, BINOP_PIPE}};
    return parse_binary_left(parser, parse_range, ops, 1);
}

/**
 * Parse range expressions (a..b or a..=b).
 * @param parser The parser to use.
 * @return The parsed expression.
 */
static Expr* parse_range(Parser* parser) {
    // FERN_STYLE: allow(assertion-density) simple precedence level
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

/**
 * Parse logical OR expressions.
 * @param parser The parser to use.
 * @return The parsed expression.
 */
static Expr* parse_logical_or(Parser* parser) {
    // FERN_STYLE: allow(assertion-density) simple precedence level
    static const BinOpMapping ops[] = {{TOKEN_OR, BINOP_OR}};
    return parse_binary_left(parser, parse_logical_and, ops, 1);
}

/**
 * Parse logical AND expressions.
 * @param parser The parser to use.
 * @return The parsed expression.
 */
static Expr* parse_logical_and(Parser* parser) {
    // FERN_STYLE: allow(assertion-density) simple precedence level
    static const BinOpMapping ops[] = {{TOKEN_AND, BINOP_AND}};
    return parse_binary_left(parser, parse_equality, ops, 1);
}

/**
 * Parse equality expressions (== !=).
 * @param parser The parser to use.
 * @return The parsed expression.
 */
static Expr* parse_equality(Parser* parser) {
    // FERN_STYLE: allow(assertion-density) simple precedence level
    static const BinOpMapping ops[] = {
        {TOKEN_EQ, BINOP_EQ},
        {TOKEN_NE, BINOP_NE},
    };
    return parse_binary_left(parser, parse_comparison, ops, 2);
}

/**
 * Parse comparison expressions (< <= > >=).
 * @param parser The parser to use.
 * @return The parsed expression.
 */
static Expr* parse_comparison(Parser* parser) {
    // FERN_STYLE: allow(assertion-density) simple precedence level
    static const BinOpMapping ops[] = {
        {TOKEN_LT, BINOP_LT},
        {TOKEN_LE, BINOP_LE},
        {TOKEN_GT, BINOP_GT},
        {TOKEN_GE, BINOP_GE},
        {TOKEN_IN, BINOP_IN},
    };
    return parse_binary_left(parser, parse_term, ops, 5);
}

/**
 * Parse additive expressions (+ -).
 * @param parser The parser to use.
 * @return The parsed expression.
 */
static Expr* parse_term(Parser* parser) {
    // FERN_STYLE: allow(assertion-density) simple precedence level
    static const BinOpMapping ops[] = {
        {TOKEN_PLUS, BINOP_ADD},
        {TOKEN_MINUS, BINOP_SUB},
    };
    return parse_binary_left(parser, parse_factor, ops, 2);
}

static Expr* parse_power(Parser* parser);

/**
 * Parse multiplicative expressions (* / %).
 * @param parser The parser to use.
 * @return The parsed expression.
 */
static Expr* parse_factor(Parser* parser) {
    // FERN_STYLE: allow(assertion-density) simple precedence level
    static const BinOpMapping ops[] = {
        {TOKEN_STAR, BINOP_MUL},
        {TOKEN_SLASH, BINOP_DIV},
        {TOKEN_PERCENT, BINOP_MOD},
    };
    return parse_binary_left(parser, parse_power, ops, 3);
}

/**
 * Parse power expressions (** right-associative).
 * @param parser The parser to use.
 * @return The parsed expression.
 */
static Expr* parse_power(Parser* parser) {
    // FERN_STYLE: allow(assertion-density) simple precedence level
    Expr* expr = parse_unary(parser);
    
    if (match(parser, TOKEN_POWER)) {
        SourceLoc loc = parser->previous.loc;
        Expr* right = parse_power(parser); // Right-recursive for right-associativity
        expr = expr_binary(parser->arena, BINOP_POW, expr, right, loc);
    }
    
    return expr;
}

/**
 * Parse unary expressions (- !).
 * @param parser The parser to use.
 * @return The parsed expression.
 */
static Expr* parse_unary(Parser* parser) {
    // FERN_STYLE: allow(assertion-density) simple precedence level
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

/**
 * Parse postfix try operator (?).
 * @param parser The parser to use.
 * @param expr The expression to apply the operator to.
 * @return The try expression.
 */
static Expr* parse_postfix_try(Parser* parser, Expr* expr) {
    // FERN_STYLE: allow(assertion-density) trivial postfix operator
    SourceLoc loc = parser->previous.loc;
    return expr_try(parser->arena, expr, loc);
}

/**
 * Parse postfix index access ([]).
 * @param parser The parser to use.
 * @param expr The expression being indexed.
 * @return The index expression.
 */
static Expr* parse_postfix_index(Parser* parser, Expr* expr) {
    // FERN_STYLE: allow(assertion-density) trivial postfix operator
    SourceLoc loc = parser->previous.loc;
    Expr* index = parse_expression(parser);
    consume(parser, TOKEN_RBRACKET, "Expected ']' after index expression");
    return expr_index(parser->arena, expr, index, loc);
}

/**
 * Parse postfix dot access (.).
 * @param parser The parser to use.
 * @param expr The expression being accessed.
 * @return The dot access expression.
 */
static Expr* parse_postfix_dot(Parser* parser, Expr* expr) {
    // FERN_STYLE: allow(assertion-density) trivial postfix operator
    SourceLoc loc = parser->previous.loc;
    
    if (match(parser, TOKEN_INT)) {
        // Tuple field access: expr.0, expr.1
        return expr_dot(parser->arena, expr, parser->previous.text, loc);
    }
    
    if (match(parser, TOKEN_FLOAT)) {
        // Handle expr.0.1 which lexes as dot + float "0.1"
        // Split float text "0.1" into two dot accesses
        const char* text = string_cstr(parser->previous.text);
        const char* dot_pos = strchr(text, '.');
        if (dot_pos) {
            size_t first_len = (size_t)(dot_pos - text);
            String* first_field = string_new_len(parser->arena, text, first_len);
            String* second_field = string_new(parser->arena, dot_pos + 1);
            expr = expr_dot(parser->arena, expr, first_field, loc);
            return expr_dot(parser->arena, expr, second_field, loc);
        }
    }
    
    Token field_tok = consume(parser, TOKEN_IDENT, "Expected field name after '.'");
    return expr_dot(parser->arena, expr, field_tok.text, loc);
}

/**
 * Parse postfix function call (()).
 * @param parser The parser to use.
 * @param expr The function expression being called.
 * @return The call expression.
 */
static Expr* parse_postfix_call(Parser* parser, Expr* expr) {
    // FERN_STYLE: allow(assertion-density, bounded-loops) loop bounded by argument count
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
    return call;
}

/**
 * Parse postfix operators (call, dot, index, try).
 * @param parser The parser to use.
 * @return The parsed expression with all postfix operators applied.
 */
static Expr* parse_call(Parser* parser) {
    // FERN_STYLE: allow(assertion-density, bounded-loops) loop bounded by postfix chain length
    Expr* expr = parse_primary_internal(parser);
    
    while (true) {
        if (match(parser, TOKEN_QUESTION)) {
            expr = parse_postfix_try(parser, expr);
        } else if (match(parser, TOKEN_LBRACKET)) {
            expr = parse_postfix_index(parser, expr);
        } else if (match(parser, TOKEN_DOT)) {
            expr = parse_postfix_dot(parser, expr);
        } else if (check(parser, TOKEN_LPAREN) && !g_newline_seen) {
            /* Function call: only allow if no newline was crossed.
             * This prevents (tuple) on next line from being parsed as call args.
             * We use check() instead of match() so we can test g_newline_seen first. */
            advance(parser);
            expr = parse_postfix_call(parser, expr);
        } else {
            break;
        }
    }
    
    return expr;
}

/**
 * Parse primary expressions (entry point for primary parsing).
 * @param parser The parser to use.
 * @return The parsed primary expression.
 */
Expr* parse_primary(Parser* parser) {
    // FERN_STYLE: allow(assertion-density) trivial delegation
    return parse_primary_internal(parser);
}

/**
 * Parse a pattern: _, identifier, literal, or constructor.
 * @param parser The parser to use.
 * @return The parsed pattern.
 */
static Pattern* parse_pattern(Parser* parser) {
    // FERN_STYLE: allow(assertion-density, function-length, bounded-loops) complex pattern parsing
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

        // Nullary constructor patterns: None (without parentheses)
        // These are constructor patterns with zero arguments
        const char* name = string_cstr(ident_tok.text);
        if (strcmp(name, "None") == 0) {
            PatternVec* empty_args = PatternVec_new(parser->arena);
            return pattern_constructor(parser->arena, ident_tok.text, empty_args, ident_tok.loc);
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

/**
 * Parse primary expressions (internal implementation).
 * @param parser The parser to use.
 * @return The parsed primary expression.
 */
static Expr* parse_primary_internal(Parser* parser) {
    // FERN_STYLE: allow(assertion-density, function-length, bounded-loops) main parsing entry point handles all expression types
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

    // Map literal or record update: %{ ... }
    // Map: %{ key: value, ... }
    // Record update: %{ record | field: value, ... }
    if (check(parser, TOKEN_PERCENT) && lexer_peek(parser->lexer).type == TOKEN_LBRACE) {
        advance(parser); // consume %
        advance(parser); // consume {
        SourceLoc loc = parser->previous.loc;

        if (check(parser, TOKEN_RBRACE)) {
            // Empty map: %{ }
            advance(parser);
            return expr_map(parser->arena, MapEntryVec_new(parser->arena), loc);
        }

        // Parse first expression — could be map key or record base
        Expr* first = parse_expression(parser);

        if (match(parser, TOKEN_BAR)) {
            // Record update: %{ record | field: value, ... }
            RecordFieldVec* fields = RecordFieldVec_new(parser->arena);
            do {
                Token name_tok = consume(parser, TOKEN_IDENT, "Expected field name in record update");
                consume(parser, TOKEN_COLON, "Expected ':' after field name");
                Expr* value = parse_expression(parser);
                RecordField field = { .name = name_tok.text, .value = value };
                RecordFieldVec_push(parser->arena, fields, field);
            } while (match(parser, TOKEN_COMMA));
            consume(parser, TOKEN_RBRACE, "Expected '}' after record update");
            return expr_record_update(parser->arena, first, fields, loc);
        }

        // Map literal: first was a key, expect colon and value
        consume(parser, TOKEN_COLON, "Expected ':' after map key");
        Expr* first_value = parse_expression(parser);
        MapEntryVec* entries = MapEntryVec_new(parser->arena);
        MapEntry first_entry = { .key = first, .value = first_value };
        MapEntryVec_push(parser->arena, entries, first_entry);

        while (match(parser, TOKEN_COMMA)) {
            Expr* key = parse_expression(parser);
            consume(parser, TOKEN_COLON, "Expected ':' after map key");
            Expr* value = parse_expression(parser);
            MapEntry entry = { .key = key, .value = value };
            MapEntryVec_push(parser->arena, entries, entry);
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
        
        /* Track dedent count before parsing arms.
         * Multi-line match arms are indented, and when we return to the match's
         * indentation level, we don't want that dedent to propagate to outer blocks. */
        int starting_dedent_count = g_dedent_seen;
        
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
        } while (match(parser, TOKEN_COMMA) || 
                 (!g_newline_seen && can_start_pattern(parser)) ||
                 (g_newline_seen && g_dedent_seen == starting_dedent_count && can_start_pattern(parser)));
        /* Match arms continue with:
         * - comma (explicit separator), OR
         * - same-line pattern (no newline), OR
         * - newline + pattern at same indentation (no dedent since match started) */
        
        /* Consume any dedent caused by the match expression's indented arms.
         * This prevents the dedent from propagating to outer blocks (e.g., for loop body). */
        if (g_dedent_seen > starting_dedent_count) {
            g_dedent_seen = starting_dedent_count;
        }
        
        return expr_match(parser->arena, value, arms, loc);
    }
    
    // If expression
    if (match(parser, TOKEN_IF)) {
        SourceLoc loc = parser->previous.loc;
        
        Expr* condition = parse_expression(parser);
        consume(parser, TOKEN_COLON, "Expected ':' after if condition");
        
        /* Check if then branch is on same line (inline if) or indented block */
        Expr* then_branch;
        bool is_inline = !g_newline_seen;
        if (is_inline) {
            then_branch = parse_expression(parser);
        } else {
            then_branch = parse_indented_block_with_indent(parser, true);
        }
        
        Expr* else_branch = NULL;
        if (match(parser, TOKEN_ELSE)) {
            consume(parser, TOKEN_COLON, "Expected ':' after else");
            /* For inline if, else is also inline; for block if, else is block */
            if (is_inline) {
                else_branch = parse_expression(parser);
            } else {
                else_branch = parse_indented_block_with_indent(parser, true);
            }
        }
        
        return expr_if(parser->arena, condition, then_branch, else_branch, loc);
    }
    
    // Block expression: { stmt, stmt, expr }
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

        // Parse do body (supports indented blocks)
        consume(parser, TOKEN_DO, "Expected 'do' after with bindings");
        Expr* body = parse_indented_block(parser);

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
        Expr* body = parse_indented_block_with_indent(parser, true);  /* Track dedent */
        return expr_for(parser->arena, var_tok.text, iterable, body, loc);
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

        // Empty tuple () is Unit
        if (match(parser, TOKEN_RPAREN)) {
            ExprVec* elements = ExprVec_new(parser->arena);
            return expr_tuple(parser->arena, elements, loc);
        }

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

/**
 * Parse type annotations (Int, Result(T, E), (A, B) -> C).
 * @param parser The parser to use.
 * @return The parsed type expression.
 */
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

        // If followed by ->, it's a function type
        if (match(parser, TOKEN_ARROW)) {
            TypeExpr* return_type = parse_type(parser);
            return type_function(parser->arena, params, return_type, loc);
        }

        // Empty () is unit type
        if (params->len == 0) {
            String* unit_name = string_new(parser->arena, "()");
            return type_named(parser->arena, unit_name, NULL, loc);
        }

        // (T1, T2, ...) is a tuple type
        return type_tuple_expr(parser->arena, params, loc);
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

/**
 * Detect if fn params are typed (name: Type) or pattern-based.
 * @param parser The parser to check.
 * @return True if params are typed, false if pattern-based.
 */
static bool is_typed_params(Parser* parser) {
    // FERN_STYLE: allow(assertion-density) simple peek-ahead detection
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

/**
 * Parse a single statement.
 * @param parser The parser to use.
 * @return The parsed statement.
 */
Stmt* parse_stmt(Parser* parser) {
    // FERN_STYLE: allow(assertion-density, function-length) main statement parser handles all statement types
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

            // Parse body after colon (may be indented block or single expression)
            consume(parser, TOKEN_COLON, "Expected ':' after function signature");
            Expr* body = parse_indented_block(parser);

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
        
        /* Track dedent count before parsing expression to detect block boundary */
        int return_start_dedent = g_dedent_seen;
        
        if (!check(parser, TOKEN_EOF) && !check(parser, TOKEN_RBRACE) && !check(parser, TOKEN_COMMA)) {
            value = parse_expression(parser);
        }

        // Postfix guard: return expr if condition
        // But only if we didn't cross a block boundary (dedent)
        Stmt* ret = stmt_return(parser->arena, value, loc);
        if (g_dedent_seen == return_start_dedent && match(parser, TOKEN_IF)) {
            ret->data.return_stmt.condition = parse_expression(parser);
        }
        return ret;
    }
    
    // Expression statement (with optional postfix if)
    Expr* expr = parse_expression(parser);

    if (match(parser, TOKEN_IF)) {
        Expr* cond = parse_expression(parser);
        expr = expr_if(parser->arena, cond, expr, NULL, expr->loc);
    }

    return stmt_expr(parser->arena, expr, expr->loc);
}

/**
 * Parse multiple statements until EOF, grouping multi-clause functions.
 * @param parser The parser to use.
 * @return A vector of parsed statements.
 */
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
