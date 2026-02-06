/* Benchmark Publication Integration Tests */

#include "test.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
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
    if (!pipe) return result;

    result.output = read_pipe_all(pipe);
    int status = pclose(pipe);
    if (WIFEXITED(status)) {
        result.exit_code = WEXITSTATUS(status);
    }
    return result;
}

static char* read_file_text(const char* path) {
    struct stat st = {0};
    if (stat(path, &st) != 0) return NULL;

    FILE* file = fopen(path, "rb");
    if (!file) return NULL;

    char* text = (char*)malloc((size_t)st.st_size + 1);
    if (!text) {
        fclose(file);
        return NULL;
    }

    size_t read_n = fread(text, 1, (size_t)st.st_size, file);
    fclose(file);
    text[read_n] = '\0';
    return text;
}

void test_publish_benchmarks_script_generates_report(void) {
    char output_file[] = "/tmp/fern_benchmark_report_XXXXXX.md";
    int fd = mkstemps(output_file, 3);
    ASSERT_TRUE(fd >= 0);
    close(fd);
    unlink(output_file);

    char cmd[4096];
    snprintf(
        cmd,
        sizeof(cmd),
        "python3 scripts/publish_benchmarks.py "
        "--skip-release-build --startup-runs 5 --case-runs 3 --output %s 2>&1",
        output_file
    );

    CmdResult result = run_cmd(cmd);
    ASSERT_EQ(result.exit_code, 0);
    ASSERT_NOT_NULL(result.output);
    ASSERT_TRUE(strstr(result.output, "Wrote benchmark report:") != NULL);

    struct stat st = {0};
    ASSERT_EQ(stat(output_file, &st), 0);
    ASSERT_TRUE(st.st_size > 0);

    char* report = read_file_text(output_file);
    ASSERT_NOT_NULL(report);
    ASSERT_TRUE(strstr(report, "# Fern Benchmark Report") != NULL);
    ASSERT_TRUE(strstr(report, "## Reproduce") != NULL);
    ASSERT_TRUE(strstr(report, "### tiny_cli") != NULL);
    ASSERT_TRUE(strstr(report, "### http_api") != NULL);
    ASSERT_TRUE(strstr(report, "### actor_app") != NULL);

    free(report);
    free(result.output);
    unlink(output_file);
}

void run_benchmark_publication_tests(void) {
    printf("\n=== Benchmark Publication Tests ===\n");
    TEST_RUN(test_publish_benchmarks_script_generates_report);
}
