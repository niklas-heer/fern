/* Fern Lexer Implementation */

#include "lexer.h"
#include <assert.h>
#include <ctype.h>
#include <string.h>

#define MAX_INDENT_LEVELS 100

struct Lexer {
    Arena* arena;
    const char* source;
    const char* current;
    String* filename;
    size_t line;
    size_t column;
    int interp_depth;       // > 0 when inside string interpolation {expr}
    int interp_brace_depth; // tracks nested {} inside interpolation expr
    int bracket_depth;      // tracks nesting of (), [], {} - no INDENT/DEDENT inside
    
    // Indentation tracking
    int indent_stack[MAX_INDENT_LEVELS];  // Stack of indentation levels
    int indent_top;                        // Top of indent stack (index)
    int pending_dedents;                   // Number of DEDENT tokens to emit
    bool at_line_start;                    // True if at the beginning of a line
    bool emit_newline;                     // True if we need to emit a NEWLINE
};

/**
 * Check if at end of source.
 * @param lex The lexer.
 * @return True if at end.
 */
static bool is_at_end(Lexer* lex) {
    assert(lex != NULL);
    assert(lex->current != NULL);
    return *lex->current == '\0';
}

/**
 * Peek current character.
 * @param lex The lexer.
 * @return The current character.
 */
static char peek(Lexer* lex) {
    assert(lex != NULL);
    assert(lex->current != NULL);
    return *lex->current;
}

static char peek_next(Lexer* lex) __attribute__((unused));

/**
 * Peek at the next character without consuming.
 * @param lex The lexer.
 * @return The next character.
 */
static char peek_next(Lexer* lex) {
    assert(lex != NULL);
    assert(lex->current != NULL);
    if (is_at_end(lex)) return '\0';
    return lex->current[1];
}

/**
 * Advance to next character.
 * @param lex The lexer.
 * @return The consumed character.
 */
static char advance(Lexer* lex) {
    assert(lex != NULL);
    assert(lex->current != NULL);
    char c = *lex->current++;
    if (c == '\n') {
        lex->line++;
        lex->column = 1;
    } else {
        lex->column++;
    }
    return c;
}

/**
 * Match expected character.
 * @param lex The lexer.
 * @param expected The expected character.
 * @return True if matched and consumed.
 */
static bool match(Lexer* lex, char expected) {
    assert(lex != NULL);
    assert(lex->current != NULL);
    if (is_at_end(lex)) return false;
    if (*lex->current != expected) return false;
    advance(lex);
    return true;
}

/**
 * Skip horizontal whitespace only (spaces, tabs, carriage return).
 * @param lex The lexer.
 */
static void skip_horizontal_whitespace(Lexer* lex) {
    assert(lex != NULL);
    assert(lex->current != NULL);
    while (!is_at_end(lex)) {
        char c = peek(lex);
        if (c == ' ' || c == '\t' || c == '\r') {
            advance(lex);
        } else {
            break;
        }
    }
}

/**
 * Skip a single-line comment (# ...).
 * @param lex The lexer.
 */
static void skip_line_comment(Lexer* lex) {
    // FERN_STYLE: allow(assertion-density) trivial loop bounded by line end
    assert(lex != NULL);
    while (!is_at_end(lex) && peek(lex) != '\n') {
        advance(lex);
    }
}

/**
 * Skip a block comment.
 * @param lex The lexer.
 */
static void skip_block_comment(Lexer* lex) {
    // FERN_STYLE: allow(assertion-density) trivial loop bounded by comment end
    assert(lex != NULL);
    advance(lex); // consume '/'
    advance(lex); // consume '*'
    while (!is_at_end(lex)) {
        if (peek(lex) == '*' && peek_next(lex) == '/') {
            advance(lex); // consume '*'
            advance(lex); // consume '/'
            break;
        }
        advance(lex);
    }
}

/**
 * Count indentation (spaces/tabs) at current position.
 * @param lex The lexer.
 * @return The indentation level.
 */
static int count_indentation(Lexer* lex) {
    // FERN_STYLE: allow(assertion-density) simple loop counting whitespace
    assert(lex != NULL);
    int indent = 0;
    const char* p = lex->current;
    while (*p == ' ' || *p == '\t') {
        if (*p == ' ') {
            indent += 1;
        } else {
            // Tab = advance to next multiple of 8
            indent = (indent + 8) & ~7;
        }
        p++;
    }
    return indent;
}

/**
 * Advance past indentation whitespace.
 * @param lex The lexer.
 */
static void consume_indentation(Lexer* lex) {
    // FERN_STYLE: allow(assertion-density) simple whitespace consumer
    assert(lex != NULL);
    while (!is_at_end(lex) && (peek(lex) == ' ' || peek(lex) == '\t')) {
        advance(lex);
    }
}

/**
 * Check if byte starts a multi-byte UTF-8 sequence.
 * @param c The byte to check.
 * @return True if UTF-8 start byte.
 */
static bool is_utf8_start(unsigned char c) {
    // FERN_STYLE: allow(assertion-density) pure predicate with no state
    // UTF-8 multi-byte sequences start with 0b11xxxxxx (>= 0xC0)
    // Single-byte ASCII starts with 0b0xxxxxxx (< 0x80)
    return c >= 0xC0;
}

/**
 * Check if byte is a UTF-8 continuation byte.
 * @param c The byte to check.
 * @return True if UTF-8 continuation byte.
 */
static bool is_utf8_cont(unsigned char c) {
    // FERN_STYLE: allow(assertion-density) pure predicate with no state
    // UTF-8 continuation bytes are 0b10xxxxxx (0x80-0xBF)
    return (c & 0xC0) == 0x80;
}

/**
 * Check if character is valid at start of identifier.
 * @param c The character to check.
 * @return True if valid identifier start.
 */
static bool is_ident_start(char c) {
    // FERN_STYLE: allow(assertion-density) pure predicate with no state
    unsigned char uc = (unsigned char)c;
    // ASCII letters and underscore
    if (isalpha(c) || c == '_') return true;
    // UTF-8 multi-byte sequence start (Unicode letters, emoji, etc.)
    if (is_utf8_start(uc)) return true;
    return false;
}

/**
 * Check if character is valid in identifier (continuation).
 * @param c The character to check.
 * @return True if valid identifier continuation.
 */
static bool is_ident_cont(char c) {
    // FERN_STYLE: allow(assertion-density) pure predicate with no state
    unsigned char uc = (unsigned char)c;
    // ASCII letters, digits, and underscore
    if (isalnum(c) || c == '_') return true;
    // UTF-8 multi-byte sequence start (Unicode letters, emoji, etc.)
    if (is_utf8_start(uc)) return true;
    // UTF-8 continuation bytes (part of a multi-byte character)
    if (is_utf8_cont(uc)) return true;
    return false;
}

/**
 * Create token.
 * @param lex The lexer.
 * @param type The token type.
 * @param start Start of token text.
 * @param end End of token text.
 * @return The new token.
 */
static Token make_token(Lexer* lex, TokenType type, const char* start, const char* end) {
    assert(lex != NULL);
    assert(start != NULL && end != NULL && end >= start);
    Token tok;
    tok.type = type;
    
    size_t len = end - start;
    tok.text = string_new_len(lex->arena, start, len);
    
    tok.loc.filename = lex->filename;
    tok.loc.line = lex->line;
    tok.loc.column = lex->column;
    
    return tok;
}

/**
 * Process escape sequences in a string, converting \{ to { and \} to }.
 * Also handles standard escapes: \n, \t, \r, \\, \".
 * @param arena The arena for allocation.
 * @param start Start of string content.
 * @param end End of string content.
 * @return New string with escapes processed.
 */
static String* process_string_escapes(Arena* arena, const char* start, const char* end) {
    assert(arena != NULL);
    assert(start != NULL && end != NULL);
    
    size_t len = end - start;
    /* Allocate buffer - result will be at most len bytes (escapes shrink) */
    char* buf = arena_alloc(arena, len + 1);
    size_t out = 0;
    
    for (const char* p = start; p < end; p++) {
        if (*p == '\\' && p + 1 < end) {
            char next = *(p + 1);
            switch (next) {
                case 'n':  buf[out++] = '\n'; p++; break;
                case 't':  buf[out++] = '\t'; p++; break;
                case 'r':  buf[out++] = '\r'; p++; break;
                case '\\': buf[out++] = '\\'; p++; break;
                case '"':  buf[out++] = '"';  p++; break;
                case '{':  buf[out++] = '{';  p++; break;
                case '}':  buf[out++] = '}';  p++; break;
                default:
                    /* Unknown escape - keep as-is */
                    buf[out++] = *p;
                    break;
            }
        } else {
            buf[out++] = *p;
        }
    }
    buf[out] = '\0';
    
    return string_new_len(arena, buf, out);
}

/**
 * Create string token with escape processing.
 * @param lex The lexer.
 * @param type The token type.
 * @param start Start of string content.
 * @param end End of string content.
 * @return The new token with processed escapes.
 */
static Token make_string_token(Lexer* lex, TokenType type, const char* start, const char* end) {
    assert(lex != NULL);
    assert(start != NULL && end != NULL && end >= start);
    Token tok;
    tok.type = type;
    
    tok.text = process_string_escapes(lex->arena, start, end);
    
    tok.loc.filename = lex->filename;
    tok.loc.line = lex->line;
    tok.loc.column = lex->column;
    
    return tok;
}

/**
 * Check if string matches keyword.
 * @param start Start of string.
 * @param length Length of string.
 * @param rest The keyword to match.
 * @param type The token type if matched.
 * @return The token type.
 */
static TokenType check_keyword(const char* start, size_t length, const char* rest, TokenType type) {
    assert(start != NULL);
    assert(rest != NULL);
    if (strlen(rest) == length && memcmp(start, rest, length) == 0) {
        return type;
    }
    return TOKEN_IDENT;
}

/**
 * Identify keyword or identifier.
 * @param start Start of identifier.
 * @param length Length of identifier.
 * @return The token type.
 */
static TokenType identifier_type(const char* start, size_t length) {
    // FERN_STYLE: allow(function-length) keyword lookup requires one case per keyword
    assert(start != NULL);
    assert(length > 0);
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

        case 'w':
            if (length == 4) return check_keyword(start, length, "with", TOKEN_WITH);
            if (length == 5) return check_keyword(start, length, "where", TOKEN_WHERE);
            break;
    }
    
    return TOKEN_IDENT;
}

/**
 * Lex identifier or keyword.
 * @param lex The lexer.
 * @return The token.
 */
static Token lex_identifier(Lexer* lex) {
    // FERN_STYLE: allow(bounded-loops) loop bounded by input source length
    assert(lex != NULL);
    assert(lex->current != NULL);
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

/**
 * Lex number (integer or float).
 * @param lex The lexer.
 * @return The token.
 */
static Token lex_number(Lexer* lex) {
    // FERN_STYLE: allow(bounded-loops) loop bounded by input source length
    assert(lex != NULL);
    assert(lex->current != NULL);
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

/**
 * Lex a string segment: from current position to next { or closing ".
 * @param lex The lexer.
 * @param if_interp Token type if interpolation found.
 * @param if_end Token type if string ends.
 * @return The token.
 */
static Token lex_string_segment(Lexer* lex, TokenType if_interp, TokenType if_end) {
    assert(lex != NULL);
    assert(lex->current != NULL);
    const char* start = lex->current;

    while (!is_at_end(lex) && peek(lex) != '"') {
        if (peek(lex) == '\\') {
            advance(lex);  // consume backslash
            if (!is_at_end(lex)) advance(lex);  // consume escaped char
            continue;
        }
        /* Unescaped { starts interpolation, but only if not followed by " */
        if (peek(lex) == '{' && peek_next(lex) != '"') break;
        advance(lex);
    }

    if (is_at_end(lex)) {
        return make_token(lex, TOKEN_ERROR, start, lex->current);
    }

    const char* end = lex->current;

    /* Only start interpolation if { is not followed by closing quote */
    if (peek(lex) == '{' && peek_next(lex) != '"') {
        advance(lex); // consume '{'
        lex->interp_depth++;
        lex->interp_brace_depth = 0;
        return make_string_token(lex, if_interp, start, end);
    }

    // peek is '"' - include any trailing { in the string
    advance(lex); // consume closing quote
    return make_string_token(lex, if_end, start, end);
}

/**
 * Lex a string literal, handling interpolation.
 * @param lex The lexer.
 * @return The token.
 */
static Token lex_string(Lexer* lex) {
    assert(lex != NULL);
    assert(lex->current != NULL);
    const char* start = lex->current;  // After opening quote

    while (!is_at_end(lex) && peek(lex) != '"') {
        if (peek(lex) == '\\') {
            advance(lex);  // consume backslash
            if (!is_at_end(lex)) advance(lex);  // consume escaped char
            continue;
        }
        /* Unescaped { starts interpolation, but only if not followed by " */
        if (peek(lex) == '{' && peek_next(lex) != '"') break;
        advance(lex);
    }

    if (is_at_end(lex)) {
        return make_token(lex, TOKEN_ERROR, start, lex->current);
    }

    const char* end = lex->current;

    /* Only start interpolation if { is not followed by closing quote */
    if (peek(lex) == '{' && peek_next(lex) != '"') {
        // String with interpolation: "Hello, {name}"
        advance(lex); // consume '{'
        lex->interp_depth++;
        lex->interp_brace_depth = 0;
        return make_string_token(lex, TOKEN_STRING_BEGIN, start, end);
    }

    // Regular string (no interpolation) - include any trailing { in the string
    advance(lex);  // Closing quote
    return make_string_token(lex, TOKEN_STRING, start, end);
}

/**
 * Skip all whitespace including newlines (for backward compatibility).
 * @param lex The lexer.
 */
static void skip_whitespace(Lexer* lex) {
    // FERN_STYLE: allow(assertion-density) simple whitespace skipper
    assert(lex != NULL);
    while (!is_at_end(lex)) {
        char c = peek(lex);
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            advance(lex);
        } else if (c == '#') {
            skip_line_comment(lex);
        } else if (c == '/' && peek_next(lex) == '*') {
            skip_block_comment(lex);
        } else {
            break;
        }
    }
}

/**
 * Create a new lexer for the given source code.
 * @param arena The arena for allocations.
 * @param source The source code.
 * @return The new lexer.
 */
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
    lex->interp_depth = 0;
    lex->interp_brace_depth = 0;
    lex->bracket_depth = 0;
    
    // Initialize indentation tracking
    lex->indent_stack[0] = 0;  // Start with indent level 0
    lex->indent_top = 0;
    lex->pending_dedents = 0;
    lex->at_line_start = true;  // Start of file is like start of line
    lex->emit_newline = false;
    
    return lex;
}

/**
 * Lex a single token (without indentation handling).
 * @param lex The lexer.
 * @return The token.
 */
static Token lex_token(Lexer* lex) {
    // FERN_STYLE: allow(function-length) token lexing requires many cases
    assert(lex != NULL);
    assert(lex->current != NULL);
    
    skip_horizontal_whitespace(lex);
    
    // Skip comments
    while (!is_at_end(lex)) {
        if (peek(lex) == '#') {
            skip_line_comment(lex);
        } else if (peek(lex) == '/' && peek_next(lex) == '*') {
            skip_block_comment(lex);
            skip_horizontal_whitespace(lex);
        } else {
            break;
        }
    }
    
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
    // Track bracket depth for indentation handling (no INDENT/DEDENT inside brackets)
    switch (c) {
        case '(': lex->bracket_depth++; return make_token(lex, TOKEN_LPAREN, lex->current - 1, lex->current);
        case ')': if (lex->bracket_depth > 0) lex->bracket_depth--; return make_token(lex, TOKEN_RPAREN, lex->current - 1, lex->current);
        case '[': lex->bracket_depth++; return make_token(lex, TOKEN_LBRACKET, lex->current - 1, lex->current);
        case ']': if (lex->bracket_depth > 0) lex->bracket_depth--; return make_token(lex, TOKEN_RBRACKET, lex->current - 1, lex->current);
        case '{': lex->bracket_depth++; return make_token(lex, TOKEN_LBRACE, lex->current - 1, lex->current);
        case '}': if (lex->bracket_depth > 0) lex->bracket_depth--; return make_token(lex, TOKEN_RBRACE, lex->current - 1, lex->current);
        case ',': return make_token(lex, TOKEN_COMMA, lex->current - 1, lex->current);
        case ':': return make_token(lex, TOKEN_COLON, lex->current - 1, lex->current);
        case '+': return make_token(lex, TOKEN_PLUS, lex->current - 1, lex->current);
        case '/': return make_token(lex, TOKEN_SLASH, lex->current - 1, lex->current);
        case '%': return make_token(lex, TOKEN_PERCENT, lex->current - 1, lex->current);
        case '_': return make_token(lex, TOKEN_UNDERSCORE, lex->current - 1, lex->current);
        case '@': return make_token(lex, TOKEN_AT, lex->current - 1, lex->current);
        case '?': return make_token(lex, TOKEN_QUESTION, lex->current - 1, lex->current);
    }
    
    // Unknown character
    return make_token(lex, TOKEN_ERROR, lex->current - 1, lex->current);
}

/**
 * Get the next token from the lexer.
 * @param lex The lexer.
 * @return The next token.
 */
Token lexer_next(Lexer* lex) {
    // FERN_STYLE: allow(function-length) indentation state machine is complex
    assert(lex != NULL);
    assert(lex->current != NULL);
    
    // When inside brackets/parens/braces, skip all indentation handling
    // This allows multi-line expressions inside delimiters
    if (lex->bracket_depth > 0) {
        // Clear any pending layout tokens - they'll be re-computed when we exit brackets
        lex->pending_dedents = 0;
        lex->emit_newline = false;
        lex->at_line_start = false;
        // Skip newlines inside brackets (they're just whitespace in this context)
        while (peek(lex) == '\n') {
            advance(lex);
            // Also skip any leading whitespace on continuation lines
            skip_horizontal_whitespace(lex);
            // Skip comments on blank lines
            if (peek(lex) == '#') {
                skip_line_comment(lex);
            }
        }
        Token tok = lex_token(lex);
        // If we just closed the last bracket, handle trailing newline normally
        if (lex->bracket_depth == 0) {
            skip_horizontal_whitespace(lex);
            if (peek(lex) == '#') {
                skip_line_comment(lex);
            }
            if (peek(lex) == '\n') {
                advance(lex);
                lex->emit_newline = true;
            }
        }
        return tok;
    }
    
    // First, emit any pending DEDENT tokens
    if (lex->pending_dedents > 0) {
        lex->pending_dedents--;
        return make_token(lex, TOKEN_DEDENT, lex->current, lex->current);
    }
    
    // If we need to emit a NEWLINE from previous iteration
    if (lex->emit_newline) {
        lex->emit_newline = false;
        lex->at_line_start = true;
        return make_token(lex, TOKEN_NEWLINE, lex->current, lex->current);
    }
    
    // Handle start of line - process indentation
    if (lex->at_line_start) {
        lex->at_line_start = false;
        
        // Skip blank lines and comment-only lines
        // Important: We must count indentation BEFORE consuming whitespace
        // FERN_STYLE: allow(bounded-loops) loop bounded by source length via is_at_end
        while (!is_at_end(lex)) {
            // Count indentation at this line (before consuming anything)
            int line_indent = count_indentation(lex);
            
            // Now skip the whitespace
            consume_indentation(lex);
            
            // Skip line comments
            if (peek(lex) == '#') {
                skip_line_comment(lex);
            }
            
            // Skip block comments
            while (peek(lex) == '/' && peek_next(lex) == '*') {
                skip_block_comment(lex);
                skip_horizontal_whitespace(lex);
            }
            
            // If we hit a newline, this is a blank/comment-only line - skip it
            if (peek(lex) == '\n') {
                advance(lex);  // consume newline
                continue;
            }
            
            // If we hit EOF or actual content, process this line's indentation
            if (is_at_end(lex)) {
                // At EOF, emit any remaining DEDENT tokens
                if (lex->indent_top > 0) {
                    lex->indent_top--;
                    lex->pending_dedents = lex->indent_top;
                    return make_token(lex, TOKEN_DEDENT, lex->current, lex->current);
                }
                return make_token(lex, TOKEN_EOF, lex->current, lex->current);
            }
            
            // We have actual content - compare indentation
            int current_indent = lex->indent_stack[lex->indent_top];
            
            if (line_indent > current_indent) {
                // Increased indentation - push new level and emit INDENT
                lex->indent_top++;
                assert(lex->indent_top < MAX_INDENT_LEVELS);
                lex->indent_stack[lex->indent_top] = line_indent;
                return make_token(lex, TOKEN_INDENT, lex->current, lex->current);
            } else if (line_indent < current_indent) {
                // Decreased indentation - pop levels and queue DEDENT tokens
                int dedent_count = 0;
                while (lex->indent_top > 0 && 
                       lex->indent_stack[lex->indent_top] > line_indent) {
                    lex->indent_top--;
                    dedent_count++;
                }
                if (dedent_count > 1) {
                    lex->pending_dedents = dedent_count - 1;
                }
                if (dedent_count > 0) {
                    return make_token(lex, TOKEN_DEDENT, lex->current, lex->current);
                }
            }
            // Same indentation or processed indent change - break to lex content
            break;
        }
    }
    
    // Now lex the actual token
    Token tok = lex_token(lex);
    
    // If we just lexed something and the next char is newline, mark it
    // But only if not inside brackets (multi-line expressions in brackets ignore layout)
    if (tok.type != TOKEN_EOF && tok.type != TOKEN_ERROR && lex->bracket_depth == 0) {
        skip_horizontal_whitespace(lex);
        // Skip trailing comments on the same line
        if (peek(lex) == '#') {
            skip_line_comment(lex);
        }
        if (peek(lex) == '\n') {
            advance(lex);  // consume the newline
            lex->emit_newline = true;
        }
    }
    
    // At EOF, emit remaining DEDENT tokens
    if (tok.type == TOKEN_EOF && lex->indent_top > 0) {
        lex->indent_top--;
        lex->pending_dedents = lex->indent_top;
        return make_token(lex, TOKEN_DEDENT, lex->current, lex->current);
    }
    
    return tok;
}

/**
 * Peek at the next token without consuming it.
 * @param lex The lexer.
 * @return The next token.
 */
Token lexer_peek(Lexer* lex) {
    assert(lex != NULL);
    assert(lex->current != NULL);
    
    /* Save ALL lexer state - not just position */
    const char* saved_current = lex->current;
    size_t saved_line = lex->line;
    size_t saved_column = lex->column;
    int saved_interp_depth = lex->interp_depth;
    int saved_interp_brace_depth = lex->interp_brace_depth;
    int saved_indent_top = lex->indent_top;
    int saved_pending_dedents = lex->pending_dedents;
    bool saved_at_line_start = lex->at_line_start;
    bool saved_emit_newline = lex->emit_newline;
    int saved_indent_stack[MAX_INDENT_LEVELS];
    memcpy(saved_indent_stack, lex->indent_stack, sizeof(lex->indent_stack));
    
    Token tok = lexer_next(lex);
    
    /* Restore ALL lexer state */
    lex->current = saved_current;
    lex->line = saved_line;
    lex->column = saved_column;
    lex->interp_depth = saved_interp_depth;
    lex->interp_brace_depth = saved_interp_brace_depth;
    lex->indent_top = saved_indent_top;
    lex->pending_dedents = saved_pending_dedents;
    lex->at_line_start = saved_at_line_start;
    lex->emit_newline = saved_emit_newline;
    memcpy(lex->indent_stack, saved_indent_stack, sizeof(lex->indent_stack));
    
    return tok;
}

/**
 * Save the current lexer state for later restoration.
 * @param lex The lexer.
 * @return The saved state.
 */
LexerState lexer_save(Lexer* lex) {
    assert(lex != NULL);
    assert(lex->current != NULL);
    return (LexerState){
        .current = lex->current,
        .line = lex->line,
        .column = lex->column,
    };
}

/**
 * Restore the lexer to a previously saved state.
 * @param lex The lexer.
 * @param state The state to restore.
 */
void lexer_restore(Lexer* lex, LexerState state) {
    assert(lex != NULL);
    assert(state.current != NULL);
    lex->current = state.current;
    lex->line = state.line;
    lex->column = state.column;
}

/**
 * Check if the lexer has reached end of file.
 * @param lex The lexer.
 * @return True if at end of file.
 */
bool lexer_is_eof(Lexer* lex) {
    assert(lex != NULL);
    assert(lex->current != NULL);
    skip_whitespace(lex);
    return is_at_end(lex);
}
