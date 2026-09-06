/** Scanner ABI boundary regressions; malformed saved state must reset safely. */
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "../src/scanner.c"

/** Require a serialized reset state without accessing the scanner's private representation. */
static void check_reset(void *scanner) {
    char state[TREE_SITTER_SERIALIZATION_BUFFER_SIZE];
    unsigned length = tree_sitter_fern_external_scanner_serialize(scanner, state);
    assert(length == 6);
    assert((unsigned char)state[0] == 1 && state[1] == 0);
    assert(state[2] == 0 && state[3] == 0 && state[4] == 0 && state[5] == 0);
}

typedef struct { TSLexer lexer; uint32_t column; uint32_t leading; bool ended; } Mock;
/** Report a controlled column without manufacturing an enormous source allocation. */
static uint32_t mock_column(TSLexer *lexer) { return ((Mock *)lexer)->column; }
/** Model end-of-file independently from the lookahead code point. */
static bool mock_eof(const TSLexer *lexer) { return ((const Mock *)lexer)->ended; }
/** Advance virtual leading spaces without allocating a large source fixture. */
static void mock_advance(TSLexer *lexer, bool skip) {
    (void)skip;
    Mock *mock = (Mock *)lexer;
    if (mock->leading > 0) {
        mock->leading--;
        mock->column++;
        if (mock->leading == 0) lexer->lookahead = 'x';
    }
}
/** Marks do not affect our fixed-position lexer. */
static void mock_mark(TSLexer *lexer) { (void)lexer; }

/** Confirm 32-bit columns, exact maximum state serialization and incremental EOF restoration. */
static void check_columns(void *scanner) {
    Mock mock = {.lexer = {.lookahead = ' ', .advance = mock_advance,
        .mark_end = mock_mark, .get_column = mock_column, .eof = mock_eof}, .leading = 65536};
    bool valid[] = {false, true, false};
    char state[TREE_SITTER_SERIALIZATION_BUFFER_SIZE];
    assert(tree_sitter_fern_external_scanner_scan(scanner, &mock.lexer, valid));
    assert(mock.lexer.result_symbol == INDENT);
    unsigned size = tree_sitter_fern_external_scanner_serialize(scanner, state);
    assert(size == 10 && state[8] == 1 && state[9] == 0);
    tree_sitter_fern_external_scanner_deserialize(scanner, state, size);
    mock.ended = true;
    mock.column = 0;
    valid[INDENT] = false;
    valid[DEDENT] = true;
    assert(tree_sitter_fern_external_scanner_scan(scanner, &mock.lexer, valid));
    check_reset(scanner);
    mock.column = 65536;
    valid[DEDENT] = false;
    valid[INDENT] = true;
    assert(!tree_sitter_fern_external_scanner_scan(scanner, &mock.lexer, valid));
    check_reset(scanner);
}

/** Preserve all128 levels across save/restore, and refuse another level without corrupting state. */
static void check_stack(void *scanner) {
    char state[TREE_SITTER_SERIALIZATION_BUFFER_SIZE] = {0};
    state[0] = (char)128;
    for (unsigned i = 1; i < 128; i++) state[2 + 4*i] = (char)i;
    tree_sitter_fern_external_scanner_deserialize(scanner, state, 514);
    char saved[TREE_SITTER_SERIALIZATION_BUFFER_SIZE];
    assert(tree_sitter_fern_external_scanner_serialize(scanner, saved) == 514);
    assert(memcmp(state, saved, 514) == 0);
    Mock mock = {.lexer = {.lookahead = ' ', .advance = mock_advance,
        .mark_end = mock_mark, .get_column = mock_column, .eof = mock_eof}, .leading = 128};
    bool valid[] = {false, true, false};
    assert(!tree_sitter_fern_external_scanner_scan(scanner, &mock.lexer, valid));
    mock.ended = true;
    valid[INDENT] = false;
    valid[DEDENT] = true;
    for (unsigned i = 0; i < 127; i++) {
        assert(tree_sitter_fern_external_scanner_scan(scanner, &mock.lexer, valid));
    }
    assert(!tree_sitter_fern_external_scanner_scan(scanner, &mock.lexer, valid));
    check_reset(scanner);
}

/** Exercise both former 16-bit edges, the explicit column ceiling, and a stalled lexer. */
static void check_column_limits(void *scanner) {
    const uint32_t columns[] = {65535, 65536, COLUMN_MAX, COLUMN_MAX + 1};
    bool valid[] = {false, true, false};
    for (unsigned i = 0; i < 4; i++) {
        tree_sitter_fern_external_scanner_deserialize(scanner, "", 0);
        Mock mock = {.lexer = {.lookahead = ' ', .advance = mock_advance,
            .mark_end = mock_mark, .get_column = mock_column, .eof = mock_eof}, .leading = columns[i]};
        assert(tree_sitter_fern_external_scanner_scan(scanner, &mock.lexer, valid) == (i < 3));
    }
    Mock stalled = {.lexer = {.lookahead = ' ', .advance = mock_advance,
        .mark_end = mock_mark, .get_column = mock_column, .eof = mock_eof}, .column = 0};
    assert(!tree_sitter_fern_external_scanner_scan(scanner, &stalled.lexer, valid));
    check_reset(scanner);
}

/** Preserve the post-dedent separator across snapshots and emit it exactly once. */
static void check_boundary(void *scanner) {
    tree_sitter_fern_external_scanner_deserialize(scanner, "", 0);
    Mock mock = {.lexer = {.lookahead = ' ', .advance = mock_advance,
        .mark_end = mock_mark, .get_column = mock_column, .eof = mock_eof}, .leading = 4};
    bool valid[] = {true, true, true};
    assert(tree_sitter_fern_external_scanner_scan(scanner, &mock.lexer, valid));
    assert(mock.lexer.result_symbol == INDENT);
    mock.column = 0;
    assert(tree_sitter_fern_external_scanner_scan(scanner, &mock.lexer, valid));
    assert(mock.lexer.result_symbol == DEDENT);
    char state[TREE_SITTER_SERIALIZATION_BUFFER_SIZE];
    unsigned size = tree_sitter_fern_external_scanner_serialize(scanner, state);
    assert(size == 6 && (unsigned char)state[1] == 128);
    tree_sitter_fern_external_scanner_deserialize(scanner, state, size);
    assert(tree_sitter_fern_external_scanner_scan(scanner, &mock.lexer, valid));
    assert(mock.lexer.result_symbol == NEWLINE);
    assert(!tree_sitter_fern_external_scanner_scan(scanner, &mock.lexer, valid));
    check_reset(scanner);
}

/** Recovery must not manufacture indentation from an interior token column. */
static void check_interior_column(void *scanner) {
    tree_sitter_fern_external_scanner_deserialize(scanner, "", 0);
    Mock mock = {.lexer = {.lookahead = 'x', .advance = mock_advance,
        .mark_end = mock_mark, .get_column = mock_column, .eof = mock_eof}, .column = 8};
    bool valid[] = {true, true, true};
    assert(!tree_sitter_fern_external_scanner_scan(scanner, &mock.lexer, valid));
    check_reset(scanner);
}

/** Malformed saved states must remain bounded under every possible serialized length. */
static void check_saved_lengths(void *scanner) {
    char state[TREE_SITTER_SERIALIZATION_BUFFER_SIZE + 1];
    memset(state, 255, sizeof(state));
    for (unsigned length = 0; length <= sizeof(state); length++) {
        tree_sitter_fern_external_scanner_deserialize(scanner, state, length);
        check_reset(scanner);
    }
}

/** Check empty, truncated, oversized and inconsistent scanner data before any native parse. */
int main(void) {
    void *scanner = tree_sitter_fern_external_scanner_create();
    assert(scanner != NULL);
    char state[TREE_SITTER_SERIALIZATION_BUFFER_SIZE + 1] = {0};
    check_reset(scanner);
    const unsigned lengths[] = {0, 1, 3, 7, sizeof(state)};
    for (unsigned i = 0; i < sizeof(lengths) / sizeof(lengths[0]); i++) {
        tree_sitter_fern_external_scanner_deserialize(scanner, state, lengths[i]);
        check_reset(scanner);
    }
    memset(state, 255, sizeof(state));
    tree_sitter_fern_external_scanner_deserialize(scanner, state, sizeof(state));
    check_reset(scanner);
    check_columns(scanner);
    check_stack(scanner);
    check_column_limits(scanner);
    check_saved_lengths(scanner);
    check_boundary(scanner);
    check_interior_column(scanner);
    tree_sitter_fern_external_scanner_destroy(scanner);
    return 0;
}
