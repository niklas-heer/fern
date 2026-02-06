/* REPL Tests */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "test.h"
#include "arena.h"
#include "repl.h"
#include "type_env.h"
#include "type.h"
#include "fern_string.h"
#include <assert.h>
#include <unistd.h>

static void capture_stdout_begin(int* saved_stdout_fd, FILE** tmp_file);
static void capture_stdout_end(int saved_stdout_fd, FILE* tmp_file, char* buffer, size_t size);

/* Test REPL creation */
void test_repl_create(void) {
    Arena* arena = arena_create(4 * 1024 * 1024);
    ASSERT_NOT_NULL(arena);
    
    Repl* repl = repl_new(arena);
    ASSERT_NOT_NULL(repl);
    ASSERT_FALSE(repl_should_exit(repl));
    
    arena_destroy(arena);
}

/* Test evaluating integer literal */
void test_repl_eval_int_literal(void) {
    Arena* arena = arena_create(4 * 1024 * 1024);
    Repl* repl = repl_new(arena);
    ASSERT_NOT_NULL(repl);
    
    bool result = repl_eval_line(repl, "42");
    ASSERT_TRUE(result);
    
    arena_destroy(arena);
}

/* Test evaluating string literal */
void test_repl_eval_string_literal(void) {
    Arena* arena = arena_create(4 * 1024 * 1024);
    Repl* repl = repl_new(arena);
    ASSERT_NOT_NULL(repl);
    
    bool result = repl_eval_line(repl, "\"hello\"");
    ASSERT_TRUE(result);
    
    arena_destroy(arena);
}

/* Test evaluating boolean literal */
void test_repl_eval_bool_literal(void) {
    Arena* arena = arena_create(4 * 1024 * 1024);
    Repl* repl = repl_new(arena);
    ASSERT_NOT_NULL(repl);
    
    bool result = repl_eval_line(repl, "true");
    ASSERT_TRUE(result);
    
    result = repl_eval_line(repl, "false");
    ASSERT_TRUE(result);
    
    arena_destroy(arena);
}

/* Test evaluating arithmetic expression */
void test_repl_eval_expression(void) {
    Arena* arena = arena_create(4 * 1024 * 1024);
    Repl* repl = repl_new(arena);
    ASSERT_NOT_NULL(repl);
    
    bool result = repl_eval_line(repl, "1 + 2");
    ASSERT_TRUE(result);
    
    result = repl_eval_line(repl, "10 * 5");
    ASSERT_TRUE(result);
    
    arena_destroy(arena);
}

/* Test evaluating arithmetic expression prints computed value */
void test_repl_eval_expression_prints_value(void) {
    Arena* arena = arena_create(4 * 1024 * 1024);
    Repl* repl = repl_new(arena);
    ASSERT_NOT_NULL(repl);

    int saved_stdout_fd = -1;
    FILE* tmp_file = NULL;
    char output[256];

    capture_stdout_begin(&saved_stdout_fd, &tmp_file);
    bool result = repl_eval_line(repl, "1 + 2");
    ASSERT_TRUE(result);
    capture_stdout_end(saved_stdout_fd, tmp_file, output, sizeof(output));

    ASSERT_TRUE(strstr(output, "3 : Int") != NULL);

    arena_destroy(arena);
}

/* Test let binding creates variable */
void test_repl_let_binding(void) {
    Arena* arena = arena_create(4 * 1024 * 1024);
    Repl* repl = repl_new(arena);
    ASSERT_NOT_NULL(repl);
    
    // Define a variable
    bool result = repl_eval_line(repl, "let x = 42");
    ASSERT_TRUE(result);
    
    // Check type environment has the variable
    TypeEnv* env = repl_type_env(repl);
    ASSERT_NOT_NULL(env);
    
    String* name = string_new(arena, "x");
    Type* type = type_env_lookup(env, name);
    ASSERT_NOT_NULL(type);
    ASSERT_EQ(type->kind, TYPE_INT);
    
    arena_destroy(arena);
}

/* Test :quit command sets exit flag */
void test_repl_quit_command(void) {
    Arena* arena = arena_create(4 * 1024 * 1024);
    Repl* repl = repl_new(arena);
    ASSERT_NOT_NULL(repl);
    
    ASSERT_FALSE(repl_should_exit(repl));
    
    bool result = repl_eval_line(repl, ":quit");
    ASSERT_TRUE(result);
    ASSERT_TRUE(repl_should_exit(repl));
    
    arena_destroy(arena);
}

/* Test :q shorthand for quit */
void test_repl_quit_shorthand(void) {
    Arena* arena = arena_create(4 * 1024 * 1024);
    Repl* repl = repl_new(arena);
    ASSERT_NOT_NULL(repl);
    
    ASSERT_FALSE(repl_should_exit(repl));
    
    bool result = repl_eval_line(repl, ":q");
    ASSERT_TRUE(result);
    ASSERT_TRUE(repl_should_exit(repl));
    
    arena_destroy(arena);
}

/* Test :help command */
void test_repl_help_command(void) {
    Arena* arena = arena_create(4 * 1024 * 1024);
    Repl* repl = repl_new(arena);
    ASSERT_NOT_NULL(repl);
    
    bool result = repl_eval_line(repl, ":help");
    ASSERT_TRUE(result);
    ASSERT_FALSE(repl_should_exit(repl));  // Help doesn't exit
    
    arena_destroy(arena);
}

/* Test :type command */
void test_repl_type_command(void) {
    Arena* arena = arena_create(4 * 1024 * 1024);
    Repl* repl = repl_new(arena);
    ASSERT_NOT_NULL(repl);
    
    bool result = repl_eval_line(repl, ":type 42");
    ASSERT_TRUE(result);
    
    result = repl_eval_line(repl, ":t 1 + 2");
    ASSERT_TRUE(result);
    
    arena_destroy(arena);
}

/* Test unknown command */
void test_repl_unknown_command(void) {
    Arena* arena = arena_create(4 * 1024 * 1024);
    Repl* repl = repl_new(arena);
    ASSERT_NOT_NULL(repl);
    
    bool result = repl_eval_line(repl, ":unknown");
    ASSERT_FALSE(result);  // Unknown command should fail
    
    arena_destroy(arena);
}

/* Test parse error handling */
void test_repl_parse_error(void) {
    Arena* arena = arena_create(4 * 1024 * 1024);
    Repl* repl = repl_new(arena);
    ASSERT_NOT_NULL(repl);
    
    bool result = repl_eval_line(repl, "let = 5");  // Invalid syntax
    ASSERT_FALSE(result);  // Should fail but not crash
    
    // REPL should still be usable after error
    result = repl_eval_line(repl, "42");
    ASSERT_TRUE(result);
    
    arena_destroy(arena);
}

/* Test type error handling */
void test_repl_type_error(void) {
    Arena* arena = arena_create(4 * 1024 * 1024);
    Repl* repl = repl_new(arena);
    ASSERT_NOT_NULL(repl);
    
    bool result = repl_eval_line(repl, "undefined_var");  // Undefined variable
    ASSERT_FALSE(result);  // Should fail
    
    // REPL should still be usable after error
    result = repl_eval_line(repl, "42");
    ASSERT_TRUE(result);
    
    arena_destroy(arena);
}

/* Test empty line handling */
void test_repl_empty_line(void) {
    Arena* arena = arena_create(4 * 1024 * 1024);
    Repl* repl = repl_new(arena);
    ASSERT_NOT_NULL(repl);
    
    bool result = repl_eval_line(repl, "");
    ASSERT_TRUE(result);  // Empty line is OK
    
    result = repl_eval_line(repl, "   ");
    ASSERT_TRUE(result);  // Whitespace-only is OK
    
    arena_destroy(arena);
}

void run_repl_tests(void) {
    printf("\n=== REPL Tests ===\n");
    TEST_RUN(test_repl_create);
    TEST_RUN(test_repl_eval_int_literal);
    TEST_RUN(test_repl_eval_string_literal);
    TEST_RUN(test_repl_eval_bool_literal);
    TEST_RUN(test_repl_eval_expression);
    TEST_RUN(test_repl_eval_expression_prints_value);
    TEST_RUN(test_repl_let_binding);
    TEST_RUN(test_repl_quit_command);
    TEST_RUN(test_repl_quit_shorthand);
    TEST_RUN(test_repl_help_command);
    TEST_RUN(test_repl_type_command);
    TEST_RUN(test_repl_unknown_command);
    TEST_RUN(test_repl_parse_error);
    TEST_RUN(test_repl_type_error);
    TEST_RUN(test_repl_empty_line);
}
static void capture_stdout_begin(int* saved_stdout_fd, FILE** tmp_file) {
    assert(saved_stdout_fd != NULL);
    assert(tmp_file != NULL);

    *tmp_file = tmpfile();
    ASSERT_NOT_NULL(*tmp_file);

    fflush(stdout);
    *saved_stdout_fd = dup(STDOUT_FILENO);
    ASSERT_TRUE(*saved_stdout_fd >= 0);
    ASSERT_TRUE(dup2(fileno(*tmp_file), STDOUT_FILENO) >= 0);
}

static void capture_stdout_end(int saved_stdout_fd, FILE* tmp_file, char* buffer, size_t size) {
    assert(tmp_file != NULL);
    assert(buffer != NULL);
    assert(size > 0);

    fflush(stdout);
    ASSERT_TRUE(dup2(saved_stdout_fd, STDOUT_FILENO) >= 0);
    close(saved_stdout_fd);

    rewind(tmp_file);
    size_t n = fread(buffer, 1, size - 1, tmp_file);
    buffer[n] = '\0';
    fclose(tmp_file);
    return;
}
