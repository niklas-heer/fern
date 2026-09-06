/* Canonical Example Integration Tests */

#ifdef __linux__
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#endif

#include "test.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>

typedef struct {
    int exit_code;
    char* output;
} CmdResult;

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

static char* make_tmp_output_path(void) {
    char tmpl[] = "/tmp/fern_canonical_example_out_XXXXXX";
    int fd = mkstemp(tmpl);
    if (fd < 0) return NULL;
    close(fd);
    unlink(tmpl);

    char* path = (char*)malloc(strlen(tmpl) + 1);
    if (!path) return NULL;
    strcpy(path, tmpl);
    return path;
}

void test_canonical_examples_exist(void) {
    ASSERT_EQ(access("examples/tiny_cli.fn", F_OK), 0);
    ASSERT_EQ(access("examples/http_api.fn", F_OK), 0);
    ASSERT_EQ(access("examples/actor_app.fn", F_OK), 0);
}

void test_canonical_examples_type_check(void) {
    const char* examples[] = {
        "examples/tiny_cli.fn",
        "examples/http_api.fn",
        "examples/actor_app.fn",
        NULL
    };

    for (int i = 0; examples[i] != NULL; i++) {
        char cmd[512];
        snprintf(cmd, sizeof(cmd), "./bin/fern check %s 2>&1", examples[i]);
        CmdResult result = run_cmd(cmd);
        ASSERT_EQ(result.exit_code, 0);
        ASSERT_NOT_NULL(result.output);
        ASSERT_TRUE(strstr(result.output, "No type errors") != NULL);
        free(result.output);
    }
}

void test_canonical_examples_build(void) {
    const char* examples[] = {
        "examples/tiny_cli.fn",
        "examples/http_api.fn",
        "examples/actor_app.fn",
        NULL
    };

    for (int i = 0; examples[i] != NULL; i++) {
        char* output_path = make_tmp_output_path();
        ASSERT_NOT_NULL(output_path);

        char cmd[1536];
        snprintf(
            cmd,
            sizeof(cmd),
            "mise run runtime-lib >/dev/null 2>&1 && ./bin/fern build -o %s %s 2>&1",
            output_path,
            examples[i]
        );
        CmdResult result = run_cmd(cmd);
        ASSERT_EQ(result.exit_code, 0);
        ASSERT_NOT_NULL(result.output);
        ASSERT_TRUE(strstr(result.output, "Created executable:") != NULL);

        struct stat st = {0};
        ASSERT_EQ(stat(output_path, &st), 0);
        ASSERT_TRUE((st.st_mode & S_IXUSR) != 0);

        free(result.output);
        unlink(output_path);
        free(output_path);
    }
}

void run_canonical_examples_tests(void) {
    printf("\n=== Canonical Example Tests ===\n");
    TEST_RUN(test_canonical_examples_exist);
    TEST_RUN(test_canonical_examples_type_check);
    TEST_RUN(test_canonical_examples_build);
}
