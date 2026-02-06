/* LSP JSON-RPC Integration Tests */

#include "test.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

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

void test_lsp_rpc_smoke_script_passes(void) {
    CmdResult result = run_cmd("python3 scripts/lsp_rpc_smoke.py 2>&1");
    ASSERT_EQ(result.exit_code, 0);
    ASSERT_NOT_NULL(result.output);
    ASSERT_TRUE(strstr(result.output, "LSP RPC smoke checks passed") != NULL);
    free(result.output);
}

void run_lsp_rpc_integration_tests(void) {
    printf("\n=== LSP RPC Integration Tests ===\n");
    TEST_RUN(test_lsp_rpc_smoke_script_passes);
}
