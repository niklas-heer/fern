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
    bool valid[] = {false, true, false, false, false};
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
    bool valid[] = {false, true, false, false, false};
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
    bool valid[] = {false, true, false, false, false};
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
    bool valid[] = {true, true, true, false, false};
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
    bool valid[] = {true, true, true, false, false};
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

typedef struct {
    TSLexer lexer;
    const int32_t *text;
    unsigned index, start_column, marked, advances;
    bool stalled_space;
} KeywordMock;

/** Advance bounded code points, including a non-ASCII identifier continuation. */
static void keyword_advance(TSLexer *lexer, bool skip) {
    (void)skip;
    KeywordMock *mock = (KeywordMock *)lexer;
    mock->advances++;
    if (mock->stalled_space && lexer->lookahead == ' ') return;
    if (mock->text[mock->index] != 0) mock->index++;
    lexer->lookahead = mock->text[mock->index];
}

/** Keep the token endpoint independent from bounded keyword lookahead. */
static void keyword_mark(TSLexer *lexer) {
    KeywordMock *mock = (KeywordMock *)lexer;
    mock->marked = mock->index;
}

/** Report absolute columns without relying on parser-owned memory. */
static uint32_t keyword_column(TSLexer *lexer) {
    KeywordMock *mock = (KeywordMock *)lexer;
    return mock->start_column + mock->index;
}

/** Report the end of a fixed keyword fixture. */
static bool keyword_eof(const TSLexer *lexer) { return lexer->lookahead == 0; }

/** Supply a tiny real advancing lexer while preserving the native scanner interface. */
static KeywordMock keyword_mock(const int32_t *text, unsigned column) {
    KeywordMock mock = {.lexer = {.lookahead = text[0], .advance = keyword_advance,
        .mark_end = keyword_mark, .get_column = keyword_column, .eof = keyword_eof},
        .text = text, .start_column = column};
    return mock;
}

/** Check actual token boundaries and valid-symbol discrimination without intentional test aborts. */
static bool check_keyword_case(void *scanner, const int32_t *text, unsigned column,
                               const bool *valid, int expected) {
    char before[TREE_SITTER_SERIALIZATION_BUFFER_SIZE], after[sizeof(before)];
    tree_sitter_fern_external_scanner_deserialize(scanner, "", 0);
    unsigned size = tree_sitter_fern_external_scanner_serialize(scanner, before);
    KeywordMock mock = keyword_mock(text, column);
    bool found = tree_sitter_fern_external_scanner_scan(scanner, &mock.lexer, valid);
    if (found != (expected >= 0)) return false;
    if (found && (mock.lexer.result_symbol != expected || mock.marked != 2)) return false;
    if (tree_sitter_fern_external_scanner_serialize(scanner, after) != size) return false;
    return memcmp(before, after, size) == 0;
}

/** Keyword-prefixed names and continuation lambdas must not become root declarations. */
static bool check_keywords(void *scanner) {
    const bool recovery[] = {true, true, true, true, true};
    const bool ordinary[] = {false, false, false, false, true};
    const bool root[] = {false, false, false, true, false};
    const bool none[] = {false, false, false, false, false};
    const int32_t declaration[] = {'f', 'n', ' ', 'n', 'a', 'm', 'e', 0};
    const int32_t lambda[] = {'f', 'n', ' ', '(', ')', 0};
    const int32_t ascii[] = {'f', 'n', 'v', 'a', 'l', 'u', 'e', 0};
    const int32_t unicode[] = {'f', 'n', 0x3bb, 0};
    const int32_t comment[] = {'f', 'n', ' ', '/', '*', 0};
    return check_keyword_case(scanner, declaration, 0, recovery, TOP_LEVEL_FN) &&
        check_keyword_case(scanner, declaration, 4, ordinary, FN) &&
        check_keyword_case(scanner, declaration, 0, ordinary, -1) &&
        check_keyword_case(scanner, declaration, 0, root, TOP_LEVEL_FN) &&
        check_keyword_case(scanner, declaration, 0, none, -1) &&
        check_keyword_case(scanner, lambda, 0, ordinary, FN) &&
        check_keyword_case(scanner, lambda, 0, recovery, FN) &&
        check_keyword_case(scanner, lambda, 0, root, -1) &&
        check_keyword_case(scanner, comment, 0, ordinary, FN) &&
        check_keyword_case(scanner, ascii, 0, recovery, -1) &&
        check_keyword_case(scanner, unicode, 0, recovery, -1);
}

/** Bound keyword lookahead even when a hostile lexer never advances past whitespace. */
static bool check_keyword_limit(void *scanner) {
    const bool valid[] = {false, false, false, false, true};
    const int32_t text[] = {'f', 'n', ' ', 0};
    tree_sitter_fern_external_scanner_deserialize(scanner, "", 0);
    KeywordMock mock = keyword_mock(text, 0);
    mock.stalled_space = true;
    if (!tree_sitter_fern_external_scanner_scan(scanner, &mock.lexer, valid)) return false;
    return mock.lexer.result_symbol == FN && mock.marked == 2 && mock.advances == COLUMN_MAX + 2;
}

/** Drain indentation and its saved boundary before consuming a root function token. */
static bool check_keyword_dedent(void *scanner) {
    const char indented[] = {2, 0, 0, 0, 0, 0, 4, 0, 0, 0};
    const bool valid[] = {true, true, true, true, true};
    const int32_t text[] = {'f', 'n', ' ', 'n', 0};
    tree_sitter_fern_external_scanner_deserialize(scanner, indented, sizeof(indented));
    KeywordMock mock = keyword_mock(text, 0);
    const unsigned expected[] = {DEDENT, NEWLINE, TOP_LEVEL_FN};
    for (unsigned i = 0; i < 3; i++) {
        if (!tree_sitter_fern_external_scanner_scan(scanner, &mock.lexer, valid)) return false;
        if (mock.lexer.result_symbol != expected[i]) return false;
        char saved[TREE_SITTER_SERIALIZATION_BUFFER_SIZE];
        unsigned size = tree_sitter_fern_external_scanner_serialize(scanner, saved);
        tree_sitter_fern_external_scanner_deserialize(scanner, saved, size);
    }
    return mock.marked == 2;
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
    bool keywords_ok = check_keywords(scanner) && check_keyword_limit(scanner) &&
        check_keyword_dedent(scanner);
    tree_sitter_fern_external_scanner_destroy(scanner);
    return keywords_ok ? 0 : 1;
}
