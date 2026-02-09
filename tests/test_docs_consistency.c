/* Docs Consistency Checks */

#include "test.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

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

static char* make_tmp_dir(void) {
    char templ[] = "/tmp/fern_docs_consistency_XXXXXX";
    char* made = mkdtemp(templ);
    if (!made) return NULL;

    char* out = (char*)malloc(strlen(made) + 1);
    if (!out) return NULL;
    strcpy(out, made);
    return out;
}

void test_docs_consistency_script_passes_on_repository_docs(void) {
    CmdResult result = run_cmd("python3 scripts/check_docs_consistency.py --root . 2>&1");
    ASSERT_EQ(result.exit_code, 0);
    ASSERT_NOT_NULL(result.output);
    free(result.output);
}

void test_docs_consistency_fails_when_required_roadmap_marker_missing(void) {
    char* tmp = make_tmp_dir();
    ASSERT_NOT_NULL(tmp);

    char cmd[8192];
    snprintf(
        cmd,
        sizeof(cmd),
        "mkdir -p %s/docs && "
        "cp README.md BUILD.md ROADMAP.md DECISIONS.md DESIGN.md FERN_STYLE.md CLAUDE.md %s && "
        "cp docs/README.md %s/docs/README.md && "
        "python3 -c \"from pathlib import Path; "
        "p = Path('%s/ROADMAP.md'); "
        "text = p.read_text(encoding='utf-8'); "
        "p.write_text(text.replace('Quality gate:', 'Quality gate (missing):', 1), encoding='utf-8')\" && "
        "python3 scripts/check_docs_consistency.py --root %s 2>&1",
        tmp,
        tmp,
        tmp,
        tmp,
        tmp
    );

    CmdResult result = run_cmd(cmd);
    ASSERT_EQ(result.exit_code, 1);
    ASSERT_NOT_NULL(result.output);
    ASSERT_TRUE(strstr(result.output, "missing status marker") != NULL);

    free(result.output);
    snprintf(cmd, sizeof(cmd), "rm -rf %s", tmp);
    CmdResult cleanup = run_cmd(cmd);
    free(cleanup.output);
    free(tmp);
}

void run_docs_consistency_tests(void) {
    printf("\n=== Docs Consistency Tests ===\n");
    TEST_RUN(test_docs_consistency_script_passes_on_repository_docs);
    TEST_RUN(test_docs_consistency_fails_when_required_roadmap_marker_missing);
}
