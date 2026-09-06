/**
 * @file lexer.h
 * @brief Fern Lexer - Tokenizes source code into a stream of tokens.
 *
 * The lexer is the first stage of the compiler pipeline. It reads raw
 * source code and produces tokens that the parser can consume.
 *
 * Uses arena allocation for memory safety - all allocations are freed
 * together when the arena is destroyed.
 *
 * @example
 *     Arena* arena = arena_create(4096);
 *     Lexer* lex = lexer_new(arena, "let x = 42");
 *     while (!lexer_is_eof(lex)) {
 *         Token tok = lexer_next(lex);
 *         printf("%s: %s\n", token_type_name(tok.type), string_cstr(tok.text));
 *     }
 *     arena_destroy(arena);
 */

#ifndef FERN_LEXER_H
#define FERN_LEXER_H

#include "arena.h"
#include "token.h"
#include "fern_string.h"

/** @brief Opaque lexer state. */
typedef struct Lexer Lexer;

/**
 * @brief Create a new lexer for the given source code.
 *
 * The lexer will tokenize the source string from beginning to end.
 * Use lexer_next() to retrieve tokens one at a time.
 *
 * @param arena Memory arena for allocations (must not be NULL)
 * @param source Source code string to tokenize (must not be NULL)
 * @return New Lexer instance, never NULL
 *
 * @see lexer_next, lexer_peek
 */
Lexer* lexer_new(Arena* arena, const char* source);

/**
 * @brief Get the next token from the lexer.
 *
 * Advances the lexer position and returns the next token.
 * Returns TOKEN_EOF when the end of input is reached.
 *
 * @param lex Lexer instance (must not be NULL)
 * @return Next token in the stream
 *
 * @see lexer_peek
 */
Token lexer_next(Lexer* lex);

/**
 * @brief Peek at the next token without consuming it.
 *
 * Returns the next token but does not advance the lexer position.
 * Multiple calls to peek() return the same token.
 *
 * @param lex Lexer instance (must not be NULL)
 * @return Next token in the stream (without consuming)
 *
 * @see lexer_next
 */
Token lexer_peek(Lexer* lex);

/**
 * @brief Check if the lexer has reached end of file.
 *
 * @param lex Lexer instance (must not be NULL)
 * @return true if at EOF, false otherwise
 */
bool lexer_is_eof(Lexer* lex);

/**
 * @brief Snapshot of lexer state for save/restore.
 *
 * Used for speculative parsing where we may need to backtrack
 * if a parse attempt fails.
 *
 * @see lexer_save, lexer_restore
 */
#define FERN_LEXER_MAX_INDENT_LEVELS 100

typedef struct {
    const char* current;  /**< Current position in source */
    size_t line;          /**< Current line number */
    size_t column;        /**< Current column number */
    int interp_depth;     /**< Active string interpolation depth */
    int interp_brace_depth; /**< Nested braces inside interpolation */
    int bracket_depth;    /**< Layout suppression inside delimiters */
    int indent_stack[FERN_LEXER_MAX_INDENT_LEVELS]; /**< Saved indentation columns */
    int indent_top;       /**< Active indentation stack index */
    int pending_dedents;  /**< Dedents still awaiting emission */
    bool at_line_start;   /**< Whether indentation must be scanned */
    bool emit_newline;    /**< Whether a newline token is pending */
} LexerState;

/**
 * @brief Save the current lexer state.
 *
 * @param lex Lexer instance (must not be NULL)
 * @return Snapshot of current state
 *
 * @see lexer_restore
 */
LexerState lexer_save(Lexer* lex);

/**
 * @brief Restore a previously saved lexer state.
 *
 * @param lex Lexer instance (must not be NULL)
 * @param state Unmodified state from lexer_save() for this same live lexer
 *
 * @see lexer_save
 */
void lexer_restore(Lexer* lex, LexerState state);

#endif /* FERN_LEXER_H */
