/* LSP Tests */

#include "test.h"
#include "arena.h"
#include "lsp.h"

#include <string.h>

static bool has_completion_label(LspCompletionItem* items, size_t count, const char* label) {
    if (!items || !label) return false;
    for (size_t i = 0; i < count; i++) {
        if (items[i].label && strcmp(string_cstr(items[i].label), label) == 0) {
            return true;
        }
    }
    return false;
}

static bool has_edit_at(LspTextEdit* edits, size_t count, int line, int character, const char* new_text) {
    if (!edits || !new_text) return false;
    for (size_t i = 0; i < count; i++) {
        if (edits[i].range.start.line == line &&
            edits[i].range.start.character == character &&
            edits[i].new_text &&
            strcmp(string_cstr(edits[i].new_text), new_text) == 0) {
            return true;
        }
    }
    return false;
}

static bool has_action_title(LspCodeAction* actions, size_t count, const char* needle) {
    if (!actions || !needle) return false;
    for (size_t i = 0; i < count; i++) {
        if (actions[i].title && strstr(string_cstr(actions[i].title), needle) != NULL) {
            return true;
        }
    }
    return false;
}

void test_lsp_diagnostics_report_precise_location(void) {
    Arena* arena = arena_create(64 * 1024);
    LspServer* server = lsp_server_new(arena, NULL);
    ASSERT_NOT_NULL(server);

    const char* uri = "file:///tmp/lsp_diag.fn";
    const char* source = "fn main() -> Int:\n    \"oops\"\n";
    lsp_document_open(server, uri, source, 1);

    size_t count = 0;
    LspDiagnostic* diags = lsp_get_diagnostics(server, uri, &count);
    ASSERT_NOT_NULL(diags);
    ASSERT_EQ(count, 1);
    ASSERT_EQ(diags[0].range.start.line, 1);
    ASSERT_TRUE(diags[0].range.start.character >= 4);
    ASSERT_TRUE(strstr(string_cstr(diags[0].message), "declared return type") != NULL);

    lsp_server_free(server);
    arena_destroy(arena);
}

void test_lsp_completion_includes_symbols_and_keywords(void) {
    Arena* arena = arena_create(64 * 1024);
    LspServer* server = lsp_server_new(arena, NULL);
    ASSERT_NOT_NULL(server);

    const char* uri = "file:///tmp/lsp_completion.fn";
    const char* source =
        "fn add(x: Int, y: Int) -> Int:\n"
        "    x + y\n"
        "\n"
        "fn main() -> Int:\n"
        "    ad\n";
    lsp_document_open(server, uri, source, 1);

    size_t count = 0;
    LspPosition symbol_pos = {.line = 4, .character = 6};
    LspCompletionItem* items = lsp_get_completions(server, uri, symbol_pos, &count);
    ASSERT_NOT_NULL(items);
    ASSERT_TRUE(count > 0);
    ASSERT_TRUE(has_completion_label(items, count, "add"));

    size_t keyword_count = 0;
    LspPosition keyword_pos = {.line = 4, .character = 4};
    LspCompletionItem* keyword_items = lsp_get_completions(server, uri, keyword_pos, &keyword_count);
    ASSERT_NOT_NULL(keyword_items);
    ASSERT_TRUE(keyword_count > 0);
    ASSERT_TRUE(has_completion_label(keyword_items, keyword_count, "fn"));

    lsp_server_free(server);
    arena_destroy(arena);
}

void test_lsp_rename_returns_definition_and_call_edits(void) {
    Arena* arena = arena_create(64 * 1024);
    LspServer* server = lsp_server_new(arena, NULL);
    ASSERT_NOT_NULL(server);

    const char* uri = "file:///tmp/lsp_rename.fn";
    const char* source =
        "fn add(x: Int, y: Int) -> Int:\n"
        "    x + y\n"
        "\n"
        "fn main() -> Int:\n"
        "    add(1, 2)\n";
    lsp_document_open(server, uri, source, 1);

    size_t edit_count = 0;
    LspPosition rename_pos = {.line = 4, .character = 5};
    LspTextEdit* edits = lsp_get_rename_edits(server, uri, rename_pos, "sum", &edit_count);
    ASSERT_NOT_NULL(edits);
    ASSERT_EQ(edit_count, 2);
    ASSERT_TRUE(has_edit_at(edits, edit_count, 0, 3, "sum"));
    ASSERT_TRUE(has_edit_at(edits, edit_count, 4, 4, "sum"));

    lsp_server_free(server);
    arena_destroy(arena);
}

void test_lsp_code_actions_suggest_result_fix(void) {
    Arena* arena = arena_create(64 * 1024);
    LspServer* server = lsp_server_new(arena, NULL);
    ASSERT_NOT_NULL(server);

    const char* uri = "file:///tmp/lsp_actions.fn";
    const char* source =
        "fn main() -> Int:\n"
        "    fs.read(\"notes.txt\")\n"
        "    0\n";
    lsp_document_open(server, uri, source, 1);

    size_t count = 0;
    LspRange range = {
        .start = {.line = 1, .character = 4},
        .end = {.line = 1, .character = 8},
    };
    LspCodeAction* actions = lsp_get_code_actions(server, uri, range, &count);
    ASSERT_NOT_NULL(actions);
    ASSERT_TRUE(count > 0);
    ASSERT_TRUE(has_action_title(actions, count, "Result"));

    lsp_server_free(server);
    arena_destroy(arena);
}

void run_lsp_tests(void) {
    printf("\n=== LSP Tests ===\n");
    TEST_RUN(test_lsp_diagnostics_report_precise_location);
    TEST_RUN(test_lsp_completion_includes_symbols_and_keywords);
    TEST_RUN(test_lsp_rename_returns_definition_and_call_edits);
    TEST_RUN(test_lsp_code_actions_suggest_result_fix);
}
