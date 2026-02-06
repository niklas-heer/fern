/* CLI Main Integration Tests */

#include "test.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

static char* read_pipe_all(FILE* pipe) {
    if (!pipe) return NULL;

    size_t cap = 1024;
    size_t len = 0;
    char* buf = (char*)malloc(cap);
    if (!buf) return NULL;

    int ch = 0;
    while ((ch = fgetc(pipe)) != EOF) {
        if (len + 1 >= cap) {
            size_t next_cap = cap * 2;
            char* next = (char*)realloc(buf, next_cap);
            if (!next) {
                free(buf);
                return NULL;
            }
            buf = next;
            cap = next_cap;
        }
        buf[len++] = (char)ch;
    }
    buf[len] = '\0';
    return buf;
}

typedef struct {
    int exit_code;
    char* output;
} CmdResult;

static CmdResult run_cmd(const char* cmd) {
    CmdResult result;
    result.exit_code = -1;
    result.output = NULL;

    FILE* pipe = popen(cmd, "r");
    if (!pipe) {
        return result;
    }

    result.output = read_pipe_all(pipe);
    int status = pclose(pipe);
    if (WIFEXITED(status)) {
        result.exit_code = WEXITSTATUS(status);
    }

    return result;
}

static char* write_tmp_source(const char* source) {
    char tmpl[] = "/tmp/fern_cli_test_XXXXXX.fn";
    int fd = mkstemps(tmpl, 3);
    if (fd < 0) return NULL;

    FILE* file = fdopen(fd, "w");
    if (!file) {
        close(fd);
        unlink(tmpl);
        return NULL;
    }

    fputs(source, file);
    fclose(file);

    char* path = (char*)malloc(strlen(tmpl) + 1);
    if (!path) {
        unlink(tmpl);
        return NULL;
    }

    strcpy(path, tmpl);
    return path;
}

void test_cli_help_lists_global_flags(void) {
    CmdResult result = run_cmd("./bin/fern --help 2>&1");
    ASSERT_EQ(result.exit_code, 0);
    ASSERT_NOT_NULL(result.output);

    ASSERT_TRUE(strstr(result.output, "--color=auto|always|never") != NULL);
    ASSERT_TRUE(strstr(result.output, "--quiet") != NULL);
    ASSERT_TRUE(strstr(result.output, "--verbose") != NULL);

    free(result.output);
}

void test_cli_quiet_suppresses_check_success_output(void) {
    char* source_path = write_tmp_source("fn main():\n    0\n");
    ASSERT_NOT_NULL(source_path);

    char cmd[512];
    snprintf(cmd, sizeof(cmd), "./bin/fern check %s 2>&1", source_path);
    CmdResult normal = run_cmd(cmd);
    ASSERT_EQ(normal.exit_code, 0);
    ASSERT_NOT_NULL(normal.output);
    ASSERT_TRUE(strstr(normal.output, "No type errors") != NULL);

    snprintf(cmd, sizeof(cmd), "./bin/fern --quiet check %s 2>&1", source_path);
    CmdResult quiet = run_cmd(cmd);
    ASSERT_EQ(quiet.exit_code, 0);
    ASSERT_NOT_NULL(quiet.output);
    ASSERT_STR_EQ(quiet.output, "");

    free(normal.output);
    free(quiet.output);
    unlink(source_path);
    free(source_path);
}

void test_cli_verbose_emits_debug_lines(void) {
    char* source_path = write_tmp_source("fn main():\n    0\n");
    ASSERT_NOT_NULL(source_path);

    char cmd[512];
    snprintf(cmd, sizeof(cmd), "./bin/fern --verbose check %s 2>&1", source_path);
    CmdResult verbose = run_cmd(cmd);

    ASSERT_EQ(verbose.exit_code, 0);
    ASSERT_NOT_NULL(verbose.output);
    ASSERT_TRUE(strstr(verbose.output, "verbose: command=check") != NULL);

    free(verbose.output);
    unlink(source_path);
    free(source_path);
}

void test_cli_color_mode_always_and_never(void) {
    CmdResult always = run_cmd("./bin/fern --color=always build 2>&1");
    ASSERT_EQ(always.exit_code, 1);
    ASSERT_NOT_NULL(always.output);
    ASSERT_TRUE(strstr(always.output, "\033[") != NULL);

    CmdResult never = run_cmd("./bin/fern --color=never build 2>&1");
    ASSERT_EQ(never.exit_code, 1);
    ASSERT_NOT_NULL(never.output);
    ASSERT_TRUE(strstr(never.output, "\033[") == NULL);

    free(always.output);
    free(never.output);
}

void run_cli_main_tests(void) {
    printf("\n=== CLI Main Tests ===\n");
    TEST_RUN(test_cli_help_lists_global_flags);
    TEST_RUN(test_cli_quiet_suppresses_check_success_output);
    TEST_RUN(test_cli_verbose_emits_debug_lines);
    TEST_RUN(test_cli_color_mode_always_and_never);
}
