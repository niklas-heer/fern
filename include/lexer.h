/* Fern Lexer
 * 
 * Tokenizes Fern source code into a stream of tokens.
 * Uses arena allocation for memory safety.
 */

#ifndef FERN_LEXER_H
#define FERN_LEXER_H

#include "arena.h"
#include "token.h"
#include "fern_string.h"

/* Lexer state */
typedef struct Lexer Lexer;

/* Create a new lexer for the given source code */
Lexer* lexer_new(Arena* arena, const char* source);

/* Get the next token from the lexer */
Token lexer_next(Lexer* lex);

/* Peek at the next token without consuming it */
Token lexer_peek(Lexer* lex);

/* Check if we're at end of file */
bool lexer_is_eof(Lexer* lex);

/* Lexer state snapshot for save/restore (used for speculative parsing) */
typedef struct {
    const char* current;
    size_t line;
    size_t column;
} LexerState;

/* Save and restore lexer state */
LexerState lexer_save(Lexer* lex);
void lexer_restore(Lexer* lex, LexerState state);

#endif /* FERN_LEXER_H */
