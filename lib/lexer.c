/* Fern Lexer Implementation */

#include "lexer.h"
#include <ctype.h>
#include <string.h>
#include <assert.h>

struct Lexer {
    Arena* arena;
    const char* source;
    const char* current;
    String* filename;
    size_t line;
    size_t column;
    int interp_depth;       // > 0 when inside string interpolation {expr}
    int interp_brace_depth; // tracks nested {} inside interpolation expr
};

/* Helper: Check if at end of source */
static bool is_at_end(Lexer* lex) {
    return *lex->current == '\0';
}

/* Helper: Peek current character */
static char peek(Lexer* lex) {
    return *lex->current;
}

/* Helper: Peek next character */
static char peek_next(Lexer* lex) __attribute__((unused));
static char peek_next(Lexer* lex) {
    if (is_at_end(lex)) return '\0';
    return lex->current[1];
}

/* Helper: Advance to next character */
static char advance(Lexer* lex) {
    char c = *lex->current++;
    if (c == '\n') {
        lex->line++;
        lex->column = 1;
    } else {
        lex->column++;
    }
    return c;
}

/* Helper: Match expected character */
static bool match(Lexer* lex, char expected) {
    if (is_at_end(lex)) return false;
    if (*lex->current != expected) return false;
    advance(lex);
    return true;
}

/* Helper: Skip whitespace (but not newlines for now) */
static void skip_whitespace(Lexer* lex) {
    while (!is_at_end(lex)) {
        char c = peek(lex);
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            advance(lex);
        } else if (c == '#') {
            // Skip single-line comment
            while (!is_at_end(lex) && peek(lex) != '\n') {
                advance(lex);
            }
        } else {
            break;
        }
    }
}

/* Helper: Check if character is valid in identifier */
static bool is_ident_start(char c) {
    return isalpha(c) || c == '_';
}

static bool is_ident_cont(char c) {
    return isalnum(c) || c == '_';
}

/* Helper: Create token */
static Token make_token(Lexer* lex, TokenType type, const char* start, const char* end) {
    Token tok;
    tok.type = type;
    
    size_t len = end - start;
    tok.text = string_new_len(lex->arena, start, len);
    
    tok.loc.filename = lex->filename;
    tok.loc.line = lex->line;
    tok.loc.column = lex->column;
    
    return tok;
}

/* Helper: Check if string matches keyword */
static TokenType check_keyword(const char* start, size_t length, const char* rest, TokenType type) {
    if (strlen(rest) == length && memcmp(start, rest, length) == 0) {
        return type;
    }
    return TOKEN_IDENT;
}

/* Helper: Identify keyword or identifier */
static TokenType identifier_type(const char* start, size_t length) {
    // Check keywords based on first character for efficiency
    switch (start[0]) {
        case 'a':
            if (length == 2) return check_keyword(start, length, "as", TOKEN_AS);
            if (length == 3) return check_keyword(start, length, "and", TOKEN_AND);
            if (length == 5) return check_keyword(start, length, "after", TOKEN_AFTER);
            break;
        case 'd':
            if (length > 1) {
                switch (start[1]) {
                    case 'e':
                        if (length == 5) return check_keyword(start, length, "defer", TOKEN_DEFER);
                        if (length == 6) return check_keyword(start, length, "derive", TOKEN_DERIVE);
                        break;
                    case 'o': return check_keyword(start, length, "do", TOKEN_DO);
                }
            }
            break;
        case 'e': return check_keyword(start, length, "else", TOKEN_ELSE);
        case 'b': return check_keyword(start, length, "break", TOKEN_BREAK);
        case 'c': return check_keyword(start, length, "continue", TOKEN_CONTINUE);
        case 'f':
            if (length > 1) {
                switch (start[1]) {
                    case 'a': return check_keyword(start, length, "false", TOKEN_FALSE);
                    case 'n': return check_keyword(start, length, "fn", TOKEN_FN);
                    case 'o': return check_keyword(start, length, "for", TOKEN_FOR);
                }
            }
            break;
        case 'i':
            if (length > 1) {
                switch (start[1]) {
                    case 'f': return check_keyword(start, length, "if", TOKEN_IF);
                    case 'm':
                        if (length > 2) {
                            switch (start[2]) {
                                case 'p':
                                    if (length == 4) return check_keyword(start, length, "impl", TOKEN_IMPL);
                                    if (length == 6) return check_keyword(start, length, "import", TOKEN_IMPORT);
                                    break;
                            }
                        }
                        break;
                    case 'n': return check_keyword(start, length, "in", TOKEN_IN);
                }
            }
            break;
        case 'l':
            if (length == 3) return check_keyword(start, length, "let", TOKEN_LET);
            if (length == 4) return check_keyword(start, length, "loop", TOKEN_LOOP);
            break;
        case 'm':
            if (length == 5) return check_keyword(start, length, "match", TOKEN_MATCH);
            if (length == 6) return check_keyword(start, length, "module", TOKEN_MODULE);
            break;
        case 'n':
            if (length == 3) return check_keyword(start, length, "not", TOKEN_NOT);
            if (length == 7) return check_keyword(start, length, "newtype", TOKEN_NEWTYPE);
            break;
        case 'o': return check_keyword(start, length, "or", TOKEN_OR);
        case 'p': return check_keyword(start, length, "pub", TOKEN_PUB);
        case 'r':
            if (length > 1) {
                switch (start[1]) {
                    case 'e':
                        if (length == 6) return check_keyword(start, length, "return", TOKEN_RETURN);
                        if (length == 7) return check_keyword(start, length, "receive", TOKEN_RECEIVE);
                        break;
                }
            }
            break;
        case 's':
            if (length > 1) {
                switch (start[1]) {
                    case 'p': return check_keyword(start, length, "spawn", TOKEN_SPAWN);
                    case 'e': return check_keyword(start, length, "send", TOKEN_SEND);
                }
            }
            break;
        case 't':
            if (length > 1) {
                switch (start[1]) {
                    case 'r':
                        if (length == 4) return check_keyword(start, length, "true", TOKEN_TRUE);
                        if (length == 5) return check_keyword(start, length, "trait", TOKEN_TRAIT);
                        break;
                    case 'y': return check_keyword(start, length, "type", TOKEN_TYPE);
                }
            }
            break;
        case 'u': return check_keyword(start, length, "unless", TOKEN_UNLESS);
        case 'w':
            if (length == 4) return check_keyword(start, length, "with", TOKEN_WITH);
            if (length == 5) {
                if (start[1] == 'h') {
                    if (start[2] == 'i') return check_keyword(start, length, "while", TOKEN_WHILE);
                    if (start[2] == 'e') return check_keyword(start, length, "where", TOKEN_WHERE);
                }
            }
            break;
    }
    
    return TOKEN_IDENT;
}

/* Lex identifier or keyword */
static Token lex_identifier(Lexer* lex) {
    const char* start = lex->current - 1;  // We already consumed first char
    
    while (is_ident_cont(peek(lex))) {
        advance(lex);
    }
    
    size_t length = lex->current - start;
    
    // Special case: bare underscore is TOKEN_UNDERSCORE, not an identifier
    if (length == 1 && start[0] == '_') {
        return make_token(lex, TOKEN_UNDERSCORE, start, lex->current);
    }
    
    TokenType type = identifier_type(start, length);
    
    return make_token(lex, type, start, lex->current);
}

/* Lex number (integer or float) */
static Token lex_number(Lexer* lex) {
    const char* start = lex->current - 1;  // We already consumed first digit

    // Check for hex (0x), binary (0b), octal (0o)
    if (start[0] == '0' && peek(lex) != '\0') {
        char next = peek(lex);
        if (next == 'x' || next == 'X') {
            advance(lex);  // consume 'x'
            while (isxdigit(peek(lex)) || peek(lex) == '_') {
                advance(lex);
            }
            return make_token(lex, TOKEN_INT, start, lex->current);
        }
        if (next == 'b' || next == 'B') {
            advance(lex);  // consume 'b'
            while (peek(lex) == '0' || peek(lex) == '1' || peek(lex) == '_') {
                advance(lex);
            }
            return make_token(lex, TOKEN_INT, start, lex->current);
        }
        if (next == 'o' || next == 'O') {
            advance(lex);  // consume 'o'
            while ((peek(lex) >= '0' && peek(lex) <= '7') || peek(lex) == '_') {
                advance(lex);
            }
            return make_token(lex, TOKEN_INT, start, lex->current);
        }
    }

    while (isdigit(peek(lex))) {
        advance(lex);
    }

    // Check for float: digits followed by '.' and more digits
    if (peek(lex) == '.' && isdigit(lex->current[1])) {
        advance(lex);  // consume '.'
        while (isdigit(peek(lex))) {
            advance(lex);
        }
        return make_token(lex, TOKEN_FLOAT, start, lex->current);
    }

    return make_token(lex, TOKEN_INT, start, lex->current);
}

/* Lex string literal */
/* Lex a string segment: from current position to next { or closing " */
static Token lex_string_segment(Lexer* lex, TokenType if_interp, TokenType if_end) {
    const char* start = lex->current;

    while (!is_at_end(lex) && peek(lex) != '"' && peek(lex) != '{') {
        if (peek(lex) == '\\') {
            advance(lex);  // consume backslash
            if (!is_at_end(lex)) advance(lex);  // consume escaped char
            continue;
        }
        advance(lex);
    }

    if (is_at_end(lex)) {
        return make_token(lex, TOKEN_ERROR, start, lex->current);
    }

    const char* end = lex->current;

    if (peek(lex) == '{') {
        advance(lex); // consume '{'
        lex->interp_depth++;
        lex->interp_brace_depth = 0;
        return make_token(lex, if_interp, start, end);
    }

    // peek is '"'
    advance(lex); // consume closing quote
    return make_token(lex, if_end, start, end);
}

static Token lex_string(Lexer* lex) {
    const char* start = lex->current;  // After opening quote

    while (!is_at_end(lex) && peek(lex) != '"' && peek(lex) != '{') {
        if (peek(lex) == '\\') {
            advance(lex);  // consume backslash
            if (!is_at_end(lex)) advance(lex);  // consume escaped char
            continue;
        }
        advance(lex);
    }

    if (is_at_end(lex)) {
        return make_token(lex, TOKEN_ERROR, start, lex->current);
    }

    const char* end = lex->current;

    if (peek(lex) == '{') {
        // String with interpolation: "Hello, {
        advance(lex); // consume '{'
        lex->interp_depth++;
        lex->interp_brace_depth = 0;
        return make_token(lex, TOKEN_STRING_BEGIN, start, end);
    }

    // Regular string (no interpolation)
    advance(lex);  // Closing quote
    return make_token(lex, TOKEN_STRING, start, end);
}

Lexer* lexer_new(Arena* arena, const char* source) {
    assert(arena != NULL);
    assert(source != NULL);
    
    Lexer* lex = arena_alloc(arena, sizeof(Lexer));
    lex->arena = arena;
    lex->source = source;
    lex->current = source;
    lex->filename = string_new(arena, "<input>");
    lex->line = 1;
    lex->column = 1;
    
    return lex;
}

Token lexer_next(Lexer* lex) {
    assert(lex != NULL);
    
    skip_whitespace(lex);
    
    if (is_at_end(lex)) {
        return make_token(lex, TOKEN_EOF, lex->current, lex->current);
    }
    
    char c = advance(lex);

    // Handle string interpolation: } closes interpolation, resume string
    if (c == '}' && lex->interp_depth > 0 && lex->interp_brace_depth == 0) {
        lex->interp_depth--;
        // Continue lexing the rest of the string after }
        return lex_string_segment(lex, TOKEN_STRING_MID, TOKEN_STRING_END);
    }

    // Track brace nesting inside interpolation expressions
    if (lex->interp_depth > 0) {
        if (c == '{') lex->interp_brace_depth++;
        if (c == '}') lex->interp_brace_depth--;
    }

    // Identifiers and keywords
    if (is_ident_start(c)) {
        return lex_identifier(lex);
    }
    
    // Numbers
    if (isdigit(c)) {
        return lex_number(lex);
    }
    
    // String literals
    if (c == '"') {
        return lex_string(lex);
    }
    
    // Two-character operators
    switch (c) {
        case '<':
            if (match(lex, '-')) return make_token(lex, TOKEN_BIND, lex->current - 2, lex->current);
            if (match(lex, '=')) return make_token(lex, TOKEN_LE, lex->current - 2, lex->current);
            return make_token(lex, TOKEN_LT, lex->current - 1, lex->current);
            
        case '>':
            if (match(lex, '=')) return make_token(lex, TOKEN_GE, lex->current - 2, lex->current);
            return make_token(lex, TOKEN_GT, lex->current - 1, lex->current);
            
        case '=':
            if (match(lex, '=')) return make_token(lex, TOKEN_EQ, lex->current - 2, lex->current);
            if (match(lex, '>')) return make_token(lex, TOKEN_FAT_ARROW, lex->current - 2, lex->current);
            return make_token(lex, TOKEN_ASSIGN, lex->current - 1, lex->current);
            
        case '!':
            if (match(lex, '=')) return make_token(lex, TOKEN_NE, lex->current - 2, lex->current);
            // TODO: Error - '!' alone is not valid
            break;
            
        case '-':
            if (match(lex, '>')) return make_token(lex, TOKEN_ARROW, lex->current - 2, lex->current);
            return make_token(lex, TOKEN_MINUS, lex->current - 1, lex->current);
            
        case '|':
            if (match(lex, '>')) return make_token(lex, TOKEN_PIPE, lex->current - 2, lex->current);
            return make_token(lex, TOKEN_BAR, lex->current - 1, lex->current);
            
        case '*':
            if (match(lex, '*')) return make_token(lex, TOKEN_POWER, lex->current - 2, lex->current);
            return make_token(lex, TOKEN_STAR, lex->current - 1, lex->current);
            
        case '.':
            if (match(lex, '.')) {
                if (match(lex, '.')) return make_token(lex, TOKEN_ELLIPSIS, lex->current - 3, lex->current);
                if (match(lex, '=')) return make_token(lex, TOKEN_DOTDOTEQ, lex->current - 3, lex->current);
                return make_token(lex, TOKEN_DOTDOT, lex->current - 2, lex->current);
            }
            return make_token(lex, TOKEN_DOT, lex->current - 1, lex->current);
    }
    
    // Single-character tokens
    switch (c) {
        case '(': return make_token(lex, TOKEN_LPAREN, lex->current - 1, lex->current);
        case ')': return make_token(lex, TOKEN_RPAREN, lex->current - 1, lex->current);
        case '[': return make_token(lex, TOKEN_LBRACKET, lex->current - 1, lex->current);
        case ']': return make_token(lex, TOKEN_RBRACKET, lex->current - 1, lex->current);
        case '{': return make_token(lex, TOKEN_LBRACE, lex->current - 1, lex->current);
        case '}': return make_token(lex, TOKEN_RBRACE, lex->current - 1, lex->current);
        case ',': return make_token(lex, TOKEN_COMMA, lex->current - 1, lex->current);
        case ':': return make_token(lex, TOKEN_COLON, lex->current - 1, lex->current);
        case '+': return make_token(lex, TOKEN_PLUS, lex->current - 1, lex->current);
        case '/': return make_token(lex, TOKEN_SLASH, lex->current - 1, lex->current);
        case '%': return make_token(lex, TOKEN_PERCENT, lex->current - 1, lex->current);
        case '_': return make_token(lex, TOKEN_UNDERSCORE, lex->current - 1, lex->current);
        case '@': return make_token(lex, TOKEN_AT, lex->current - 1, lex->current);
    }
    
    // Unknown character
    return make_token(lex, TOKEN_ERROR, lex->current - 1, lex->current);
}

Token lexer_peek(Lexer* lex) {
    // Save lexer state
    const char* saved_current = lex->current;
    size_t saved_line = lex->line;
    size_t saved_column = lex->column;
    
    Token tok = lexer_next(lex);
    
    // Restore lexer state
    lex->current = saved_current;
    lex->line = saved_line;
    lex->column = saved_column;
    
    return tok;
}

LexerState lexer_save(Lexer* lex) {
    return (LexerState){
        .current = lex->current,
        .line = lex->line,
        .column = lex->column,
    };
}

void lexer_restore(Lexer* lex, LexerState state) {
    lex->current = state.current;
    lex->line = state.line;
    lex->column = state.column;
}

bool lexer_is_eof(Lexer* lex) {
    assert(lex != NULL);
    skip_whitespace(lex);
    return is_at_end(lex);
}
