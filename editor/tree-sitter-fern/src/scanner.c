/** Authored indentation scanner. Decision84 permits the required Tree-sitter allocator lifecycle. */
#include <tree_sitter/parser.h>
#include <tree_sitter/alloc.h>
#include <stdint.h>
#include <string.h>

/* Ordering is part of the grammar/scanner ABI. */
enum TokenType { NEWLINE, INDENT, DEDENT };
#define INDENT_LEVELS 128
#define COLUMN_MAX (1024u * 1024u)
typedef struct {
    uint32_t columns[INDENT_LEVELS];
    uint16_t count;
    bool boundary;
} Scanner;
_Static_assert(2 + 4 * INDENT_LEVELS <= TREE_SITTER_SERIALIZATION_BUFFER_SIZE,
               "complete indentation state must fit Tree-sitter serialization");

/** Reset supplied state to the root level; also used for untrusted invalid serialized inputs. */
static void reset(Scanner *scanner) {
    memset(scanner, 0, sizeof(*scanner));
    scanner->count = 1;
}

/** Allocate one parser-owned state with Tree-sitter's allocator; return NULL on allocation failure. */
void *tree_sitter_fern_external_scanner_create(void) {
    Scanner *scanner = ts_calloc(1, sizeof(*scanner));
    if (scanner != NULL) reset(scanner);
    return scanner;
}

/** Release only parser-owned scanner state through the matching Tree-sitter allocator. */
void tree_sitter_fern_external_scanner_destroy(void *payload) {
    ts_free(payload);
}

/** Serialize every active level in a portable bounded format; return zero only for invalid state. */
unsigned tree_sitter_fern_external_scanner_serialize(void *payload, char *buffer) {
    const Scanner *scanner = payload;
    if (scanner == NULL || scanner->count == 0 || scanner->count > INDENT_LEVELS) return 0;
    buffer[0] = (char)(scanner->count & 255);
    buffer[1] = scanner->boundary ? (char)128 : 0;
    for (unsigned i = 0; i < scanner->count; i++) {
        for (unsigned byte = 0; byte < 4; byte++) {
            buffer[2 + i * 4 + byte] = (char)(scanner->columns[i] >> (byte * 8));
        }
    }
    return 2 + 4 * scanner->count;
}

/** Restore checked ascending columns; malformed lengths/counts/columns reset without assertions. */
void tree_sitter_fern_external_scanner_deserialize(void *payload, const char *buffer, unsigned length) {
    Scanner *scanner = payload;
    if (scanner == NULL) return;
    reset(scanner);
    if (length < 6 || length > 2 + 4 * INDENT_LEVELS) return;
    unsigned count = (unsigned char)buffer[0];
    if (((unsigned char)buffer[1] & 127) != 0) return;
    if (count == 0 || count > INDENT_LEVELS || length != 2 + 4 * count) return;
    for (unsigned i = 0; i < count; i++) {
        uint32_t column = 0;
        for (unsigned byte = 0; byte < 4; byte++) {
            column |= (uint32_t)(unsigned char)buffer[2 + i * 4 + byte] << (byte * 8);
        }
        if (column > COLUMN_MAX || (i == 0 ? column != 0 : column <= scanner->columns[i - 1])) {
            reset(scanner);
            return;
        }
        scanner->columns[i] = column;
    }
    scanner->count = (uint16_t)count;
    scanner->boundary = (unsigned char)buffer[1] == 128;
}

/** Consume exactly one logical line ending when requested; CRLF remains a single newline token. */
static bool newline(TSLexer *lexer, const bool *valid) {
    if (!valid[NEWLINE] || (lexer->lookahead != '\n' && lexer->lookahead != '\r')) return false;
    int32_t first = lexer->lookahead;
    lexer->advance(lexer, false);
    if (first == '\r' && lexer->lookahead == '\n') lexer->advance(lexer, false);
    lexer->mark_end(lexer);
    lexer->result_symbol = NEWLINE;
    return true;
}

/** Emit one bounded indentation transition, preserving remaining dedents for subsequent scans. */
static bool indentation(Scanner *scanner, TSLexer *lexer, const bool *valid, uint32_t column, bool at_start) {
    uint32_t current = scanner->columns[scanner->count - 1];
    if (valid[DEDENT] && scanner->count > 1 && (lexer->eof(lexer) || column < current)) {
        scanner->count--;
        scanner->boundary = !lexer->eof(lexer) && lexer->lookahead != ')' &&
            lexer->lookahead != ']' && lexer->lookahead != '}' && lexer->lookahead != ',';
        lexer->result_symbol = DEDENT;
        lexer->mark_end(lexer);
        return true;
    }
    if (scanner->boundary && valid[NEWLINE] && column >= current) {
        scanner->boundary = false;
        lexer->result_symbol = NEWLINE;
        lexer->mark_end(lexer);
        return true;
    }
    if (at_start && valid[INDENT] && !lexer->eof(lexer) && column > current && column <= COLUMN_MAX && scanner->count < INDENT_LEVELS) {
        scanner->columns[scanner->count++] = column;
        lexer->result_symbol = INDENT;
        lexer->mark_end(lexer);
        return true;
    }
    return false;
}

/** Scan only valid external tokens; blank/comment lines never change the indentation stack. */
bool tree_sitter_fern_external_scanner_scan(void *payload, TSLexer *lexer, const bool *valid) {
    Scanner *scanner = payload;
    if (scanner == NULL || scanner->count == 0 || scanner->count > INDENT_LEVELS) return false;
    if (newline(lexer, valid)) return true;
    unsigned spaces = 0;
    while (lexer->lookahead == ' ' && spaces < COLUMN_MAX) {
        lexer->advance(lexer, true);
        spaces++;
    }
    if (lexer->lookahead == ' ' || lexer->lookahead == '\t' || lexer->lookahead == '#') return false;
    if (newline(lexer, valid)) return true;
    return indentation(scanner, lexer, valid, lexer->get_column(lexer),
                       lexer->get_column(lexer) == spaces);
}
