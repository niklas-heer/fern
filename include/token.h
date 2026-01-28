/* Fern Lexer - Token Types
 * 
 * Defines all token types for the Fern language based on DESIGN.md specification.
 */

#ifndef FERN_TOKEN_H
#define FERN_TOKEN_H

#include "fern_string.h"
#include <stddef.h>

/* Token types for Fern language */
typedef enum {
    /* Special tokens */
    TOKEN_EOF,
    TOKEN_ERROR,
    TOKEN_NEWLINE,
    TOKEN_INDENT,
    TOKEN_DEDENT,
    
    /* Literals */
    TOKEN_INT,          // 42, 0xFF, 0b1010, 0o755
    TOKEN_FLOAT,        // 3.14, 1.0e10
    TOKEN_STRING,       // "hello", """multi-line"""
    TOKEN_TRUE,         // true
    TOKEN_FALSE,        // false
    
    /* Identifiers and keywords */
    TOKEN_IDENT,        // variable_name, function_name
    
    /* Keywords */
    TOKEN_LET,          // let
    TOKEN_FN,           // fn
    TOKEN_RETURN,       // return
    TOKEN_IF,           // if
    TOKEN_ELSE,         // else
    TOKEN_MATCH,        // match
    TOKEN_WITH,         // with
    TOKEN_DO,           // do
    TOKEN_DEFER,        // defer
    TOKEN_PUB,          // pub
    TOKEN_IMPORT,       // import
    TOKEN_TYPE,         // type
    TOKEN_TRAIT,        // trait
    TOKEN_IMPL,         // impl
    TOKEN_AND,          // and
    TOKEN_OR,           // or
    TOKEN_NOT,          // not
    TOKEN_AS,           // as
    TOKEN_MODULE,       // module
    TOKEN_FOR,          // for
    TOKEN_WHILE,        // while
    TOKEN_LOOP,         // loop
    TOKEN_BREAK,        // break
    TOKEN_CONTINUE,     // continue
    TOKEN_IN,           // in
    TOKEN_SPAWN,        // spawn
    TOKEN_SEND,         // send
    TOKEN_RECEIVE,      // receive
    
    /* Operators */
    TOKEN_PLUS,         // +
    TOKEN_MINUS,        // -
    TOKEN_STAR,         // *
    TOKEN_SLASH,        // /
    TOKEN_PERCENT,      // %
    TOKEN_POWER,        // **
    
    TOKEN_EQ,           // ==
    TOKEN_NE,           // !=
    TOKEN_LT,           // <
    TOKEN_LE,           // <=
    TOKEN_GT,           // >
    TOKEN_GE,           // >=
    
    TOKEN_ASSIGN,       // =
    TOKEN_BIND,         // <-
    TOKEN_PIPE,         // |>
    TOKEN_BAR,          // |
    TOKEN_ARROW,        // ->
    TOKEN_FAT_ARROW,    // =>
    
    /* Delimiters */
    TOKEN_LPAREN,       // (
    TOKEN_RPAREN,       // )
    TOKEN_LBRACKET,     // [
    TOKEN_RBRACKET,     // ]
    TOKEN_LBRACE,       // {
    TOKEN_RBRACE,       // }
    
    TOKEN_COMMA,        // ,
    TOKEN_COLON,        // :
    TOKEN_DOT,          // .
    TOKEN_DOTDOT,       // ..
    TOKEN_DOTDOTEQ,     // ..=
    TOKEN_ELLIPSIS,     // ...
    TOKEN_UNDERSCORE,   // _
    TOKEN_AT,           // @
    
    /* String interpolation tokens */
    TOKEN_STRING_BEGIN, // "Hello, {     (text before first interpolation)
    TOKEN_STRING_MID,   // } world {    (text between interpolations)
    TOKEN_STRING_END,   // } end"       (text after last interpolation)
    
    /* Comments (usually skipped, but tracked for doc comments) */
    TOKEN_COMMENT,      // # comment
    TOKEN_BLOCK_COMMENT,  // /* comment */
    TOKEN_DOC_COMMENT,  // @doc """..."""
} TokenType;

/* Source location for error reporting */
typedef struct {
    String* filename;
    size_t line;
    size_t column;
} SourceLoc;

/* Token structure */
typedef struct {
    TokenType type;
    String* text;       // Actual text from source (for literals, identifiers)
    SourceLoc loc;      // Location in source
} Token;

/* Get human-readable name for token type */
const char* token_type_name(TokenType type);

/* Check if a token is a keyword */
bool token_is_keyword(TokenType type);

/* Check if a token is an operator */
bool token_is_operator(TokenType type);

/* Check if a token is a literal */
bool token_is_literal(TokenType type);

#endif /* FERN_TOKEN_H */
