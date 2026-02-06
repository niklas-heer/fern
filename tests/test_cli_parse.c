/* CLI Parse Command Tests */

#include "test.h"
#include "arena.h"
#include "cli_parse.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static char* read_stream(FILE* file) {
    if (!file) return NULL;
    if (fflush(file) != 0) return NULL;
    if (fseek(file, 0, SEEK_END) != 0) return NULL;
    long size = ftell(file);
    if (size < 0) return NULL;
    if (fseek(file, 0, SEEK_SET) != 0) return NULL;
    char* buffer = (char*)malloc((size_t)size + 1);
    if (!buffer) return NULL;
    size_t read_bytes = fread(buffer, 1, (size_t)size, file);
    buffer[read_bytes] = '\0';
    return buffer;
}

typedef struct {
    int saved_fd;
    FILE* tmp;
} StderrCapture;

static bool capture_stderr_start(StderrCapture* cap) {
    if (!cap) return false;
    cap->saved_fd = -1;
    cap->tmp = NULL;

    int stderr_fd = fileno(stderr);
    cap->saved_fd = dup(stderr_fd);
    if (cap->saved_fd < 0) return false;

    cap->tmp = tmpfile();
    if (!cap->tmp) {
        close(cap->saved_fd);
        cap->saved_fd = -1;
        return false;
    }

    if (dup2(fileno(cap->tmp), stderr_fd) < 0) {
        fclose(cap->tmp);
        cap->tmp = NULL;
        close(cap->saved_fd);
        cap->saved_fd = -1;
        return false;
    }

    return true;
}

static char* capture_stderr_stop(StderrCapture* cap) {
    if (!cap || cap->saved_fd < 0 || !cap->tmp) return NULL;

    fflush(stderr);
    dup2(cap->saved_fd, fileno(stderr));
    close(cap->saved_fd);
    cap->saved_fd = -1;

    char* output = read_stream(cap->tmp);
    fclose(cap->tmp);
    cap->tmp = NULL;
    return output;
}

void test_cli_parse_golden_output(void) {
    Arena* arena = arena_create(4096);
    FILE* out = tmpfile();
    ASSERT_NOT_NULL(out);

    const char* source = "let x = 1";
    int result = fern_parse_source(arena, "sample.fn", source, out);
    ASSERT_EQ(result, 0);

    char* output = read_stream(out);
    ASSERT_NOT_NULL(output);

    const char* expected =
        "AST for sample.fn:\n\n"
        "Let:\n"
        "  pattern:\n"
        "    PatIdent: x\n"
        "  value:\n"
        "    Int: 1\n"
        "\n";

    ASSERT_STR_EQ(output, expected);

    free(output);
    fclose(out);
    arena_destroy(arena);
}

void test_cli_parse_match_output(void) {
    Arena* arena = arena_create(4096);
    FILE* out = tmpfile();
    ASSERT_NOT_NULL(out);

    const char* source = "match x: 1 -> 2, _ -> 3";
    int result = fern_parse_source(arena, "match.fn", source, out);
    ASSERT_EQ(result, 0);

    char* output = read_stream(out);
    ASSERT_NOT_NULL(output);

    const char* expected =
        "AST for match.fn:\n\n"
        "ExprStmt:\n"
        "  Match:\n"
        "    value:\n"
        "      Ident: x\n"
        "    arms: (2)\n"
        "      arm 0:\n"
        "        PatLit:\n"
        "          Int: 1\n"
        "        body:\n"
        "          Int: 2\n"
        "      arm 1:\n"
        "        PatWildcard: _\n"
        "        body:\n"
        "          Int: 3\n"
        "\n";

    ASSERT_STR_EQ(output, expected);

    free(output);
    fclose(out);
    arena_destroy(arena);
}

void test_cli_parse_list_comp_output(void) {
    Arena* arena = arena_create(4096);
    FILE* out = tmpfile();
    ASSERT_NOT_NULL(out);

    const char* source = "[x * 2 for x in nums if x > 0]";
    int result = fern_parse_source(arena, "list_comp.fn", source, out);
    ASSERT_EQ(result, 0);

    char* output = read_stream(out);
    ASSERT_NOT_NULL(output);

    const char* expected =
        "AST for list_comp.fn:\n\n"
        "ExprStmt:\n"
        "  ListComp: [... for x in ...]\n"
        "    body:\n"
        "      Binary: *\n"
        "        Ident: x\n"
        "        Int: 2\n"
        "    iterable:\n"
        "      Ident: nums\n"
        "    condition:\n"
        "      Binary: >\n"
        "        Ident: x\n"
        "        Int: 0\n"
        "\n";

    ASSERT_STR_EQ(output, expected);

    free(output);
    fclose(out);
    arena_destroy(arena);
}

void test_cli_parse_error_returns_nonzero(void) {
    Arena* arena = arena_create(4096);
    FILE* out = tmpfile();
    ASSERT_NOT_NULL(out);

    const char* source = "let = 5";
    StderrCapture cap = {0};
    ASSERT_TRUE(capture_stderr_start(&cap));
    int result = fern_parse_source(arena, "bad.fn", source, out);
    char* err_output = capture_stderr_stop(&cap);
    ASSERT_EQ(result, 1);

    char* output = read_stream(out);
    ASSERT_NOT_NULL(output);
    ASSERT_STR_EQ(output, "");

    ASSERT_NOT_NULL(err_output);
    ASSERT_TRUE(strstr(err_output, "bad.fn") != NULL);
    ASSERT_TRUE(strstr(err_output, "parse error") != NULL);

    free(output);
    free(err_output);
    fclose(out);
    arena_destroy(arena);
}

void run_cli_parse_tests(void) {
    printf("\n=== CLI Parse Tests ===\n");
    TEST_RUN(test_cli_parse_golden_output);
    TEST_RUN(test_cli_parse_match_output);
    TEST_RUN(test_cli_parse_list_comp_output);
    TEST_RUN(test_cli_parse_error_returns_nonzero);
}
